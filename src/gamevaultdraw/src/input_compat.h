#pragma once

namespace gamevaultdraw
{
// Installs narrowly-scoped Win32 compatibility hooks for known original
// Win9x-era game input property pages. Safe to call repeatedly.
void InstallInputCompatibility() noexcept;
}
