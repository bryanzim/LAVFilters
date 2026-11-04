@ECHO OFF
SETLOCAL

PUSHD "%~dp0"

SET nbCOMMIT_PART=0
SET OLDVER=
SET BASE=
SET LINE=

:: LAV_VERSION_BUILD is the commits on upstream since the last official
:: release. upstream/master tracks that line, so the count matches the
:: upstream beta number. origin/master is the fallback for a clone without
:: that remote; it also carries this fork's own commits and merges, so it
:: counts high. Neither clone has upstream tags, so the release base is the
:: newest commit on that line that changed the version macros, which is
:: the commit upstream tags.
CALL git rev-parse --verify --quiet "upstream/master^{commit}" >NUL 2>&1
IF NOT ERRORLEVEL 1 SET "LINE=upstream/master"
IF NOT DEFINED LINE (
    CALL git rev-parse --verify --quiet "origin/master^{commit}" >NUL 2>&1
    IF NOT ERRORLEVEL 1 SET "LINE=origin/master"
)
IF NOT DEFINED LINE GOTO WRITE_VER

FOR /F "delims=" %%A IN ('git describe --tags --abbrev^=0 --match "0.*" %LINE% 2^>NUL') DO SET "BASE=%%A"
IF DEFINED BASE GOTO COUNT

FOR /F "delims=" %%A IN ('git log -1 --format^=%%H %LINE% -G "define LAV_VERSION_" -- includes/version.h') DO SET "BASE=%%A"
IF NOT DEFINED BASE GOTO WRITE_VER

:COUNT
FOR /F "delims=" %%A IN ('git rev-list --count %BASE%..%LINE%') DO SET "nbCOMMIT_PART=%%A"

:WRITE_VER

:: check if info changed, and write if needed
IF EXIST includes\version_rev.h (
    SET /P OLDVER=<includes\version_rev.h
)
SET NEWVER=#define LAV_VERSION_BUILD %nbCOMMIT_PART%
IF NOT "%NEWVER%" == "%OLDVER%" (
    :: swapped order to avoid trailing newlines
    > includes\version_rev.h ECHO %NEWVER%
)
GOTO :END

:END
POPD
ENDLOCAL
