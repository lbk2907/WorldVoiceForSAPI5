// WorldVoice.h - what the voice and the configuration program share: the settings, the installed SAPI 5 voices,
// and WorldVoice's language detection (the NVDA add-on's languageDetection, rule for rule).
//
// Settings, HKCU\SOFTWARE\WorldVoice:
//   MainLanguage       the main language, e.g. "en" (its voice reads numbers and anything with no script)
//   LatinLanguage      the language assumed for Latin characters when the text around them is not a Latin language
//   HanLanguage        the same for CJK characters (Han): "zh", "ja" or "ko"
//   ArabicLanguage, CyrillicLanguage, DevanagariLanguage   the same for those scripts (WorldVoice itself has
//                      settings for Latin, CJK and Arabic; the other scripts it can switch belong to one language)
//   NumbersFollowText  DWORD 1: numbers are read in the language of the text around them (WorldVoice's "ignore
//                      numbers in language detection"); 0 (the default): by the main language's voice
//   Voices\<language>  the voice for that language (a SAPI 5 voice token id); missing = none: WorldVoice does
//                      not switch to that language. With no voice chosen at all, Windows' default voice reads
//                      everything (see VoiceMap).
//   Adjustments\<language>  DWORDs Rate, Pitch, Volume, each -10..10, 0 = as the speaking program asks: rate and
//                      pitch in SAPI's steps, added to the program's; volume 10% per step
//   Log                DWORD 1: the voice writes %TEMP%\WorldVoice.log
#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <algorithm>
#include <cwctype>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "ScriptData.h"

