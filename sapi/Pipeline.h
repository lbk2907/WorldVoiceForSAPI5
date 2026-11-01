// Pipeline.h - WorldVoice's number stages and punctuation pauses, done on SAPI's text fragments before the text is split
// by language, as the add-on does them before its language detection. A rule-for-rule port of the add-on's pipeline,
// version 7.0, in its order: remove_thousands_separators, inject_numbers (_number_item_segments, _translate_number and
// numbers.number_pieces), inject_punctuation_pause and number_wait_factor, each with _merge_edge_breaks where the
// add-on has it. The number language, the other half of inject_numbers, is read in the split (WorldVoiceSapi.cpp),
// where the add-on's detection does it; but a number language that is a language also puts its switches around each
// number here, where they change the voice, as inject_numbers does, because the stages after it see them as commands
// (no punctuation pause before a number read by another voice, for one). The add-on's other pauses (between items, in
// say all, between CJK and Southeast Asian characters) are not part of WorldVoice for SAPI 5.
//
// The add-on works on NVDA's speech sequence: strings, pauses (BreakCommand) and other commands. Here SAPI's fragments
// make that sequence: a text is the text of fragments in a row with the same state (NVDA's SAPI 5 driver writes the
// strings between two commands as one text, which SAPI may still cut into several fragments), a silence is a pause,
// and anything else is a command: a bookmark, or the change of rate, pitch, volume, language or spelling that comes
// between two texts of different states (a language change alone is NVDA's LangChangeCommand, which the pause
// between numbers looks through). What comes out is fragments again, each still pointing into the program's
// own text with its own position in it, so word positions stay right; a pause is a silence fragment, also one of 0 ms
// (the add-on's 0 ms break still keeps the text on its two sides apart). Spelled text is left as it is: NVDA spells
// punctuation by name.
//
// Where it does not do what the add-on does (both READMEs say so):
// - inject_numbers' dot rule, a full stop right before or after a number set apart with a space ("2.54.2" read as
//   "2.54 . 2"), is left out (user request, 2026-10-13).
// - number_wait_factor strips the spaces from both ends of every text and drops blank texts (_remove_space). Here the
//   texts keep their spaces, as SAPI joins the fragments a voice gets; only its pauses are put in.
// - In Number and Group modes a symbol inside a number ("12:30") is spoken as itself: the add-on names it from NVDA's
//   symbol tables (symbols.symbol_namer), which a SAPI voice does not have; it does the same where NVDA has no name.
#pragma once
#include <sapi.h>
#include <sapiddk.h>
#include <string>
#include <vector>
#include "../common/WorldVoice.h"

namespace wv {

class Pipeline {
public:
    // base is the main language, and tokenLangId the language of WorldVoice's own voice registration: text marked
    // with another language is in that language (as the split takes it).
    Pipeline(const Settings& s, const Detector& detector, const std::wstring& base, LANGID tokenLangId)
        : m_mode(s.numberMode), m_group((size_t)s.groupSize), m_numberMs((ULONG)(std::max)(0, s.NumberWaitMs())),
          m_punctuation(s.punctuationPause), m_punctMs((ULONG)(std::max)(0, s.PunctuationWaitMs())),
          m_remove(s.removePunctuation), m_detector(detector), m_base(base), m_tokenLangId(tokenLangId),
          m_numberLanguage(s.numberLanguage == kNumbersFollow || s.numberLanguage == kNumbersMain ? std::wstring() : s.numberLanguage) {
        // punctuation_pause_characters: every character typed counts, a space too
        const std::wstring& c = s.punctuationCharacters;
        size_t units = 1;
        for (size_t i = 0; i < c.size(); i += units) m_chars.push_back(CodePointAt(c.c_str(), c.size(), i, &units));
    }

