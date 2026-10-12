WorldVoice for SAPI 5

One SAPI 5 voice, named WorldVoice, that reads each part of a text with the voice chosen for its language: Chinese
with a Chinese voice, English with an English voice, and so on, switching in the middle of a sentence when the text
does. It splits the text the way the WorldVoice NVDA add-on by Tseng Woody does (see "Where it comes from"), so it
brings that add-on's voice switching to every SAPI 5 program, 64-bit or 32-bit.
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
    Chinese, Japanese and Korean all use): while the text around it is already in one of that script's languages, it
    stays in that language (kanji in Japanese text are read in Japanese). Otherwise it goes to the language chosen for
    that script on the General tab, when that language has a voice; else to the first of the script's languages that
    has one.
    A language with no voice chosen is never switched to: its text stays with the language around it.
Spaces, punctuation and symbols stay with the text around them.
Numbers are read in the main language. With "Ignore numbers when detecting text language" ticked, they are read in
the language of the text around them instead.
Text the program marks with a language (NVDA's automatic language switching, a <lang> tag in SAPI's XML) takes that
language in place of the main language, for itself and its numbers, and the detection carries on from it. When that
language has no voice chosen, the main language's voice reads it.
Spelled text (character by character) goes the same way, one character at a time; spaces and digits go to the main
language (or the program's marked one).
Languages that share a script cannot be told apart by it: Latin text in English text stays English, so a French word
in an English sentence is read by the English voice. Only text in a script of its own, or marked by the program,
switches.


The configuration program

General tab
    Main language
        The language of the text: what WorldVoice reads in when nothing else decides, and the language of numbers.
        The list holds the languages that have a voice chosen. When the main language has none, English is used if it
        has a voice, else another language that has one.
    Language detection
        Language assumed for Latin, CJK, Arabic, Cyrillic and Devanagari characters: the language for that script's
        text when the text around it is not already in one of the script's languages (see above). Each list holds the
        script's languages that have a voice chosen, and is unavailable when there are none. The WorldVoice add-on
        offers Latin, CJK and Arabic; Cyrillic and Devanagari are added here, for the same reason.
        Ignore numbers when detecting text language: numbers are read in the language of the text around them, not by
        the main language's voice. Off by default.
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
Log tab
    The debug log: see "The debug log".
Reset all to default (on every tab)
    Asks first, then clears every WorldVoice setting: every voice choice, adjustment and language setting, and it
    turns the debug log off.
The configuration program lists the voices registered for 64-bit programs. A 32-bit program uses the 32-bit
registration of the same voices; a voice that is missing there, or cannot be loaded, is replaced by the main
language's voice.


How it sounds

WorldVoice always gives the program 44100 Hz, 16-bit mono sound, whatever the voices speak in: programs open their
sound player once, when they load the voice, so the format cannot change with the voice reading. 44100 Hz is above
the rates SAPI 5 voices speak in (11025 to 32000 Hz for those tried), so nothing is lost; each voice's sound is
converted to it with WorldVoice's own filter, which adds nothing audible (what it leaves is at
least 97 dB down, the limit of 16-bit sound; Windows' own converter leaves mirror images of the sound only 16 to
39 dB down).
Voices begin and end what they say with silence, as if each piece were a whole text. So that switching languages
does not pause, the silence at the start of a piece is cut to 20 ms, and at its end, when another piece follows, to a
natural pause: 250 ms after the end of a sentence, 150 ms after a comma, colon or semicolon, 80 ms otherwise. Silence
inside a piece is kept.
Each piece goes to its voice marked with its language (the voice's own locale of it, such as English (United
Kingdom) for a British voice), so a voice that speaks several languages reads each piece in the right one.
Word positions and bookmarks refer to the program's own text, so following the text as it is read works as with any
voice. Interrupting reaches the voice reading at once.


Where the settings are kept

HKEY_CURRENT_USER\SOFTWARE\WorldVoice:
    MainLanguage, LatinLanguage, HanLanguage, ArabicLanguage, CyrillicLanguage, DevanagariLanguage
        language codes such as en, zh, ja (defaults: en, en, zh, ar, ru, hi)
    NumbersFollowText
        DWORD, 1 when "Ignore numbers when detecting text language" is ticked
    Voices\<language>
        the voice chosen for the language (its SAPI 5 token, such as
        HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Speech\Voices\Tokens\TTS_MS_EN-US_ZIRA_11.0); no value means None
    Adjustments\<language>
        DWORDs Rate, Pitch and Volume, -10 to 10
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

The language detection is a rule-for-rule port of the WorldVoice NVDA add-on's (version 6.2, its languageDetection
module: find_language_for_script, _language_for_character, add_detected_language_commands and process_for_spelling),
and its table of the script of every character is made from that add-on's code. WorldVoice is by Tseng Woody:
https://github.com/tsengwoody/WorldVoice. This voice also has the add-on's name. The rest (the SAPI 5 voice, the
sound conversion and the configuration program) is written for this voice.


Licence

WorldVoice for SAPI 5 is free software: you can redistribute it and/or modify it under the terms of the GNU General
Public License version 2, as published by the Free Software Foundation. It is distributed in the hope that it will
be useful, but without any warranty; without even the implied warranty of merchantability or fitness for a
particular purpose. The full text is in LICENSE.txt, next to this file. The source code is at
https://github.com/lbk2907/WorldVoiceForSAPI5.
WorldVoice for SAPI 5's own code: copyright Loh Boon Keat.
The language detection's rules and the script table come from the WorldVoice add-on by Tseng Woody
(https://github.com/tsengwoody/WorldVoice), which is under the GNU General Public License.
The script table's Unicode data: Unicode, Inc.'s character database (Unicode 16.0.0), as the add-on has it.


Credits

Tseng Woody, for the WorldVoice NVDA add-on, whose language detection this follows rule for rule.
Claude Opus 5.5 (Anthropic), at ultracode effort, which wrote all of WorldVoice for SAPI 5 by vibe coding with Loh
Boon Keat.
