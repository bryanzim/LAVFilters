@ECHO OFF
SETLOCAL

PUSHD "%~dp0"

SET nbCOMMIT_PART=0
SET OLDVER=
SET BASE=

:: LAV_VERSION_BUILD is the commits on upstream since the last official
:: release. origin/master tracks that line. Commits only on this branch
:: are not counted. This clone has no upstream tags, so the release base
:: is the newest origin/master commit that changed the version macros,
:: which is the commit upstream tags.
CALL git rev-parse --verify origin/master >NUL 2>&1
IF ERRORLEVEL 1 GOTO WRITE_VER

FOR /F "delims=" %%A IN ('git describe --tags --abbrev^=0 --match "0.*" origin/master 2^>NUL') DO SET "BASE=%%A"
IF DEFINED BASE GOTO COUNT

FOR /F "delims=" %%A IN ('git log -1 --format^=%%H origin/master -G "define LAV_VERSION_" -- includes/version.h') DO SET "BASE=%%A"
IF NOT DEFINED BASE GOTO WRITE_VER

:COUNT
FOR /F "delims=" %%A IN ('git rev-list --count %BASE%..origin/master') DO SET "nbCOMMIT_PART=%%A"

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