    // The fragments with the number stages and the punctuation pauses done, in out, linked by pNext; false when nothing
    // changed, and the fragments are to be used as they are. numbers says, for each fragment of out, whether it is
    // inside a number language's switches: a piece of a number, or a pause between two, all read in that language.
    bool Apply(const SPVTEXTFRAG* frags, std::vector<SPVTEXTFRAG>& out, std::vector<bool>& numbers) {
        m_changed = false;
        std::vector<Item> items, next;
        Gather(frags, items);
        if (m_mode == L"value") RemoveThousands(items);
        InjectNumbers(items, next);
        items.swap(next);
        next.clear();
        if (m_punctuation && !m_chars.empty()) {
            PunctuationPause(items, next);
            MergeEdgeBreaks(next);
            items.swap(next);
            next.clear();
        }
        if (m_numberMs > 0) {
            NumberWait(items, next);
            items.swap(next);
        }
        if (!m_changed) return false;
        out.clear();
        numbers.clear();
        for (const Item& item : items) {
            Fragments(item, out);
            numbers.resize(out.size(), item.number);
        }
        for (size_t i = 0; i + 1 < out.size(); i++) out[i].pNext = &out[i + 1];
        if (!out.empty()) out.back().pNext = nullptr;
        return true;
    }

private:
    // Where a character of a text comes from: SAPI's fragment and its place in it.
    struct Src { const SPVTEXTFRAG* frag; ULONG pos; };
    // The add-on's kinds of pause: a BreakCommand of the program's, NumberBreakCommand, NumberEdgeBreakCommand,
    // NumberItemEdgeBreakCommand and PunctuationBreakCommand.
    enum class Pause { Plain, Number, NumberEdge, NumberItemEdge, Punctuation };
    struct Item {
        // Language: a change of language alone between two texts, or a number language's switch (WVLangChangeCommand)
        enum Kind { Text, Spelled, Break, Command, Language } kind = Command;
        std::wstring text;                   // Text, Spelled
        std::vector<Src> src;                // Text: where each UTF-16 unit of the text comes from
        const SPVTEXTFRAG* frag = nullptr;   // SAPI's fragment (Text: its first; Break: a silence of the program's);
                                             // null for a pause made here, or a command between two texts
        ULONG ms = 0;                        // Break: its length
        Pause pause = Pause::Plain;          // Break: its kind
        Src at = {};                         // a pause made here: the character it goes after (before, when first)
        bool first = false;
        bool number = false;                 // Text, Break: inside a number language's switches
    };

    bool InChars(uint32_t cp) const {
        for (uint32_t c : m_chars) if (c == cp) return true;
        return false;
    }
    // str.strip() leaves nothing
    static bool IsBlank(const std::wstring& t, size_t a, size_t b) {
        size_t units = 1;
        for (size_t i = a; i < b; i += units) if (!IsSpace(CodePointAt(t.c_str(), b, i, &units))) return false;
        return true;
    }
    // str.isspace(): not empty, and every character a space
    static bool IsSpaces(const std::wstring& t, size_t a, size_t b) { return b > a && IsBlank(t, a, b); }
    static bool Spoken(const Item& item) {                   // a text with more than spaces (_is_spoken)
        return (item.kind == Item::Text || item.kind == Item::Spelled) && !IsBlank(item.text, 0, item.text.size());
    }
    static bool IsEdge(const Item& item) {
        return item.kind == Item::Break && (item.pause == Pause::NumberEdge || item.pause == Pause::NumberItemEdge);
    }
    static bool IsNumberBreak(const Item& item) {
        return item.kind == Item::Break && (item.pause == Pause::Number || item.pause == Pause::NumberEdge || item.pause == Pause::NumberItemEdge);
    }
    // The whole text (but spaces at its ends) is one NUMBER_RE match (NUMBER_RE.fullmatch, after _remove_space).
    static bool WholeNumber(const Item& item) {
        if (item.kind != Item::Text) return false;
        const std::wstring& t = item.text;
        size_t a = 0, b = t.size(), units = 1;
        while (a < b && IsSpace(CodePointAt(t.c_str(), b, a, &units))) a += units;
        while (b > a && IsSpace(CodePointBefore(t.c_str(), b))) b -= (b >= 2 && IS_LOW_SURROGATE(t[b - 1]) && IS_HIGH_SURROGATE(t[b - 2])) ? 2 : 1;
        size_t start = 0, end = 0;
        return a < b && NextNumber(t.c_str() + a, b - a, 0, start, end) && start == 0 && end == b - a;
    }
    static bool SameText(const wchar_t* a, const wchar_t* b) {
        if (!a || !*a) return !b || !*b;
        return b && wcscmp(a, b) == 0;
    }
    static bool SameState(const SPVSTATE& a, const SPVSTATE& b, bool language = true) {
        return a.eAction == b.eAction && (!language || a.LangID == b.LangID) && a.EmphAdj == b.EmphAdj && a.RateAdj == b.RateAdj &&
               a.Volume == b.Volume && a.PitchAdj.MiddleAdj == b.PitchAdj.MiddleAdj &&
               a.PitchAdj.RangeAdj == b.PitchAdj.RangeAdj && a.ePartOfSpeech == b.ePartOfSpeech &&
               SameText(a.Context.pCategory, b.Context.pCategory) && SameText(a.Context.pBefore, b.Context.pBefore) &&
               SameText(a.Context.pAfter, b.Context.pAfter);
    }

