#pragma once

#define _WIN32_WINNT 0x0500
#define _WIN32_IE    0x0700

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shlwapi.h>
#include <tchar.h>
#include <cstdlib>
#include "MinimalMemory.hpp"

#undef memset
#define memset(p, v, n) Minimal::memset(p, v, n)
