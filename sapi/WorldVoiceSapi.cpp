// WorldVoiceSapi.cpp - WorldVoice for SAPI 5: one SAPI 5 voice that reads each part of a text with the voice chosen
// for its language, the way the WorldVoice NVDA add-on does it.
//
// Each Speak call first gets WorldVoice's number mode and punctuation pauses (Pipeline.h), then is split by language
// with WorldVoice's detection (common\WorldVoice.h): the script of every character decides, spaces and punctuation
// stay with the text around them, numbers go to the main language's voice. Each piece is then spoken by its voice's own SAPI 5 engine, created from the voice's token and driven
// directly with a relay site: the audio goes on to the program in one format (16-bit mono, at the highest rate of the
// voices chosen), every voice converted up to it with a clean filter; the events keep the original text positions, and an interrupt reaches
// the engine at once.
//
// The voices come from HKLM and HKCU\SOFTWARE\Microsoft\Speech\Voices\Tokens, as registered for the program's own
// bitness (a 32-bit program uses 32-bit voices). A voice that cannot be loaded is replaced by the main voice.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <sapi.h>
#include <sapiddk.h>
#include <sperror.h>
#include <atomic>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <map>
#include <memory>
#include <new>
#include <string>
#include <vector>
#include "../common/WorldVoice.h"
#include "Pipeline.h"
#include "Resampler.h"

static HMODULE g_module = nullptr;
static LONG g_objects = 0, g_locks = 0;

// ---------------------------------------------------------------------------
// Logging: off unless HKCU\SOFTWARE\WorldVoice has DWORD Log = 1; then appends to %TEMP%\WorldVoice.log. The switch is
// looked at again once a second, so turning it on or off (the configuration program's Log tab) takes effect at once.
// ---------------------------------------------------------------------------

static bool LogEnabled() {
    static std::atomic<ULONGLONG> checked{ 0 };
    static std::atomic<bool> on{ false };
    ULONGLONG now = GetTickCount64();
    if (now - checked.load() >= 1000) {
        DWORD v = 0;
        on = wv::ReadDword(HKEY_CURRENT_USER, wv::kSettingsKey, L"Log", v) && v;
        checked = now;
    }
    return on;
}

static void Log(const char* fmt, ...) {
    if (!LogEnabled()) return;
    char line[2048];
    va_list args;
    va_start(args, fmt);
    vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);
    wchar_t temp[MAX_PATH];
    if (!GetTempPathW(MAX_PATH, temp)) return;
    FILE* f = nullptr;
    if (_wfopen_s(&f, (std::wstring(temp) + L"WorldVoice.log").c_str(), L"a") == 0 && f) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        fprintf(f, "%02d:%02d:%02d.%03d [%lu] %s\n", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, GetCurrentThreadId(), line);
        fclose(f);
    }
}

static std::string Utf8(const std::wstring& w) {
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0');
    if (n) WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], n, nullptr, nullptr);
    return s;
}

// ---------------------------------------------------------------------------
// Audio format
// ---------------------------------------------------------------------------

// WorldVoice speaks in 16-bit mono at the highest rate of the voices chosen in it, settled when a program loads it
// (OutRate), so no voice loses anything: the others are converted up with WorldVoice's own filter (Resampler.h),
// which leaves each as it sounds on its own. It was always 44100 Hz until 31 October 2026; but NVDA's SAPI 5 driver
// stops its player in the middle of what it holds at every interrupt, and that cut clicked in 44100 Hz sound made
// from Eloquence's 11025 Hz, where Eloquence on its own, at 11025 Hz, does not click (user report). At its own rate a
// voice reaches the program exactly as it would without WorldVoice. 44100 Hz is what it gives when no voice loads.
static WAVEFORMATEX Pcm16Mono(DWORD rate) {
    WAVEFORMATEX w = { WAVE_FORMAT_PCM, 1, rate, rate * 2, 2, 16, 0 };
    return w;
}
static const DWORD kDefaultRate = 44100;
static bool SameFormat(const WAVEFORMATEX& a, const WAVEFORMATEX& b) {
    return a.wFormatTag == b.wFormatTag && a.nChannels == b.nChannels && a.nSamplesPerSec == b.nSamplesPerSec && a.wBitsPerSample == b.wBitsPerSample;
}

// ---------------------------------------------------------------------------
// One voice WorldVoice speaks through: its engine, created from its token, and the format it speaks in.
// ---------------------------------------------------------------------------

struct SubVoice {
    std::wstring id;
    ISpObjectToken* token = nullptr;
    ISpTTSEngine* engine = nullptr;
    GUID fmtId = GUID_NULL;
    WAVEFORMATEX* wfx = nullptr;

    ~SubVoice() {
        if (wfx) CoTaskMemFree(wfx);
        if (engine) engine->Release();
        if (token) token->Release();
    }

    HRESULT Create(const std::wstring& tokenId) {
        id = tokenId;
        HRESULT hr = CoCreateInstance(CLSID_SpObjectToken, nullptr, CLSCTX_ALL, IID_ISpObjectToken, (void**)&token);
        if (SUCCEEDED(hr)) hr = token->SetId(nullptr, tokenId.c_str(), FALSE);
        if (SUCCEEDED(hr)) hr = token->CreateInstance(nullptr, CLSCTX_INPROC_SERVER, IID_ISpTTSEngine, (void**)&engine);
        return hr;
    }