namespace wv {

static const wchar_t* const kSettingsKey = L"SOFTWARE\\WorldVoice";
static const wchar_t* const kVoicesKey = L"SOFTWARE\\WorldVoice\\Voices";
static const wchar_t* const kAdjustmentsKey = L"SOFTWARE\\WorldVoice\\Adjustments";
static const wchar_t* const kNoVoice = L"-";
// SAPI XML <context id="WorldVoiceTest:<language>">: the configuration program's test of one language's voice.
static const wchar_t* const kTestContext = L"WorldVoiceTest:";

// {47BE0B8C-0DF1-4FB0-BBE9-5EA76A365C4C}
static const CLSID CLSID_WorldVoice = { 0x47be0b8c, 0x0df1, 0x4fb0, { 0xbb, 0xe9, 0x5e, 0xa7, 0x6a, 0x36, 0x5c, 0x4c } };
static const wchar_t* const kClsidText = L"{47BE0B8C-0DF1-4FB0-BBE9-5EA76A365C4C}";

// ---------------------------------------------------------------------------
// Characters
// ---------------------------------------------------------------------------

static const CodeRange* FindRange(const CodeRange* table, size_t n, uint32_t cp) {
    size_t lo = 0, hi = n;
    while (lo < hi) {
        size_t mid = (lo + hi) / 2;
        if (table[mid].last < cp) lo = mid + 1; else hi = mid;
    }
    return (lo < n && table[lo].first <= cp && cp <= table[lo].last) ? &table[lo] : nullptr;
}
inline const char* ScriptOf(uint32_t cp) {
    const CodeRange* r = FindRange(kScriptRanges, ARRAYSIZE(kScriptRanges), cp);
    return r ? kScriptNames[r->value] : nullptr;
}
inline bool IsDigit(uint32_t cp) { return FindRange(kDigits, ARRAYSIZE(kDigits), cp) != nullptr; }
inline bool IsSpace(uint32_t cp) { return FindRange(kSpaces, ARRAYSIZE(kSpaces), cp) != nullptr; }

// The code point at text[i], and how many UTF-16 units it takes.
inline uint32_t CodePointAt(const wchar_t* text, size_t len, size_t i, size_t* units) {
    wchar_t c = text[i];
    if (IS_HIGH_SURROGATE(c) && i + 1 < len && IS_LOW_SURROGATE(text[i + 1])) {
        *units = 2;
        return 0x10000 + (((uint32_t)c - 0xD800) << 10) + ((uint32_t)text[i + 1] - 0xDC00);
    }
    *units = 1;
    return c;
}

// ---------------------------------------------------------------------------
// Languages
// ---------------------------------------------------------------------------

// "en_US", "zh-CN" -> "en", "zh" (WorldVoice's get_primary_language).
inline std::wstring PrimaryLanguage(const std::wstring& locale) {
    std::wstring p = locale.substr(0, locale.find_first_of(L"-_"));
    for (wchar_t& c : p) c = (wchar_t)towlower(c);
    return p;
}

inline std::wstring LanguageOfLcid(LCID lcid) {
    wchar_t name[LOCALE_NAME_MAX_LENGTH] = {};
    if (!LCIDToLocaleName(lcid, name, LOCALE_NAME_MAX_LENGTH, 0)) return std::wstring();
    return PrimaryLanguage(name);
}

// "zh" -> "Chinese"
inline std::wstring LanguageName(const std::wstring& code) {
    wchar_t name[128] = {};
    if (GetLocaleInfoEx(code.c_str(), LOCALE_SENGLISHLANGUAGENAME, name, ARRAYSIZE(name)) > 0 && name[0]) return name;
    return code;
}

// The languages a script can be, in WorldVoice's order; empty for a script with none.
inline std::vector<std::wstring> ScriptLanguageList(const char* script) {
    std::vector<std::wstring> out;
    for (const ScriptLanguages& s : kScriptLanguages) {
        if (strcmp(s.script, script) != 0) continue;
        std::wstring cur;
        for (const char* p = s.languages;; p++) {
            if (*p == ' ' || *p == 0) { if (!cur.empty()) out.push_back(cur); cur.clear(); if (!*p) break; }
            else cur += (wchar_t)*p;
        }
        break;
    }
    return out;
}

// ---------------------------------------------------------------------------
// Settings
// ---------------------------------------------------------------------------

inline std::wstring ReadString(HKEY root, const wchar_t* key, const wchar_t* name) {
    wchar_t buf[1024];
    DWORD size = sizeof(buf);
    if (RegGetValueW(root, key, name, RRF_RT_REG_SZ, nullptr, buf, &size) != ERROR_SUCCESS) return std::wstring();
    return buf;
}
inline bool ReadDword(HKEY root, const wchar_t* key, const wchar_t* name, DWORD& out) {
    DWORD v = 0, size = sizeof(v);
    if (RegGetValueW(root, key, name, RRF_RT_REG_DWORD, nullptr, &v, &size) != ERROR_SUCCESS) return false;
    out = v;
    return true;
}

// A language's adjustments to the speaking program's rate, pitch and volume; all 0 = none.
struct Adjustment {
    int rate = 0, pitch = 0, volume = 0;
    bool None() const { return !rate && !pitch && !volume; }
};
inline int ClampAdjustment(int v) { return v < -10 ? -10 : (v > 10 ? 10 : v); }

struct Settings {
    std::wstring mainLanguage = L"en", latinLanguage = L"en", hanLanguage = L"zh", arabicLanguage = L"ar";
    std::wstring cyrillicLanguage = L"ru", devanagariLanguage = L"hi";
    bool numbersFollowText = false;
    std::map<std::wstring, std::wstring> voices;     // language -> token id, or kNoVoice
    std::map<std::wstring, Adjustment> adjustments;  // language -> its adjustments

    Adjustment AdjustmentFor(const std::wstring& lang) const {
        auto it = adjustments.find(PrimaryLanguage(lang));
        return it == adjustments.end() ? Adjustment() : it->second;
    }

