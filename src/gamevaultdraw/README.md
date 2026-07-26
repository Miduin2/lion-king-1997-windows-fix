# GameVaultDraw

GameVaultDraw is a small, auditable DirectDraw compatibility layer for the
Windows release of Disney's The Lion King included in the European
*Disney's Classic Video Games* collection.

The resulting 32-bit `ddraw.dll` loads the genuine Windows `ddraw.dll`, wraps
the DirectDraw, surface and palette interfaces, and writes diagnostic details
to `GameVaultDraw.log`. It does not emulate video modes or modify the desktop.

The first compatibility repair is deliberately narrow: if a blit destination
has an invalid bottom edge but a valid source rectangle, GameVaultDraw derives
the intended height from the source before forwarding the call. This covers a
Win9x-era uninitialised-`RECT` behaviour observed in these Disney conversions
without hardcoding a desktop resolution.

For 8-bit software running on a true-colour desktop, it also preserves a
logical palette association when native DirectDraw returns
`DDERR_INVALIDPIXELFORMAT`. The application can subsequently retrieve and
update that palette while its already-converted 32-bit surfaces remain native.

Diagnostic builds register a vectored exception observer after the first
`DirectDrawCreate` call. It records access type, x86 registers and a bounded
stack snapshot, then returns `EXCEPTION_CONTINUE_SEARCH`; it never handles or
suppresses the application's exception.

The Lion King launcher also uses a temporary `SUBST` drive because the game
constructs commands such as `title.mid alias 0 wait` in a fixed-size buffer.
Long preservation paths can overwrite its x86 stack and return address. The
mapping provides a short path without relocating the preserved game and is
removed in the launcher's `finally` block.

`GameVaultDraw.ini` controls borderless presentation and timing. The default
uses a 4:3 image fitted to the current monitor without changing the Windows
display mode and limits primary-surface presentation to 60 frames per second.
Set `Fullscreen=0` for the original window or `FrameLimit=0` to disable timing.
The borderless window is explicitly marked `WS_EX_APPWINDOW`, made activatable
and detached from any legacy owner so it remains visible in the taskbar and
the Alt+Tab switcher.

Experimental builds can provide DWM with a custom 32-bit iconic thumbnail and
live preview derived from the last locked 320x200 frame. This compensates for
the compositor being unable to capture DirectDraw's primary-surface output.

The final build maps `F2` to the game's native Properties command and restores
a safe `Esc` exit confirmation. It also repairs the Joystick and Keyboard
property pages. Their Win9x edit-control callbacks truncate the pointer in
`EM_GETSEL` to 16 bits; GameVaultDraw hooks only the executable's relevant IAT
calls, recognises known callbacks by RVA and reconstructs only the affected
pointer when its companion pointer proves that they are adjacent.

The validated 1.0.0 Lion King release therefore supports the original System,
Sound, Difficulty, Joystick and Keyboard pages, including changes to
difficulty and keyboard bindings. It also hides the busy cursor over the
enabled game surface while preserving it for Properties and exit dialogs.

## Build

Run `build.cmd` from a normal command prompt. It discovers Visual Studio 2022
C++ Build Tools through `vswhere.exe`. Set `GV_VS` explicitly if the installer
cannot discover a non-standard installation.

The output is written to `build\Win32\Release\ddraw.dll`.

Run `smoke-test.cmd` to compile and execute a tiny 32-bit host that loads the
diagnostic DLL, calls `DirectDrawCreate` and `GetDisplayMode`, and releases the
object. The smoke test never calls `SetDisplayMode`.

## Safety

- The compatibility build forwards to the system DirectDraw implementation.
- It never calls `ChangeDisplaySettings` itself.
- The smoke test never requests a display-mode or cooperative-level change.
- It does not inject into unrelated processes; Windows loads it only when it
  is placed beside a program that imports `ddraw.dll`.
- Do not place it in a Windows system directory.