    // SAPI's fragments as the add-on's speech sequence.
    static void Gather(const SPVTEXTFRAG* frags, std::vector<Item>& items) {
        for (const SPVTEXTFRAG* f = frags; f; f = f->pNext) {
            bool speak = f->State.eAction == SPVA_Speak && f->pTextStart && f->ulTextLen;
            bool spelled = f->State.eAction == SPVA_SpellOut && f->pTextStart && f->ulTextLen;
            if (speak || spelled) {
                Item* prev = items.empty() ? nullptr : &items.back();
                bool prevText = prev && (prev->kind == Item::Text || prev->kind == Item::Spelled);
                if (speak && prevText && prev->kind == Item::Text && SameState(prev->frag->State, f->State)) {
                    Append(*prev, f);
                    continue;
                }
                if (prevText) {                                // the command NVDA has between two texts
                    Item c;
                    if (speak && prev->kind == Item::Text && SameState(prev->frag->State, f->State, false)) c.kind = Item::Language;
                    items.push_back(c);
                }
                Item t;
                t.kind = speak ? Item::Text : Item::Spelled;
                t.frag = f;
                Append(t, f);
                items.push_back(t);
            } else {
                Item c;
                c.kind = f->State.eAction == SPVA_Silence ? Item::Break : Item::Command;
                c.frag = f;
                if (c.kind == Item::Break) c.ms = f->State.SilenceMSecs;
                items.push_back(c);
            }
        }
    }
    static void Append(Item& t, const SPVTEXTFRAG* f) {
        t.text.append(f->pTextStart, f->ulTextLen);
        for (ULONG i = 0; i < f->ulTextLen; i++) t.src.push_back({ f, i });
    }

    // Building a text: the add-on's strings in a row are one (merge_consecutive_strings), a pause ends it.
    static Item Start(const Item& from) {
        Item t;
        t.kind = Item::Text;
        t.frag = from.frag;
        t.number = from.number;
        return t;
    }
    static void Put(Item& cur, const Item& in, size_t a, size_t b) {
        cur.text.append(in.text, a, b - a);
        cur.src.insert(cur.src.end(), in.src.begin() + a, in.src.begin() + b);
    }
    static void Flush(Item& cur, std::vector<Item>& out) {
        if (cur.text.empty()) return;
        out.push_back(cur);
        cur.text.clear();
        cur.src.clear();
    }
    // A pause made here, after in's character before - 1 (or before its first one, when before is 0).
    static Item MadePause(const Item& in, size_t before, ULONG ms, Pause kind) {
        Item p;
        p.kind = Item::Break;
        p.ms = ms;
        p.pause = kind;
        p.first = before == 0;
        p.at = in.src[before == 0 ? 0 : before - 1];
        return p;
    }
    void PutPause(Item& cur, const Item& in, size_t before, ULONG ms, Pause kind, std::vector<Item>& out) {
        Flush(cur, out);
        out.push_back(MadePause(in, before, ms, kind));
        out.back().number = cur.number;
        m_changed = true;
    }

