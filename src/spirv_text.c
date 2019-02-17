// spirv_text.c - Johan Smet - BSD-3-Clause (see LICENSE)
//
// SPIR-V binary to text string conversion

#include "spirv_text.h"
#include "spirv_binary.h"
#include "spirv_module.h"
#include "dyn_array.h"

#include <stdbool.h>

#define JS_SPIRV_NAMES_IMPLEMENTATION
#include "spirv/spirv_names.h"

static inline char *spirv_string_opcode_no_result(SpvOp op) {
    char *result = NULL;
    arr_printf(result, "%16s%s", "", spirv_op_name(op));
    return result;
}

static inline char *spirv_string_opcode_result_id(SpvOp op, uint32_t result_id) {
    char *result = NULL;
    char s_id[16] = "";

    snprintf(s_id, 16, "%%%d", result_id);
    arr_printf(result, "%13s = %s", s_id, spirv_op_name(op));
    return result;
}

static inline void spirv_string_bitmask(char **result, uint32_t bitmask, const char *names[]) {

    if (bitmask == 0) {
        arr_printf(*result, " %s", names[0]);
        return;
    }

    const char *fmt = " %s";

    for (int b = 0; b < 32; ++b) {
        uint32_t mask = 1 << b; 
        if ((bitmask & mask) == mask) {
            arr_printf(*result, fmt, names[mask]);
            fmt = "|%s";
        }
    }
}

static inline char *spirv_text_SpvOpCapability(SPIRV_module *module, SPIRV_opcode *opcode) {
    char *result = spirv_string_opcode_no_result(opcode->op.kind);
    arr_printf(result, " %s", spirv_capability_name(opcode->optional[0]));
    return result;
}

static inline char *spirv_text_SpvOpSource(SPIRV_module *module, SPIRV_opcode *opcode) {
    char *result = spirv_string_opcode_no_result(opcode->op.kind);
    arr_printf(result, " %s v%d", 
            spirv_source_language_name(opcode->optional[0]),
            opcode->optional[1]);
    // TODO: optional file & source
    return result;
}

static inline char *spirv_text_SpvOpName(SPIRV_module *module, SPIRV_opcode *opcode) {
    char *result = spirv_string_opcode_no_result(opcode->op.kind);
    arr_printf(result, " %%%d \"%s\"", 
            opcode->optional[0],
            (const char *) &opcode->optional[1]);
    return result;
}

static inline char *spirv_text_SpvOpMemberName(SPIRV_module *module, SPIRV_opcode *opcode) {
    char *result = spirv_string_opcode_no_result(opcode->op.kind);
    arr_printf(result, " %%%d %d \"%s\"", 
            opcode->optional[0],
            opcode->optional[1],
            (const char *) &opcode->optional[2]);
    return result;
}

static inline char *spirv_text_SpvOpExtInst(SPIRV_module *module, SPIRV_opcode *opcode) {
    char *result = spirv_string_opcode_result_id(opcode->op.kind, opcode->optional[1]);
    arr_printf(result, " %%%d %%%d %d",
                    opcode->optional[0],
                    opcode->optional[2],
                    opcode->optional[3]);
    for (int idx = 4; idx < opcode->op.length - 1; ++idx) {
        arr_printf(result, " %%%%d", opcode->optional[idx]);
    }

    return result;
}

static inline char *spirv_text_SpvOpMemoryModel(SPIRV_module *module, SPIRV_opcode *opcode) {
    char *result = spirv_string_opcode_no_result(opcode->op.kind);
    arr_printf(result, " %s %s",
                    spirv_addressing_model_name(opcode->optional[0]),
                    spirv_memory_model_name(opcode->optional[1]));
    return result;
}

static inline char *spirv_text_SpvOpEntryPoint(SPIRV_module *module, SPIRV_opcode *opcode) {
    char *result = spirv_string_opcode_no_result(opcode->op.kind);
    arr_printf(result, " %s %%%d \"%s\"",
                    spirv_execution_model_name(opcode->optional[0]),
                    opcode->optional[1],
                    (const char *) &opcode->optional[2]);

    size_t len = (strlen((const char *) &opcode->optional[2]) + 3) / 4;

    for (int idx = 2+len; idx < opcode->op.length - 1; ++idx) {
        arr_printf(result, " %%%d", opcode->optional[idx]);
    }

    return result;
}

static inline char *spirv_text_SpvOpTypeImage(SPIRV_module *module, SPIRV_opcode *opcode) {
    char *result = spirv_string_opcode_result_id(opcode->op.kind, opcode->optional[0]);
    arr_printf(result, " %%%d %s %d %d %d %d %s",
                    opcode->optional[1], 
                    spirv_dim_name(opcode->optional[2]),
                    opcode->optional[3], 
                    opcode->optional[4],
                    opcode->optional[5], 
                    opcode->optional[6], 
                    spirv_image_format_name(opcode->optional[7])); 
    
    if (opcode->op.length == 10) {
        arr_printf(result, " %s", spirv_access_qualifier_name(opcode->optional[8]));
    }

    return result;
}

static inline char *spirv_text_SpvOpTypePointer(SPIRV_module *module, SPIRV_opcode *opcode) {
    char *result = spirv_string_opcode_result_id(opcode->op.kind, opcode->optional[0]);
    arr_printf(result, " %s %%%d",
                    spirv_storage_class_name(opcode->optional[1]),
                    opcode->optional[2]);
    return result;
}

static inline char *spirv_text_SpvOpTypePipe(SPIRV_module *module, SPIRV_opcode *opcode) {
    char *result = spirv_string_opcode_result_id(opcode->op.kind, opcode->optional[0]);
    arr_printf(result, " %s", spirv_access_qualifier_name(opcode->optional[1]));
    return result;
}

static inline char *spirv_text_SpvOpTypeForwardPointer(SPIRV_module *module, SPIRV_opcode *opcode) {
    char *result = spirv_string_opcode_no_result(opcode->op.kind);
    arr_printf(result, " %%%d %s",
                    opcode->optional[0],
                    spirv_storage_class_name(opcode->optional[1]));
    return result;
}

