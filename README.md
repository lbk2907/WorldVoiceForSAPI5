# WorldVoice for SAPI 5

One SAPI 5 voice, named **WorldVoice**, that reads each part of a text with the voice chosen for its language: Chinese
with a Chinese voice, English with an English voice, and so on, switching in the middle of a sentence when the text
does. It splits a text the way the [WorldVoice NVDA add-on](https://github.com/tsengwoody/WorldVoice) by Tseng Woody
does, and so brings that add-on's voice switching to every SAPI 5 program, 64-bit or 32-bit, with the add-on's number
mode and pauses at punctuation.

WorldVoice has no voice of its own: it speaks through the SAPI 5 voices already installed on the computer. Voices of
other kinds (Windows' OneCore voices, for example) are not used.

Version 1.0, by Loh Boon Keat. Free software under the GNU General Public License version 2 (see [Licence](#licence)).

Made entirely by vibe coding, with Claude Opus 5.5 at ultracode effort.

## Contents

- [Features](#features)
- [Requirements](#requirements)
- [Installing](#installing)
- [Getting started](#getting-started)
- [How WorldVoice splits a text](#how-worldvoice-splits-a-text)
- [Numbers and punctuation](#numbers-and-punctuation)
- [The configuration program](#the-configuration-program)
- [How it sounds](#how-it-sounds)
- [Where the settings are kept](#where-the-settings-are-kept)
- [The debug log](#the-debug-log)
- [What the setup installs](#what-the-setup-installs)
- [Building](#building)
- [What is in this repository](#what-is-in-this-repository)
- [Where it comes from](#where-it-comes-from)
- [Licence](#licence)
- [Credits](#credits)

## Features

- **Switches voices by language, mid-sentence**, with the WorldVoice add-on's own rules: its language detection
  (version 6.2), ported rule for rule, and its table of the Unicode script of every character.
- **Works in every SAPI 5 program**, 64-bit and 32-bit: screen readers such as NVDA (through its SAPI 5 driver),
  reading programs, and anything else that speaks with SAPI 5.
- **A voice, rate, pitch and volume for each language**, set in a configuration program; every change is used from the
  next sentence, also in programs already speaking.
- **The add-on's number mode and pauses at punctuation**: numbers read as values, digit by digit or two digits at a
  time, and a pause of your choosing after punctuation, with the marks left out if you like.
- **Follows the speaking program**: text the program marks with a language (NVDA's automatic language switching, a
  `<lang>` tag in SAPI's XML) and spelled text go the way the add-on sends them.
- **Smooth switching**: each voice's silence before and after it speaks is cut to a natural pause, so changing
  language does not stop the flow.
- **One sound format**: always 44100 Hz, 16-bit mono, converted from each voice with its own high-quality filter.
- **Follows the text as it is read**: word positions and bookmarks refer to the program's own text, and interrupting
  stops the voice at once.
- **Screen reader friendly configuration program**: plain tabs (Ctrl+Tab and Ctrl+Shift+Tab switch them), and no
  wall of text read out on opening.
- **An optional debug log** of how each text was split and read.

## Requirements

- Windows 10 or 11, 64-bit.
- SAPI 5 voices for the languages you want to hear.

## Installing

1. Get `WorldVoice for SAPI 5 Setup.exe`: the latest release's setup downloads straight from
   <https://github.com/lbk2907/WorldVoiceForSAPI5/releases/latest/download/WorldVoice.for.SAPI.5.Setup.exe> (every
   release is on the [Releases page](https://github.com/lbk2907/WorldVoiceForSAPI5/releases)), or build it yourself
   (see [Building](#building)).
2. Run it. It needs administrator rights. It offers a Start Menu shortcut and a desktop shortcut to the configuration
   program.

To uninstall, use Windows Settings, Apps, Installed apps, "WorldVoice for SAPI 5". That removes the files and the
voice's registration. The settings stay (see [Where the settings are kept](#where-the-settings-are-kept)); the
configuration program's "Reset all to default" clears them first if you want them gone.

## Getting started

1. Install SAPI 5 voices for the languages you want to hear.
2. Install WorldVoice (see [Installing](#installing)).
3. Open WorldVoice's configuration program (the "WorldVoice" shortcut, or `WorldVoiceConfig.exe` in the install
   folder). On the Voices tab, choose a voice for each language. A language left at None is not switched to.
4. Choose the voice "WorldVoice" in the program that speaks. In NVDA: Speech settings, Synthesizer "Microsoft Speech
   API version 5", Voice "WorldVoice".

Until a voice is chosen for at least one language, one voice reads everything: Windows' default SAPI 5 voice, else an
American English voice, else the first voice installed. The configuration program says so at the bottom of its
window.

Every change in the configuration program is saved at once (there is no Apply button) and used from the voice's next
sentence, also in programs already speaking with it.

## How WorldVoice splits a text

Each character's script (its writing system) decides its language:

- **A script that belongs to one language** goes to it: Hiragana and Katakana to Japanese, Hangul to Korean, Thai to
  Thai, Greek to Greek, Hebrew to Hebrew, and so on for Armenian, Bengali, Bopomofo (Chinese), Georgian, Gujarati,
  Gurmukhi (Punjabi), Kannada, Khmer, Lao, Malayalam, Mongolian, Myanmar, Oriya, Sinhala, Tamil, Telugu and Tibetan.
- **A script that several languages share** (Latin, Cyrillic, Arabic, Devanagari, and the Chinese characters that
  Chinese, Japanese and Korean all use): while the text around it is already in one of that script's languages, it
  stays in that language (kanji in Japanese text are read in Japanese). Otherwise it goes to the language chosen for
  that script on the General tab, when that language has a voice; else to the first of the script's languages that
  has one.
- **A language with no voice chosen** is never switched to: its text stays with the language around it.

Also:

- Spaces, punctuation and symbols stay with the text around them.
- Numbers are read in the main language. With "Ignore numbers when detecting text language" ticked, they are read in
  the language of the text around them instead.
- Text the program marks with a language (NVDA's automatic language switching, a `<lang>` tag in SAPI's XML) takes
  that language in place of the main language, for itself and its numbers, and the detection carries on from it. When
  that language has no voice chosen, the main language's voice reads it.
- Spelled text (character by character) goes the same way, one character at a time; spaces and digits go to the main
  language (or the program's marked one).

Languages that share a script cannot be told apart by it: Latin text in English text stays English, so a French word
in an English sentence is read by the English voice. Only text in a script of its own, or marked by the program,
switches.

## Numbers and punctuation

Before the text is split by language, WorldVoice does what the add-on does with numbers and punctuation (the settings
are on the configuration program's [Numbers and punctuation tab](#numbers-and-punctuation-tab)).

**Number mode:**

- **Value** (the default): numbers as they are written, for the voice to read its own way: "2.54.2" and
  "192.168.1.1" stay as they are. (The add-on also sets a full stop next to a number apart with a space, so that
  "2.54.2" is read as "2.54 . 2"; WorldVoice for SAPI 5 leaves that out, in every mode, by user request.)
- **Number**: digit by digit, "12345" as "1 2 3 4 5".
- **Pair**: two digits at a time from the left, "12345" as "12 34 5"; a sign goes with the first pair ("-1234" as
  "-12 34"), and a decimal number is read as a value. Between the pairs goes a space, or, with a pair wait factor, a
  pause: 5 ms for each step of the factor, at most 500 ms.

**Pauses at punctuation.** With a punctuation wait factor (0, the default, is no pause), WorldVoice pauses after the
punctuation pause characters: 5 ms for each step, at most 500 ms.

- Marks in a row ("?!", "...") are one pause.
- The pause comes only where text follows: later in the same text, or in the next text the program sends straight
  after it; not across a bookmark, nor across a change of rate, pitch, volume, language or spelling the program makes
  there (NVDA's commands); and never at the very end.
- A mark with a digit after it is not a pause point ("12,500", "3.5").
- Where the program already asks for a silence, its silence takes the place of the pause: they do not add up.
- The characters are, unless you change them: the comma, full stop, semicolon, colon, exclamation mark, question mark
  and ellipsis, and the Arabic comma, semicolon and question mark (`،؛,.;:!؟?…`). Spaces around them are ignored; with
  none, there are no pauses.

**Removing the marks.** With "Remove configured punctuation at pause points" ticked, the marks at those pause points
are left out of the text, with or without the pause, so the voice neither reads them nor pauses on its own there. A
mark at the very end stays. A mark where the program asks for a silence is still left out.

WorldVoice gets the text after the program has made it ready: a mark a screen reader reads out as a word (NVDA at
punctuation level "all") arrives as that word, and is not a mark any more. Spelled text is left as it is. In Number
mode, the add-on can also replace each digit with a word from its own symbols dictionary; WorldVoice has no such
dictionary, so digits stay digits.

## The configuration program

It has four tabs. Ctrl+Tab and Ctrl+Shift+Tab move between them.

### General tab

- **Main language**: the language of the text, what WorldVoice reads in when nothing else decides, and the language
  of numbers. The list holds the languages that have a voice chosen. When the main language has none, English is used
  if it has a voice, else another language that has one.
- **Language detection**: the language assumed for Latin, CJK, Arabic, Cyrillic and Devanagari characters, for that
  script's text when the text around it is not already in one of the script's languages (see
  [How WorldVoice splits a text](#how-worldvoice-splits-a-text)). Each list holds the script's languages that have a
  voice chosen, and is unavailable when there are none. The WorldVoice add-on offers Latin, CJK and Arabic; Cyrillic
  and Devanagari are added here, for the same reason.
- **Ignore numbers when detecting text language**: numbers are read in the language of the text around them, not by
  the main language's voice. Off by default.

### Voices tab

- **Languages and their voices**: every language an installed voice speaks, with its voice and its rate, pitch and
  volume adjustments.
- **Voice for the selected language**: None (the default: WorldVoice does not switch to that language), or one of the
  voices that speak it.
- **Rate, Pitch and Volume**: from -10 to +10; 0 (the default) is as the speaking program asks. Rate and pitch are
  added to the program's, in SAPI's steps; volume changes by 10% a step, so it can also go above the program's. Each
  language has its own.
- **Test**: type a text and press Play to hear it with the selected language's voice and adjustments. With "Test all
  languages together, as WorldVoice reads them" ticked, Play reads it the way WorldVoice does, each language with its
  own voice. The test speaks through the installed WorldVoice voice, so it sounds exactly as in use.

### Numbers and punctuation tab

The add-on's settings of the same names (see [Numbers and punctuation](#numbers-and-punctuation)). Each slider goes
from 0 to 100, and shows its pause beside it.

- **Number mode**: Value (the default), Number (digit by digit) or Pair (two digits at a time).
- **Pair wait factor**: in Pair mode, the pause between the pairs; 0 (the default): a space.
- **Enable punctuation pause control**: ticked by default. Untick it for no pauses at punctuation, whatever the wait
  factor.
- **Remove configured punctuation at pause points**: off by default.
- **Punctuation wait factor**: the pause at punctuation; 0 (the default): none. Unavailable while the pause control is
  off.
- **Punctuation pause characters**: the marks to pause at (and to remove). Unavailable while both boxes above are
  unticked.

### Log tab

The debug log: see [The debug log](#the-debug-log).

### Reset all to default

On every tab. It asks first, then clears every WorldVoice setting: every voice choice, adjustment, language, number
and punctuation setting, and it turns the debug log off.

The configuration program lists the voices registered for 64-bit programs. A 32-bit program uses the 32-bit
registration of the same voices; a voice that is missing there, or cannot be loaded, is replaced by the main
language's voice.

## How it sounds

- **One format.** WorldVoice always gives the program 44100 Hz, 16-bit mono sound, whatever the voices speak in:
  programs open their sound player once, when they load the voice, so the format cannot change with the voice reading.
  44100 Hz is above the rates SAPI 5 voices speak in (11025 to 32000 Hz for those tried), so nothing is lost. Each
  voice's sound is converted with WorldVoice's own filter (`sapi\Resampler.h`), which adds nothing audible: what it
  leaves is at least 97 dB down, the limit of 16-bit sound (Windows' own converter leaves mirror images of the sound
  only 16 to 39 dB down).
- **Natural pauses.** Voices begin and end what they say with silence, as if each piece were a whole text. So that
  switching languages does not pause, the silence at the start of a piece is cut to 20 ms, and at its end, when another
  piece follows, to a natural pause: 250 ms after the end of a sentence, 150 ms after a comma, colon or semicolon, 80
  ms otherwise, with any silence asked for there (by the program, or a pause at punctuation) on top. Silence inside a
  piece is kept.
- **The right language for each piece.** Each piece goes to its voice marked with its language (the voice's own locale
  of it, such as English (United Kingdom) for a British voice), so a voice that speaks several languages reads each
  piece in the right one.
- **Following the text.** Word positions and bookmarks refer to the program's own text, so following the text as it is
  read works as with any voice. Interrupting reaches the voice reading at once.

## Where the settings are kept

In `HKEY_CURRENT_USER\SOFTWARE\WorldVoice`:

- `MainLanguage`, `LatinLanguage`, `HanLanguage`, `ArabicLanguage`, `CyrillicLanguage`, `DevanagariLanguage`:
  language codes such as `en`, `zh`, `ja` (defaults: `en`, `en`, `zh`, `ar`, `ru`, `hi`).
- `NumbersFollowText`: DWORD, 1 when "Ignore numbers when detecting text language" is ticked.
- `Voices\<language>`: the voice chosen for the language, as its SAPI 5 token, such as
  `HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Speech\Voices\Tokens\TTS_MS_EN-US_ZIRA_11.0`; no value means None.
- `Adjustments\<language>`: DWORDs `Rate`, `Pitch` and `Volume`, -10 to 10.
- `NumberMode`: `value` (the default), `number` or `pair`.
- `PairWaitFactor`, `PunctuationWaitFactor`: DWORDs, 0 to 100 (default 0).
- `PunctuationPause`, `RemovePunctuation`: DWORDs, 1 when "Enable punctuation pause control" (the default) or "Remove
  configured punctuation at pause points" is ticked.
- `PunctuationCharacters`: the punctuation pause characters; no value means the default ones.
- `Log`: DWORD, 1 while the debug log is on.

## The debug log

For finding out what the voice does: how WorldVoice split each text by language, which voice read each piece, how
much audio it gave and how long it all took, and any voice that could not be loaded. It is off unless you turn it on.

**Turning it on and off.** On the configuration program's Log tab: "Write a debug log". It takes effect within a
second, also in programs already using the voice. The tab also shows where the log is and how big; "Open the log
folder" opens it in File Explorer with the log selected, to copy or send it, and "Clear the log..." deletes it, after
asking. The switch is the DWORD value `Log` (1 on, 0 off) in `HKEY_CURRENT_USER\SOFTWARE\WorldVoice`. Without the
configuration program, in a Command Prompt or the Windows+R box:

```bat
reg add "HKCU\SOFTWARE\WorldVoice" /v Log /t REG_DWORD /d 1 /f
reg add "HKCU\SOFTWARE\WorldVoice" /v Log /t REG_DWORD /d 0 /f
```

**Where it is.** `%TEMP%\WorldVoice.log`, usually `C:\Users\<your name>\AppData\Local\Temp\WorldVoice.log`. Each line
starts with the time. The voice adds to the file and never empties it, so clear it when you no longer need it.

The log holds all the text the voice read. Turn it off when you have finished, and look through it before you share
it.

## What the setup installs

Everything goes to `C:\Program Files\WorldVoice`:

- `x64\WorldVoiceSapi.dll` and `x86\WorldVoiceSapi.dll`: the voice, for 64-bit and 32-bit programs
- `WorldVoiceConfig.exe`: the configuration program
- `README.txt`: the user's guide (`setup\README.txt` here)
- `LICENSE.txt`: the GNU General Public License version 2

The voice is registered for 64-bit and 32-bit programs as "WorldVoice"
(`HKLM\SOFTWARE\Microsoft\Speech\Voices\Tokens\WorldVoice`; class `{47BE0B8C-0DF1-4FB0-BBE9-5EA76A365C4C}`). The setup
offers a Start Menu shortcut and a desktop shortcut to the configuration program, and on its last page offers to open
the README, unticked.

## Building

### What it needs

- Windows 10 or 11, 64-bit.
- [Visual Studio Build Tools](https://visualstudio.microsoft.com/downloads/) (2022 or later) with "Desktop development
  with C++".
- [Inno Setup 7](https://jrsoftware.org/isdl.php), in its default folder (`C:\Program Files\Inno Setup 7`, or
  `C:\Program Files (x86)\Inno Setup 7`), for the setup. Without it, `build.cmd` still builds the voice and the
  configuration program, and says that the setup was not made.
- [Python 3](https://www.python.org/downloads/), only to make `common\ScriptData.h` again (see
  [The script table](#the-script-table)).

### Build

From the project folder, in a Command Prompt:

```bat
build.cmd
```

This builds `build\x64\WorldVoiceSapi.dll`, `build\x86\WorldVoiceSapi.dll` and `build\WorldVoiceConfig.exe`, then the
setup, `Output\WorldVoice for SAPI 5 Setup.exe` (under a minute). To clean the folder afterwards, delete `build\` and
`Output\`.

### The script table

Only needed when the WorldVoice add-on's language detection changes; `common\ScriptData.h` in this repository was made
from the add-on's version 6.2.

```bat
python tools\make_scripts.py "<WorldVoice add-on>\synthDrivers\WorldVoice\languageDetection"
```

This writes `common\ScriptData.h`: the Unicode script of every character (from the add-on's `script_data.py`, Unicode
16.0.0), the languages each script can be and the scripts with a language setting (from its `__init__.py`), and the
digits and spaces as Python's `str.isdigit()`, `str.isdecimal()` and `str.isspace()` see them, which the add-on uses.
The table is made from the add-on rather than taken from Windows, whose Unicode data is older, so that WorldVoice
splits a text exactly as the add-on does.

## What is in this repository

- `build.cmd`: builds the voice and the configuration program into `build\`, then the setup into `Output\`.
- `common\WorldVoice.h`: the settings, the installed voices, and the language detection (shared by both).
- `common\ScriptData.h`: the script table (generated, see [The script table](#the-script-table)).
- `sapi\WorldVoiceSapi.cpp`: the voice, which splits the text and drives each language's voice.
- `sapi\Pipeline.h`: the add-on's number mode and pauses at punctuation.
- `sapi\Resampler.h`: the filter that converts each voice's sound to 44100 Hz.
- `sapi\WorldVoiceSapi.rc` and `.def`: the voice's version information and exports.
- `config\`: the configuration program (`WorldVoiceConfig.cpp`, `.rc`, `resource.h`, `.manifest`).
- `setup\worldvoice.iss`: the setup (Inno Setup).
- `setup\README.txt`: the user's guide the setup installs.
- `tools\make_scripts.py`: makes `common\ScriptData.h`.
- `LICENSE`: the GNU General Public License version 2.
- `workflow.md` and `CLAUDE.md`: the conventions for working on this project.

Made while building: `build\` and `Output\` (the setup); `.gitignore` leaves them out.

## Where it comes from

The language detection is a rule-for-rule port of the WorldVoice NVDA add-on's (version 6.2, its `languageDetection`
module: `find_language_for_script`, `_language_for_character`, `add_detected_language_commands` and
`process_for_spelling`), and `common\ScriptData.h` is generated from that add-on's code. The number mode and the pauses
at punctuation are a rule-for-rule port of the add-on's speech pipeline (`inject_number_mode`, with
`iter_number_speech_segments_mode` and `_translate_number`, and `inject_punctuation_pause`), as its master branch has
them after version 6.2 (September 2026). WorldVoice is by Tseng Woody: <https://github.com/tsengwoody/WorldVoice>. This
voice also has the add-on's name. The rest (the SAPI 5 voice, the sound conversion and the configuration program) is
written for this voice.

## Licence

WorldVoice for SAPI 5 is free software: you can redistribute it and/or modify it under the terms of the GNU General
Public License version 2, as published by the Free Software Foundation. It is distributed in the hope that it will be
useful, but without any warranty; without even the implied warranty of merchantability or fitness for a particular
purpose. See [LICENSE](LICENSE) for the full text.

- WorldVoice for SAPI 5's own code: copyright Loh Boon Keat.
- The language detection's rules, the number mode, the pauses at punctuation and `common\ScriptData.h` come from the
  [WorldVoice add-on](https://github.com/tsengwoody/WorldVoice) by Tseng Woody, which is under the GNU General Public
  License.
- The script table's Unicode data: Unicode, Inc.'s character database (Unicode 16.0.0), as the add-on has it.

## Credits

- Tseng Woody, for the [WorldVoice NVDA add-on](https://github.com/tsengwoody/WorldVoice), whose language detection,
  number mode and pauses at punctuation this follows rule for rule.
- Claude Opus 5.5 (Anthropic), at ultracode effort, which wrote all of WorldVoice for SAPI 5 by vibe coding with Loh
  Boon Keat.
