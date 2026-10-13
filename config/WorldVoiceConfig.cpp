// WorldVoiceConfig.cpp - configuration program for WorldVoice for SAPI 5.
//
// Settings go to HKCU\SOFTWARE\WorldVoice (see common\WorldVoice.h) as soon as they change; the voice reads them at
// every sentence. The voices listed are the SAPI 5 voices registered for 64-bit programs.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <initguid.h>     // defines PROPID_ACC_ROLE
#include <oleacc.h>
#include <shellapi.h>
#include <sapi.h>
#include <string>
#include <vector>
#include "../common/WorldVoice.h"
#include "resource.h"

static HINSTANCE g_inst;
static IAccPropServices* g_accProps;
static ISpVoice* g_voice;
static std::vector<wv::VoiceInfo> g_voices;
static wv::Settings g_settings;
static std::vector<std::wstring> g_allLanguages;     // every language an installed voice has, by name
static std::vector<std::wstring> g_voiceChoices;     // what each item of the Voice list stands for
static bool g_filling = false;
static const wchar_t* kTitle = L"WorldVoice for SAPI 5";
static const wchar_t* kWorldVoiceToken = L"HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Speech\\Voices\\Tokens\\WorldVoice";
static const wchar_t* kNone = L"";                   // Voice list: no voice, WorldVoice does not switch to the language

// ---------------------------------------------------------------------------
// Settings
// ---------------------------------------------------------------------------

static void SetString(const wchar_t* name, const std::wstring& value) {
    RegSetKeyValueW(HKEY_CURRENT_USER, wv::kSettingsKey, name, REG_SZ, value.c_str(), (DWORD)((value.size() + 1) * sizeof(wchar_t)));
}

static void Save() {
    SetString(L"MainLanguage", g_settings.mainLanguage);
    SetString(L"HanLanguage", g_settings.hanLanguage);
    SetString(L"LatinLanguage", g_settings.latinLanguage);
    SetString(L"ArabicLanguage", g_settings.arabicLanguage);
    SetString(L"CyrillicLanguage", g_settings.cyrillicLanguage);
    SetString(L"DevanagariLanguage", g_settings.devanagariLanguage);
    auto setDword = [](const wchar_t* name, DWORD v) { RegSetKeyValueW(HKEY_CURRENT_USER, wv::kSettingsKey, name, REG_DWORD, &v, sizeof(v)); };
    setDword(L"NumbersFollowText", g_settings.numbersFollowText ? 1 : 0);
    SetString(L"NumberMode", g_settings.numberMode);
    setDword(L"PairWaitFactor", (DWORD)g_settings.pairWaitFactor);
    setDword(L"PunctuationPause", g_settings.punctuationPause ? 1 : 0);
    setDword(L"PunctuationWaitFactor", (DWORD)g_settings.punctuationWaitFactor);
    setDword(L"RemovePunctuation", g_settings.removePunctuation ? 1 : 0);
    SetString(L"PunctuationCharacters", g_settings.punctuationCharacters);
    HKEY k;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, wv::kVoicesKey, 0, nullptr, 0, KEY_READ | KEY_WRITE, nullptr, &k, nullptr) != ERROR_SUCCESS) return;
    for (const std::wstring& lang : g_allLanguages) {
        auto it = g_settings.voices.find(lang);
        if (it == g_settings.voices.end()) RegDeleteValueW(k, lang.c_str());
        else RegSetValueExW(k, lang.c_str(), 0, REG_SZ, (const BYTE*)it->second.c_str(), (DWORD)((it->second.size() + 1) * sizeof(wchar_t)));
    }
    RegCloseKey(k);
    // Each language's adjustments, in Adjustments\<language>; a language with none has no key.
    for (const std::wstring& lang : g_allLanguages) {
        std::wstring sub = std::wstring(wv::kAdjustmentsKey) + L"\\" + lang;
        wv::Adjustment a = g_settings.AdjustmentFor(lang);
        if (a.None()) { RegDeleteKeyW(HKEY_CURRENT_USER, sub.c_str()); continue; }
        DWORD rate = (DWORD)a.rate, pitch = (DWORD)a.pitch, volume = (DWORD)a.volume;
        RegSetKeyValueW(HKEY_CURRENT_USER, sub.c_str(), L"Rate", REG_DWORD, &rate, sizeof(rate));
        RegSetKeyValueW(HKEY_CURRENT_USER, sub.c_str(), L"Pitch", REG_DWORD, &pitch, sizeof(pitch));
        RegSetKeyValueW(HKEY_CURRENT_USER, sub.c_str(), L"Volume", REG_DWORD, &volume, sizeof(volume));
    }
}