static inline char *spirv_text_SpvOpExecutionMode(SPIRV_module *module, SPIRV_opcode *opcode) {
    char *result = spirv_string_opcode_no_result(opcode->op.kind);
    arr_printf(result, " %%%d %s",
                    opcode->optional[0],
                    spirv_execution_mode_name(opcode->optional[1]));
    return result;
}

static inline char *spirv_text_SpvOpExecutionModeId(SPIRV_module *module, SPIRV_opcode *opcode) {
    char *result = spirv_string_opcode_no_result(opcode->op.kind);
    arr_printf(result, " %%%d %s",
                    opcode->optional[0],
                    spirv_execution_mode_name(opcode->optional[1]));
    for (int idx=2; idx < opcode->op.length - 1; ++idx) {
        arr_printf(result, " %%%d", opcode->optional[idx]);
    }
    return result;
}

static inline char *spirv_text_SpvOpConstantSampler(SPIRV_module *module, SPIRV_opcode *opcode) {
    char *result = spirv_string_opcode_result_id(opcode->op.kind, opcode->optional[1]);
    arr_printf(result, " %%%d %s %d %s",
                    opcode->optional[0],
                    spirv_sampler_addressing_mode_name(opcode->optional[2]),
                    opcode->optional[3],
                    spirv_sampler_filter_mode_name(opcode->optional[4]));
    return result;
}

static inline char *spirv_text_SpvOpSpecConstantOp(SPIRV_module *module, SPIRV_opcode *opcode) {
    char *result = spirv_string_opcode_result_id(opcode->op.kind, opcode->optional[1]);
    arr_printf(result, " %%%d %s",
                    opcode->optional[0],
                    spirv_op_name(opcode->optional[2]));
    for (int idx=3; idx < opcode->op.length - 1; ++idx) {
        arr_printf(result, " %%%d", opcode->optional[idx]);
    }
    return result;
}

static inline char *spirv_text_SpvOpFunction(SPIRV_module *module, SPIRV_opcode *opcode) {
    char *result = spirv_string_opcode_result_id(opcode->op.kind, opcode->optional[1]);
    arr_printf(result, " %%%d", opcode->optional[0]);
    spirv_string_bitmask(&result, opcode->optional[2], SPIRV_FUNCTION_CONTROL);
    arr_printf(result, " %%%d", opcode->optional[3]);
    return result;
}

static inline char *spirv_text_SpvOpVariable(SPIRV_module *module, SPIRV_opcode *opcode) {
    char *result = spirv_string_opcode_result_id(opcode->op.kind, opcode->optional[1]);
    arr_printf(result, " %%%d %s",
                    opcode->optional[0],
                    spirv_storage_class_name(opcode->optional[2]));
    for (int idx=3; idx < opcode->op.length - 1; ++idx) {
        arr_printf(result, " %%%d", opcode->optional[idx]);
    }
    return result;
}

static inline char *spirv_text_SpvOpLoad(SPIRV_module *module, SPIRV_opcode *opcode) {
    char *result = spirv_string_opcode_result_id(opcode->op.kind, opcode->optional[1]);
    arr_printf(result, " %%%d %%%d",
                    opcode->optional[0],
                    opcode->optional[2]);
    if (opcode->op.length == 5) {
        spirv_string_bitmask(&result, opcode->optional[3], SPIRV_MEMORY_ACCESS);
    }
    return result;
}

static inline char *spirv_text_SpvOpStore(SPIRV_module *module, SPIRV_opcode *opcode) {
    char *result = spirv_string_opcode_no_result(opcode->op.kind);
    arr_printf(result, " %%%d %%%d",
                    opcode->optional[0],
                    opcode->optional[1]);
    if (opcode->op.length == 4) {
        spirv_string_bitmask(&result, opcode->optional[2], SPIRV_MEMORY_ACCESS);
    }
    return result;
}

static inline char *spirv_text_SpvOpCopyMemory(SPIRV_module *module, SPIRV_opcode *opcode) {
    return spirv_text_SpvOpStore(module, opcode);
}

static inline char *spirv_text_SpvOpCopyMemorySized(SPIRV_module *module, SPIRV_opcode *opcode) {
    char *result = spirv_string_opcode_no_result(opcode->op.kind);
    arr_printf(result, " %%%d %%%d %%%d",
                    opcode->optional[0],
                    opcode->optional[1],
                    opcode->optional[2]);
    if (opcode->op.length == 5) {
        spirv_string_bitmask(&result, opcode->optional[3], SPIRV_MEMORY_ACCESS);
    }
    return result;
}

static inline char *spirv_text_SpvOpDecorate(SPIRV_module *module, SPIRV_opcode *opcode) {
    char *result = spirv_string_opcode_no_result(opcode->op.kind);
    arr_printf(result, " %%%d %s",
                    opcode->optional[0],
                    spirv_decoration_name(opcode->optional[1]));
    for (int idx=2; idx < opcode->op.length - 1; ++idx) {
        if (opcode->optional[1] != SpvDecorationBuiltIn) {
            arr_printf(result, " %d", opcode->optional[idx]);
        } else {
            arr_printf(result, " %s", spirv_builtin_name(opcode->optional[idx]));
        }
    }
    return result;
}

static inline char *spirv_text_SpvOpDecorateId(SPIRV_module *module, SPIRV_opcode *opcode) {
    char *result = spirv_string_opcode_no_result(opcode->op.kind);
    arr_printf(result, " %%%d %s",
                    opcode->optional[0],
                    spirv_decoration_name(opcode->optional[1]));
    for (int idx=2; idx < opcode->op.length - 1; ++idx) {
        arr_printf(result, " %%%d", opcode->optional[idx]);
    }
    return result;
}

static inline char *spirv_text_SpvOpMemberDecorate(SPIRV_module *module, SPIRV_opcode *opcode) {
    char *result = spirv_string_opcode_no_result(opcode->op.kind);
    arr_printf(result, " %%%d %d %s",
                    opcode->optional[0],
                    opcode->optional[1],
                    spirv_decoration_name(opcode->optional[2]));
    for (int idx=3; idx < opcode->op.length - 1; ++idx) {
        if (opcode->optional[2] != SpvDecorationBuiltIn) {
            arr_printf(result, " %d", opcode->optional[idx]);
        } else {
            arr_printf(result, " %s", spirv_builtin_name(opcode->optional[idx]));
        }
    }
    return result;
}   

