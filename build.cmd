@echo off
rem Builds build\x64\WorldVoiceSapi.dll, build\x86\WorldVoiceSapi.dll and build\WorldVoiceConfig.exe, then the setup,
rem Output\WorldVoice for SAPI 5 Setup.exe (with Inno Setup 7).
setlocal
set "HERE=%~dp0"
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq delims=" %%i in (`call "%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VS=%%i"
if not defined VS (echo Visual Studio C++ build tools not found & exit /b 1)
rem /utf-8: the sources are UTF-8 (the configuration program's sample text has Chinese, Japanese and Korean).
set "CL_FLAGS=/nologo /O2 /MT /EHsc /W4 /utf-8 /DUNICODE /D_UNICODE"

call :dll x64 x64 || exit /b 1
call :dll x86 x64_x86 || exit /b 1
call :config || exit /b 1
echo Build OK
call :setup || exit /b 1
exit /b 0

:dll
setlocal
call "%VS%\VC\Auxiliary\Build\vcvarsall.bat" %2 >nul 2>nul || exit /b 1
set "OUT=%HERE%build\%1"
if not exist "%OUT%" mkdir "%OUT%"
pushd "%OUT%"
rc /nologo /fo WorldVoiceSapi.res "%HERE%sapi\WorldVoiceSapi.rc" || (popd & exit /b 1)
cl %CL_FLAGS% /LD "%HERE%sapi\WorldVoiceSapi.cpp" WorldVoiceSapi.res /Fe:WorldVoiceSapi.dll /link /DEF:"%HERE%sapi\WorldVoiceSapi.def" sapi.lib ole32.lib advapi32.lib || (popd & exit /b 1)
del /q *.obj *.exp *.lib WorldVoiceSapi.res 2>nul
popd
echo Built %1\WorldVoiceSapi.dll
exit /b 0

:config
setlocal
call "%VS%\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>nul || exit /b 1
if not exist "%HERE%build" mkdir "%HERE%build"
pushd "%HERE%build"
rc /nologo /i "%HERE%config" /fo WorldVoiceConfig.res "%HERE%config\WorldVoiceConfig.rc" || (popd & exit /b 1)
cl %CL_FLAGS% "%HERE%config\WorldVoiceConfig.cpp" WorldVoiceConfig.res /Fe:WorldVoiceConfig.exe /link /SUBSYSTEM:WINDOWS comctl32.lib user32.lib advapi32.lib ole32.lib oleaut32.lib oleacc.lib sapi.lib shell32.lib || (popd & exit /b 1)
del /q *.obj WorldVoiceConfig.res 2>nul
popd
echo Built WorldVoiceConfig.exe
exit /b 0

:setup
rem Inno Setup 7 in its default folder: the 64-bit Program Files (also when this runs from a 32-bit program), or the
rem 32-bit one. Without it, the programs are built but not the setup.
set "ISCC=%ProgramW6432%\Inno Setup 7\ISCC.exe"
if not exist "%ISCC%" set "ISCC=%ProgramFiles(x86)%\Inno Setup 7\ISCC.exe"
if not exist "%ISCC%" (echo Inno Setup 7 not found: the programs are built, but not the setup. See README.md. & exit /b 1)
"%ISCC%" /Q "%HERE%setup\worldvoice.iss" || exit /b 1
echo Built Output\WorldVoice for SAPI 5 Setup.exe
exit /b 0