    // The format the voice speaks in when asked for this one, as SAPI asks a voice before each Speak (the voice may
    // take the format asked for, or give its own, which its settings may change).
    HRESULT Format(const GUID* targetId, const WAVEFORMATEX* target) {
        GUID id2 = GUID_NULL;
        WAVEFORMATEX* answer = nullptr;
        HRESULT hr = engine->GetOutputFormat(targetId, target, &id2, &answer);
        if (SUCCEEDED(hr) && (id2 != SPDFID_WaveFormatEx || !answer)) hr = SPERR_UNSUPPORTED_FORMAT;
        if (FAILED(hr)) { if (answer) CoTaskMemFree(answer); return hr; }
        if (wfx) CoTaskMemFree(wfx);
        fmtId = id2;
        wfx = answer;
        return S_OK;
    }
};

// ---------------------------------------------------------------------------
// The audio of one Speak call on its way to SAPI's site, from every voice that reads a piece of it.
//
// NVDA's SAPI 5 driver (2026.3, synthDrivers\sapi5) lets a bit of an interrupted speech through, heard as a click when
// arrow keys interrupt quickly (user report, 2026-10-31; Microsoft's voices click there too). Two ways:
// - At the start of a speech it keeps the audio back until it has 50 ms, and its cancel does not empty what it kept:
//   a speech interrupted before then left its start behind, and the next one began with it, cut short. So the first
//   60 ms of a speech go in one write, which it plays at once; a speech shorter than that goes whole at its end.
// - It takes up audio again before the voice has been told to stop. So the audio goes in writes of 10 ms, none once
//   the speech is stopped, and while the driver cancels it takes none of a write (it reports 0 bytes): then nothing
//   more is written until the stop comes (half a second at most, once a speech).
// ---------------------------------------------------------------------------

class Output {
public:
    Output(ISpTTSEngineSite* site, DWORD rate) : m_site(site), m_rate(rate) {}

    HRESULT Write(const int16_t* s, size_t n) {
        if (!m_started) {
            m_start.insert(m_start.end(), s, s + n);
            if (m_start.size() < m_rate * 60 / 1000) return (m_site->GetActions() & SPVES_ABORT) ? E_ABORT : S_OK;
            return WriteStart(false);
        }
        const size_t piece = m_rate / 100;
        for (size_t at = 0; at < n; at += piece) {
            if (m_site->GetActions() & SPVES_ABORT) return E_ABORT;
            HRESULT hr = Pass(s + at, (std::min)(piece, n - at), true);
            if (FAILED(hr)) return hr;
        }
        return S_OK;
    }
    // The end of the speech: a start shorter than 60 ms goes now (the driver plays it at the end of the speech)
    HRESULT Finish() { return WriteStart(true); }

private:
    HRESULT WriteStart(bool end) {
        m_started = true;
        if (m_start.empty()) return S_OK;
        std::vector<int16_t> start;
        start.swap(m_start);
        if (m_site->GetActions() & SPVES_ABORT) return E_ABORT;
        return Pass(start.data(), start.size(), !end);
    }
    HRESULT Pass(const int16_t* s, size_t n, bool wait) {
        ULONG asked = (ULONG)(n * 2), done = 0;
        HRESULT hr = m_site->Write(s, asked, &done);
        if (FAILED(hr) || done || !wait || m_waited) return hr;
        m_waited = true;
        ULONGLONG until = GetTickCount64() + 500;
        while (GetTickCount64() < until) {
            if (m_site->GetActions() & SPVES_ABORT) {
                Log("speak: the program took none of the audio and stopped the speech; nothing more was written");
                return E_ABORT;
            }
            Sleep(1);
        }
        Log("speak: the program took none of the audio, but did not stop the speech; it goes on");
        return S_OK;
    }

    ISpTTSEngineSite* m_site;
    DWORD m_rate;
    std::vector<int16_t> m_start;      // the start of the speech, until it is 60 ms
    bool m_started = false, m_waited = false;
};

// ---------------------------------------------------------------------------
// The site a voice speaks into: its audio goes on to SAPI's site in WorldVoice's format, its events with their
// audio positions moved onto WorldVoice's stream. Everything else is SAPI's site's answer, except the language's
// adjustments: the rate is the program's plus the language's rate (SAPI's -10..10), and the volume is scaled by
// 10% per step of the language's volume (so it can also go above the program's).
//
// Faster language switches: a voice starts and ends what it says with silence, as if the text began and ended
// there (Tingting ends with 0.6 s of it, Zira with 0.75 s). So the silence at the start of a piece is cut to
// 20 ms, and at the end of a piece that another one follows, to a natural pause (see Finish). Silence inside a
// piece is kept. Cuts happen only in silence, so nothing clicks. Events wait until the audio up to their position
// has been decided, and then go on with their positions moved to match.
// ---------------------------------------------------------------------------

class RelaySite : public ISpTTSEngineSite {
public:
    // filter: the conversion from the voice's rate to WorldVoice's, null when they are the same.
    RelaySite(ISpTTSEngineSite* real, Output& output, ULONGLONG outBase, const WAVEFORMATEX& in, const WAVEFORMATEX& out,
              std::shared_ptr<const wv::ResampleFilter> filter, const wv::Adjustment& adjust)
        : m_real(real), m_output(output), m_base(outBase), m_inFull(&in), m_in(in), m_outFormat(out), m_leadKeep(out.nSamplesPerSec * 20 / 1000),
          m_rateAdjust(adjust.rate), m_gain(1.0 + adjust.volume / 10.0) {
        if (filter) m_resampler.reset(new wv::Resampler(std::move(filter)));
    }
    ~RelaySite() {
        for (Queued& q : m_events) Free(q);
        if (m_conv) m_conv->Release();
        if (m_stream) m_stream->Release();
        if (m_memory) m_memory->Release();
    }

