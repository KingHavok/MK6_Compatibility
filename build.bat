@echo off
REM Build TVicHW32.dll shim
REM
REM Option 1: MinGW (32-bit) - works on any platform
REM   i686-w64-mingw32-gcc -shared -o TVicHW32.dll TVicHW32.c TVicHW32.def -lkernel32
REM
REM Option 2: MSVC (from a 32-bit Developer Command Prompt)
REM   cl /LD /O2 TVicHW32.c /link /DEF:TVicHW32.def /OUT:TVicHW32.dll
REM
REM After building, copy TVicHW32.dll to the MK6 directory (replacing the original).

echo Building TVicHW32.dll shim (32-bit)...

where cl >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo Using MSVC...
    cl /LD /O2 TVicHW32.c /link /DEF:TVicHW32.def /OUT:TVicHW32.dll
) else (
    where i686-w64-mingw32-gcc >nul 2>&1
    if %ERRORLEVEL% EQU 0 (
        echo Using MinGW...
        i686-w64-mingw32-gcc -shared -o TVicHW32.dll TVicHW32.c TVicHW32.def -lkernel32
    ) else (
        echo ERROR: No 32-bit C compiler found.
        echo Install MinGW-w64 ^(i686^) or run from a VS x86 Developer Command Prompt.
        exit /b 1
    )
)

if exist TVicHW32.dll (
    echo.
    echo Success! Copy TVicHW32.dll to the MK6 directory:
    echo   copy TVicHW32.dll ..\TVicHW32.dll
) else (
    echo Build failed.
    exit /b 1
)