static inline char *spirv_text_SpvOpGroupMemberDecorate(SPIRV_module *module, SPIRV_opcode *opcode) {
    char *result = spirv_string_opcode_no_result(opcode->op.kind);
    arr_printf(result, " %%%d", opcode->optional[0]);
    for (int idx = 1; idx < opcode->op.length - 1; idx += 2) {
        arr_printf(result, " %%%d %d", opcode->optional[idx], opcode->optional[idx+1]);
    }
    return result;
}

static inline char *spirv_text_SpvOpLoopMerge(SPIRV_module *module, SPIRV_opcode *opcode) {
    char *result = spirv_string_opcode_no_result(opcode->op.kind);
    arr_printf(result, " %%%d %%%d", opcode->optional[0], opcode->optional[1]);
    spirv_string_bitmask(&result, opcode->optional[2], SPIRV_LOOP_CONTROL);
    return result;
}

static inline char *spirv_text_SpvOpSelectionMerge(SPIRV_module *module, SPIRV_opcode *opcode) {
    char *result = spirv_string_opcode_no_result(opcode->op.kind);
    arr_printf(result, " %%%d", opcode->optional[0]);
    spirv_string_bitmask(&result, opcode->optional[1], SPIRV_SELECTION_CONTROL);
    return result;
}

static inline char *spirv_text_SpvOpSwitch(SPIRV_module *module, SPIRV_opcode *opcode) {
    char *result = spirv_string_opcode_no_result(opcode->op.kind);
    arr_printf(result, " %%%d %%%d", opcode->optional[0], opcode->optional[1]);
    for (int idx = 2; idx < opcode->op.length - 1; idx += 2) {
        arr_printf(result, " %%%d %d", opcode->optional[idx], opcode->optional[idx+1]);
    }
    return result;
}

static inline char *spirv_text_string(SPIRV_module *module, SPIRV_opcode *opcode) {
    char *result = spirv_string_opcode_no_result(opcode->op.kind);
    arr_printf(result, " \"%s\"", (const char *) &opcode->optional[0]);
    return result;
}

static inline char *spirv_text_result_string(SPIRV_module *module, SPIRV_opcode *opcode) {
    char *result = spirv_string_opcode_result_id(opcode->op.kind, opcode->optional[0]);
    arr_printf(result, " \"%s\"", (const char *) &opcode->optional[1]);
    return result;
}

static inline char *spirv_text_result_number_list(SPIRV_module *module, SPIRV_opcode *opcode) {
    char *result = spirv_string_opcode_result_id(opcode->op.kind, opcode->optional[0]);
    for (int idx=1; idx < opcode->op.length - 1; ++idx) {
        arr_printf(result, " %d", opcode->optional[idx]);
    }
    return result;
}

static inline char *spirv_text_result_id_number_list(SPIRV_module *module, SPIRV_opcode *opcode) {
    char *result = spirv_string_opcode_result_id(opcode->op.kind, opcode->optional[0]);
    arr_printf(result, " %%%d", opcode->optional[1]);
    for (int idx=2; idx < opcode->op.length - 1; ++idx) {
        arr_printf(result, " %d", opcode->optional[idx]);
    }
    return result;
}

static inline char *spirv_text_result_id_list(SPIRV_module *module, SPIRV_opcode *opcode) {
    char *result = spirv_string_opcode_result_id(opcode->op.kind, opcode->optional[0]);
    for (int idx=1; idx < opcode->op.length - 1; ++idx) {
        arr_printf(result, " %%%d", opcode->optional[idx]);
    }
    return result;
}

static inline char *spirv_text_id_list(SPIRV_module *module, SPIRV_opcode *opcode) {
    char *result = spirv_string_opcode_no_result(opcode->op.kind);
    for (int idx=0; idx < opcode->op.length - 1; ++idx) {
        arr_printf(result, " %%%d", opcode->optional[idx]);
    }
    return result;
}

static inline char *spirv_text_type_result_number_list(SPIRV_module *module, SPIRV_opcode *opcode) {
    char *result = spirv_string_opcode_result_id(opcode->op.kind, opcode->optional[1]);
    arr_printf(result, " %%%d", opcode->optional[0]);
    for (int idx=2; idx < opcode->op.length - 1; ++idx) {
        arr_printf(result, " %d", opcode->optional[idx]);
    }
    return result;
}

static inline char *spirv_text_type_result_id_list(SPIRV_module *module, SPIRV_opcode *opcode) {
    char *result = spirv_string_opcode_result_id(opcode->op.kind, opcode->optional[1]);
    arr_printf(result, " %%%d", opcode->optional[0]);
    for (int idx=2; idx < opcode->op.length - 1; ++idx) {
        arr_printf(result, " %%%d", opcode->optional[idx]);
    }
    return result;
}

static inline char *spirv_text_type_result_id_number(SPIRV_module *module, SPIRV_opcode *opcode) {
    char *result = spirv_string_opcode_result_id(opcode->op.kind, opcode->optional[1]);
    arr_printf(result, " %%%d %%%d %d", 
                    opcode->optional[0],
                    opcode->optional[2],
                    opcode->optional[3]);
    return result;
}

static inline char *spirv_text_type_result_n_id_number_list(SPIRV_module *module, SPIRV_opcode *opcode, int n) {
    char *result = spirv_string_opcode_result_id(opcode->op.kind, opcode->optional[1]);
    arr_printf(result, " %%%d", opcode->optional[0]);
    for (int idx = 0; idx < n; ++idx) {
        arr_printf(result, " %%%d", opcode->optional[2+idx]);
    }
    for (int idx=2+n; idx < opcode->op.length - 1; ++idx) {
        arr_printf(result, " %d", opcode->optional[idx]);
    }
    return result;
}

static inline char *spirv_text_n_id_number_list(SPIRV_module *module, SPIRV_opcode *opcode, int n) {
    char *result = spirv_string_opcode_no_result(opcode->op.kind);
    for (int idx = 0; idx < n; ++idx) {
        arr_printf(result, " %%%d", opcode->optional[idx]);
    }
    for (int idx=n; idx < opcode->op.length - 1; ++idx) {
        arr_printf(result, " %d", opcode->optional[idx]);
    }
    return result;
}

