WorldVoice for SAPI 5

One SAPI 5 voice, named WorldVoice, that reads each part of a text with the voice chosen for its language: Chinese
with a Chinese voice, English with an English voice, and so on, switching in the middle of a sentence when the text
does. It splits the text the way the WorldVoice NVDA add-on by Tseng Woody does (see "Where it comes from"), so it
brings that add-on's voice switching to every SAPI 5 program, 64-bit or 32-bit, with the add-on's number mode and
pauses at punctuation.
WorldVoice has no voice of its own: it speaks through the SAPI 5 voices already installed on the computer. Voices of
other kinds (Windows' OneCore voices, for example) are not used.

Version 1.0, by Loh Boon Keat. Free software under the GNU General Public License version 2 (see "Licence").
Made entirely by vibe coding, with Claude Opus 5.5 at ultracode effort.
Source code and new versions: https://github.com/lbk2907/WorldVoiceForSAPI5


Getting started

1. Install SAPI 5 voices for the languages you want to hear.
2. Install WorldVoice with "WorldVoice for SAPI 5 Setup.exe" (it needs administrator rights).
3. Open WorldVoice's configuration program (the "WorldVoice" shortcut, or WorldVoiceConfig.exe in the install
   folder). On the Voices tab, choose a voice for each language. A language left at None is not switched to.
4. Choose the voice "WorldVoice" in the program that speaks. In NVDA: Speech settings, Synthesizer "Microsoft Speech
   API version 5", Voice "WorldVoice".
Until a voice is chosen for at least one language, one voice reads everything: Windows' default SAPI 5 voice, else an
American English voice, else the first voice installed. The configuration program says so at the bottom of its
window.
Every change in the configuration program is saved at once (there is no Apply button) and used from the voice's next
sentence, also in programs already speaking with it.


How WorldVoice splits a text