    // remove_thousands_separators (value mode): the commas of each valid thousands run go, THOUSANDS_RE =
    // (?<![\d,])\d{1,3}(?:,\d{3})+(?!,*\d): "12,345,678" -> "12345678", "1,234,5" and "a,1,234" stay.
    void RemoveThousands(std::vector<Item>& items) {
        for (Item& item : items) {
            if (item.kind != Item::Text) continue;
            const std::wstring& t = item.text;
            size_t n = t.size(), p = 0, u = 1;
            std::vector<bool> drop(n, false);
            bool any = false;
            auto digitAt = [&](size_t i) { return i < n && IsDecimal(CodePointAt(t.c_str(), n, i, &u)); };
            while (p < n) {
                if (!digitAt(p)) { p += IS_HIGH_SURROGATE(t[p]) && p + 1 < n ? 2 : 1; continue; }
                uint32_t before = CodePointBefore(t.c_str(), p);
                size_t e = p, digits = 0;
                while (digitAt(e)) { e += u; digits++; }
                if (IsDecimal(before) || before == L',' || digits > 3) { p = e; continue; }
                size_t end = e;
                std::vector<size_t> commas;
                while (end < n && t[end] == L',') {
                    size_t q = end + 1, k = 0;
                    while (k < 3 && digitAt(q)) { q += u; k++; }
                    if (k < 3) break;
                    commas.push_back(end);
                    end = q;
                }
                size_t z = end;
                while (z < n && t[z] == L',') z++;
                bool valid = !commas.empty() && !digitAt(z);
                if (valid) { for (size_t c : commas) drop[c] = true; any = true; }
                p = valid ? end : e;
            }
            if (!any) continue;
            Item kept = item;
            kept.text.clear();
            kept.src.clear();
            for (size_t i = 0; i < n; i++) if (!drop[i]) { kept.text += t[i]; kept.src.push_back(item.src[i]); }
            item = kept;
            m_changed = true;
        }
    }

    // inject_numbers, item by item (_inject_item_numbers), then _merge_edge_breaks where it has edge breaks.
    void InjectNumbers(const std::vector<Item>& items, std::vector<Item>& out) {
        if (m_mode == L"none" && m_numberLanguage.empty()) { out = items; return; }
        // In Number and Group modes the number pause also goes before and after a number; a pause of 0 adds none.
        bool edges = (m_mode == L"number" || m_mode == L"group") && m_numberMs > 0;
        std::vector<bool> spokenAfter(items.size(), false);
        bool spoken = false;
        for (size_t i = items.size(); i-- > 0;) {        // _spoken_later: one utterance (SAPI has no end of utterance)
            spokenAfter[i] = spoken;
            spoken = spoken || Spoken(items[i]);
        }
        bool spokenBefore = false;
        for (size_t i = 0; i < items.size(); i++) {
            const Item& item = items[i];
            if (item.kind != Item::Text) {
                out.push_back(item);
                spokenBefore = spokenBefore || Spoken(item);
                continue;
            }
            NumberItem(item, edges, edges && spokenBefore, edges && spokenAfter[i], out);
            spokenBefore = spokenBefore || Spoken(item);
        }
        if (edges) MergeEdgeBreaks(out);
    }

