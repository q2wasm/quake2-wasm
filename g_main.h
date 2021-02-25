#pragma once

#include "game/api.h"

extern game_import_t gi;
extern game_export_t globals;
extern char mod_directory[64];

static inline void wasm_error(const char *text)
{
	__debugbreak();
	gi.error("%s", text);
}