    void Read() {
        *this = Settings();
        auto str = [](const wchar_t* name, std::wstring& out) { std::wstring v = ReadString(HKEY_CURRENT_USER, kSettingsKey, name); if (!v.empty()) out = PrimaryLanguage(v); };
        str(L"MainLanguage", mainLanguage);
        str(L"LatinLanguage", latinLanguage);
        str(L"HanLanguage", hanLanguage);
        str(L"ArabicLanguage", arabicLanguage);
        str(L"CyrillicLanguage", cyrillicLanguage);
        str(L"DevanagariLanguage", devanagariLanguage);
        DWORD d = 0;
        if (ReadDword(HKEY_CURRENT_USER, kSettingsKey, L"NumbersFollowText", d)) numbersFollowText = d != 0;
        HKEY k;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, kVoicesKey, 0, KEY_READ, &k) == ERROR_SUCCESS) {
            for (DWORD i = 0;; i++) {
                wchar_t name[128], data[1024];
                DWORD nameLen = ARRAYSIZE(name), dataSize = sizeof(data), type = 0;
                if (RegEnumValueW(k, i, name, &nameLen, nullptr, &type, (BYTE*)data, &dataSize) != ERROR_SUCCESS) break;
                if (type == REG_SZ && name[0]) voices[PrimaryLanguage(name)] = data;
            }
            RegCloseKey(k);
        }
        if (RegOpenKeyExW(HKEY_CURRENT_USER, kAdjustmentsKey, 0, KEY_READ, &k) == ERROR_SUCCESS) {
            for (DWORD i = 0;; i++) {
                wchar_t name[128];
                DWORD nameLen = ARRAYSIZE(name);
                if (RegEnumKeyExW(k, i, name, &nameLen, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) break;
                std::wstring sub = std::wstring(kAdjustmentsKey) + L"\\" + name;
                Adjustment a;
                DWORD v = 0;
                if (ReadDword(HKEY_CURRENT_USER, sub.c_str(), L"Rate", v)) a.rate = ClampAdjustment((int)v);
                if (ReadDword(HKEY_CURRENT_USER, sub.c_str(), L"Pitch", v)) a.pitch = ClampAdjustment((int)v);
                if (ReadDword(HKEY_CURRENT_USER, sub.c_str(), L"Volume", v)) a.volume = ClampAdjustment((int)v);
                if (!a.None()) adjustments[PrimaryLanguage(name)] = a;
            }
            RegCloseKey(k);
        }
    }

    // The language chosen for a script with a setting (WorldVoice's SCRIPT_CONFIG_KEYS, plus Cyrillic and
    // Devanagari), or empty.
    std::wstring ConfiguredFor(const char* script) const {
        if (!strcmp(script, "Latin")) return latinLanguage;
        if (!strcmp(script, "Han")) return hanLanguage;
        if (!strcmp(script, "Arabic")) return arabicLanguage;
        if (!strcmp(script, "Cyrillic")) return cyrillicLanguage;
        if (!strcmp(script, "Devanagari")) return devanagariLanguage;
        return std::wstring();
    }
};

// ---------------------------------------------------------------------------
// Installed voices
// ---------------------------------------------------------------------------

struct VoiceInfo {
    std::wstring id;                        // token id, as SAPI's SpObjectToken::SetId takes it
    std::wstring name;
    std::vector<std::wstring> languages;    // primary language codes, from Attributes\Language
    std::vector<std::wstring> locales;      // the full locale names, such as "en-AU"
};

