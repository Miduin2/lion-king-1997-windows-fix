#pragma once

#include <windows.h>

namespace gamevaultdraw
{
void Trace(const char* format, ...);
void TraceResult(const char* operation, HRESULT result);
}

