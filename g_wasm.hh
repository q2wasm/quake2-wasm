#pragma once

extern "C"
{
#include "g_main.h"
#include "g_wasm.h"
}

template<size_t argN>
inline uint32_t wasm_call(wasm_function_inst_t func, uint32_t (&args)[argN], size_t num_args = argN)
{
	if (!wasm_runtime_call_wasm(wasm.exec_env, func, num_args, args))
		gi.error("Couldn't call WASM function: %s\n", wasm_runtime_get_exception(wasm.module_inst));

	return args[0];
}

inline void wasm_call(wasm_function_inst_t func)
{
	if (!wasm_runtime_call_wasm(wasm.exec_env, func, 0, nullptr))
		gi.error("Couldn't call WASM function: %s\n", wasm_runtime_get_exception(wasm.module_inst));
}