# Changelog

## 1.0.1 — 2026-08-17

- Fixed installation and restoration from folders whose paths contain spaces.
- Fixed direct PowerShell use without an explicit `-GameDirectory` argument.
- Removed the installer scripts' dependency on the `Get-FileHash` cmdlet.
- Clarified how to copy only the supported `LIONKING` folder from the 1997
  compilation disc before applying the patch.
- Clarified that `F2` opens the difficulty and control-remapping options.
- Preserved all runtime binaries and game compatibility behaviour from 1.0.0.

## 1.0.0 — 2026-07-26

- First public release for the supported 1997 European Windows executable.
- Added verified executable repairs for colour detection, EPFS startup,
  obsolete BIOS timing and privileged CPU instructions.
- Added GameVaultDraw borderless 4:3 presentation, palette conversion, 60 FPS
  pacing, stable Alt+Tab/taskbar integration and DWM previews.
- Added safe native Keyboard and Joystick property-page compatibility.
- Added `F2` Properties access and an English `Esc` exit confirmation.
- Added contextual cursor hiding during gameplay.
- Added native console-free short-path launcher.
- Added hash-gated installer, backup and restoration scripts.
