// Pipeline.h - WorldVoice's number mode and punctuation pauses, done on SAPI's text fragments before the text is split
// by language, as the add-on does them before its language detection. A rule-for-rule port of the add-on's pipeline
// functions iter_number_speech_segments_mode and _translate_number (number mode) and inject_punctuation_pause, as its
// master branch has them after version 6.2 (September 2026).
//
// The add-on works on NVDA's speech sequence: strings, pauses (BreakCommand) and other commands. Here SAPI's fragments
// make that sequence: a text is the text of fragments in a row with the same state (NVDA's SAPI 5 driver writes the
// strings between two commands as one text, which SAPI may still cut into several fragments), a silence is a pause,
// and anything else is a command: a bookmark, or the change of rate, pitch, volume, language or spelling that comes
// between two texts of different states. What comes out is fragments again, each still pointing into the program's
// own text with its own position in it, so word positions stay right; a space put in reads from kSpace, and a pause is
// a silence fragment. Spelled text is left as it is: NVDA spells punctuation by name.
#pragma once
#include <sapi.h>
#include <sapiddk.h>
#include <string>
#include <vector>
#include "../common/WorldVoice.h"

namespace wv {

class Pipeline {
public:
    explicit Pipeline(const Settings& s)
        : m_mode(s.numberMode), m_pairMs((ULONG)s.PairWaitMs()),
          m_waitMs(s.punctuationPause ? (ULONG)s.PunctuationWaitMs() : 0), m_remove(s.removePunctuation) {
        // punctuation_pause_characters.strip()
        const std::wstring& c = s.punctuationCharacters;
        size_t units = 1;
        for (size_t i = 0; i < c.size(); i += units) m_chars.push_back(CodePointAt(c.c_str(), c.size(), i, &units));
        while (!m_chars.empty() && IsSpace(m_chars.front())) m_chars.erase(m_chars.begin());
        while (!m_chars.empty() && IsSpace(m_chars.back())) m_chars.pop_back();
    }

    // The fragments with the number mode and the punctuation pauses done, in out, linked by pNext; false when nothing
    // changed, and the fragments are to be used as they are.
    bool Apply(const SPVTEXTFRAG* frags, std::vector<SPVTEXTFRAG>& out) {
        m_changed = false;
        std::vector<Item> items, numbered, paused;
        Gather(frags, items);
        for (const Item& item : items) {
            if (item.kind == Item::Text) NumberMode(item, numbered);
            else numbered.push_back(item);
        }
        // inject_punctuation_pause: nothing to do without a pause or removal, or without characters
        if ((m_waitMs > 0 || m_remove) && !m_chars.empty()) PunctuationPause(numbered, paused);
        else paused.swap(numbered);
        if (!m_changed) return false;
        out.clear();
        for (const Item& item : paused) Fragments(item, out);
        for (size_t i = 0; i + 1 < out.size(); i++) out[i].pNext = &out[i + 1];
        if (!out.empty()) out.back().pNext = nullptr;
        return true;
    }

private:
    // Where a character of a text comes from: SAPI's fragment and its place in it, or, added, a space put in there.
    struct Src { const SPVTEXTFRAG* frag; ULONG pos; bool added; };
    struct Item {
        enum Kind { Text, Spelled, Break, Command } kind = Command;
        std::wstring text;                   // Text, Spelled
        std::vector<Src> src;                // Text: where each UTF-16 unit of the text comes from
        const SPVTEXTFRAG* frag = nullptr;   // SAPI's fragment (Text: its first); null for a pause or command made here
        ULONG ms = 0;                        // a pause made here: its length
        Src after = {};                      // a pause made here: the character it follows
    };

    static constexpr const wchar_t* kSpace = L" ";