    // _number_item_segments: the item's text, and each number with its pauses, its number language's switches and its
    // pieces. Text parts and numbers alternate; whitespace between two numbers becomes the pause between them; an item
    // that a number begins (or ends), with spoken text before (or after) the item, gets an item edge pause there. The
    // switches are there only where the number language's voice is not that of the language in force
    // (deduplicate_language_command), and then around every number of the item.
    void NumberItem(const Item& in, bool edges, bool edgeBefore, bool edgeAfter, std::vector<Item>& out) {
        const std::wstring& t = in.text;
        size_t n = t.size();
        std::vector<std::pair<size_t, size_t>> numbers = NumbersForLanguage(t.c_str(), n);
        if (numbers.empty()) { out.push_back(in); return; }
        bool wrap = !m_numberLanguage.empty() && !m_detector.SameVoice(m_numberLanguage, InForce(in));
        struct Part { bool number; size_t a, b; };
        std::vector<Part> parts;
        size_t pos = 0;
        for (const auto& m : numbers) {
            if (m.first > pos) parts.push_back({ false, pos, m.first });
            parts.push_back({ true, m.first, m.second });
            pos = m.second;
        }
        if (pos < n) parts.push_back({ false, pos, n });
        size_t last = parts.size() - 1;
        auto spokenPart = [&](size_t k) { return !IsBlank(t, parts[k].a, parts[k].b); };
        int firstSpoken = -1, lastSpoken = -1;
        for (size_t k = 0; k < parts.size(); k++) if (spokenPart(k)) { if (firstSpoken < 0) firstSpoken = (int)k; lastSpoken = (int)k; }
        Item cur = Start(in);
        if (edgeBefore && firstSpoken >= 0 && parts[firstSpoken].number) PutPause(cur, in, 0, m_numberMs, Pause::NumberItemEdge, out);
        for (size_t k = 0; k < parts.size(); k++) {
            const Part& p = parts[k];
            if (!p.number) {
                if (edges && k > 0 && k < last && IsSpaces(t, p.a, p.b)) { PutPause(cur, in, p.a, m_numberMs, Pause::Number, out); continue; }
                Put(cur, in, p.a, p.b);        // (the dot rule is left out, see above)
                continue;
            }
            if (edges && k > 0 && spokenPart(k - 1)) PutPause(cur, in, p.a, m_numberMs, Pause::NumberEdge, out);
            if (wrap) { PutLanguage(cur, out); cur.number = true; }
            Translate(cur, in, p.a, p.b, out);
            if (wrap) { PutLanguage(cur, out); cur.number = false; }
            if (edges && k < last && spokenPart(k + 1)) PutPause(cur, in, p.b, m_numberMs, Pause::NumberEdge, out);
        }
        if (edgeAfter && lastSpoken >= 0 && parts[lastSpoken].number) PutPause(cur, in, n, m_numberMs, Pause::NumberItemEdge, out);
        Flush(cur, out);
    }

    // The language in force at a text: the one SAPI marks on it (NVDA's LangChangeCommand), else the main language.
    std::wstring InForce(const Item& item) const {
        LANGID id = item.frag->State.LangID;
        std::wstring marked = (id && id != m_tokenLangId) ? LanguageOfLcid(id) : std::wstring();
        return marked.empty() ? m_base : marked;
    }
    // A number language's switch: it ends the text; the split reads the number in its language.
    static void PutLanguage(Item& cur, std::vector<Item>& out) {
        Flush(cur, out);
        Item c;
        c.kind = Item::Language;
        out.push_back(c);
    }

    // _translate_number with numbers.number_pieces: value mode keeps the number whole; number and group modes cut each
    // digit run from the left into pieces of 1 or GroupSize digits (a leading sign joins the first) and make each
    // symbol a piece of its own (NUMBER_TOKEN_RE = ^[+-]?\d+|\d+|[symbols]), with a number pause between every two
    // pieces, even one of 0 ms, so each reaches the voice on its own.
    void Translate(Item& cur, const Item& in, size_t a, size_t b, std::vector<Item>& out) {
        if (m_mode != L"number" && m_mode != L"group") { Put(cur, in, a, b); return; }
        const std::wstring& t = in.text;
        size_t size = m_mode == L"number" ? 1 : m_group, u = 1;
        std::vector<std::pair<size_t, size_t>> pieces;
        size_t k = a;
        while (k < b) {
            size_t start = k;
            bool sign = k == a && (t[k] == L'+' || t[k] == L'-') && k + 1 < b && IsDecimal(CodePointAt(t.c_str(), b, k + 1, &u));
            if (sign || IsDecimal(CodePointAt(t.c_str(), b, k, &u))) {      // a digit token, cut into groups
                if (sign) k++;
                size_t count = 0, groupStart = start;
                while (k < b && IsDecimal(CodePointAt(t.c_str(), b, k, &u))) {
                    k += u;
                    if (++count == size) { pieces.push_back({ groupStart, k }); groupStart = k; count = 0; }
                }
                if (count) pieces.push_back({ groupStart, k });
                continue;
            }
            CodePointAt(t.c_str(), b, k, &u);                              // a symbol: a piece of its own
            pieces.push_back({ k, k + u });
            k += u;
        }
        for (size_t i = 0; i < pieces.size(); i++) {
            if (i) PutPause(cur, in, pieces[i].first, m_numberMs, Pause::Number, out);
            Put(cur, in, pieces[i].first, pieces[i].second);
        }
    }