    // A voice that does not speak 16-bit mono (8-bit, stereo, ...): SAPI's own converter makes it 16-bit mono, at the
    // voice's own rate, into a memory stream. (Its rate conversion is crude; the rate is the Resampler's job.)
    HRESULT Init() {
        WAVEFORMATEX mono = Pcm16Mono(m_in.nSamplesPerSec);
        if (SameFormat(m_in, mono)) return S_OK;
        HRESULT hr = CreateStreamOnHGlobal(nullptr, TRUE, &m_memory);
        if (SUCCEEDED(hr)) hr = CoCreateInstance(CLSID_SpStream, nullptr, CLSCTX_ALL, IID_ISpStream, (void**)&m_stream);
        if (SUCCEEDED(hr)) hr = m_stream->SetBaseStream(m_memory, SPDFID_WaveFormatEx, &mono);
        if (SUCCEEDED(hr)) hr = CoCreateInstance(CLSID_SpStreamFormatConverter, nullptr, CLSCTX_ALL, IID_ISpStreamFormatConverter, (void**)&m_conv);
        if (SUCCEEDED(hr)) hr = m_conv->SetBaseStream(m_stream, FALSE, TRUE);
        if (SUCCEEDED(hr)) hr = m_conv->SetFormat(SPDFID_WaveFormatEx, m_inFull);
        return hr;
    }

    // After the voice's Speak: the converter's last samples, then the silence the voice ended with is cut to keepMs
    // when another piece follows (more = true), or kept whole at the very end. The events still waiting go on.
    HRESULT Finish(bool more, ULONG keepMs) {
        if (m_resampler) {
            m_resampled.clear();
            m_resampler->Flush(m_resampled);
            Feed(m_resampled.data(), m_resampled.size());
        }
        size_t keep = m_pending.size();
        if (more) keep = (std::min)(keep, (size_t)(m_outFormat.nSamplesPerSec * keepMs / 1000));
        HRESULT hr = keep ? WriteOut(m_pending.data(), keep) : S_OK;
        m_pending.clear();
        m_decided = m_raw;
        m_tailCut = true;
        ForwardEvents();
        return hr;
    }
    ULONGLONG OutBytes() const { return m_written * 2; }

    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_ISpEventSink || riid == IID_ISpTTSEngineSite) { *ppv = static_cast<ISpTTSEngineSite*>(this); return S_OK; }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return 2; }     // lives on the stack for one Speak call
    STDMETHODIMP_(ULONG) Release() override { return 1; }

    STDMETHODIMP AddEvents(const SPEVENT* events, ULONG count) override {
        if (!events) return S_OK;
        for (ULONG i = 0; i < count; i++) {
            // The event's position in the voice's audio, in samples, then in WorldVoice's.
            ULONGLONG offset = events[i].ullAudioStreamOffset, sample = offset / (m_in.nBlockAlign ? m_in.nBlockAlign : 2);
            if (m_conv) {
                ULONGLONG base = 0;
                if (SUCCEEDED(m_conv->ScaleConvertedToBaseOffset(offset, &base))) sample = base / 2;
            }
            if (m_resampler) sample = m_resampler->OutPosition(sample);
            Queued q = { events[i], sample, std::wstring() };
            // The voice may free what an event points to once AddEvents returns: keep a copy.
            if (q.e.elParamType == SPET_LPARAM_IS_STRING && q.e.lParam) { q.text = reinterpret_cast<const wchar_t*>(q.e.lParam); }
            else if (q.e.elParamType == SPET_LPARAM_IS_POINTER && q.e.lParam && q.e.wParam) {
                void* copy = CoTaskMemAlloc(q.e.wParam);
                if (copy) memcpy(copy, reinterpret_cast<const void*>(q.e.lParam), q.e.wParam);
                q.e.lParam = (LPARAM)copy;
            } else if ((q.e.elParamType == SPET_LPARAM_IS_TOKEN || q.e.elParamType == SPET_LPARAM_IS_OBJECT) && q.e.lParam) {
                reinterpret_cast<IUnknown*>(q.e.lParam)->AddRef();
            }
            m_events.push_back(q);
        }
        ForwardEvents();
        return S_OK;
    }
    STDMETHODIMP GetEventInterest(ULONGLONG* interest) override { return m_real->GetEventInterest(interest); }
    STDMETHODIMP_(DWORD) GetActions() override { return m_real->GetActions(); }
    STDMETHODIMP GetRate(long* rate) override {
        HRESULT hr = m_real->GetRate(rate);
        if (SUCCEEDED(hr) && rate && m_rateAdjust) {
            long r = *rate + m_rateAdjust;
            *rate = r < -10 ? -10 : (r > 10 ? 10 : r);
        }
        return hr;
    }
    STDMETHODIMP GetVolume(USHORT* volume) override { return m_real->GetVolume(volume); }
    STDMETHODIMP GetSkipInfo(SPVSKIPTYPE* type, long* count) override { return m_real->GetSkipInfo(type, count); }
    STDMETHODIMP CompleteSkip(long skipped) override { return m_real->CompleteSkip(skipped); }

    STDMETHODIMP Write(const void* data, ULONG size, ULONG* written) override {
        if (written) *written = size;
        if (!size) return S_OK;
        if (!m_conv) return Convert(static_cast<const int16_t*>(data), size / 2);
        ULONG used = 0;
        HRESULT hr = m_conv->Write(data, size, &used);
        if (FAILED(hr)) return hr;
        LARGE_INTEGER zero = {};
        ULARGE_INTEGER end = {};
        m_memory->Seek(zero, STREAM_SEEK_CUR, &end);
        if (end.QuadPart > m_drained) {
            HGLOBAL mem = nullptr;
            GetHGlobalFromStream(m_memory, &mem);
            const BYTE* bytes = static_cast<const BYTE*>(GlobalLock(mem));
            if (bytes) {
                ULONG count = (ULONG)(end.QuadPart - m_drained);
                hr = Convert(reinterpret_cast<const int16_t*>(bytes + m_drained), count / 2);
                GlobalUnlock(mem);
                m_drained = end.QuadPart;
            }
        }
        return hr;
    }