    bool InChars(uint32_t cp) const {
        for (uint32_t c : m_chars) if (c == cp) return true;
        return false;
    }
    static bool IsAsciiDigit(wchar_t c) { return c >= L'0' && c <= L'9'; }
    // str.strip() leaves nothing
    static bool IsBlank(const std::wstring& t, size_t a, size_t b) {
        size_t units = 1;
        for (size_t i = a; i < b; i += units) if (!IsSpace(CodePointAt(t.c_str(), b, i, &units))) return false;
        return true;
    }
    static bool SameText(const wchar_t* a, const wchar_t* b) {
        if (!a || !*a) return !b || !*b;
        return b && wcscmp(a, b) == 0;
    }
    static bool SameState(const SPVSTATE& a, const SPVSTATE& b) {
        return a.eAction == b.eAction && a.LangID == b.LangID && a.EmphAdj == b.EmphAdj && a.RateAdj == b.RateAdj &&
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
                if (prevText) items.push_back(Item());     // the command NVDA has between two texts
                Item t;
                t.kind = speak ? Item::Text : Item::Spelled;
                t.frag = f;
                Append(t, f);
                items.push_back(t);
            } else {
                Item c;
                c.kind = f->State.eAction == SPVA_Silence ? Item::Break : Item::Command;
                c.frag = f;
                items.push_back(c);
            }
        }
    }
    static void Append(Item& t, const SPVTEXTFRAG* f) {
        t.text.append(f->pTextStart, f->ulTextLen);
        for (ULONG i = 0; i < f->ulTextLen; i++) t.src.push_back({ f, i, false });
    }

    // Building a text: the add-on's strings in a row are one (merge_consecutive_strings), a pause ends it.
    static Item Start(const Item& from) {
        Item t;
        t.kind = Item::Text;
        t.frag = from.frag;
        return t;
    }
    static void Put(Item& cur, const Item& in, size_t a, size_t b) {
        cur.text.append(in.text, a, b - a);
        cur.src.insert(cur.src.end(), in.src.begin() + a, in.src.begin() + b);
    }
    // A space put in before in's character at, or at its end.
    void Space(Item& cur, const Item& in, size_t at) {
        Src s = at < in.src.size() ? in.src[at] : in.src[at - 1];
        if (at >= in.src.size() && !s.added) s.pos++;
        s.added = true;
        cur.text += L' ';
        cur.src.push_back(s);
        m_changed = true;
    }
    // A pause put in after in's character before - 1.
    void Pause(Item& cur, const Item& in, size_t before, ULONG ms, std::vector<Item>& out) {
        Flush(cur, out);
        Item p;
        p.kind = Item::Break;
        p.ms = ms;
        p.after = in.src[before - 1];
        out.push_back(p);
        m_changed = true;
    }
    static void Flush(Item& cur, std::vector<Item>& out) {
        if (cur.text.empty()) return;
        out.push_back(cur);
        cur.text.clear();
        cur.src.clear();
    }

    // iter_number_speech_segments_mode, with _NUMBER_RE = [+-]?[0-9]+(?:\.[0-9]+)?
    void NumberMode(const Item& in, std::vector<Item>& out) {
        const std::wstring& t = in.text;
        size_t n = t.size(), pos = 0, i = 0;
        bool previousWasNumber = false;
        Item cur = Start(in);
        while (i < n) {
            size_t j = i;
            if ((t[j] == L'+' || t[j] == L'-') && j + 1 < n && IsAsciiDigit(t[j + 1])) j++;
            if (!IsAsciiDigit(t[j])) { i++; continue; }
            size_t start = i;
            while (j < n && IsAsciiDigit(t[j])) j++;
            if (j + 1 < n && t[j] == L'.' && IsAsciiDigit(t[j + 1])) {
                j++;
                while (j < n && IsAsciiDigit(t[j])) j++;
            }
            if (start > pos) {
                if (previousWasNumber && t[pos] == L'.') Space(cur, in, pos);
                Put(cur, in, pos, start);
                if (t[start - 1] == L'.') Space(cur, in, start);    // prefix.endswith(".")
            }
            TranslateNumber(cur, in, start, j, out);
            pos = i = j;
            previousWasNumber = true;
        }
        if (pos < n) {
            if (previousWasNumber && t[pos] == L'.') Space(cur, in, pos);
            Put(cur, in, pos, n);
        }
        Flush(cur, out);
    }

    // _translate_number (WorldVoice's digit replacements from its symbols file are not used here)
    void TranslateNumber(Item& cur, const Item& in, size_t a, size_t b, std::vector<Item>& out) {
        const std::wstring& t = in.text;
        if (m_mode == L"pair") {
            // A decimal number is read as a value; digits are read in pairs from the left, the sign with the first.
            if (t.find(L'.', a) < b) { Put(cur, in, a, b); return; }
            size_t digits = (t[a] == L'+' || t[a] == L'-') ? a + 1 : a;
            for (size_t k = digits; k < b; k += 2) {
                if (k > digits) {
                    if (m_pairMs > 0) Pause(cur, in, k, m_pairMs, out);
                    else Space(cur, in, k);
                }
                Put(cur, in, k == digits ? a : k, (std::min)(k + 2, b));
            }
            return;
        }
        if (m_mode != L"number") { Put(cur, in, a, b); return; }    // value
        bool previousWasDigit = false;
        for (size_t k = a; k < b; k++) {
            bool isDigit = IsAsciiDigit(t[k]);
            if (isDigit && previousWasDigit) Space(cur, in, k);
            Put(cur, in, k, k + 1);
            previousWasDigit = isDigit;
        }
    }

    // inject_punctuation_pause: the walk through the sequence. A pause already there between two texts takes the
    // place of the punctuation pause (they never stack), but the mark is still removed.
    void PunctuationPause(const std::vector<Item>& items, std::vector<Item>& out) {
        if (items.empty()) return;
        const Item* previous = &items[0];
        std::vector<const Item*> pending;
        for (size_t i = 1; i < items.size(); i++) {
            const Item& current = items[i];
            if (current.kind == Item::Break) { pending.push_back(&current); continue; }
            bool textFollows = (current.kind == Item::Text || current.kind == Item::Spelled) && !IsBlank(current.text, 0, current.text.size());
            Emit(*previous, textFollows && pending.empty(), textFollows, out);
            for (const Item* p : pending) out.push_back(*p);
            pending.clear();
            previous = &current;
        }
        Emit(*previous, false, false, out);
        for (const Item* p : pending) out.push_back(*p);
    }

    // inject_punctuation_pause's emit, for one text.
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
        bool boundaryPause = m_waitMs > 0 && endsWithMark && pauseTextFollows;
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
            bool eligibleHere = internal[k];
            if (!m_remove || !(eligibleHere || (boundaryEligible && k == lastPunctuation))) Put(cur, item, p.a, p.b);
            else m_changed = true;
            if (eligibleHere && m_waitMs > 0) Pause(cur, item, p.b, m_waitMs, out);
        }
        // The text ends with one of the characters: a pause before the next text.
        if (boundaryPause) Pause(cur, item, n, m_waitMs, out);
        Flush(cur, out);
    }

    // An item as SAPI's fragments.
    static void Fragments(const Item& item, std::vector<SPVTEXTFRAG>& out) {
        if (item.kind == Item::Text) {
            for (size_t i = 0; i < item.src.size();) {
                const Src& s = item.src[i];
                SPVTEXTFRAG f = *s.frag;
                f.pNext = nullptr;
                f.ulTextSrcOffset = s.frag->ulTextSrcOffset + s.pos;
                if (s.added) {
                    f.pTextStart = kSpace;
                    f.ulTextLen = 1;
                    out.push_back(f);
                    i++;
                    continue;
                }
                size_t j = i + 1;
                while (j < item.src.size() && !item.src[j].added && item.src[j].frag == s.frag && item.src[j].pos == item.src[j - 1].pos + 1) j++;
                f.pTextStart = s.frag->pTextStart + s.pos;
                f.ulTextLen = (ULONG)(j - i);
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
        // A pause made here: a silence, in the state of the text it follows, placed just after its character.
        SPVTEXTFRAG f = *item.after.frag;
        f.pNext = nullptr;
        f.State.eAction = SPVA_Silence;
        f.State.SilenceMSecs = item.ms;
        f.pTextStart = kSpace;
        f.ulTextLen = 0;
        f.ulTextSrcOffset = item.after.frag->ulTextSrcOffset + item.after.pos + (item.after.added ? 0 : 1);
        out.push_back(f);
    }

    std::wstring m_mode;
    ULONG m_pairMs, m_waitMs;
    bool m_remove;
    std::vector<uint32_t> m_chars;
    bool m_changed = false;
};

}  // namespace wv
