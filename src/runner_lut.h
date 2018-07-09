// runner_lut.h - Johan Smet - BSD-3-Clause (see LICENSE)
//
// lookup tables used by Runners

#ifndef JS_SHADER_SIM_RUNNER_LUT_H
#define JS_SHADER_SIM_RUNNER_LUT_H

#include "types.h"
#include "runner.h"

void runner_lut_init(void);
bool runner_lut_lookup_builtin(const char *key, int32_t *value);
bool runner_lut_lookup_datatype(const char *key, TypeKind *value);
bool runner_lut_lookup_cmp_op(const char *key, RunnerCmpOp *value);

#endif // JS_SHADER_SIM_RUNNER_LUT_H