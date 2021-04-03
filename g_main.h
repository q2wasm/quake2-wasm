#pragma once

#include "game/g_api.h"

extern game_import_t gi;
extern game_export_t globals;

static inline void wasm_error(const char *text)
{
	__debugbreak();
	gi.error("%s", text);
}