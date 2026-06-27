@ECHO OFF
IF "%1" EQU "client" GOTO CHKARG2
IF "%1" EQU "server" GOTO CHKARG2
GOTO INVARG
:CHKARG2
IF "%2" NEQ "" GOTO BUILDFW
GOTO INVARG
:BUILDFW
py %~dp0build.py "%1"
IF "%ERRORLEVEL%" NEQ "0" (
    ECHO Build failed: %ERRORLEVEL%
    GOTO EXITSCRIPT
)
ECHO Start write flash
py -m esptool "--port" "%2" "--chip" "esp32c3" "write-flash" "0x0" "%~dp0build\%1\FuckerDetectorX.ino.merged.bin"
IF "%ERRORLEVEL%" NEQ "0" (
    ECHO Write flash failed: %ERRORLEVEL%
    GOTO EXITSCRIPT
)
GOTO EXITSCRIPT
:INVARG
ECHO Usage: %0 ^<build_mode: client^|server^> ^<serial_id^>
SET "ERRORLEVEL=1"
:EXITSCRIPT