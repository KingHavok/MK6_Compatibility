# TVicHW32 Shim for MK6Emu

A drop-in replacement for `TVicHW32.dll` that allows MK6Emu to run on modern Windows (10/11) without requiring Win2000 compatibility mode or the original kernel-mode hardware I/O driver. Also replaces `KEYBOARDCONNECTOR.exe` with a built-in keyboard hook.

## What it does

**TVicHW32 stubs:** The original `TVicHW32.dll` is a kernel-mode driver library for direct hardware I/O port access (by EnTech Taiwan). It requires a `.SYS`/`.VXD` driver that won't load on modern Windows due to driver signing requirements. Since MK6Emu doesn't need real hardware I/O, the shim stubs all 10 imported functions — `OpenTVicHW32` returns a fake handle, all port read/write functions are no-ops, and `MapPhysToLinear` allocates a zeroed memory block.

**Version spoofing:** MK6Emu's audio initialization (DirectSound/waveOut) fails on Windows Vista+ because the app or its MFC runtime branches on the OS version. The shim uses IAT (Import Address Table) hooking to intercept `GetVersion` and `GetVersionExA` calls, making the app believe it's running on Windows 2000 SP4 (version 5.0, build 2195). This causes the audio subsystem to take the legacy initialization path, which works correctly through the modern WASAPI translation layer.

**Keyboard connector:** A low-level keyboard hook (`WH_KEYBOARD_LL`) replaces the external `KEYBOARDCONNECTOR.exe` AutoHotkey app. The hook is active whenever the "MK6 Emulator" window or any "Roms Screen" window is in the foreground, and inactive for all other applications.

## Keyboard map

### Native keys (forwarded to MK6 Emulator)

These keys are sent as keystrokes to the emulator window when a Roms Screen is in the foreground:

| Key | Function |
|-----|----------|
| `1`-`8` | Top button row (Reserve/Gamble, Bet lines) |
| `Q` `W` `E` `R` `T` `Y` `U` `I` | Bottom button row (Take Win, Play Reels, etc.) |
| `Space` | Service / generic |
| `F4` | Toggle panel visibility |

### Button clicks (sent as BM_CLICK to emulator child buttons)

| Key | Button | Function |
|-----|--------|----------|
| `CapsLock` | Button17 | Audit |
| `F` | Button18 | Jackpot |
| `A` | Button19 | Logic |
| `S` | Button20 | Main-Opt |
| `D` | Button21 | Main-Mec |
| `Enter` | Button22 | Coins |

### Special keys

| Key | Action |
|-----|--------|
| `G` | Credit payout combo: sends Take Win (`q`), then clicks Jackpot (Button18) twice |
| `Shift` | Toggle fullscreen (Alt+Enter) and move cursor off-screen |
| `Escape` | Close emulator |

## Exported functions

The shim exports the following functions by ordinal (matching the original DLL's export table):

| Ordinal | Function | Behavior |
|---------|----------|----------|
| 1 | `OpenTVicHW32` | Returns a fake handle (always succeeds) |
| 2 | `CloseTVicHW32` | No-op |
| 3 | `GetActiveHW` | Returns the fake handle |
| 12 | `GetPortByte` | Returns 0 |
| 13 | `SetPortByte` | No-op |
| 14 | `GetPortWord` | Returns 0 |
| 15 | `SetPortWord` | No-op |
| 16 | `GetPortLong` | Returns 0 |
| 17 | `SetPortLong` | No-op |
| 30 | `MapPhysToLinear` | Allocates zeroed memory via `VirtualAlloc` |

## Building

### Requirements

A C compiler targeting 32-bit Windows (PE32). The EXE is a 32-bit application.

### MinGW (any platform)

```bash
i686-w64-mingw32-gcc -shared -o TVicHW32.dll TVicHW32.c TVicHW32.def -lkernel32 -luser32 -O2 -Wl,--enable-stdcall-fixup
```

### MSVC (x86 Developer Command Prompt)

```bat
cl /LD /O2 TVicHW32.c /link /DEF:TVicHW32.def /OUT:TVicHW32.dll
```

### Windows batch

```bat
build.bat
```

## Installation

1. Back up the original `TVicHW32.dll` in the MK6Emu directory.
2. Copy the built `TVicHW32.dll` into the MK6Emu directory.
3. Run `MK6Emu.exe` — no compatibility mode needed, no `KEYBOARDCONNECTOR.exe` needed.

## How the IAT hook works

When the shim DLL loads (`DllMain` at `DLL_PROCESS_ATTACH`, before `WinMain`), it:

1. Resolves the addresses of the real `GetVersion` and `GetVersionExA` in `kernel32.dll`
2. Walks the main executable's PE import table
3. Finds the IAT entries pointing to those functions
4. Overwrites them with pointers to the shim's replacements that return Windows 2000 version info

This is the same technique Windows' own Application Compatibility Engine (`apphelp.dll`) uses when you set compatibility mode on an executable.

## License

MIT