// Every SAPI 5 voice registered for this process's bitness, machine-wide then per user, except WorldVoice itself.
inline std::vector<VoiceInfo> ListVoices() {
    std::vector<VoiceInfo> out;
    static const wchar_t* tokens = L"SOFTWARE\\Microsoft\\Speech\\Voices\\Tokens";
    struct Root { HKEY key; const wchar_t* prefix; } roots[] = {
        { HKEY_LOCAL_MACHINE, L"HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Speech\\Voices\\Tokens\\" },
        { HKEY_CURRENT_USER, L"HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Speech\\Voices\\Tokens\\" },
    };
    for (const Root& r : roots) {
        HKEY k;
        if (RegOpenKeyExW(r.key, tokens, 0, KEY_READ, &k) != ERROR_SUCCESS) continue;
        for (DWORD i = 0;; i++) {
            wchar_t sub[256];
            DWORD subLen = ARRAYSIZE(sub);
            if (RegEnumKeyExW(k, i, sub, &subLen, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) break;
            std::wstring path = std::wstring(tokens) + L"\\" + sub;
            std::wstring clsid = ReadString(r.key, path.c_str(), L"CLSID");
            if (clsid.empty() || _wcsicmp(clsid.c_str(), kClsidText) == 0) continue;
            VoiceInfo v;
            v.id = r.prefix + std::wstring(sub);
            std::wstring attrs = path + L"\\Attributes";
            v.name = ReadString(r.key, attrs.c_str(), L"Name");
            if (v.name.empty()) v.name = ReadString(r.key, path.c_str(), nullptr);
            if (v.name.empty()) v.name = sub;
            std::wstring langs = ReadString(r.key, attrs.c_str(), L"Language");
            size_t pos = 0;
            while (pos <= langs.size()) {
                size_t semi = langs.find(L';', pos);
                std::wstring hex = langs.substr(pos, semi == std::wstring::npos ? std::wstring::npos : semi - pos);
                wchar_t locale[LOCALE_NAME_MAX_LENGTH] = {};
                if (!hex.empty() && LCIDToLocaleName((LCID)wcstoul(hex.c_str(), nullptr, 16), locale, LOCALE_NAME_MAX_LENGTH, 0)) {
                    std::wstring code = PrimaryLanguage(locale);
                    if (std::find(v.languages.begin(), v.languages.end(), code) == v.languages.end()) v.languages.push_back(code);
                    v.locales.push_back(locale);
                }
                if (semi == std::wstring::npos) break;
                pos = semi + 1;
            }
            out.push_back(v);
        }
        RegCloseKey(k);
    }
    return out;
}

// A good voice for a language when none was chosen: Windows' default SAPI 5 voice if it speaks the language, else
// a voice of the language's main locale (English: en-US, Chinese: zh-CN, Arabic: ar-SA), else the first one.
inline std::wstring AutomaticVoice(const std::vector<VoiceInfo>& voices, const std::wstring& lang) {
    std::wstring defaultId = ReadString(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Speech\\Voices", L"DefaultTokenId");
    wchar_t mainLocale[LOCALE_NAME_MAX_LENGTH] = {};
    ResolveLocaleName(lang.c_str(), mainLocale, LOCALE_NAME_MAX_LENGTH);
    const VoiceInfo* first = nullptr;
    const VoiceInfo* main = nullptr;
    for (const VoiceInfo& v : voices) {
        if (std::find(v.languages.begin(), v.languages.end(), lang) == v.languages.end()) continue;
        if (!defaultId.empty() && _wcsicmp(v.id.c_str(), defaultId.c_str()) == 0) return v.id;
        if (!first) first = &v;
        if (!main && mainLocale[0])
            for (const std::wstring& l : v.locales) if (_wcsicmp(l.c_str(), mainLocale) == 0) { main = &v; break; }
    }
    return main ? main->id : (first ? first->id : std::wstring());
}

// Which voice reads each language: only the languages a voice was chosen for (WorldVoice switches to no other).
// With none chosen yet, one voice reads everything: Windows' default SAPI 5 voice, else an American English
// voice, else the first one (see AutomaticVoice); IsFallback() then says so.
class VoiceMap {
public:
    void Build(const Settings& s, const std::vector<VoiceInfo>& voices) {
        m_voices.clear();
        m_languages.clear();
        m_fallback = false;
        for (const auto& kv : s.voices)
            if (kv.second != kNoVoice && Find(voices, kv.second)) { m_languages.insert(kv.first); m_voices[kv.first] = kv.second; }
        if (m_languages.empty()) {
            std::wstring defaultId = ReadString(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Speech\\Voices", L"DefaultTokenId");
            const VoiceInfo* v = nullptr;
            for (const VoiceInfo& x : voices) if (_wcsicmp(x.id.c_str(), defaultId.c_str()) == 0) v = &x;
            if (!v) { std::wstring id = AutomaticVoice(voices, L"en"); for (const VoiceInfo& x : voices) if (x.id == id) v = &x; }
            if (!v && !voices.empty()) v = &voices[0];
            if (v && !v->languages.empty()) {
                m_main = v->languages[0];
                m_languages.insert(m_main);
                m_voices[m_main] = v->id;
                m_fallback = true;
            }
            return;
        }
        m_main = s.mainLanguage;
        if (!m_languages.count(m_main)) m_main = m_languages.count(L"en") ? L"en" : *m_languages.begin();
    }
    bool IsFallback() const { return m_fallback; }
    const std::set<std::wstring>& Languages() const { return m_languages; }      // WorldVoice's availableLanguages
    bool Has(const std::wstring& lang) const { return m_languages.count(PrimaryLanguage(lang)) != 0; }
    std::wstring VoiceFor(const std::wstring& lang) const {
        auto it = m_voices.find(PrimaryLanguage(lang));
        return it == m_voices.end() ? std::wstring() : it->second;
    }
    const std::wstring& MainLanguage() const { return m_main; }

private:
    static bool Find(const std::vector<VoiceInfo>& voices, const std::wstring& id) {
        for (const VoiceInfo& v : voices) if (_wcsicmp(v.id.c_str(), id.c_str()) == 0) return true;
        return false;
    }
    std::set<std::wstring> m_languages;
    std::map<std::wstring, std::wstring> m_voices;
    std::wstring m_main;
    bool m_fallback = false;
};

// ---------------------------------------------------------------------------
// Language detection (WorldVoice's LanguageDetector)
// ---------------------------------------------------------------------------

class Detector {
public:
    Detector(const Settings& s, const VoiceMap& map) : m_settings(s), m_map(map) {}

    // find_language_for_script
    std::wstring LanguageForScript(const char* script, const std::wstring& current) const {
        std::vector<std::wstring> candidates = ScriptLanguageList(script);
        std::wstring currentPrimary = PrimaryLanguage(current);
        if (candidates.empty() || std::find(candidates.begin(), candidates.end(), currentPrimary) != candidates.end()) return current;
        std::wstring configured = m_settings.ConfiguredFor(script);
        if (!configured.empty()) {
            std::wstring p = PrimaryLanguage(configured);
            if (std::find(candidates.begin(), candidates.end(), p) != candidates.end() && m_map.Has(p)) return configured;
        }
        for (const std::wstring& c : candidates) if (m_map.Has(c)) return c;
        return current;
    }

    // _language_for_character
    std::wstring LanguageForCharacter(uint32_t cp, const std::wstring& current, const std::wstring& base) const {
        if (IsDigit(cp)) return m_settings.numbersFollowText ? current : base;
        const char* script = ScriptOf(cp);
        if (!script || !strcmp(script, "Common") || !strcmp(script, "Inherited")) return current;
        return LanguageForScript(script, current);
    }

    // process_for_spelling's choice for one character
    std::wstring LanguageForSpelling(uint32_t cp, const std::wstring& current, const std::wstring& base) const {
        std::wstring target;
        if (IsSpace(cp) || IsDigit(cp)) target = base;
        else {
            const char* script = ScriptOf(cp);
            target = (!script || !strcmp(script, "Common") || !strcmp(script, "Inherited")) ? current : LanguageForScript(script, current);
        }
        if (PrimaryLanguage(target) == PrimaryLanguage(base)) target = base;
        return target;
    }

private:
    const Settings& m_settings;
    const VoiceMap& m_map;
};

}  // namespace wv