private:
    struct Queued { SPEVENT e; ULONGLONG raw; std::wstring text; };   // raw: position in the converted audio, before the cuts, in samples

    static const int kSilence = 100;                              // |sample| below this counts as silence (-50 dB)

    // The voice's 16-bit mono audio, at its own rate, to WorldVoice's rate.
    HRESULT Convert(const int16_t* s, size_t n) {
        if (!m_resampler) return Feed(s, n);
        m_resampled.clear();
        m_resampler->Process(s, n, m_resampled);
        return Feed(m_resampled.data(), m_resampled.size());
    }

    // The voice's audio in WorldVoice's format: the language's volume, then the cuts described above.
    HRESULT Feed(const int16_t* s, size_t n) {
        m_scaled.assign(s, s + n);
        if (m_gain != 1.0)
            for (int16_t& v : m_scaled) {
                long x = lround(v * m_gain);
                v = (int16_t)(x > 32767 ? 32767 : (x < -32768 ? -32768 : x));
            }
        // Quiet samples wait in m_pending until sound follows (then they go out in front of it) or the piece ends
        // (see Finish). At the start, only the first m_leadKeep of them stay; the rest goes.
        m_out.clear();
        for (int16_t v : m_scaled) {
            if (abs(v) < kSilence) {
                if (!m_leading) m_pending.push_back(v);
                else if (m_leadKept < m_leadKeep) { m_pending.push_back(v); m_leadKept++; }
                else m_leadDropped++;
                continue;
            }
            m_leading = false;
            if (!m_pending.empty()) { m_out.insert(m_out.end(), m_pending.begin(), m_pending.end()); m_pending.clear(); }
            m_out.push_back(v);
        }
        HRESULT hr = WriteOut(m_out.data(), m_out.size());
        m_raw += n;
        m_decided = m_raw - m_pending.size();
        ForwardEvents();
        return hr;
    }

    HRESULT WriteOut(const int16_t* s, size_t n) {
        if (!n) return S_OK;
        HRESULT hr = m_output.Write(s, n);
        m_written += n;
        return hr;
    }

    // Where a position in the voice's audio ends up in what was written.
    ULONGLONG Map(ULONGLONG raw) const {
        ULONGLONG cutBefore = raw > m_leadKeep ? (std::min)(raw - m_leadKeep, m_leadDropped) : 0;
        ULONGLONG pos = raw - cutBefore;
        if (m_tailCut && pos > m_written) pos = m_written;
        return pos;
    }

    // Hands on, in order, the events whose audio has been decided.
    void ForwardEvents() {
        size_t n = 0;
        while (n < m_events.size() && (m_tailCut || m_events[n].raw <= m_decided)) n++;
        if (!n) return;
        std::vector<SPEVENT> out;
        for (size_t i = 0; i < n; i++) {
            SPEVENT e = m_events[i].e;
            e.ullAudioStreamOffset = m_base + Map(m_events[i].raw) * 2;
            if (e.elParamType == SPET_LPARAM_IS_STRING && e.lParam) e.lParam = (LPARAM)m_events[i].text.c_str();
            out.push_back(e);
        }
        m_real->AddEvents(out.data(), (ULONG)out.size());
        for (size_t i = 0; i < n; i++) Free(m_events[i]);
        m_events.erase(m_events.begin(), m_events.begin() + n);
    }

    static void Free(Queued& q) {
        if (q.e.elParamType == SPET_LPARAM_IS_POINTER && q.e.lParam) CoTaskMemFree(reinterpret_cast<void*>(q.e.lParam));
        else if ((q.e.elParamType == SPET_LPARAM_IS_TOKEN || q.e.elParamType == SPET_LPARAM_IS_OBJECT) && q.e.lParam) reinterpret_cast<IUnknown*>(q.e.lParam)->Release();
        q.e.lParam = 0;
    }

    ISpTTSEngineSite* m_real;
    Output& m_output;
    ULONGLONG m_base, m_drained = 0;
    const WAVEFORMATEX* m_inFull;                                  // with any extension (cbSize), for the converter
    WAVEFORMATEX m_in, m_outFormat;
    size_t m_leadKeep;                                             // silence kept at a piece's start: 20 ms
    long m_rateAdjust;
    double m_gain;
    std::vector<int16_t> m_scaled, m_pending, m_out, m_resampled;
    std::unique_ptr<wv::Resampler> m_resampler;
    bool m_leading = true, m_tailCut = false;
    size_t m_leadKept = 0;
    ULONGLONG m_leadDropped = 0, m_raw = 0, m_decided = 0, m_written = 0;   // in samples
    std::vector<Queued> m_events;
    ISpStreamFormatConverter* m_conv = nullptr;
    ISpStream* m_stream = nullptr;
    IStream* m_memory = nullptr;
};

// ---------------------------------------------------------------------------
// The engine
// ---------------------------------------------------------------------------

