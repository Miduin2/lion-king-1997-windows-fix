# Validation

## Automated

- GameVaultDraw x86 Release build: passed.
- DirectDraw smoke test at 1920×1080, 32 bpp: passed.
- Launcher x86 Release build: passed.
- Short-path launcher integration and cleanup test: passed.
- Installer original-hash rejection/byte verification/final hash: passed.
- Restoration from verified backup: passed.

## Manual game validation

Tested on Windows 10 22H2 with the supported European executable:

- opening sequence and title screen;
- borderless 4:3 presentation and normal gameplay speed;
- music and sound;
- multiple lives, death and game-over flow;
- arrows and action controls;
- Properties access with `F2`;
- System, Sound, Difficulty, Joystick and Keyboard pages;
- remapping an action during gameplay;
- pause and English exit confirmation with `Esc`;
- taskbar, repeated Alt+Tab and DWM thumbnail/live preview;
- cursor hidden during play and restored for modal dialogs.

The game resets custom input bindings on every launch. This is documented as
original-edition behaviour rather than presented as persistent configuration.

## Accepted minor quirk

After clicking the taskbar icon, Windows can rarely leave the taskbar visible
until focus is restored with another Alt+Tab cycle. Gameplay and display state
remain stable.
