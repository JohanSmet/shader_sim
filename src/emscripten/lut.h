// lut.h - Johan Smet - BSD-3-Clause (see LICENSE)
//
// lookup tables for enums to string

#ifndef JS_SHADER_SIM_EMSCRIPTEN_LUT_H
#define JS_SHADER_SIM_EMSCRIPTEN_LUT_H

#include "spirv_module.h"

void lut_init(void);
const char *lut_lookup_builtin(int32_t builtin);
const char *lut_lookup_type_kind(TypeKind kind);
const char *lut_lookup_variable_kind(VariableKind kind);
const char *lut_lookup_variable_interface(VariableInterface if_type);

#endif // JS_SHADER_SIM_EMSCRIPTEN_LUT_H