static inline char *spirv_text_type_result_n_id_imageop_id_list(SPIRV_module *module, SPIRV_opcode *opcode, int n) {
    char *result = spirv_string_opcode_result_id(opcode->op.kind, opcode->optional[1]);
    arr_printf(result, " %%%d", opcode->optional[0]);
    for (int idx = 0; idx < n; ++idx) {
        arr_printf(result, " %%%d", opcode->optional[2+idx]);
    }
    if (opcode->op.length > n + 2) {
        spirv_string_bitmask(&result, opcode->optional[n + 2], SPIRV_IMAGE_OPERANDS);
    }
    for (int idx=3+n; idx < opcode->op.length - 1; ++idx) {
        arr_printf(result, " %%%d", opcode->optional[idx]);
    }
    return result;
}

static inline char *spirv_text_n_id_imageop_id_list(SPIRV_module *module, SPIRV_opcode *opcode, int n) {
    char *result = spirv_string_opcode_no_result(opcode->op.kind);
    for (int idx = 0; idx < n; ++idx) {
        arr_printf(result, " %%%d", opcode->optional[idx]);
    }
    if (opcode->op.length > n) {
        spirv_string_bitmask(&result, opcode->optional[n], SPIRV_IMAGE_OPERANDS);
    }
    for (int idx=1+n; idx < opcode->op.length - 1; ++idx) {
        arr_printf(result, " %%%d", opcode->optional[idx]);
    }
    return result;
}

static inline char *spirv_text_id_groupop_id(SPIRV_module *module, SPIRV_opcode *opcode) {
    char *result = spirv_string_opcode_no_result(opcode->op.kind);
    arr_printf(result, " %%%d %s %%%d",
                    opcode->optional[0],
                    spirv_group_operation_name(opcode->optional[1]),
                    opcode->optional[2]);
    return result;
} 

///////////////////////////////////////////////////////////////////////////////
//
// public functions
//

int spirv_text_header_num_lines(SPIRV_header *header) {
    return 3;
}

char *spirv_text_header_line(SPIRV_header *header, int line) {
    char *result = NULL;

    switch (line) {
        case 0: 
            arr_printf(result, "// Version: %d.%d", header->version_high, header->version_low);
            break;
        case 1: 
            arr_printf(result, "// Generator: %d v%d", header->generator >> 16, header->generator & 0x0000ffff);
            break;
        case 2: 
            arr_printf(result, "// Bound ids: %d", header->bound_ids);
            break;
        default:
            assert(false);
    }

    return result;
}