    // _merge_edge_breaks: each run between two spoken texts (breaks, blank text and other commands) that holds an
    // edge break and another break keeps one break: the longest; on a tie the first that is not an edge break, or the
    // first edge break. The other items of the run keep their order.
    void MergeEdgeBreaks(std::vector<Item>& items) {
        std::vector<Item> out, run;
        auto flushRun = [&]() {
            int kept = -1, breaks = 0;
            bool edge = false;
            for (size_t i = 0; i < run.size(); i++) {
                if (run[i].kind != Item::Break) continue;
                breaks++;
                edge = edge || IsEdge(run[i]);
                if (kept < 0 || run[i].ms > run[kept].ms || (run[i].ms == run[kept].ms && IsEdge(run[kept]) && !IsEdge(run[i]))) kept = (int)i;
            }
            bool merge = breaks >= 2 && edge;
            for (size_t i = 0; i < run.size(); i++) {
                if (merge && run[i].kind == Item::Break && (int)i != kept) { m_changed = true; continue; }
                out.push_back(run[i]);
            }
            run.clear();
        };
        for (const Item& item : items) {
            if (Spoken(item)) { flushRun(); out.push_back(item); }
            else run.push_back(item);
        }
        flushRun();
        items.swap(out);
    }

    // inject_punctuation_pause (_punctuation_pauses): the walk through the sequence. A pause already there between two
    // texts suppresses the boundary pause of the text before it, unless it is a number's edge pause (the merge then
    // keeps one of them); a mark that opens the next text is still a pause point of its own. Where removing the
    // marks empties a text between two number pauses, the one after it goes, so they do not come together.
    void PunctuationPause(const std::vector<Item>& items, std::vector<Item>& out) {
        if (items.empty()) return;
        const Item* previous = &items[0];
        std::vector<Item> pending, emitted;
        int lastBreak = -1;            // the last pause sent since the last spoken text: its index in out, or -1
        for (size_t i = 1; i < items.size(); i++) {
            const Item& current = items[i];
            if (current.kind == Item::Break) { pending.push_back(current); continue; }
            bool textFollows = Spoken(current);
            bool pauseFollows = textFollows;
            for (const Item& p : pending) pauseFollows = pauseFollows && IsEdge(p);
            emitted.clear();
            Emit(*previous, pauseFollows, textFollows, emitted);
            bool removed = Spoken(*previous);
            for (const Item& e : emitted) removed = removed && !Spoken(e);
            if (removed && lastBreak >= 0 && IsNumberBreak(out[lastBreak]) && !pending.empty() && IsNumberBreak(pending[0])) {
                pending.erase(pending.begin());
                m_changed = true;
            }
            for (const std::vector<Item>* list : { &emitted, &pending })
                for (const Item& c : *list) {
                    if (Spoken(c)) lastBreak = -1;
                    else if (c.kind == Item::Break) lastBreak = (int)out.size();
                    out.push_back(c);
                }
            pending.clear();
            previous = &current;
        }
        emitted.clear();
        Emit(*previous, false, false, emitted);
        out.insert(out.end(), emitted.begin(), emitted.end());
        out.insert(out.end(), pending.begin(), pending.end());
    }