class WorldVoiceEngine : public ISpTTSEngine, public ISpObjectWithToken {
public:
    WorldVoiceEngine() { InterlockedIncrement(&g_objects); }
    ~WorldVoiceEngine() {
        m_subs.clear();
        if (m_token) m_token->Release();
        InterlockedDecrement(&g_objects);
    }

    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (riid == IID_IUnknown || riid == __uuidof(ISpTTSEngine)) *ppv = static_cast<ISpTTSEngine*>(this);
        else if (riid == __uuidof(ISpObjectWithToken)) *ppv = static_cast<ISpObjectWithToken*>(this);
        else { *ppv = nullptr; return E_NOINTERFACE; }
        AddRef();
        return S_OK;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&m_refs); }
    STDMETHODIMP_(ULONG) Release() override {
        ULONG n = InterlockedDecrement(&m_refs);
        if (n == 0) delete this;
        return n;
    }

    STDMETHODIMP SetObjectToken(ISpObjectToken* token) override {
        if (!token) return E_INVALIDARG;
        if (m_token) return SPERR_ALREADY_INITIALIZED;
        m_token = token;
        m_token->AddRef();
        // The token's own language: text SAPI marks with another language (a <lang> tag, NVDA's language
        // switching) is taken as that language, as WorldVoice takes NVDA's LangChangeCommand.
        ISpDataKey* attrs = nullptr;
        if (SUCCEEDED(m_token->OpenKey(L"Attributes", &attrs))) {
            wchar_t* lang = nullptr;
            if (SUCCEEDED(attrs->GetStringValue(L"Language", &lang)) && lang) { m_tokenLangId = (LANGID)wcstoul(lang, nullptr, 16); CoTaskMemFree(lang); }
            attrs->Release();
        }
        return S_OK;
    }
    STDMETHODIMP GetObjectToken(ISpObjectToken** token) override {
        if (!token) return E_POINTER;
        *token = m_token;
        if (m_token) m_token->AddRef();
        return m_token ? S_OK : S_FALSE;
    }

    // Always the one format settled when the program first asks (see Pcm16Mono): programs open their audio player
    // once, when they load the voice (NVDA does), so the format must not change while it is loaded.
    STDMETHODIMP GetOutputFormat(const GUID*, const WAVEFORMATEX*, GUID* fmtId, WAVEFORMATEX** wfx) override {
        if (!fmtId || !wfx) return E_POINTER;
        WAVEFORMATEX* out = (WAVEFORMATEX*)CoTaskMemAlloc(sizeof(WAVEFORMATEX));
        if (!out) return E_OUTOFMEMORY;
        *out = Pcm16Mono(OutRate());
        *fmtId = SPDFID_WaveFormatEx;
        *wfx = out;
        return S_OK;
    }

    STDMETHODIMP Speak(DWORD flags, REFGUID, const WAVEFORMATEX* format, const SPVTEXTFRAG* frags, ISpTTSEngineSite* site) override;

private:
    struct Piece { std::wstring language; std::vector<SPVTEXTFRAG> frags; };

    void Refresh(wv::Settings& settings, wv::VoiceMap& map);
    DWORD OutRate();
    SubVoice* Voice(const std::wstring& id);
    void KeepChosenVoices(const wv::VoiceMap& map);
    std::shared_ptr<const wv::ResampleFilter> FilterFrom(DWORD rate);
    void Split(const wv::Detector& detector, const std::wstring& base, const SPVTEXTFRAG* frags, const std::vector<bool>& inNumber, std::vector<Piece>& pieces);

    LONG m_refs = 1;
    ISpObjectToken* m_token = nullptr;
    LANGID m_tokenLangId = 0x409;
    std::map<std::wstring, std::unique_ptr<SubVoice>> m_subs;     // by token id; null = could not be loaded
    std::vector<wv::VoiceInfo> m_voices;
    ULONGLONG m_voicesTime = 0;
    std::map<DWORD, std::shared_ptr<const wv::ResampleFilter>> m_filters;   // by the voice's rate, made once
    std::atomic<DWORD> m_outRate{ 0 };                            // WorldVoice's rate, once settled (OutRate)
    SRWLOCK m_rateLock = SRWLOCK_INIT;
};

// WorldVoice's rate, settled the first time it is needed (when a program loads the voice and asks its format): the
// highest rate the chosen voices speak in when asked for 44100 Hz (with none chosen, the voice that reads
// everything). It stays while the program has the voice loaded: choosing a voice with a higher rate, or changing a
// voice's own rate, is heard as such once the program loads WorldVoice again (as a voice's own rate setting is).
DWORD WorldVoiceEngine::OutRate() {
    if (DWORD rate = m_outRate) return rate;
    AcquireSRWLockExclusive(&m_rateLock);
    if (!m_outRate) {
        wv::Settings settings;
        wv::VoiceMap map;
        Refresh(settings, map);
        WAVEFORMATEX ask = Pcm16Mono(kDefaultRate);
        DWORD rate = 0;
        for (const std::wstring& lang : map.Languages()) {
            SubVoice* v = Voice(map.VoiceFor(lang));
            if (v && SUCCEEDED(v->Format(&SPDFID_WaveFormatEx, &ask)) && v->wfx->nSamplesPerSec > rate) rate = v->wfx->nSamplesPerSec;
        }
        m_outRate = rate ? rate : kDefaultRate;
        Log("format: %lu Hz 16-bit mono, the highest rate of the voices chosen", (unsigned long)m_outRate);
    }
    ReleaseSRWLockExclusive(&m_rateLock);
    return m_outRate;
}

// The conversion from a voice's rate to WorldVoice's (null when there is none to do).
std::shared_ptr<const wv::ResampleFilter> WorldVoiceEngine::FilterFrom(DWORD rate) {
    DWORD out = OutRate();
    if (!rate || rate == out) return nullptr;
    std::shared_ptr<const wv::ResampleFilter>& f = m_filters[rate];
    if (!f) f = std::make_shared<const wv::ResampleFilter>(rate, out);
    return f;
}

// The settings as they are now, and the voices (listed again after 5 s).
void WorldVoiceEngine::Refresh(wv::Settings& settings, wv::VoiceMap& map) {
    settings.Read();
    ULONGLONG now = GetTickCount64();
    if (m_voices.empty() || now - m_voicesTime > 5000) { m_voices = wv::ListVoices(); m_voicesTime = now; }
    map.Build(settings, m_voices);
}