// An adjustment as shown: "0", "+3", "-2".
static std::wstring AdjustmentText(int v) { return v > 0 ? L"+" + std::to_wstring(v) : std::to_wstring(v); }

static bool HasLanguage(const wv::VoiceInfo& v, const std::wstring& lang) {
    for (const std::wstring& l : v.languages) if (l == lang) return true;
    return false;
}

// ---------------------------------------------------------------------------
// The window
// ---------------------------------------------------------------------------

static int AddItem(HWND d, int id, const std::wstring& text) {
    return (int)SendDlgItemMessageW(d, id, CB_ADDSTRING, 0, (LPARAM)text.c_str());
}

// A list of languages; the item data indexes into the vector given.
static void FillLanguages(HWND d, int id, const std::vector<std::wstring>& langs, const std::wstring& selected) {
    SendDlgItemMessageW(d, id, CB_RESETCONTENT, 0, 0);
    int sel = 0;
    for (size_t i = 0; i < langs.size(); i++) {
        int item = AddItem(d, id, wv::LanguageName(langs[i]));
        SendDlgItemMessageW(d, id, CB_SETITEMDATA, item, (LPARAM)i);
    }
    for (int item = 0, n = (int)SendDlgItemMessageW(d, id, CB_GETCOUNT, 0, 0); item < n; item++)
        if (langs[(size_t)SendDlgItemMessageW(d, id, CB_GETITEMDATA, item, 0)] == selected) sel = item;
    SendDlgItemMessageW(d, id, CB_SETCURSEL, sel, 0);
}

static std::wstring SelectedLanguage(HWND d, int id, const std::vector<std::wstring>& langs) {
    int item = (int)SendDlgItemMessageW(d, id, CB_GETCURSEL, 0, 0);
    if (item < 0) return std::wstring();
    size_t i = (size_t)SendDlgItemMessageW(d, id, CB_GETITEMDATA, item, 0);
    return i < langs.size() ? langs[i] : std::wstring();
}

// ---- Voices tab ----

// What a language's voice is, as the list shows it: the chosen voice, or None (WorldVoice does not switch to it).
static std::wstring VoiceText(const std::wstring& lang) {
    auto it = g_settings.voices.find(lang);
    if (it != g_settings.voices.end())
        for (const wv::VoiceInfo& v : g_voices) if (_wcsicmp(v.id.c_str(), it->second.c_str()) == 0) return v.name;
    return L"None";
}

// One row's voice and adjustment columns.
static void SetRowText(HWND list, int row, const std::wstring& lang) {
    wv::Adjustment a = g_settings.AdjustmentFor(lang);
    std::wstring texts[] = { VoiceText(lang), AdjustmentText(a.rate), AdjustmentText(a.pitch), AdjustmentText(a.volume) };
    for (int c = 0; c < 4; c++) ListView_SetItemText(list, row, c + 1, &texts[c][0]);
}

