# Working on WorldVoice for SAPI 5

WorldVoice for SAPI 5 is one SAPI 5 voice that reads each part of a text with the voice chosen for its language,
splitting the text the way the WorldVoice NVDA add-on by Tseng Woody does.  `README.md` says what it is and how to
build it; `setup\README.txt` is the user's guide the setup installs.  These are the conventions for working on it.

## The add-on's rules come first

* **Split a text exactly as the add-on does.**  The language detection in `common\WorldVoice.h` is a rule-for-rule
  port of the add-on's `languageDetection` module, version 6.2 (`find_language_for_script`,
  `_language_for_character`, `add_detected_language_commands` and `process_for_spelling`).  The number mode and the
  pauses at punctuation in `sapi\Pipeline.h` are the same kind of port of the add-on's speech pipeline
  (`iter_number_speech_segments_mode`, `_translate_number` and `inject_punctuation_pause`), from its master branch
  after 6.2, and run before the split, as the add-on runs them before its detection.  A text split or changed
  differently from the add-on is a bug, unless somebody asked for the difference.
* **SAPI's fragments are the add-on's speech sequence.**  `Pipeline.h` takes the text of fragments in a row with the
  same state as one string (NVDA's SAPI 5 driver writes the strings between two commands as one text), a silence as a
  `BreakCommand`, and a bookmark, or a change of state between two texts, as another command.  Keep that mapping when
  porting more of the pipeline, and keep every piece of text pointing into the program's own text, so word positions
  stay right.
* **Work from the add-on's code, not from memory.**  Read the add-on's function while porting or changing its C++
  counterpart.  The add-on is at <https://github.com/tsengwoody/WorldVoice>, and on a computer with it installed in
  `%APPDATA%\nvda\addons\WorldVoice\synthDrivers\WorldVoice\languageDetection`.
* **The script table is generated, never edited.**  `common\ScriptData.h` comes from `tools\make_scripts.py`:
  change the generator and run it again.  It takes the digits and spaces from the Python that runs it
  (`str.isdigit()`, `str.isspace()`), so run it with a Python whose Unicode version (`unicodedata.unidata_version`)
  is the one the add-on's NVDA runs on, and say which in the commit.
* **Every departure is written down.**  What this voice adds or does its own way (the Cyrillic and Devanagari
  settings, the 44100 Hz conversion, the trimmed silences, each language's rate, pitch and volume) is described in
  `README.md` and `setup\README.txt`.  If it was asked for, say so: "(user request)".

## Two READMEs

* **`README.md` is the GitHub page**: everything, building included.  **`setup\README.txt` is the user's guide** the
  setup installs: the same text without building, the script table and the repository's layout.  A change a user
  would notice goes in both.
* `setup\README.txt` is plain text: UTF-8 with a byte order mark, LF line endings, lines of at most 118 characters,
  and a heading is a line of its own with two blank lines before it.
* **Written for a screen reader.**  The maintainer reads with NVDA: lists, not tables or drawn trees, and a link they
  are to follow is a link they can click.  The same goes for anything reported to them.
* The setup carries `setup\README.txt`, so a change to it reaches users with the next setup (`build.cmd`).

## Commits

* One piece of work per commit, with a subject line and a body that says **why**, not just what.
* **Pull, read, fix, commit, push, in that order, for every task.**  Pull before reading a line of the code, not just
  before pushing: a fix worked out on code someone has since changed is a fix to the wrong code.
* Never commit what the build leaves behind: `build\` and `Output\` are ignored.
* **When the maintainer asks for pushing to wait** until a set of work is finished, each piece is still committed on
  its own as it is done, and the lot is pushed once at the end.
* **No GitHub workflows.**  Builds are made by hand with `build.cmd`, as for the maintainer's other projects on a free
  account.  A build step belongs in the README, not in `.github\`.
* **Files are LF** (user request, 2026-10-13: Windows' own tools all read LF nowadays, Notepad, the compiler and
  Inno Setup included).  `.gitattributes` keeps them LF on every machine (`* text=auto eol=lf`), whatever the
  machine's own Git settings.  The one exception is a Windows batch file (`.cmd`, `.bat`): Command Prompt can miss a
  `call :label` in a batch file with LF endings, so `build.cmd` stays CRLF.  A file that must keep its endings, or
  its bytes, gets its own line in `.gitattributes`, as the batch files and the DLLs and programs do.  On Windows,
  Python's text mode writes CRLF: a script that writes a file opens it with `newline='\n'` (as
  `tools\make_scripts.py` does), or with `newline=''` to keep the endings it has.  Check `git diff --stat` before
  committing: it should show only the lines you meant.

## Things that break quietly

* **The voice's class id is in three places**: `CLSID_WorldVoice` and `kClsidText` in `common\WorldVoice.h`, and
  `Clsid` in `setup\worldvoice.iss`.  Change them together, or the registered voice points at a class the DLL does not
  make, and programs just fail to load it.
* **The configuration window's class, `WorldVoiceConfigWnd`, is in three places**: `CLASS` in
  `config\WorldVoiceConfig.rc`, and `RegisterClassExW` and `FindWindowW` in `config\WorldVoiceConfig.cpp`.  A dialog
  whose class is not registered is never created: the program quits with no window and no message.
* **The settings key is shared.**  `HKEY_CURRENT_USER\SOFTWARE\WorldVoice` (`kSettingsKey` in `common\WorldVoice.h`)
  is written by the configuration program and read by the voice, also by a voice already speaking in a program.
* **Two builds, two registrations.**  The voice is a 64-bit and a 32-bit DLL, registered for each (`HKLM64` and
  `HKLM32` in the setup).  The configuration program lists the voices registered for 64-bit programs; a 32-bit
  program looks in the 32-bit registration, and a chosen voice missing there is replaced by the main language's
  voice.  Test both.
* **The sound format is fixed.**  WorldVoice always gives 44100 Hz, 16-bit mono, because programs open their sound
  player once, when they load the voice.  Do not make it follow the voice that is reading.

## Testing

Tests are written for the change at hand and are not kept in the repository.  A test never touches the maintainer's
own WorldVoice: it uses a test copy with its own settings key, class id and voice registration, keeps any window it
opens hidden, and speaks into a WAV file (SAPI's file stream), never to the speakers, and never through the screen
reader the maintainer is using at that moment.  Never change the computer's default sound device for a test.

**Test through the way programs get there.**  Speak through SAPI with the registered test voice, from a 64-bit and a
32-bit program, rather than calling the splitter directly: that gets the bug a program would.

**Check the splitting against the add-on itself.**  Run the add-on's `languageDetection` in Python on the same texts
and compare.  A rule needs all of them: when a change turns on something applied across a whole set (every script,
every language setting), measure the whole set before settling it, and say in the commit how many were looked at.

**Test a thing once.**  A check that has passed for code nobody has touched since will pass again.  Test what the
change touches, and nothing else; do not run a check again to prove the same thing, unless what it covers has changed
underneath it, and then say which change made it worth running.

**Leave nothing running.**  A test that starts processes (a load generator, a hidden window, a setup) checks at the
end that every one of them has gone.  On 2026-10-12, four busy loops left by a load test for another voice were found
to have run for a whole day, slowing the computer and holding a folder open.

## Working with agents

Big pieces of work go to an agent; small ones are done directly.  The maintainer's usage is limited, so:

* **One agent at a time.**  Agents share one working tree, and two at once would commit each other's half-finished
  files.  Wait for one to report before starting the next.
* **An agent cut off by a usage limit leaves its work uncommitted.**  Before anything else is started, look at
  `git status`: resume that agent with its context rather than start another over its files.
* **The brief carries what an agent cannot know.**  This file first; the test copy; silent tests; line endings; one
  commit per piece; and what the maintainer decided, in their words.  An agent reads the code; it does not read the
  conversation.
* **A refused permission is not a puzzle.**  When the permission check turns an action away (a push, a download), the
  agent stops and says so, and the maintainer decides.  Nobody finds another way round it.
* **What an agent reports goes to the maintainer in lists**, not drawn trees or wide tables.
* **Deleting a folder sends it to the Recycle Bin**, never past it.