    // inject_punctuation_pause's emit, for one text: a pause (PunctuationBreakCommand, even of 0 ms) at every pause
    // point, and, with removal, the mark there left out.
    void Emit(const Item& item, bool pauseTextFollows, bool removalTextFollows, std::vector<Item>& out) {
        if (item.kind != Item::Text) { out.push_back(item); return; }
        const std::wstring& t = item.text;
        size_t n = t.size(), units = 1;
        // _split_punctuation_segments with the regular expression [chars]+(?![chars\d]): a run of the characters is a
        // pause point unless a digit follows it (the comma in "12,500", the full stop in "3.5").
        struct Part { bool punct; size_t a, b; };
        std::vector<Part> parts;
        size_t pos = 0;
        for (size_t i = 0; i < n;) {
            if (!InChars(CodePointAt(t.c_str(), n, i, &units))) { i += units; continue; }
            size_t end = i;
            while (end < n && InChars(CodePointAt(t.c_str(), n, end, &units))) end += units;
            if (end == n || !IsDecimal(CodePointAt(t.c_str(), n, end, &units))) {
                if (i > pos) parts.push_back({ false, pos, i });
                parts.push_back({ true, i, end });
                pos = end;
            }
            i = end;
        }
        if (pos < n) parts.push_back({ false, pos, n });
        // The text's last character but spaces is one of the characters (item.rstrip()[-1] in chars).
        bool endsWithMark = false;
        for (size_t i = n; i > 0;) {
            size_t c = i - 1;
            if (c > 0 && IS_LOW_SURROGATE(t[c]) && IS_HIGH_SURROGATE(t[c - 1])) c--;
            uint32_t cp = CodePointAt(t.c_str(), n, c, &units);
            if (IsSpace(cp)) { i = c; continue; }
            endsWithMark = InChars(cp);
            break;
        }
        bool boundaryEligible = endsWithMark && removalTextFollows;
        bool boundaryPause = endsWithMark && pauseTextFollows;
        size_t lastPunctuation = parts.size();
        for (size_t k = 0; k < parts.size(); k++) if (parts[k].punct) lastPunctuation = k;
        // The internal pause points: the runs with text (not just spaces) after them in this text.
        std::vector<bool> internal(parts.size(), false);
        bool anyInternal = false, textAfter = false;
        for (size_t k = parts.size(); k-- > 0;) {
            if (parts[k].punct) { internal[k] = textAfter; anyInternal = anyInternal || textAfter; }
            else if (!IsBlank(t, parts[k].a, parts[k].b)) textAfter = true;
        }
        if (m_remove && !anyInternal && !boundaryEligible) { out.push_back(item); return; }
        Item cur = Start(item);
        for (size_t k = 0; k < parts.size(); k++) {
            const Part& p = parts[k];
            if (!p.punct) { Put(cur, item, p.a, p.b); continue; }
            if (!m_remove || !(internal[k] || (boundaryEligible && k == lastPunctuation))) Put(cur, item, p.a, p.b);
            else m_changed = true;
            if (internal[k]) PutPause(cur, item, p.b, m_punctMs, Pause::Punctuation, out);
        }
        // The text ends with one of the characters: a pause before the next text.
        if (boundaryPause) PutPause(cur, item, n, m_punctMs, Pause::Punctuation, out);
        Flush(cur, out);
    }

    // number_wait_factor: a number pause between two texts that are each a whole number, and between numbers that only
    // whitespace separates inside one text, which become texts of their own without that whitespace
    // (split_spaced_numbers). Language changes and blank texts (which the add-on drops) do not count, and the pause goes
    // after the first language change between the two (_insert_number_breaks); any other command keeps the numbers
    // apart, a pause too.
    void NumberWait(const std::vector<Item>& items, std::vector<Item>& out) {
        std::vector<Item> split;
        for (const Item& item : items) {
            if (item.kind == Item::Text) SplitSpaced(item, split);
            else split.push_back(item);
        }
        size_t previous = SIZE_MAX, after = 0;      // the item before in out, and where the items after it start
        for (const Item& item : split) {
            if (item.kind == Item::Language || (item.kind == Item::Text && IsBlank(item.text, 0, item.text.size()))) {
                out.push_back(item);
                continue;
            }
            if (previous != SIZE_MAX && WholeNumber(out[previous]) && WholeNumber(item)) {
                size_t at = out.size();
                for (size_t i = after; i < out.size(); i++) if (out[i].kind == Item::Language) { at = i + 1; break; }
                Item pause = MadePause(out[previous], out[previous].src.size(), m_numberMs, Pause::Number);
                out.insert(out.begin() + at, pause);
                m_changed = true;
            }
            out.push_back(item);
            previous = out.size() - 1;
            after = out.size();
        }
    }