SubVoice* WorldVoiceEngine::Voice(const std::wstring& id) {
    if (id.empty()) return nullptr;
    auto it = m_subs.find(id);
    if (it != m_subs.end()) return it->second.get();
    ULONGLONG start = GetTickCount64();
    std::unique_ptr<SubVoice> v(new (std::nothrow) SubVoice());
    HRESULT hr = v ? v->Create(id) : E_OUTOFMEMORY;
    if (FAILED(hr)) {
        Log("voice %s could not be loaded: 0x%08lx", Utf8(id).c_str(), hr);
        v.reset();
    } else {
        Log("voice %s loaded in %llu ms", Utf8(id).c_str(), GetTickCount64() - start);
    }
    SubVoice* p = v.get();
    m_subs[id] = std::move(v);
    return p;
}

// The chosen voices loaded, and no others, each time the program speaks (user request, 2026-11-02). A voice chosen while
// the program has WorldVoice loaded would otherwise load when its language first came up, and that text would wait for
// it: some 700 ms for a Vocalizer voice of 14 MB not read lately. Now it starts on the speech that follows the choice,
// usually the program saying what was chosen. A voice let go frees its memory and any helpers, or stepping through the
// list of voices would leave every one of them loaded.
void WorldVoiceEngine::KeepChosenVoices(const wv::VoiceMap& map) {
    std::set<std::wstring> chosen;
    for (const std::wstring& lang : map.Languages()) chosen.insert(map.VoiceFor(lang));
    for (auto it = m_subs.begin(); it != m_subs.end();) {
        if (chosen.count(it->first)) { ++it; continue; }
        Log("voice %s let go: no longer chosen", Utf8(it->first).c_str());
        it = m_subs.erase(it);
    }
    for (const std::wstring& id : chosen) Voice(id);
}

// Splits the text into pieces by language, as WorldVoice's add_detected_language_commands (and, for spelled
// text, process_for_spelling) does. A piece keeps the original text and positions, so its voice reports word
// positions in the program's text. Silences, bookmarks and the like go with the piece they come in. inNumber marks the
// fragments (in a row from frags) that the pipeline put inside a number language's switches; empty for none.
void WorldVoiceEngine::Split(const wv::Detector& detector, const std::wstring& base, const SPVTEXTFRAG* frags, const std::vector<bool>& inNumber, std::vector<Piece>& pieces) {
    std::wstring current = base, temporary = base;      // WorldVoice's curLang and tmpLang
    LANGID langId = m_tokenLangId;
    auto add = [&](const std::wstring& lang, const SPVTEXTFRAG& f) {
        if (pieces.empty() || wv::PrimaryLanguage(pieces.back().language) != wv::PrimaryLanguage(lang)) pieces.push_back({ lang, {} });
        pieces.back().frags.push_back(f);
        pieces.back().frags.back().pNext = nullptr;
    };
    for (const SPVTEXTFRAG* f = frags; f; f = f->pNext) {
        // A language SAPI marks on the text is NVDA's language change: it becomes the current language.
        if (f->State.LangID != langId) {
            langId = f->State.LangID;
            std::wstring marked = (langId && langId != m_tokenLangId) ? wv::LanguageOfLcid(langId) : std::wstring();
            current = marked.empty() ? base : marked;
            temporary = current;
        }
        // A number inside its number language's switches (Pipeline.h): its pieces, its symbols among them, and the
        // pauses between them are read in the number language, unless that is the voice already reading; after it,
        // the language in force again.
        if (!inNumber.empty() && inNumber[f - frags]) {
            std::wstring numberLang = detector.NumberLanguageAt(current);
            add(detector.SameVoice(numberLang, temporary) ? temporary : numberLang, *f);
            temporary = current;
            continue;
        }
        bool text = (f->State.eAction == SPVA_Speak || f->State.eAction == SPVA_Pronounce || f->State.eAction == SPVA_SpellOut) && f->pTextStart && f->ulTextLen;
        if (!text) { add(temporary, *f); continue; }
        // The configuration program's voice test: <context id="WorldVoiceTest:zh">...</context> is read wholly by
        // that language's voice, with its adjustments, without switching. Programs do not send this.
        const wchar_t* category = f->State.Context.pCategory;
        if (category && wcsncmp(category, wv::kTestContext, wcslen(wv::kTestContext)) == 0) { add(category + wcslen(wv::kTestContext), *f); continue; }
        bool spell = f->State.eAction == SPVA_SpellOut;
        std::wstring spellCurrent = current;
        std::wstring& tracked = spell ? spellCurrent : temporary;     // the detection's language
        std::wstring runLang = tracked;                                // the language of the run being gathered
        size_t runStart = 0;
        auto part = [&](size_t a, size_t b) {
            SPVTEXTFRAG p = *f;
            p.pTextStart = f->pTextStart + a;
            p.ulTextLen = (ULONG)(b - a);
            p.ulTextSrcOffset = f->ulTextSrcOffset + (ULONG)a;
            add(runLang, p);
        };
        // From at on the text is in lang: the run before ends there when that is another language.
        auto cut = [&](size_t at, const std::wstring& lang) {
            if (wv::PrimaryLanguage(lang) == wv::PrimaryLanguage(runLang)) return;
            if (at > runStart) part(runStart, at);
            runStart = at;
            runLang = lang;
        };
        // The number language (the add-on's detect_with_number_language), unless numbers follow the text around them:
        // detection sees each number wrapped in it, then in the language in force again, so the text after a number
        // is detected from the language in force. The number is read in it only where that changes the voice: the
        // add-on drops a wrapper that does not, and the number stays with the text before it.
        std::wstring numberLang = spell ? std::wstring() : detector.NumberLanguageAt(current);
        std::vector<std::pair<size_t, size_t>> numbers;
        if (!numberLang.empty()) numbers = wv::NumbersForLanguage(f->pTextStart, f->ulTextLen);
        size_t nextNumber = 0;
        for (size_t i = 0; i < f->ulTextLen;) {
            if (nextNumber < numbers.size() && i == numbers[nextNumber].first) {
                cut(i, detector.SameVoice(numberLang, runLang) ? runLang : numberLang);
                tracked = current;
                i = numbers[nextNumber++].second;
                continue;
            }
            size_t units = 1;
            uint32_t cp = wv::CodePointAt(f->pTextStart, f->ulTextLen, i, &units);
            std::wstring target = spell ? detector.LanguageForSpelling(cp, spellCurrent, current)
                                        : detector.LanguageForCharacter(cp, temporary, current);
            if (wv::PrimaryLanguage(target) != wv::PrimaryLanguage(tracked)) tracked = target;
            cut(i, tracked);
            i += units;
        }
        if (f->ulTextLen > runStart) part(runStart, f->ulTextLen);
    }
}

