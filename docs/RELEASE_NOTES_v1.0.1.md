# The Lion King Windows Fix 1.0.1

This maintenance release fixes the clean-user installation and restoration
workflow. The command wrappers now handle game folders whose paths contain
spaces, and the PowerShell scripts correctly find their own folder when run
directly without a `-GameDirectory` argument.

SHA-256 verification now uses the cryptographic classes included with
Windows/.NET instead of depending on the optional `Get-FileHash` command.

The README also explains how to prepare the supported edition from the 1997
European *Disney Classic Video Games* compilation: copy the complete
`LIONKING` folder from the mounted disc to a writable folder, then copy the
release's `patch` files beside `LIONW.EXE`. The other games, the disc's
`DIRECTX` folder and the root files are not required.

For first-time players, the README now also explains that `F2` opens the
original panel for choosing the difficulty and remapping keyboard or joystick
controls.

The compiled `ddraw.dll`, launcher, executable patch recipe and runtime
behaviour are unchanged from version 1.0.0.

No original game content is included.
