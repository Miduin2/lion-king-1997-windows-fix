# Reproducibility

## Executable patch

1. Obtain `LIONW.EXE` from your own supported European 1997 Windows release.
2. Verify SHA-256:
   `3E99DC48A4B347833E3857A13E0635AB9C5A262BBCD2DBB5304A7BBE0E45DEF3`.
3. Run `installer\Install patch.ps1` with the official 1.0.0 components beside
   the executable.
4. Verify the patched SHA-256:
   `AD28370116F86CEAAB87D77B66533018716CACB6E3E010ED63E3A053BA327EC8`.

The complete, machine-readable offsets and byte sequences are in
`recipes\lion-king-1997-europe.json`. The PowerShell installer verifies all
original bytes before applying any edit.

## Building GameVaultDraw

Requirements:

- Windows 10 or later;
- Visual Studio 2022 Build Tools with x86 C++ tools.

From `src\gamevaultdraw`:

```bat
build.cmd
smoke-test.cmd
```

Expected smoke-test result:

```text
GameVaultDraw smoke test OK: 1920x1080, 32 bpp
```

Official 1.0.0 SHA-256:

- `ddraw.dll`: `519DBB0E20963BAC0A3C7BEBA874421D3B1FAF4CD70BB3AEADDEECA30AEC2C4F`
- `GameVaultDraw.ini`: `85CB3121713AD1AC81C390DDCACE335AF02B60939C30A60E3C653211F1307F62`

## Building the launcher

From `src\gamevaultlauncher`:

```bat
build.cmd
integration-test.cmd
```

The integration test builds a harmless stub game, verifies that it is launched
through a temporary root-level `SUBST` path, and verifies cleanup afterward.

Official 1.0.0 SHA-256:

- `Play Lion King.exe`:
  `124A5168F95F00472B67D40FCE370E0D34E8CD20F8A655BC19F974C3DD18348F`

Microsoft linker timestamps can make a locally rebuilt PE hash differ while
the source and behaviour remain equivalent. Release hashes identify the exact
published binaries.