// The language mark a piece goes to its voice with (SAPI's standard SPVSTATE.LangID, as a <lang> tag sets it): the
// voice's own locale of that language if it lists one (a British English voice: en-GB), else the language's main
// locale. A voice that speaks one language ignores it; one that speaks several can read each piece in its language.
static LANGID LangIdFor(const std::vector<wv::VoiceInfo>& voices, const std::wstring& id, const std::wstring& lang) {
    for (const wv::VoiceInfo& v : voices) {
        if (_wcsicmp(v.id.c_str(), id.c_str()) != 0) continue;
        for (const std::wstring& l : v.locales)
            if (wv::PrimaryLanguage(l) == wv::PrimaryLanguage(lang)) return (LANGID)LocaleNameToLCID(l.c_str(), 0);
    }
    wchar_t mainLocale[LOCALE_NAME_MAX_LENGTH] = {};
    if (!ResolveLocaleName(lang.c_str(), mainLocale, LOCALE_NAME_MAX_LENGTH)) return 0;
    return (LANGID)LocaleNameToLCID(mainLocale, 0);
}

// The pause kept where one piece hands over to the next (its voice's own closing silence is cut to this): after the
// piece's last text, a sentence's end 250 ms, a comma's 150 ms, and a switch inside a phrase ("1张") 80 ms; and any
// silence asked for after that text (a <silence> tag, a punctuation pause) on top. Only text counts: a bookmark's
// fragment holds the bookmark's name.
static ULONG PauseAfter(const std::vector<SPVTEXTFRAG>& frags) {
    ULONG asked = 0;
    for (size_t f = frags.size(); f-- > 0;) {
        const SPVTEXTFRAG& t = frags[f];
        if (t.State.eAction == SPVA_Silence) { asked += t.State.SilenceMSecs; continue; }
        if ((t.State.eAction != SPVA_Speak && t.State.eAction != SPVA_SpellOut) || !t.pTextStart || !t.ulTextLen) continue;
        for (ULONG i = t.ulTextLen; i-- > 0;) {
            wchar_t c = t.pTextStart[i];
            if (iswspace(c)) continue;
            if (wcschr(L".!?…。！？．", c)) return 250 + asked;
            if (wcschr(L",;:，、；：", c)) return 150 + asked;
            return 80 + asked;
        }
    }
    return 80 + asked;
}