Each character's script (its writing system) decides its language:
    A script that belongs to one language goes to it: Hiragana and Katakana to Japanese, Hangul to Korean, Thai to
    Thai, Greek to Greek, Hebrew to Hebrew, and so on for Armenian, Bengali, Bopomofo (Chinese), Georgian, Gujarati,
    Gurmukhi (Punjabi), Kannada, Khmer, Lao, Malayalam, Mongolian, Myanmar, Oriya, Sinhala, Tamil, Telugu and Tibetan.
    A script that several languages share (Latin, Cyrillic, Arabic, Devanagari, and the Chinese characters that
    Chinese, Japanese and Korean all use): while the text around it is already in one of that script's languages that
    has a voice, it stays in that language (kanji in Japanese text are read in Japanese). Otherwise it goes to the
    main language (or the program's marked one) when that is one of the script's languages with a voice; else to the
    language chosen for that script on the General tab, when that language has a voice; else to the first of the
    script's languages that has one.
    A language with no voice chosen is never switched to: its text stays with the language around it.
Spaces, punctuation and symbols stay with the text around them.
Numbers follow the Number language setting. With "Follow main language" (the default here, user request; the add-on's
default is the other) a number is read in the main language (or the program's marked one). With "Follow surrounding
text language" it is read in the language of the text around it, and with a language chosen, in that language.
A number is its digits, of any script (also full-width
digits such as "１２" and Arabic-Indic ones such as "١٢"), with any punctuation and symbols of Latin-1 between them:
"12,345", "5-3", "12:30" and "2×3" are one number each. A sign right after a letter or digit stays with the word, as
the hyphen in "COVID-19"; after a space it belongs to the number, as in "-3". A number goes to another voice only
where its language has a voice of its own: one read by the same voice as the text before it stays with that text.
After a number the detection starts again from the main (or marked) language.
Text the program marks with a language (NVDA's automatic language switching, a <lang> tag in SAPI's XML) takes that
language in place of the main language, and the detection carries on from it. When that language has no voice
chosen, the main language's voice reads it.
Spelled text (character by character) goes the same way, one character at a time; spaces go to the main language (or
the program's marked one), and digits follow the Number language: the language around them, the main language, or
the chosen language when it has a voice.
Languages that share a script cannot be told apart by it: Latin text in English text stays English, so a French word
in an English sentence is read by the English voice. Only text in a script of its own, or marked by the program,
switches.


Numbers and punctuation

Before the text is split by language, WorldVoice does what the add-on (version 7.0) does with numbers and
punctuation (the settings are on the configuration program's Numbers and punctuation tab).
Pauses
    Each pause is its wait factor times the global wait factor's tens, in milliseconds: at the global wait factor's
    default, 10 (shown as 1), a wait factor of 15 is a 15 ms pause; at 30 (shown as 3), it is 45 ms. There is no
    upper limit. With a global wait factor of 0 every pause is 0 ms, which still parts the text there.
Number mode
    Off: numbers are left as they are.
    Value (the default): numbers as they are written, for the voice to read its own way, but without the commas of a
    number written with thousands separators: "12,345,678" goes to the voice as "12345678". Only commas that make
    whole groups of three count: "1,234,5" and "a,1,234" stay as they are.
    Number: digit by digit, "12345" as "1 2 3 4 5".
    Group: the digits in groups from the left, of the group size (3 unless you change it): "12345" as "123 45".
    In Number and Group modes a sign goes with the first digit or group ("-1234" as "-1 2 3 4"), each symbol inside a
    number is a piece of its own ("12:30" as "1 2 : 3 0"), and between every two pieces goes the number pause, even
    one of 0 ms, so that the voice reads each piece on its own. When the number pause is above 0, it also comes
    before and after a number with text next to it, and in place of the spaces between two numbers; where it meets
    another pause (one the program asks for, or a pause at punctuation), only the longest stays.
    The add-on reads a symbol inside a number by its name, from NVDA's symbol tables ("12:30" as "1 2 colon 3 0"). A
    SAPI 5 voice has no such tables, so WorldVoice leaves the symbol for the voice to read, as the add-on does with a
    symbol NVDA has no name for.
    The add-on also sets a full stop right before or after a number apart with a space ("Room 101." as
    "Room 101 ."). WorldVoice for SAPI 5 leaves that out, in every mode, by user request.
Between numbers
    In every mode, Off too, a number pause above 0 also goes between two numbers that only spaces separate
    ("1234 5678", with a no-break space too), and between two texts the program sends that are each a number. When
    it puts these pauses in, the add-on also strips the spaces from both ends of every text the program sends;
    WorldVoice keeps them, because SAPI joins the texts a voice gets, and words would run together.
Pauses at punctuation
    With "Enable punctuation pause control" ticked (the default), WorldVoice pauses at the punctuation pause
    characters you type (there are none until you do), for the punctuation wait factor's pause. Its default, 0,
    gives a pause of 0 ms, which still parts the text there.
    Every character typed counts, a space too: type one, and every space is a pause point. Characters in a row ("?!",
    "...") are one pause point. A character with a digit after it is not a pause point ("12,500", "3.5").
    The pause comes only where text follows: later in the same text, or in the next text the program sends straight
    after it; not across a bookmark, nor across a change of rate, pitch, volume, language or spelling the program
    makes there (NVDA's commands); and never at the very end. With a number language chosen whose voice is not the
    main language's (or the program's marked one's), the switch to a number counts as such a change: a mark right
    before the number gets no pause, as in the add-on.
    Where the program already asks for a silence after a text, its silence takes the place of the pause at the
    text's end: they do not add up. Next to a number's pause in Number and Group modes, only the longer one stays.
Removing the marks
    With "Remove configured punctuation at pause points" ticked, the marks at those pause points are left out of the
    text, so the voice neither reads them nor pauses on its own there. A mark at the very end stays. A mark where the
    program asks for a silence is still left out. Where leaving out a mark empties what was between two number pauses
    ("3.5" in Number mode, with the full stop among the characters), only one of the two pauses stays.
WorldVoice gets the text after the program has made it ready: a mark a screen reader reads out as a word (NVDA at
punctuation level "all") arrives as that word, and is not a mark any more. Spelled text is left as it is.
Before the add-on's version 7.0, WorldVoice for SAPI 5 followed the add-on's master branch after version 6.2. Its
Pair mode and pair wait factor are gone: Group mode with a group size of 2 reads the digits in pairs. A punctuation
wait factor set then now gives a shorter pause (it was 5 ms a step, at most 500 ms), and the punctuation pause
characters, which were the common marks unless you changed them, are now none unless you type them.


The configuration program

General tab
    Main language
        The language of the text: what WorldVoice reads in when nothing else decides. The list holds the languages
        that have a voice chosen. When the main language has none, English is used if it has a voice, else another
        language that has one.
    Language detection
        Language assumed for Latin, CJK, Arabic, Cyrillic and Devanagari characters: the language for that script's
        text when the text around it is not already in one of the script's languages (see above). Each list holds the
        script's languages that have a voice chosen, and is unavailable when there are none. The WorldVoice add-on
        offers Latin, CJK and Arabic; Cyrillic and Devanagari are added here, for the same reason. CJK characters
        are Chinese unless you choose otherwise, where the add-on assumes Japanese (user request).
        Number language: the language numbers are read in: "Follow main language" (the default, user request), "Follow
        surrounding text language", or one of the languages that have a voice (see above). It replaces WorldVoice's
        earlier "Ignore numbers when detecting text language", as in the add-on's version 7.0; the earlier setting is
        not carried over.
Voices tab
    Languages and their voices
        Every language an installed voice speaks, with its voice and its rate, pitch and volume adjustments.
    Voice for the selected language
        None (the default: WorldVoice does not switch to that language), or one of the voices that speak it.
    Rate, Pitch and Volume
        From -10 to +10; 0 (the default) is as the speaking program asks. Rate and pitch are added to the program's,
        in SAPI's steps; volume changes by 10% a step, so it can also go above the program's. Each language has its
        own.
    Test
        Type a text and press Play to hear it with the selected language's voice and adjustments. With "Test all
        languages together, as WorldVoice reads them" ticked, Play reads it the way WorldVoice does, each language
        with its own voice. The test speaks through the installed WorldVoice voice, so it sounds exactly as in use.
Numbers and punctuation tab
    The add-on's settings of the same names (see "Numbers and punctuation"). Beside each slider is what it gives.
    Global wait factor
        From 0 to 10 (1, the default); every pause below is its own wait factor times this, in ms. The add-on's
        Speech Pipeline panel shows it the same way (it keeps it as 0 to 100, in tens).
    Number mode
        Off, Value (the default), Number (digit by digit) or Group (digits in groups).
    Group size
        In Group mode, the digits in a group, from 2 to 10 (3, the default). Unavailable in the other modes.
    Number wait factor
        From 0 to 100 (10, the default): the pause between numbers, and in Number and Group modes between a number's
        pieces and around a number.
    Enable punctuation pause control
        Ticked by default. Untick it for no pauses at punctuation and no marks left out.
    Remove configured punctuation at pause points
        Off by default. Unavailable while the pause control is off.
    Punctuation wait factor
        From 0 to 100 (0, the default): the pause at punctuation. Unavailable while the pause control is off.
    Punctuation pause characters
        The characters to pause at (and to remove), as typed, a space too; none by default. Unavailable while the
        pause control is off.
Log tab
    The debug log: see "The debug log".
Reset all to default (on every tab)
    Asks first, then clears every WorldVoice setting: every voice choice, adjustment, language, number and
    punctuation setting, and it turns the debug log off.
The configuration program lists the voices registered for 64-bit programs. A 32-bit program uses the 32-bit
registration of the same voices; a voice that is missing there, or cannot be loaded, is replaced by the main
language's voice.


How it sounds

WorldVoice gives the program one format, 16-bit mono at the highest rate of the voices chosen in it (with none
chosen, the rate of the voice that reads everything), settled when the program loads WorldVoice: programs open
their sound player once, when they load the voice, so the format cannot change with the voice reading. A voice
chosen later with a higher rate, or a voice's own rate changed, takes effect when the program loads WorldVoice
again (in NVDA, choose another synthesizer and then SAPI 5 again, or restart NVDA), as a voice's own rate setting
does. No voice is ever converted down, so nothing is lost, and a voice at that rate reaches the program exactly as
it would on its own; the others are converted up with WorldVoice's own filter, which adds nothing audible (what it
leaves is at least 97 dB down, the limit of 16-bit sound; Windows' own converter leaves mirror images of the sound
only 16 to 39 dB down). Until 31 October 2026 the format was always 44100 Hz: in NVDA, which cuts its player off
in the middle of a sound at every interrupt, that cut clicked in Eloquence's 11025 Hz sound made 44100 Hz, where
Eloquence on its own did not (user report). If no voice can be loaded, WorldVoice gives 44100 Hz.
Voices begin and end what they say with silence, as if each piece were a whole text. So that switching languages
does not pause, the silence at the start of a piece is cut to 20 ms, and at its end, when another piece follows, to a
natural pause: 250 ms after the end of a sentence, 150 ms after a comma, colon or semicolon, 80 ms otherwise, with
any silence asked for there (by the program, or a pause at punctuation) on top. Silence inside a piece is kept.
Every chosen voice is loaded when the program loads WorldVoice. A voice chosen after that starts loading with the
next thing the program says, usually the name of the voice just chosen, so that the first text in its language does
not wait for it (user request): a Vocalizer voice of 14 MB not read lately took some 700 ms. A voice that loads
inside the program rather than in a helper of its own holds that one speech up while it loads (Cerence's and
Eloquence's took up to half a second the first time, under 100 ms after that). A voice no longer chosen is let go,
with its memory and any helpers, so going through the list of voices does not leave them all loaded.
Each piece goes to its voice marked with its language (the voice's own locale of it, such as English (United
Kingdom) for a British voice), so a voice that speaks several languages reads each piece in the right one.
Word positions and bookmarks refer to the program's own text, so following the text as it is read works as with any
voice. Interrupting reaches the voice reading at once.
NVDA's SAPI 5 driver (2026.3) can let a bit of an interrupted speech through, heard as a click when arrow keys
interrupt quickly, with any SAPI 5 voice (user report). It keeps the start of each speech back until it has 50 ms,
and its cancel does not empty that, so a speech interrupted before then began the next one; and it takes sound
again before the voice has been told to stop. So WorldVoice gives the program the first 60 ms of a speech in one
piece (it plays that at once), the rest in pieces of 10 ms, and nothing once the speech is stopped; when the program
takes none of a piece, as NVDA does while it cancels, nothing more goes until the stop comes (half a second at
most). The sound itself is the same as before, byte for byte.


Where the settings are kept

HKEY_CURRENT_USER\SOFTWARE\WorldVoice:
    MainLanguage, LatinLanguage, HanLanguage, ArabicLanguage, CyrillicLanguage, DevanagariLanguage
        language codes such as en, zh, ja (defaults: en, en, zh, ar, ru, hi)
    NumberLanguage
        main (follow the main language, the default), default (follow the surrounding text; the name is the add-on's,
        whose default it is), or a language code such as en
    Voices\<language>
        the voice chosen for the language (its SAPI 5 token, such as
        HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Speech\Voices\Tokens\TTS_MS_EN-US_ZIRA_11.0); no value means None
    Adjustments\<language>
        DWORDs Rate, Pitch and Volume, -10 to 10
    GlobalWaitFactor
        DWORD, 0 to 100 in tens (default 10); other values are rounded down to tens
    NumberMode
        none, value (the default), number or group; any other value reads as value
    GroupSize
        DWORD, 2 to 10 (default 3); any other value reads as 3
    NumberWaitFactor
        DWORD, 0 to 100 (default 10)
    PunctuationWaitFactor
        DWORD, 0 to 100 (default 0)
    PunctuationPause, RemovePunctuation
        DWORDs, 1 when "Enable punctuation pause control" (the default) or "Remove configured punctuation at pause
        points" is ticked
    PunctuationCharacters
        the punctuation pause characters, as typed; no value means none
    PairWaitFactor, from earlier versions, is no longer read.
    Log
        DWORD, 1 while the debug log is on


The debug log

For finding out what the voice does: how WorldVoice split each text by language, which voice read each piece, how
much audio it gave and how long it all took, and any voice that could not be loaded. It is off unless you turn it on.
Turning it on and off
    On the configuration program's Log tab: "Write a debug log". It takes effect within a second, also in programs
    already using the voice. The tab also shows where the log is and how big; "Open the log folder" opens it in File
    Explorer with the log selected, to copy or send it, and "Clear the log..." deletes it, after asking.
    The switch is the DWORD value Log (1 on, 0 off) in HKEY_CURRENT_USER\SOFTWARE\WorldVoice. Without the
    configuration program, in a Command Prompt or the Windows+R box:
        reg add "HKCU\SOFTWARE\WorldVoice" /v Log /t REG_DWORD /d 1 /f
        reg add "HKCU\SOFTWARE\WorldVoice" /v Log /t REG_DWORD /d 0 /f
Where it is
    %TEMP%\WorldVoice.log, usually C:\Users\<your name>\AppData\Local\Temp\WorldVoice.log. Each line starts with the
    time. The voice adds to the file and never empties it, so clear it when you no longer need it.
    The log holds all the text the voice read. Turn it off when you have finished, and look through it before you
    share it.


What the setup installs

Everything goes to C:\Program Files\WorldVoice:
    x64\WorldVoiceSapi.dll and x86\WorldVoiceSapi.dll (the voice, for 64-bit and 32-bit programs)
    WorldVoiceConfig.exe (the configuration program)
    README.txt (this file)
    LICENSE.txt (the GNU General Public License version 2)
The voice is registered for 64-bit and 32-bit programs as "WorldVoice"
(HKLM\SOFTWARE\Microsoft\Speech\Voices\Tokens\WorldVoice; class {47BE0B8C-0DF1-4FB0-BBE9-5EA76A365C4C}). The setup
offers a Start Menu shortcut and a desktop shortcut to the configuration program, and on its last page offers to open
this file, unticked.
Uninstalling (Windows Settings, Apps, Installed apps, "WorldVoice for SAPI 5") removes the files and the voice's
registration. The settings stay (HKEY_CURRENT_USER\SOFTWARE\WorldVoice); "Reset all to default" clears them
first if you want them gone.


Where it comes from

The language detection is a rule-for-rule port of the WorldVoice NVDA add-on's (version 7.0, its languageDetection
module: find_language_for_script, _language_for_character, add_detected_language_commands and process_for_spelling,
with the number language of detect_with_number_language), and its table of the script of every character is made
from that add-on's code. The number mode and the pauses at punctuation are a rule-for-rule port of the same version's
speech pipeline (remove_thousands_separators, inject_numbers, inject_punctuation_pause and number_wait_factor).
WorldVoice is by Tseng Woody: https://github.com/tsengwoody/WorldVoice. This voice also has the add-on's name. The
rest (the SAPI 5 voice, the sound conversion and the configuration program) is written for this voice.


Licence

WorldVoice for SAPI 5 is free software: you can redistribute it and/or modify it under the terms of the GNU General
Public License version 2, as published by the Free Software Foundation. It is distributed in the hope that it will
be useful, but without any warranty; without even the implied warranty of merchantability or fitness for a
particular purpose. The full text is in LICENSE.txt, next to this file. The source code is at
https://github.com/lbk2907/WorldVoiceForSAPI5.
WorldVoice for SAPI 5's own code: copyright Loh Boon Keat.
The language detection's rules, the number mode, the pauses at punctuation and the script table come from the
WorldVoice add-on by Tseng Woody (https://github.com/tsengwoody/WorldVoice), which is under the GNU General Public
License.
The script table's Unicode data: Unicode, Inc.'s character database (Unicode 18.0.0), as the add-on has it.


Credits

Tseng Woody, for the WorldVoice NVDA add-on, whose language detection, number mode and pauses at punctuation this
follows rule for rule.
Claude Opus 5.5 (Anthropic), at ultracode effort, which wrote all of WorldVoice for SAPI 5 by vibe coding with Loh
Boon Keat.