char *spirv_text_opcode(SPIRV_opcode *opcode, SPIRV_module *module) {
#define OP_CASE_SPECIAL(op)                             \
    case op:                                            \
        line = spirv_text_##op(module, opcode);         \
        break;
#define OP_CASE_TYPE(op, type)                          \
    case op:                                            \
        line = spirv_text_##type(module, opcode);       \
        break;                      
#define OP_CASE_TYPE_1(op, type, n)                     \
    case op:                                            \
        line = spirv_text_##type(module, opcode, n);    \
        break;                      
#define OP_DEFAULT(op)

    char *line = NULL;

    switch (opcode->op.kind) {
        // miscellaneous instructions
        OP_DEFAULT(SpvOpNop)
        OP_CASE_TYPE(SpvOpUndef, type_result_id_list)
        OP_CASE_TYPE(SpvOpSizeOf, type_result_id_list)

        // debug instructions
        OP_CASE_TYPE(SpvOpSourceContinued, string)
        OP_CASE_SPECIAL(SpvOpSource)
        OP_CASE_TYPE(SpvOpSourceExtension, string)
        OP_CASE_SPECIAL(SpvOpName)
        OP_CASE_SPECIAL(SpvOpMemberName)
        OP_CASE_TYPE(SpvOpString, result_string)
        OP_CASE_TYPE_1(SpvOpLine, n_id_number_list, 1)
        OP_DEFAULT(SpvOpNoLine)
        OP_CASE_TYPE(SpvOpModuleProcessed, string)

        // extension instructions
        OP_CASE_TYPE(SpvOpExtension, string)
        OP_CASE_TYPE(SpvOpExtInstImport, result_string)
        OP_CASE_SPECIAL(SpvOpExtInst)

        // mode-setting instructions
        OP_CASE_SPECIAL(SpvOpMemoryModel)
        OP_CASE_SPECIAL(SpvOpEntryPoint)
        OP_CASE_SPECIAL(SpvOpExecutionMode)
        OP_CASE_SPECIAL(SpvOpCapability)
        OP_CASE_SPECIAL(SpvOpExecutionModeId)

        // type-declaration instructions
        OP_CASE_TYPE(SpvOpTypeVoid, result_number_list)
        OP_CASE_TYPE(SpvOpTypeBool, result_number_list)
        OP_CASE_TYPE(SpvOpTypeInt, result_number_list)
        OP_CASE_TYPE(SpvOpTypeFloat, result_number_list)
        OP_CASE_TYPE(SpvOpTypeVector, result_id_number_list)
        OP_CASE_TYPE(SpvOpTypeMatrix, result_id_number_list)
        OP_CASE_SPECIAL(SpvOpTypeImage)
        OP_CASE_TYPE(SpvOpTypeSampler, result_number_list)
        OP_CASE_TYPE(SpvOpTypeSampledImage, result_id_list)
        OP_CASE_TYPE(SpvOpTypeArray, result_id_list)
        OP_CASE_TYPE(SpvOpTypeRuntimeArray, result_id_list)
        OP_CASE_TYPE(SpvOpTypeStruct, result_id_list)
        OP_CASE_TYPE(SpvOpTypeOpaque, result_string)
        OP_CASE_SPECIAL(SpvOpTypePointer)
        OP_CASE_TYPE(SpvOpTypeFunction, result_id_list)
        OP_CASE_TYPE(SpvOpTypeEvent, result_number_list)
        OP_CASE_TYPE(SpvOpTypeDeviceEvent, result_number_list)
        OP_CASE_TYPE(SpvOpTypeReserveId, result_number_list)
        OP_CASE_TYPE(SpvOpTypeQueue, result_number_list)
        OP_CASE_SPECIAL(SpvOpTypePipe)
        OP_CASE_SPECIAL(SpvOpTypeForwardPointer)
        OP_CASE_TYPE(SpvOpTypePipeStorage, result_number_list)
        OP_CASE_TYPE(SpvOpTypeNamedBarrier, result_number_list)

        // constant-creation instructions
        OP_CASE_TYPE(SpvOpConstantTrue, type_result_number_list)
        OP_CASE_TYPE(SpvOpConstantFalse, type_result_number_list)
        OP_CASE_TYPE(SpvOpConstant, type_result_number_list)
        OP_CASE_TYPE(SpvOpConstantComposite, type_result_id_list)
        OP_CASE_SPECIAL(SpvOpConstantSampler)
        OP_CASE_TYPE(SpvOpConstantNull, type_result_number_list)
        OP_CASE_TYPE(SpvOpSpecConstantTrue, type_result_number_list)
        OP_CASE_TYPE(SpvOpSpecConstantFalse, type_result_number_list)
        OP_CASE_TYPE(SpvOpSpecConstant, type_result_number_list)
        OP_CASE_TYPE(SpvOpSpecConstantComposite, type_result_id_list)
        OP_CASE_SPECIAL(SpvOpSpecConstantOp)

        // function instructions
        OP_CASE_SPECIAL(SpvOpFunction)
        OP_CASE_TYPE(SpvOpFunctionParameter, type_result_number_list)
        OP_DEFAULT(SpvOpFunctionEnd)
        OP_CASE_TYPE(SpvOpFunctionCall, type_result_id_list)

        // memory instructions
        OP_CASE_SPECIAL(SpvOpVariable)
        OP_CASE_TYPE(SpvOpImageTexelPointer, type_result_id_list)
        OP_CASE_SPECIAL(SpvOpLoad)
        OP_CASE_SPECIAL(SpvOpStore)
        OP_CASE_SPECIAL(SpvOpCopyMemory)
        OP_CASE_SPECIAL(SpvOpCopyMemorySized)
        OP_CASE_TYPE(SpvOpAccessChain, type_result_id_list)
        OP_CASE_TYPE(SpvOpInBoundsAccessChain, type_result_id_list)
        OP_CASE_TYPE(SpvOpPtrAccessChain, type_result_id_list)
        OP_CASE_TYPE(SpvOpArrayLength, type_result_id_number)
        OP_CASE_TYPE(SpvOpGenericPtrMemSemantics, type_result_id_list)
        OP_CASE_TYPE(SpvOpInBoundsPtrAccessChain, type_result_id_list)

        // annotation instructions
        OP_CASE_SPECIAL(SpvOpDecorate)
        OP_CASE_SPECIAL(SpvOpMemberDecorate)
        OP_CASE_TYPE(SpvOpDecorationGroup, result_number_list)
        OP_CASE_TYPE(SpvOpGroupDecorate, id_list)
        OP_CASE_SPECIAL(SpvOpGroupMemberDecorate)
        OP_CASE_SPECIAL(SpvOpDecorateId)

        // composite instructions
        OP_CASE_TYPE(SpvOpVectorExtractDynamic, type_result_id_list)
        OP_CASE_TYPE(SpvOpVectorInsertDynamic, type_result_id_list)
        OP_CASE_TYPE_1(SpvOpVectorShuffle, type_result_n_id_number_list, 2)
        OP_CASE_TYPE(SpvOpCompositeConstruct, type_result_id_list)
        OP_CASE_TYPE_1(SpvOpCompositeExtract, type_result_n_id_number_list, 1)
        OP_CASE_TYPE_1(SpvOpCompositeInsert, type_result_n_id_number_list, 2)
        OP_CASE_TYPE(SpvOpCopyObject, type_result_id_list)
        OP_CASE_TYPE(SpvOpTranspose, type_result_id_list)

        // image instructions
        OP_CASE_TYPE(SpvOpSampledImage, type_result_id_list)
        OP_CASE_TYPE_1(SpvOpImageSampleImplicitLod, type_result_n_id_imageop_id_list, 2)
        OP_CASE_TYPE_1(SpvOpImageSampleExplicitLod, type_result_n_id_imageop_id_list, 2)
        OP_CASE_TYPE_1(SpvOpImageSampleDrefImplicitLod, type_result_n_id_imageop_id_list, 3)
        OP_CASE_TYPE_1(SpvOpImageSampleDrefExplicitLod, type_result_n_id_imageop_id_list, 3)
        OP_CASE_TYPE_1(SpvOpImageSampleProjImplicitLod, type_result_n_id_imageop_id_list, 2)
        OP_CASE_TYPE_1(SpvOpImageSampleProjExplicitLod, type_result_n_id_imageop_id_list, 2)
        OP_CASE_TYPE_1(SpvOpImageSampleProjDrefImplicitLod, type_result_n_id_imageop_id_list, 3)
        OP_CASE_TYPE_1(SpvOpImageSampleProjDrefExplicitLod, type_result_n_id_imageop_id_list, 3)
        OP_CASE_TYPE_1(SpvOpImageFetch, type_result_n_id_imageop_id_list, 2)
        OP_CASE_TYPE_1(SpvOpImageGather, type_result_n_id_imageop_id_list, 3)
        OP_CASE_TYPE_1(SpvOpImageDrefGather, type_result_n_id_imageop_id_list, 3)
        OP_CASE_TYPE_1(SpvOpImageRead, type_result_n_id_imageop_id_list, 2)
        OP_CASE_TYPE_1(SpvOpImageWrite, n_id_imageop_id_list, 3)
        OP_CASE_TYPE(SpvOpImage, type_result_id_list)
        OP_CASE_TYPE(SpvOpImageQueryFormat, type_result_id_list)
        OP_CASE_TYPE(SpvOpImageQueryOrder, type_result_id_list)
        OP_CASE_TYPE(SpvOpImageQuerySizeLod, type_result_id_list)
        OP_CASE_TYPE(SpvOpImageQuerySize, type_result_id_list)
        OP_CASE_TYPE(SpvOpImageQueryLod, type_result_id_list)
        OP_CASE_TYPE(SpvOpImageQueryLevels, type_result_id_list)
        OP_CASE_TYPE(SpvOpImageQuerySamples, type_result_id_list)
        OP_CASE_TYPE_1(SpvOpImageSparseSampleImplicitLod, type_result_n_id_imageop_id_list, 2)
        OP_CASE_TYPE_1(SpvOpImageSparseSampleExplicitLod, type_result_n_id_imageop_id_list, 2)
        OP_CASE_TYPE_1(SpvOpImageSparseSampleDrefImplicitLod, type_result_n_id_imageop_id_list, 3)
        OP_CASE_TYPE_1(SpvOpImageSparseSampleDrefExplicitLod, type_result_n_id_imageop_id_list, 3)
        OP_CASE_TYPE_1(SpvOpImageSparseSampleProjImplicitLod, type_result_n_id_imageop_id_list, 2)
        OP_CASE_TYPE_1(SpvOpImageSparseSampleProjExplicitLod, type_result_n_id_imageop_id_list, 2)
        OP_CASE_TYPE_1(SpvOpImageSparseSampleProjDrefImplicitLod, type_result_n_id_imageop_id_list, 3)
        OP_CASE_TYPE_1(SpvOpImageSparseSampleProjDrefExplicitLod, type_result_n_id_imageop_id_list, 3)
        OP_CASE_TYPE_1(SpvOpImageSparseFetch, type_result_n_id_imageop_id_list, 2)
        OP_CASE_TYPE_1(SpvOpImageSparseGather, type_result_n_id_imageop_id_list, 3)
        OP_CASE_TYPE_1(SpvOpImageSparseDrefGather, type_result_n_id_imageop_id_list, 3)
        OP_CASE_TYPE(SpvOpImageSparseTexelsResident, type_result_id_list)
        OP_CASE_TYPE_1(SpvOpImageSparseRead, type_result_n_id_imageop_id_list, 2)

        // conversion instructions
        OP_CASE_TYPE(SpvOpConvertFToU, type_result_id_list)
        OP_CASE_TYPE(SpvOpConvertFToS, type_result_id_list)
        OP_CASE_TYPE(SpvOpConvertSToF, type_result_id_list)
        OP_CASE_TYPE(SpvOpConvertUToF, type_result_id_list)
        OP_CASE_TYPE(SpvOpUConvert, type_result_id_list)
        OP_CASE_TYPE(SpvOpSConvert, type_result_id_list)
        OP_CASE_TYPE(SpvOpFConvert, type_result_id_list)
        OP_CASE_TYPE(SpvOpQuantizeToF16, type_result_id_list)
        OP_CASE_TYPE(SpvOpConvertPtrToU, type_result_id_list)
        OP_CASE_TYPE(SpvOpSatConvertSToU, type_result_id_list)
        OP_CASE_TYPE(SpvOpSatConvertUToS, type_result_id_list)
        OP_CASE_TYPE(SpvOpConvertUToPtr, type_result_id_list)
        OP_CASE_TYPE(SpvOpPtrCastToGeneric, type_result_id_list)
        OP_CASE_TYPE(SpvOpGenericCastToPtr, type_result_id_list)
        OP_CASE_TYPE(SpvOpGenericCastToPtrExplicit, type_result_id_list)
        OP_CASE_TYPE(SpvOpBitcast, type_result_id_list)

        // arithmetic instructions
        OP_CASE_TYPE(SpvOpSNegate, type_result_id_list)
        OP_CASE_TYPE(SpvOpFNegate, type_result_id_list)
        OP_CASE_TYPE(SpvOpIAdd, type_result_id_list)
        OP_CASE_TYPE(SpvOpFAdd, type_result_id_list)
        OP_CASE_TYPE(SpvOpISub, type_result_id_list)
        OP_CASE_TYPE(SpvOpFSub, type_result_id_list)
        OP_CASE_TYPE(SpvOpIMul, type_result_id_list)
        OP_CASE_TYPE(SpvOpFMul, type_result_id_list)
        OP_CASE_TYPE(SpvOpUDiv, type_result_id_list)
        OP_CASE_TYPE(SpvOpSDiv, type_result_id_list)
        OP_CASE_TYPE(SpvOpFDiv, type_result_id_list)
        OP_CASE_TYPE(SpvOpUMod, type_result_id_list)
        OP_CASE_TYPE(SpvOpSRem, type_result_id_list)
        OP_CASE_TYPE(SpvOpSMod, type_result_id_list)
        OP_CASE_TYPE(SpvOpFRem, type_result_id_list)
        OP_CASE_TYPE(SpvOpFMod, type_result_id_list)
        OP_CASE_TYPE(SpvOpVectorTimesScalar, type_result_id_list)
        OP_CASE_TYPE(SpvOpMatrixTimesScalar, type_result_id_list)
        OP_CASE_TYPE(SpvOpVectorTimesMatrix, type_result_id_list)
        OP_CASE_TYPE(SpvOpMatrixTimesVector, type_result_id_list)
        OP_CASE_TYPE(SpvOpMatrixTimesMatrix, type_result_id_list)
        OP_CASE_TYPE(SpvOpOuterProduct, type_result_id_list)
        OP_CASE_TYPE(SpvOpDot, type_result_id_list)
        OP_CASE_TYPE(SpvOpIAddCarry, type_result_id_list)
        OP_CASE_TYPE(SpvOpISubBorrow, type_result_id_list)
        OP_CASE_TYPE(SpvOpUMulExtended, type_result_id_list)
        OP_CASE_TYPE(SpvOpSMulExtended, type_result_id_list)

        // relational and logical instructions
        OP_CASE_TYPE(SpvOpAny, type_result_id_list)
        OP_CASE_TYPE(SpvOpAll, type_result_id_list)
        OP_CASE_TYPE(SpvOpIsNan, type_result_id_list)
        OP_CASE_TYPE(SpvOpIsInf, type_result_id_list)
        OP_CASE_TYPE(SpvOpIsFinite, type_result_id_list)
        OP_CASE_TYPE(SpvOpIsNormal, type_result_id_list)
        OP_CASE_TYPE(SpvOpSignBitSet, type_result_id_list)
        OP_CASE_TYPE(SpvOpLessOrGreater, type_result_id_list)
        OP_CASE_TYPE(SpvOpOrdered, type_result_id_list)
        OP_CASE_TYPE(SpvOpUnordered, type_result_id_list)
        OP_CASE_TYPE(SpvOpLogicalEqual, type_result_id_list)
        OP_CASE_TYPE(SpvOpLogicalNotEqual, type_result_id_list)
        OP_CASE_TYPE(SpvOpLogicalOr, type_result_id_list)
        OP_CASE_TYPE(SpvOpLogicalAnd, type_result_id_list)
        OP_CASE_TYPE(SpvOpLogicalNot, type_result_id_list)
        OP_CASE_TYPE(SpvOpSelect, type_result_id_list)
        OP_CASE_TYPE(SpvOpIEqual, type_result_id_list)
        OP_CASE_TYPE(SpvOpINotEqual, type_result_id_list)
        OP_CASE_TYPE(SpvOpUGreaterThan, type_result_id_list)
        OP_CASE_TYPE(SpvOpSGreaterThan, type_result_id_list)
        OP_CASE_TYPE(SpvOpUGreaterThanEqual, type_result_id_list)
        OP_CASE_TYPE(SpvOpSGreaterThanEqual, type_result_id_list)
        OP_CASE_TYPE(SpvOpULessThan, type_result_id_list)
        OP_CASE_TYPE(SpvOpSLessThan, type_result_id_list)
        OP_CASE_TYPE(SpvOpULessThanEqual, type_result_id_list)
        OP_CASE_TYPE(SpvOpSLessThanEqual, type_result_id_list)
        OP_CASE_TYPE(SpvOpFOrdEqual, type_result_id_list)
        OP_CASE_TYPE(SpvOpFUnordEqual, type_result_id_list)
        OP_CASE_TYPE(SpvOpFOrdNotEqual, type_result_id_list)
        OP_CASE_TYPE(SpvOpFUnordNotEqual, type_result_id_list)
        OP_CASE_TYPE(SpvOpFOrdLessThan, type_result_id_list)
        OP_CASE_TYPE(SpvOpFUnordLessThan, type_result_id_list)
        OP_CASE_TYPE(SpvOpFOrdGreaterThan, type_result_id_list)
        OP_CASE_TYPE(SpvOpFUnordGreaterThan, type_result_id_list)
        OP_CASE_TYPE(SpvOpFOrdLessThanEqual, type_result_id_list)
        OP_CASE_TYPE(SpvOpFUnordLessThanEqual, type_result_id_list)
        OP_CASE_TYPE(SpvOpFOrdGreaterThanEqual, type_result_id_list)
        OP_CASE_TYPE(SpvOpFUnordGreaterThanEqual, type_result_id_list)

        // bit instructions
        OP_CASE_TYPE(SpvOpShiftRightLogical, type_result_id_list)
        OP_CASE_TYPE(SpvOpShiftRightArithmetic, type_result_id_list)
        OP_CASE_TYPE(SpvOpShiftLeftLogical, type_result_id_list)
        OP_CASE_TYPE(SpvOpBitwiseOr, type_result_id_list)
        OP_CASE_TYPE(SpvOpBitwiseXor, type_result_id_list)
        OP_CASE_TYPE(SpvOpBitwiseAnd, type_result_id_list)
        OP_CASE_TYPE(SpvOpNot, type_result_id_list)
        OP_CASE_TYPE(SpvOpBitFieldInsert, type_result_id_list)
        OP_CASE_TYPE(SpvOpBitFieldSExtract, type_result_id_list)
        OP_CASE_TYPE(SpvOpBitFieldUExtract, type_result_id_list)
        OP_CASE_TYPE(SpvOpBitReverse, type_result_id_list)
        OP_CASE_TYPE(SpvOpBitCount, type_result_id_list)

        // derivative instructions
        OP_CASE_TYPE(SpvOpDPdx, type_result_id_list)
        OP_CASE_TYPE(SpvOpDPdy, type_result_id_list)
        OP_CASE_TYPE(SpvOpFwidth, type_result_id_list)
        OP_CASE_TYPE(SpvOpDPdxFine, type_result_id_list)
        OP_CASE_TYPE(SpvOpDPdyFine, type_result_id_list)
        OP_CASE_TYPE(SpvOpFwidthFine, type_result_id_list)
        OP_CASE_TYPE(SpvOpDPdxCoarse, type_result_id_list)
        OP_CASE_TYPE(SpvOpDPdyCoarse, type_result_id_list)
        OP_CASE_TYPE(SpvOpFwidthCoarse, type_result_id_list)

        // primitive instructions
        OP_DEFAULT(SpvOpEmitVertex)
        OP_DEFAULT(SpvOpEndPrimitive)
        OP_CASE_TYPE(SpvOpEmitStreamVertex, id_list)
        OP_CASE_TYPE(SpvOpEndStreamPrimitive, id_list)

        // barrier instructions
        OP_CASE_TYPE(SpvOpControlBarrier, id_list)
        OP_CASE_TYPE(SpvOpMemoryBarrier, id_list)
        OP_CASE_TYPE(SpvOpNamedBarrierInitialize, type_result_id_list)
        OP_CASE_TYPE(SpvOpMemoryNamedBarrier, id_list)

        // atomic instructions
        OP_CASE_TYPE(SpvOpAtomicLoad, type_result_id_list)
        OP_CASE_TYPE(SpvOpAtomicStore, id_list)
        OP_CASE_TYPE(SpvOpAtomicExchange, type_result_id_list)
        OP_CASE_TYPE(SpvOpAtomicCompareExchange, type_result_id_list)
        OP_CASE_TYPE(SpvOpAtomicCompareExchangeWeak, type_result_id_list)
        OP_CASE_TYPE(SpvOpAtomicIIncrement, type_result_id_list)
        OP_CASE_TYPE(SpvOpAtomicIDecrement, type_result_id_list)
        OP_CASE_TYPE(SpvOpAtomicIAdd, type_result_id_list)
        OP_CASE_TYPE(SpvOpAtomicISub, type_result_id_list)
        OP_CASE_TYPE(SpvOpAtomicSMin, type_result_id_list)
        OP_CASE_TYPE(SpvOpAtomicUMin, type_result_id_list)
        OP_CASE_TYPE(SpvOpAtomicSMax, type_result_id_list)
        OP_CASE_TYPE(SpvOpAtomicUMax, type_result_id_list)
        OP_CASE_TYPE(SpvOpAtomicAnd, type_result_id_list)
        OP_CASE_TYPE(SpvOpAtomicOr, type_result_id_list)
        OP_CASE_TYPE(SpvOpAtomicXor, type_result_id_list)
        OP_CASE_TYPE(SpvOpAtomicFlagTestAndSet, type_result_id_list)
        OP_CASE_TYPE(SpvOpAtomicFlagClear, id_list)

        // control-flow instructions
        OP_CASE_TYPE(SpvOpPhi, type_result_id_list)
        OP_CASE_SPECIAL(SpvOpLoopMerge)
        OP_CASE_SPECIAL(SpvOpSelectionMerge)
        OP_CASE_TYPE(SpvOpLabel, result_number_list)
        OP_CASE_TYPE(SpvOpBranch, id_list)
        OP_CASE_TYPE_1(SpvOpBranchConditional, n_id_number_list, 3)
        OP_CASE_SPECIAL(SpvOpSwitch)
        OP_DEFAULT(SpvOpKill)
        OP_DEFAULT(SpvOpReturn)
        OP_CASE_TYPE(SpvOpReturnValue, id_list)
        OP_DEFAULT(SpvOpUnreachable)
        OP_CASE_TYPE_1(SpvOpLifetimeStart, n_id_number_list, 1)
        OP_CASE_TYPE_1(SpvOpLifetimeStop, n_id_number_list, 1)

        // group instructions
        OP_CASE_TYPE(SpvOpGroupAsyncCopy, type_result_id_list)
        OP_CASE_TYPE(SpvOpGroupWaitEvents, id_list)
        OP_CASE_TYPE(SpvOpGroupAll, type_result_id_list)
        OP_CASE_TYPE(SpvOpGroupAny, type_result_id_list)
        OP_CASE_TYPE(SpvOpGroupBroadcast, type_result_id_list)
        OP_CASE_TYPE(SpvOpGroupIAdd, id_groupop_id)
        OP_CASE_TYPE(SpvOpGroupFAdd, id_groupop_id)
        OP_CASE_TYPE(SpvOpGroupFMin, id_groupop_id)
        OP_CASE_TYPE(SpvOpGroupUMin, id_groupop_id)
        OP_CASE_TYPE(SpvOpGroupSMin, id_groupop_id)
        OP_CASE_TYPE(SpvOpGroupFMax, id_groupop_id)
        OP_CASE_TYPE(SpvOpGroupUMax, id_groupop_id)
        OP_CASE_TYPE(SpvOpGroupSMax, id_groupop_id)

        // pipe instructions
        OP_CASE_TYPE(SpvOpReadPipe, type_result_id_list)
        OP_CASE_TYPE(SpvOpWritePipe, type_result_id_list)
        OP_CASE_TYPE(SpvOpReservedReadPipe, type_result_id_list)
        OP_CASE_TYPE(SpvOpReservedWritePipe, type_result_id_list)
        OP_CASE_TYPE(SpvOpReserveReadPipePackets, type_result_id_list)
        OP_CASE_TYPE(SpvOpReserveWritePipePackets, type_result_id_list)
        OP_CASE_TYPE(SpvOpCommitReadPipe, id_list)
        OP_CASE_TYPE(SpvOpCommitWritePipe, id_list)
        OP_CASE_TYPE(SpvOpIsValidReserveId, type_result_id_list)
        OP_CASE_TYPE(SpvOpGetNumPipePackets, type_result_id_list)
        OP_CASE_TYPE(SpvOpGetMaxPipePackets, type_result_id_list)
        OP_CASE_TYPE(SpvOpGroupReserveReadPipePackets, type_result_id_list)
        OP_CASE_TYPE(SpvOpGroupReserveWritePipePackets, type_result_id_list)
        OP_CASE_TYPE(SpvOpGroupCommitReadPipe, id_list)
        OP_CASE_TYPE(SpvOpGroupCommitWritePipe, id_list)
        OP_CASE_TYPE(SpvOpConstantPipeStorage, type_result_number_list)
        OP_CASE_TYPE(SpvOpCreatePipeFromPipeStorage, type_result_id_list)

        // device-side enqueue instructions
        OP_CASE_TYPE(SpvOpEnqueueMarker, type_result_id_list)
        OP_CASE_TYPE(SpvOpEnqueueKernel, type_result_id_list)
        OP_CASE_TYPE(SpvOpGetKernelNDrangeSubGroupCount, type_result_id_list)
        OP_CASE_TYPE(SpvOpGetKernelNDrangeMaxSubGroupSize, type_result_id_list)
        OP_CASE_TYPE(SpvOpGetKernelWorkGroupSize, type_result_id_list)
        OP_CASE_TYPE(SpvOpGetKernelPreferredWorkGroupSizeMultiple, type_result_id_list)
        OP_CASE_TYPE(SpvOpRetainEvent, id_list)
        OP_CASE_TYPE(SpvOpReleaseEvent, id_list)
        OP_CASE_TYPE(SpvOpCreateUserEvent, type_result_id_list)
        OP_CASE_TYPE(SpvOpIsValidEvent, type_result_id_list)
        OP_CASE_TYPE(SpvOpSetUserEventStatus, id_list)
        OP_CASE_TYPE(SpvOpCaptureEventProfilingInfo, id_list)
        OP_CASE_TYPE(SpvOpGetDefaultQueue, type_result_id_list)
        OP_CASE_TYPE(SpvOpBuildNDRange, type_result_id_list)
        OP_CASE_TYPE(SpvOpGetKernelLocalSizeForSubgroupCount, type_result_id_list)
        OP_CASE_TYPE(SpvOpGetKernelMaxNumSubgroups, type_result_id_list)

        default :
            line = spirv_string_opcode_no_result(opcode->op.kind);
    }

    return line;

#undef OP_CASE_SPECIAL
#undef OP_CASE_TYPE
#undef OP_CASE_TYPE_1
#undef OP_DEFAULT
}