STDMETHODIMP WorldVoiceEngine::Speak(DWORD flags, REFGUID, const WAVEFORMATEX* format, const SPVTEXTFRAG* frags, ISpTTSEngineSite* site) {
    if (!site) return E_POINTER;
    ULONGLONG start = GetTickCount64();
    wv::Settings settings;
    wv::VoiceMap map;
    Refresh(settings, map);
    const std::vector<wv::VoiceInfo>& voices = m_voices;
    wv::Detector detector(settings, map);
    std::wstring base = map.MainLanguage();
    if (base.empty()) { Log("speak: no voices installed"); return S_OK; }
    // The format SAPI asks for is the one GetOutputFormat gave.
    WAVEFORMATEX out = Pcm16Mono(OutRate());
    KeepChosenVoices(map);
    if (format && !SameFormat(*format, out))
        Log("speak: asked for %lu Hz, %u bit, %u channel(s); writing %lu Hz 16-bit mono", format->nSamplesPerSec, format->wBitsPerSample, format->nChannels, out.nSamplesPerSec);

    // A piece's voice, and the format it speaks in when asked for WorldVoice's (a copy, with any extension). Null
    // when it cannot speak.
    auto voiceFor = [&](const std::wstring& id, GUID& fmtId, std::vector<BYTE>& fmt) -> SubVoice* {
        SubVoice* v = Voice(id);
        if (!v) return nullptr;
        HRESULT hr = v->Format(&SPDFID_WaveFormatEx, &out);
        if (FAILED(hr)) { Log("voice %s: GetOutputFormat failed 0x%08lx", Utf8(id).c_str(), hr); return nullptr; }
        fmtId = v->fmtId;
        const BYTE* b = reinterpret_cast<const BYTE*>(v->wfx);
        fmt.assign(b, b + sizeof(WAVEFORMATEX) + (v->wfx->wFormatTag == WAVE_FORMAT_PCM ? 0 : v->wfx->cbSize));
        return v;
    };

    // WorldVoice's number mode and punctuation pauses, before the language detection, as in the add-on.
    wv::Pipeline pipeline(settings, detector, base, m_tokenLangId);
    std::vector<SPVTEXTFRAG> filtered;
    std::vector<bool> numbers;
    const SPVTEXTFRAG* text = frags;
    if (pipeline.Apply(frags, filtered, numbers)) text = filtered.empty() ? nullptr : filtered.data();
    std::vector<Piece> pieces;
    Split(detector, base, text, numbers, pieces);
    if (LogEnabled()) {
        std::string list;
        for (const Piece& p : pieces) {
            size_t chars = 0;
            for (const SPVTEXTFRAG& f : p.frags) chars += f.ulTextLen;
            list += " " + Utf8(p.language) + "(" + std::to_string(chars) + ")";
        }
        Log("speak: main language %s, %lu Hz; pieces:%s", Utf8(base).c_str(), out.nSamplesPerSec, list.c_str());
    }

    ULONGLONG outBytes = 0;
    Output output(site, out.nSamplesPerSec);
    for (size_t index = 0; index < pieces.size(); index++) {
        Piece& p = pieces[index];
        if (site->GetActions() & SPVES_ABORT) break;
        GUID fmtId = GUID_NULL;
        std::vector<BYTE> fmt;
        SubVoice* v = voiceFor(map.VoiceFor(p.language), fmtId, fmt);
        if (!v) v = voiceFor(map.VoiceFor(base), fmtId, fmt);
        if (!v) continue;
        const WAVEFORMATEX* in = reinterpret_cast<const WAVEFORMATEX*>(fmt.data());
        for (size_t i = 0; i + 1 < p.frags.size(); i++) p.frags[i].pNext = &p.frags[i + 1];
        LANGID langId = LangIdFor(voices, v->id, p.language);
        if (langId) for (SPVTEXTFRAG& f : p.frags) f.State.LangID = langId;
        // The language's pitch goes on each piece of text (SAPI's -10..10); its rate and volume in the relay.
        wv::Adjustment adjust = settings.AdjustmentFor(p.language);
        if (adjust.pitch)
            for (SPVTEXTFRAG& f : p.frags) {
                long m = f.State.PitchAdj.MiddleAdj + adjust.pitch;
                f.State.PitchAdj.MiddleAdj = m < -10 ? -10 : (m > 10 ? 10 : m);
            }
        if (!SameFormat(*in, out))
            Log("voice %s speaks %lu Hz, %u bit, %u channel(s): converted", Utf8(v->id).c_str(), in->nSamplesPerSec, in->wBitsPerSample, in->nChannels);
        RelaySite relay(site, output, outBytes, *in, out, FilterFrom(in->nSamplesPerSec), adjust);
        HRESULT hr = relay.Init();
        if (FAILED(hr)) { Log("converter setup failed 0x%08lx", hr); continue; }
        hr = v->engine->Speak(flags, fmtId, in, p.frags.data(), &relay);
        if (FAILED(hr)) Log("voice %s: Speak failed 0x%08lx", Utf8(v->id).c_str(), hr);
        relay.Finish(index + 1 < pieces.size(), PauseAfter(p.frags));
        outBytes += relay.OutBytes();
        if (LogEnabled()) {
            std::wstring said;
            for (const SPVTEXTFRAG& f : p.frags)
                if (f.State.eAction == SPVA_Silence) said += L"[" + std::to_wstring(f.State.SilenceMSecs) + L" ms]";
                else if (f.pTextStart && (f.State.eAction == SPVA_Speak || f.State.eAction == SPVA_SpellOut || f.State.eAction == SPVA_Pronounce))
                    said.append(f.pTextStart, f.ulTextLen);
            Log("piece %zu (%s) \"%s\": %llu ms of audio, hr 0x%08lx", index + 1, Utf8(p.language).c_str(), Utf8(said).c_str(),
                relay.OutBytes() * 500 / out.nSamplesPerSec, hr);
        }
    }
    if (!(site->GetActions() & SPVES_ABORT)) output.Finish();
    if (LogEnabled()) Log("speak: done in %llu ms%s", GetTickCount64() - start, (site->GetActions() & SPVES_ABORT) ? " (interrupted)" : "");
    return S_OK;
}

// ---------------------------------------------------------------------------
// COM plumbing
// ---------------------------------------------------------------------------

class ClassFactory : public IClassFactory {
public:
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_IClassFactory) { *ppv = static_cast<IClassFactory*>(this); AddRef(); return S_OK; }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override { InterlockedIncrement(&g_locks); return 2; }
    STDMETHODIMP_(ULONG) Release() override { InterlockedDecrement(&g_locks); return 1; }
    STDMETHODIMP CreateInstance(IUnknown* outer, REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        *ppv = nullptr;
        if (outer) return CLASS_E_NOAGGREGATION;
        WorldVoiceEngine* e = new (std::nothrow) WorldVoiceEngine();
        if (!e) return E_OUTOFMEMORY;
        HRESULT hr = e->QueryInterface(riid, ppv);
        e->Release();
        return hr;
    }
    STDMETHODIMP LockServer(BOOL lock) override {
        if (lock) InterlockedIncrement(&g_locks); else InterlockedDecrement(&g_locks);
        return S_OK;
    }
};
static ClassFactory g_factory;

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv) {
    if (!ppv) return E_POINTER;
    *ppv = nullptr;
    if (rclsid != wv::CLSID_WorldVoice) return CLASS_E_CLASSNOTAVAILABLE;
    return g_factory.QueryInterface(riid, ppv);
}

STDAPI DllCanUnloadNow() { return (g_objects == 0 && g_locks == 0) ? S_OK : S_FALSE; }

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) { g_module = module; DisableThreadLibraryCalls(module); }
    return TRUE;
}