    // split_spaced_numbers: every number that only whitespace (any space, a no-break one too) separates from a
    // neighbouring number becomes a text of its own, without that whitespace; everything else is kept as it is.
    void SplitSpaced(const Item& in, std::vector<Item>& out) {
        const std::wstring& t = in.text;
        size_t n = t.size(), start = 0, end = 0;
        std::vector<std::pair<size_t, size_t>> m;
        for (size_t from = 0; NextNumber(t.c_str(), n, from, start, end); from = end) m.push_back({ start, end });
        std::vector<bool> joined(m.size() + 1, false);
        for (size_t i = 1; i < m.size(); i++) joined[i] = IsSpaces(t, m[i - 1].second, m[i].first);
        bool any = false;
        for (size_t i = 1; i < m.size(); i++) any = any || joined[i];
        if (!any) { out.push_back(in); return; }
        size_t pos = 0;
        auto piece = [&](size_t a, size_t b) { Item p = Start(in); Put(p, in, a, b); out.push_back(p); };
        for (size_t i = 0; i < m.size(); i++) {
            if (!(joined[i] || joined[i + 1])) continue;
            if (!joined[i] && m[i].first > pos) piece(pos, m[i].first);
            piece(m[i].first, m[i].second);
            pos = m[i].second;
        }
        if (pos < n) piece(pos, n);
        m_changed = true;
    }

    // An item as SAPI's fragments.
    static void Fragments(const Item& item, std::vector<SPVTEXTFRAG>& out) {
        if (item.kind == Item::Text) {
            for (size_t i = 0; i < item.src.size();) {
                const Src& s = item.src[i];
                SPVTEXTFRAG f = *s.frag;
                f.pNext = nullptr;
                size_t j = i + 1;
                while (j < item.src.size() && item.src[j].frag == s.frag && item.src[j].pos == item.src[j - 1].pos + 1) j++;
                f.pTextStart = s.frag->pTextStart + s.pos;
                f.ulTextLen = (ULONG)(j - i);
                f.ulTextSrcOffset = s.frag->ulTextSrcOffset + s.pos;
                out.push_back(f);
                i = j;
            }
            return;
        }
        if (item.frag) {
            out.push_back(*item.frag);
            out.back().pNext = nullptr;
            return;
        }
        if (item.kind != Item::Break) return;        // the command between two texts: nothing
        // A pause made here: a silence, in the state of the text it goes with, placed at its character.
        SPVTEXTFRAG f = *item.at.frag;
        f.pNext = nullptr;
        f.State.eAction = SPVA_Silence;
        f.State.SilenceMSecs = item.ms;
        f.pTextStart = item.at.frag->pTextStart + item.at.pos;
        f.ulTextLen = 0;
        f.ulTextSrcOffset = item.at.frag->ulTextSrcOffset + item.at.pos + (item.first ? 0 : 1);
        out.push_back(f);
    }

    std::wstring m_mode;
    size_t m_group;
    ULONG m_numberMs;
    bool m_punctuation;
    ULONG m_punctMs;
    bool m_remove;
    const Detector& m_detector;
    std::wstring m_base;
    LANGID m_tokenLangId;
    std::wstring m_numberLanguage;      // a number language that is a language; empty for "default" and "main"
    std::vector<uint32_t> m_chars;
    bool m_changed = false;
};

}  // namespace wv