// The list of languages and their voices, in g_allLanguages' order; the selection stays where it was, and starts on
// the first language in the list.
static void FillList(HWND d) {
    HWND list = GetDlgItem(d, IDC_LIST);
    int sel = ListView_GetNextItem(list, -1, LVNI_SELECTED);
    ListView_DeleteAllItems(list);
    for (size_t i = 0; i < g_allLanguages.size(); i++) {
        std::wstring name = wv::LanguageName(g_allLanguages[i]);
        LVITEMW item = {};
        item.mask = LVIF_TEXT;
        item.iItem = (int)i;
        item.pszText = &name[0];
        ListView_InsertItem(list, &item);
        SetRowText(list, (int)i, g_allLanguages[i]);
    }
    if (sel < 0 && !g_allLanguages.empty()) sel = 0;
    if (sel >= 0) ListView_SetItemState(list, sel, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
}

static std::wstring ListLanguage(HWND d) {
    int sel = ListView_GetNextItem(GetDlgItem(d, IDC_LIST), -1, LVNI_SELECTED);
    return (sel >= 0 && sel < (int)g_allLanguages.size()) ? g_allLanguages[sel] : std::wstring();
}

// The voices for the language selected in the list: None (the default), then that language's voices.
static void FillVoices(HWND d) {
    std::wstring lang = ListLanguage(d);
    SendDlgItemMessageW(d, IDC_VOICE, CB_RESETCONTENT, 0, 0);
    g_voiceChoices.clear();
    AddItem(d, IDC_VOICE, L"None");
    g_voiceChoices.push_back(kNone);
    for (const wv::VoiceInfo& v : g_voices) {
        if (!HasLanguage(v, lang)) continue;
        AddItem(d, IDC_VOICE, v.name);
        g_voiceChoices.push_back(v.id);
    }
    int sel = 0;
    auto it = g_settings.voices.find(lang);
    if (it != g_settings.voices.end())
        for (size_t i = 1; i < g_voiceChoices.size(); i++) if (_wcsicmp(g_voiceChoices[i].c_str(), it->second.c_str()) == 0) sel = (int)i;
    SendDlgItemMessageW(d, IDC_VOICE, CB_SETCURSEL, sel, 0);
    // and its adjustments
    wv::Adjustment a = g_settings.AdjustmentFor(lang);
    struct { int id, valueId, value; } sliders[] = { { IDC_RATE, IDC_RATE_V, a.rate }, { IDC_PITCH, IDC_PITCH_V, a.pitch }, { IDC_VOLUME, IDC_VOLUME_V, a.volume } };
    for (auto& s : sliders) {
        SendDlgItemMessageW(d, s.id, TBM_SETPOS, TRUE, s.value);
        SetDlgItemTextW(d, s.valueId, AdjustmentText(s.value).c_str());
    }
}

// A slider moved: the selected language's adjustment, saved, and shown beside the slider and in the list.
static void OnSlider(HWND d, HWND slider) {
    std::wstring lang = ListLanguage(d);
    if (lang.empty()) return;
    int value = wv::ClampAdjustment((int)SendMessageW(slider, TBM_GETPOS, 0, 0));
    wv::Adjustment a = g_settings.AdjustmentFor(lang);
    int id = GetDlgCtrlID(slider);
    int& field = id == IDC_RATE ? a.rate : (id == IDC_PITCH ? a.pitch : a.volume);
    if (field == value) return;
    field = value;
    if (a.None()) g_settings.adjustments.erase(lang); else g_settings.adjustments[lang] = a;
    Save();
    SetDlgItemTextW(d, id == IDC_RATE ? IDC_RATE_V : (id == IDC_PITCH ? IDC_PITCH_V : IDC_VOLUME_V), AdjustmentText(value).c_str());
    HWND list = GetDlgItem(d, IDC_LIST);
    SetRowText(list, ListView_GetNextItem(list, -1, LVNI_SELECTED), lang);
}

static std::vector<std::wstring> g_mainChoices;

// The scripts with a language setting, as WorldVoice offers them (Latin, CJK, Arabic) plus Cyrillic and
// Devanagari: each list holds that script's languages that have an installed voice, by name.
struct ScriptChoice { int id, labelId; const char* script; std::wstring wv::Settings::*setting; std::vector<std::wstring> languages; };
static ScriptChoice g_scripts[] = {
    { IDC_LATIN, IDC_LATIN_L, "Latin", &wv::Settings::latinLanguage, {} },
    { IDC_HAN, IDC_HAN_L, "Han", &wv::Settings::hanLanguage, {} },
    { IDC_ARABIC, IDC_ARABIC_L, "Arabic", &wv::Settings::arabicLanguage, {} },
    { IDC_CYRILLIC, IDC_CYRILLIC_L, "Cyrillic", &wv::Settings::cyrillicLanguage, {} },
    { IDC_DEVANAGARI, IDC_DEVANAGARI_L, "Devanagari", &wv::Settings::devanagariLanguage, {} },
};

// ---- Numbers and punctuation tab ----

// WorldVoice's number modes, in its order.
static const wchar_t* const kNumberModes[] = { L"value", L"number", L"pair" };
static const wchar_t* const kNumberModeNames[] = { L"Value", L"Number (digit by digit)", L"Pair (two digits at a time)" };

// A wait factor as shown: its pause, 5 ms a step (at most 500 ms).
static std::wstring WaitText(int factor) { return std::to_wstring((std::min)(factor * 5, 500)) + L" ms"; }

// As in WorldVoice: the punctuation wait factor only with the pauses on, the characters with the pauses or the
// removal on.
static void EnablePunctuation(HWND d) {
    bool pause = g_settings.punctuationPause, chars = pause || g_settings.removePunctuation;
    for (int id : { IDC_PUNCTWAIT, IDC_PUNCTWAIT_L, IDC_PUNCTWAIT_V }) EnableWindow(GetDlgItem(d, id), pause);
    for (int id : { IDC_PUNCTCHARS, IDC_PUNCTCHARS_L }) EnableWindow(GetDlgItem(d, id), chars);
}

static void FillNumbersAndPunctuation(HWND d) {
    int mode = 0;
    for (int i = 0; i < 3; i++) if (g_settings.numberMode == kNumberModes[i]) mode = i;
    SendDlgItemMessageW(d, IDC_NUMMODE, CB_SETCURSEL, mode, 0);
    SendDlgItemMessageW(d, IDC_PAIRWAIT, TBM_SETPOS, TRUE, g_settings.pairWaitFactor);
    SetDlgItemTextW(d, IDC_PAIRWAIT_V, WaitText(g_settings.pairWaitFactor).c_str());
    CheckDlgButton(d, IDC_PUNCTPAUSE, g_settings.punctuationPause ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(d, IDC_PUNCTREMOVE, g_settings.removePunctuation ? BST_CHECKED : BST_UNCHECKED);
    SendDlgItemMessageW(d, IDC_PUNCTWAIT, TBM_SETPOS, TRUE, g_settings.punctuationWaitFactor);
    SetDlgItemTextW(d, IDC_PUNCTWAIT_V, WaitText(g_settings.punctuationWaitFactor).c_str());
    SetDlgItemTextW(d, IDC_PUNCTCHARS, g_settings.punctuationCharacters.c_str());
    EnablePunctuation(d);
}

// A wait factor's slider moved: saved, and its pause shown beside it.
static void OnWaitSlider(HWND d, HWND slider) {
    int value = wv::ClampFactor((DWORD)(std::max)(0, (int)SendMessageW(slider, TBM_GETPOS, 0, 0)));
    bool pair = GetDlgCtrlID(slider) == IDC_PAIRWAIT;
    int& field = pair ? g_settings.pairWaitFactor : g_settings.punctuationWaitFactor;
    if (field == value) return;
    field = value;
    Save();
    SetDlgItemTextW(d, pair ? IDC_PAIRWAIT_V : IDC_PUNCTWAIT_V, WaitText(value).c_str());
}

static void Fill(HWND d) {
    g_filling = true;
    // Main language: the languages that have a voice and are not turned off.
    wv::VoiceMap map;
    map.Build(g_settings, g_voices);
    std::wstring status;
    if (map.IsFallback())
        for (const wv::VoiceInfo& v : g_voices)
            if (v.id == map.VoiceFor(map.MainLanguage())) status = L"No voices chosen yet: " + v.name + L" reads everything. Choose voices on the Voices tab.";
    SetDlgItemTextW(d, IDC_STATUS, status.c_str());
    g_mainChoices.assign(map.Languages().begin(), map.Languages().end());
    std::sort(g_mainChoices.begin(), g_mainChoices.end(), [](const std::wstring& a, const std::wstring& b) { return wv::LanguageName(a) < wv::LanguageName(b); });
    FillLanguages(d, IDC_MAINLANG, g_mainChoices, map.MainLanguage());
    FillList(d);
    FillVoices(d);
    for (ScriptChoice& sc : g_scripts) {
        sc.languages.clear();
        for (const std::wstring& l : wv::ScriptLanguageList(sc.script)) if (map.Has(l)) sc.languages.push_back(l);
        std::sort(sc.languages.begin(), sc.languages.end(), [](const std::wstring& a, const std::wstring& b) { return wv::LanguageName(a) < wv::LanguageName(b); });
        FillLanguages(d, sc.id, sc.languages, g_settings.*sc.setting);
        EnableWindow(GetDlgItem(d, sc.id), !sc.languages.empty());
        EnableWindow(GetDlgItem(d, sc.labelId), !sc.languages.empty());
    }
    CheckDlgButton(d, IDC_NUMBERS, g_settings.numbersFollowText ? BST_CHECKED : BST_UNCHECKED);
    FillNumbersAndPunctuation(d);
    g_filling = false;
}

// Play: the test text read by WorldVoice (it must be installed), so it sounds exactly as in use.
static void PlayTest(HWND d) {
    if (!g_voice) {
        ISpObjectToken* token = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_SpObjectToken, nullptr, CLSCTX_ALL, IID_ISpObjectToken, (void**)&token);
        if (SUCCEEDED(hr)) hr = token->SetId(nullptr, kWorldVoiceToken, FALSE);
        if (SUCCEEDED(hr)) hr = CoCreateInstance(CLSID_SpVoice, nullptr, CLSCTX_ALL, IID_ISpVoice, (void**)&g_voice);
        if (SUCCEEDED(hr)) hr = g_voice->SetVoice(token);
        if (token) token->Release();
        if (FAILED(hr)) {
            if (g_voice) { g_voice->Release(); g_voice = nullptr; }
            SetDlgItemTextW(d, IDC_STATUS, L"WorldVoice is not installed.");
            return;
        }
    }
    int len = GetWindowTextLengthW(GetDlgItem(d, IDC_TESTTEXT));
    std::wstring text(len + 1, L'\0');
    GetDlgItemTextW(d, IDC_TESTTEXT, &text[0], len + 1);
    text.resize(len);
    if (text.find_first_not_of(L" \t") == std::wstring::npos) { SetDlgItemTextW(d, IDC_STATUS, L"Type some text to test first."); return; }
    SetDlgItemTextW(d, IDC_STATUS, L"");
    // Ticked: the text as WorldVoice reads it, each language with its own voice.
    if (IsDlgButtonChecked(d, IDC_TESTALL) == BST_CHECKED) {
        g_voice->Speak(text.c_str(), SPF_ASYNC | SPF_PURGEBEFORESPEAK | SPF_IS_NOT_XML, nullptr);
        return;
    }
    // Otherwise all of it with the selected language's voice and adjustments, through WorldVoice's test tag.
    std::wstring lang = ListLanguage(d);
    if (VoiceText(lang) == L"None") {
        SetDlgItemTextW(d, IDC_STATUS, (L"Choose a voice for " + wv::LanguageName(lang) + L" first.").c_str());
        return;
    }
    std::wstring xml = L"<context id=\"" + std::wstring(wv::kTestContext) + lang + L"\">";
    for (wchar_t c : text) {
        if (c == L'&') xml += L"&amp;";
        else if (c == L'<') xml += L"&lt;";
        else if (c == L'>') xml += L"&gt;";
        else xml += c;
    }
    xml += L"</context>";
    g_voice->Speak(xml.c_str(), SPF_ASYNC | SPF_PURGEBEFORESPEAK | SPF_IS_XML, nullptr);
}

// ---- Log tab ----
// The voice's debug log, %TEMP%\WorldVoice.log, written while HKCU\SOFTWARE\WorldVoice has DWORD Log = 1. The voice
// looks at the switch every second, so a change takes effect at once in programs already using it.

static std::wstring LogPath() {
    wchar_t temp[MAX_PATH];
    DWORD n = GetTempPathW(MAX_PATH, temp);
    return std::wstring(temp, n) + L"WorldVoice.log";
}
static bool LogOn() {
    DWORD v = 0;
    return wv::ReadDword(HKEY_CURRENT_USER, wv::kSettingsKey, L"Log", v) && v;
}
// The log's size, or false when there is none.
static bool LogSize(ULONGLONG& size) {
    WIN32_FILE_ATTRIBUTE_DATA a;
    if (!GetFileAttributesExW(LogPath().c_str(), GetFileExInfoStandard, &a)) return false;
    size = ((ULONGLONG)a.nFileSizeHigh << 32) | a.nFileSizeLow;
    return true;
}
static std::wstring SizeText(ULONGLONG bytes) {       // 1,203 KB
    std::wstring n = std::to_wstring((bytes + 1023) / 1024);
    for (int i = (int)n.size() - 3; i > 0; i -= 3) n.insert((size_t)i, L",");
    return n + L" KB";
}
static void ShowLog(HWND d) {
    CheckDlgButton(d, IDC_LOGON, LogOn() ? BST_CHECKED : BST_UNCHECKED);
    ULONGLONG size = 0;
    std::wstring info = LogSize(size) ? L"The log is " + LogPath() + L", " + SizeText(size) + L"." : L"There is no log yet.";
    SetDlgItemTextW(d, IDC_LOGINFO, info.c_str());
}
static void LogCommand(HWND d, int id) {
    ULONGLONG size = 0;
    if (id == IDC_LOGON) {
        DWORD v = IsDlgButtonChecked(d, IDC_LOGON) == BST_CHECKED ? 1 : 0;
        if (RegSetKeyValueW(HKEY_CURRENT_USER, wv::kSettingsKey, L"Log", REG_DWORD, &v, sizeof(v)) != ERROR_SUCCESS)
            MessageBoxW(d, L"Could not change the log setting.", kTitle, MB_ICONERROR);
    } else if (id == IDC_LOGOPEN) {
        // File Explorer with the log file selected (to copy or send it), or the folder it will be in when there is none.
        std::wstring path = LogPath(), args = LogSize(size) ? L"/select,\"" + path + L"\"" : L"\"" + path.substr(0, path.find_last_of(L'\\')) + L"\"";
        ShellExecuteW(d, nullptr, L"explorer.exe", args.c_str(), nullptr, SW_SHOWNORMAL);
    } else if (id == IDC_LOGCLEAR) {
        if (!LogSize(size)) MessageBoxW(d, L"There is no log yet.", kTitle, MB_ICONINFORMATION);
        else if (MessageBoxW(d, (L"Delete the log (" + SizeText(size) + L")?").c_str(), kTitle, MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) == IDYES &&
                 !DeleteFileW(LogPath().c_str()))
            MessageBoxW(d, L"Could not delete the log. Try again in a moment.", kTitle, MB_ICONERROR);
    }
    ShowLog(d);
}

// ---- Tabs ----

static HWND g_tab;

// The tabs, in order, and the first of the ids their controls have (each has 100): the controls live directly in the
// window, and a tab shows its own.
static const wchar_t* const kPages[] = { L"General", L"Voices", L"Numbers and punctuation", L"Log" };
static const int kPageIds[] = { 1100, 1200, 1400, 1300 };

static void ShowPage(HWND d, int page) {
    if (kPageIds[page] == 1300) ShowLog(d);          // the log's size as it is now
    for (HWND c = GetWindow(d, GW_CHILD); c; c = GetWindow(c, GW_HWNDNEXT)) {
        int id = GetDlgCtrlID(c);
        for (int p = 0; p < ARRAYSIZE(kPageIds); p++)
            if (id >= kPageIds[p] && id < kPageIds[p] + 100) ShowWindow(c, p == page ? SW_SHOWNA : SW_HIDE);
    }
}

static INT_PTR CALLBACK MainDlg(HWND d, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_INITDIALOG: {
        g_voices = wv::ListVoices();
        g_settings.Read();
        for (const wv::VoiceInfo& v : g_voices)
            for (const std::wstring& l : v.languages)
                if (std::find(g_allLanguages.begin(), g_allLanguages.end(), l) == g_allLanguages.end()) g_allLanguages.push_back(l);
        std::sort(g_allLanguages.begin(), g_allLanguages.end(), [](const std::wstring& a, const std::wstring& b) { return wv::LanguageName(a) < wv::LanguageName(b); });
        g_tab = GetDlgItem(d, IDC_TAB);
        TCITEMW ti = {};
        ti.mask = TCIF_TEXT;
        for (int p = 0; p < ARRAYSIZE(kPages); p++) {
            ti.pszText = const_cast<wchar_t*>(kPages[p]);
            TabCtrl_InsertItem(g_tab, p, &ti);
        }
        HWND list = GetDlgItem(d, IDC_LIST);
        ListView_SetExtendedListViewStyle(list, LVS_EX_FULLROWSELECT);
        LVCOLUMNW col = {};
        col.mask = LVCF_TEXT | LVCF_WIDTH;
        col.cx = 150;
        col.pszText = const_cast<wchar_t*>(L"Language");
        ListView_InsertColumn(list, 0, &col);
        col.cx = 280;
        col.pszText = const_cast<wchar_t*>(L"Voice");
        ListView_InsertColumn(list, 1, &col);
        const wchar_t* columns[] = { L"Rate", L"Pitch", L"Volume" };
        for (int c = 0; c < 3; c++) {
            col.cx = 50;
            col.pszText = const_cast<wchar_t*>(columns[c]);
            ListView_InsertColumn(list, c + 2, &col);
        }
        for (int id : { IDC_RATE, IDC_PITCH, IDC_VOLUME }) {
            SendDlgItemMessageW(d, id, TBM_SETRANGEMIN, FALSE, -10);
            SendDlgItemMessageW(d, id, TBM_SETRANGEMAX, TRUE, 10);
            SendDlgItemMessageW(d, id, TBM_SETPAGESIZE, 0, 2);
        }
        for (const wchar_t* name : kNumberModeNames) AddItem(d, IDC_NUMMODE, name);
        for (int id : { IDC_PAIRWAIT, IDC_PUNCTWAIT }) {
            SendDlgItemMessageW(d, id, TBM_SETRANGEMIN, FALSE, 0);
            SendDlgItemMessageW(d, id, TBM_SETRANGEMAX, TRUE, 100);
            SendDlgItemMessageW(d, id, TBM_SETPAGESIZE, 0, 10);
        }
        SendDlgItemMessageW(d, IDC_PUNCTCHARS, EM_LIMITTEXT, 200, 0);
        Fill(d);
        ShowPage(d, 0);
        if (g_voices.empty()) SetDlgItemTextW(d, IDC_STATUS, L"No SAPI 5 voices are installed.");
        // Windows' accessibility layer reports a window made from a dialog template as a dialog, and screen readers
        // then read out every text in it on opening. Say it is a plain window.
        if (SUCCEEDED(CoCreateInstance(__uuidof(CAccPropServices), nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&g_accProps)))) {
            VARIANT role; role.vt = VT_I4; role.lVal = ROLE_SYSTEM_CLIENT;
            g_accProps->SetHwndProp(d, (DWORD)OBJID_CLIENT, CHILDID_SELF, PROPID_ACC_ROLE, role);
        }
        SetFocus(g_tab);            // a screen reader then says: tab control, General tab selected
        return FALSE;
    }
    case WM_HSCROLL:
        if (!g_filling && lp) {
            int id = GetDlgCtrlID((HWND)lp);
            if (id == IDC_PAIRWAIT || id == IDC_PUNCTWAIT) OnWaitSlider(d, (HWND)lp);
            else OnSlider(d, (HWND)lp);
        }
        return TRUE;
    case WM_NOTIFY: {
        const NMHDR* n = reinterpret_cast<const NMHDR*>(lp);
        if (n->idFrom == IDC_TAB && n->code == TCN_SELCHANGE) ShowPage(d, TabCtrl_GetCurSel(g_tab));
        if (n->idFrom == IDC_LIST && n->code == LVN_ITEMCHANGED && !g_filling) {
            const NMLISTVIEW* lv = reinterpret_cast<const NMLISTVIEW*>(lp);
            if ((lv->uNewState & LVIS_SELECTED) && !(lv->uOldState & LVIS_SELECTED)) { g_filling = true; FillVoices(d); g_filling = false; }
        }
        return FALSE;
    }
    case WM_COMMAND:
        if (g_filling) return TRUE;
        switch (LOWORD(wp)) {
        case IDC_MAINLANG:
            if (HIWORD(wp) == CBN_SELCHANGE) { g_settings.mainLanguage = SelectedLanguage(d, IDC_MAINLANG, g_mainChoices); Save(); }
            break;
        case IDC_VOICE:
            if (HIWORD(wp) == CBN_SELCHANGE) {
                std::wstring lang = ListLanguage(d);
                int item = (int)SendDlgItemMessageW(d, IDC_VOICE, CB_GETCURSEL, 0, 0);
                if (lang.empty() || item < 0 || item >= (int)g_voiceChoices.size()) break;
                if (g_voiceChoices[item] == kNone) g_settings.voices.erase(lang);
                else g_settings.voices[lang] = g_voiceChoices[item];
                Save();
                Fill(d);      // the list shows the new voice; turning a language off or on changes the General lists
            }
            break;
        case IDC_LATIN: case IDC_HAN: case IDC_ARABIC: case IDC_CYRILLIC: case IDC_DEVANAGARI:
            if (HIWORD(wp) == CBN_SELCHANGE)
                for (ScriptChoice& sc : g_scripts)
                    if (sc.id == LOWORD(wp)) {
                        std::wstring lang = SelectedLanguage(d, sc.id, sc.languages);
                        if (!lang.empty()) { g_settings.*sc.setting = lang; Save(); }
                    }
            break;
        case IDC_NUMBERS:
            g_settings.numbersFollowText = IsDlgButtonChecked(d, IDC_NUMBERS) == BST_CHECKED;
            Save();
            break;
        case IDC_NUMMODE:
            if (HIWORD(wp) == CBN_SELCHANGE) {
                int item = (int)SendDlgItemMessageW(d, IDC_NUMMODE, CB_GETCURSEL, 0, 0);
                if (item >= 0 && item < 3) { g_settings.numberMode = kNumberModes[item]; Save(); }
            }
            break;
        case IDC_PUNCTPAUSE:
        case IDC_PUNCTREMOVE:
            g_settings.punctuationPause = IsDlgButtonChecked(d, IDC_PUNCTPAUSE) == BST_CHECKED;
            g_settings.removePunctuation = IsDlgButtonChecked(d, IDC_PUNCTREMOVE) == BST_CHECKED;
            Save();
            EnablePunctuation(d);
            break;
        case IDC_PUNCTCHARS:
            if (HIWORD(wp) == EN_CHANGE) {
                int len = GetWindowTextLengthW(GetDlgItem(d, IDC_PUNCTCHARS));
                std::wstring text(len + 1, L'\0');
                GetDlgItemTextW(d, IDC_PUNCTCHARS, &text[0], len + 1);
                text.resize(len);
                g_settings.punctuationCharacters = text;
                Save();
            }
            break;
        case IDC_PLAY:
            PlayTest(d);
            break;
        case IDC_LOGON:
        case IDC_LOGOPEN:
        case IDC_LOGCLEAR:
            LogCommand(d, LOWORD(wp));
            break;
        case IDC_RESET:
            // Every WorldVoice setting goes: voices, adjustments, languages, numbers, punctuation (and the debug log
            // switch).
            if (MessageBoxW(d, L"Reset all WorldVoice settings to their defaults? This clears every voice choice and adjustment.", kTitle, MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES) break;
            RegDeleteTreeW(HKEY_CURRENT_USER, wv::kSettingsKey);
            g_settings = wv::Settings();
            Fill(d);
            ShowLog(d);                 // the Log tab shows the log off at once
            SetDlgItemTextW(d, IDC_STATUS, L"All settings are back to their defaults.");
            break;
        case IDCANCEL:
            DestroyWindow(d);
            break;
        }
        return TRUE;
    case WM_CLOSE:
        DestroyWindow(d);
        return TRUE;
    case WM_DESTROY:
        if (g_voice) { g_voice->Speak(nullptr, SPF_PURGEBEFORESPEAK, nullptr); g_voice->Release(); g_voice = nullptr; }
        if (g_accProps) {
            MSAAPROPID prop = PROPID_ACC_ROLE;
            g_accProps->ClearHwndProps(d, (DWORD)OBJID_CLIENT, CHILDID_SELF, &prop, 1);
            g_accProps->Release();
            g_accProps = nullptr;
        }
        PostQuitMessage(0);
        return TRUE;
    }
    return FALSE;
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, LPWSTR, int show) {
    g_inst = inst;
    // A window class of our own: screen readers treat the window as a plain application window instead of a
    // dialog, so they do not read out every label on opening.
    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc = DefDlgProcW;
    wc.cbWndExtra = DLGWINDOWEXTRA;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"WorldVoiceConfigWnd";
    RegisterClassExW(&wc);
    // Only one copy: bring an open window to the front instead.
    if (HWND existing = FindWindowW(L"WorldVoiceConfigWnd", nullptr)) {
        ShowWindow(existing, SW_RESTORE);
        SetForegroundWindow(existing);
        return 0;
    }
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_STANDARD_CLASSES | ICC_TAB_CLASSES | ICC_LISTVIEW_CLASSES };
    InitCommonControlsEx(&icc);
    HWND d = CreateDialogParamW(inst, MAKEINTRESOURCEW(IDD_MAIN), nullptr, MainDlg, 0);
    if (!d) return 1;
    ShowWindow(d, show);
    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0) > 0) {
        // Ctrl+Tab and Ctrl+Shift+Tab switch tabs, as in standard property sheets.
        if (m.message == WM_KEYDOWN && m.wParam == VK_TAB && (GetKeyState(VK_CONTROL) & 0x8000) && (m.hwnd == d || IsChild(d, m.hwnd))) {
            int count = TabCtrl_GetItemCount(g_tab);
            int next = (TabCtrl_GetCurSel(g_tab) + ((GetKeyState(VK_SHIFT) & 0x8000) ? count - 1 : 1)) % count;
            TabCtrl_SetCurSel(g_tab, next);
            ShowPage(d, next);
            SetFocus(g_tab);
            continue;
        }
        if (!IsDialogMessageW(d, &m)) {
            TranslateMessage(&m);
            DispatchMessageW(&m);
        }
    }
    CoUninitialize();
    return 0;
}
