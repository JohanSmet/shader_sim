// spirv_text.c - Johan Smet - BSD-3-Clause (see LICENSE)
//
// SPIR-V binary to text string conversion

#include "spirv_text.h"
#include "spirv_binary.h"
#include "spirv_module.h"
#include "dyn_array.h"

#include <stdbool.h>
#include <ctype.h>
#include <assert.h>

#define JS_SPIRV_NAMES_IMPLEMENTATION
#include "spirv/spirv_names.h"

static inline void spirv_text_create_type_alias(SPIRV_module *spirv_mod, Type *type) {
    if (spirv_type_is_vector(type)) {
        arr_printf(spirv_mod->text->scratch_buf, "%%vec%d%s",
            type->count,
            (spirv_type_is_float(type)) ? "f" : 
                (spirv_type_is_signed_integer(type)) ? "i" : "u"
        );
    }

    if (spirv_type_is_matrix(type)) {
        arr_printf(spirv_mod->text->scratch_buf, "%%mat%dx%d%s",
            type->matrix.num_rows,
            type->matrix.num_cols,
            (spirv_type_is_float(type)) ? "f" : 
                (spirv_type_is_signed_integer(type)) ? "i" : "u"
        );
    }

    if (spirv_type_is_float(type) && type->count == 1) {
        arr_printf(spirv_mod->text->scratch_buf, "%%float");
    }

    if (spirv_type_is_integer(type) && type->count == 1) {
        arr_printf(spirv_mod->text->scratch_buf, "%%%s",
            (spirv_type_is_signed_integer(type)) ? "int" : "uint"
        );
    }

    if (type->kind == TypeBool && type->count == 1) {
        arr_strcat(spirv_mod->text->scratch_buf, "%bool");
    }

    if (type->kind == TypeVoid) {
        arr_strcat(spirv_mod->text->scratch_buf, "%void");
    }

    if (type->kind == TypePointer) {
        arr_printf(spirv_mod->text->scratch_buf, "%%ptr_%s_",
            spirv_storage_class_name((SpvStorageClass) type->pointer.storage_class)
        );
        for (char *s = spirv_mod->text->scratch_buf; *s; ++s) {
            *s = (char) tolower(*s);
        }
        spirv_text_create_type_alias(spirv_mod, type->base_type);
    }
}

static inline const char *spirv_text_format_id(SPIRV_module *spirv_mod, uint32_t id) {
    arr_clear(spirv_mod->text->scratch_buf);

    if (spirv_mod->text->use_id_names && arr_len(spirv_mod->text->scratch_buf) == 0) {
        const char *name = spirv_module_name_by_id(spirv_mod, id, -1);
        if (name) {
            arr_printf(spirv_mod->text->scratch_buf, "%%%s", name);
        }
    } 

    if (arr_len(spirv_mod->text->scratch_buf) == 0) {
       arr_printf(spirv_mod->text->scratch_buf, "%%%d", id);
    }

    return spirv_mod->text->scratch_buf;
}

static inline const char *spirv_text_format_type_id(SPIRV_module *spirv_mod, uint32_t id) {
    arr_clear(spirv_mod->text->scratch_buf);

    /* prefer manually defined name */
    if (spirv_mod->text->use_id_names && arr_len(spirv_mod->text->scratch_buf) == 0) {
        const char *name = spirv_module_name_by_id(spirv_mod, id, -1);
        if (name) {
            arr_printf(spirv_mod->text->scratch_buf, "%%%s", name);
        }
    } 

    /* check if there's already an alias for this id */
    if (spirv_mod->text->use_type_alias && arr_len(spirv_mod->text->scratch_buf) == 0) {
        const char *alias = map_int_str_get(&spirv_mod->text->type_aliases, id);
        if (alias) {
            arr_printf(spirv_mod->text->scratch_buf, "%s", alias);
        }
    }

    /* try to generate an alias */
    if (spirv_mod->text->use_type_alias && arr_len(spirv_mod->text->scratch_buf) == 0) {
        Type *type = spirv_module_type_by_id(spirv_mod, id);
        spirv_text_create_type_alias(spirv_mod, type);

        /* check for duplicates */
        if (arr_len(spirv_mod->text->scratch_buf) > 0 && 
            !map_str_int_has(&spirv_mod->text->type_aliases_rev, spirv_mod->text->scratch_buf)) {
            char *alias = mem_arena_allocate(&spirv_mod->allocator, arr_len(spirv_mod->text->scratch_buf) + 1);
            strncpy(alias, spirv_mod->text->scratch_buf, arr_len(spirv_mod->text->scratch_buf) + 1);

            map_int_str_put(&spirv_mod->text->type_aliases, id, alias);
            map_str_int_put(&spirv_mod->text->type_aliases_rev, alias, id);
        } else {
            arr_clear(&spirv_mod->text->scratch_buf);
        }
    }

    /* fallback to numerical id */
    if (arr_len(spirv_mod->text->scratch_buf) == 0) {
       arr_printf(spirv_mod->text->scratch_buf, "%%%d", id);
    }

    return spirv_mod->text->scratch_buf;
}

static inline void spirv_text_append(char **result, const char **strings, size_t count) {
    for (size_t idx = 0; idx < count; ++idx) {
        arr_strcat(*result, strings[idx]);
    }
}

static inline void spirv_text_start_tag(SPIRV_module *module, char *result, SPIRV_text_kind kind) {
    SPIRV_text_span span = (SPIRV_text_span) {
        .start = arr_len(result),
        .kind = kind
    };
    arr_push(module->text->spans, span);
}

static inline void spirv_text_end_tag(SPIRV_module *module, char *result) {
    module->text->spans[arr_len(module->text->spans)-1].end = (uint32_t) arr_len(result) - 1u;
}

/*
 * macros to add semantic information to the output
 */


#define APPEND_STR(...)     spirv_text_append(&result, (const char *[]){__VA_ARGS__}, sizeof((const char *[]) {__VA_ARGS__})/sizeof(const char *))
#define APPEND_FMT(fmt,...) arr_printf(result, fmt, __VA_ARGS__)
#define SPACER              arr_printf(result, " ")
#define STAG(x)             spirv_text_start_tag(module, result, SPAN_##x)
#define ETAG                spirv_text_end_tag(module, result)

#define OP(op)              (STAG(OP), APPEND_STR(spirv_op_name((op))), ETAG)
#define KEYWORD(x)          (SPACER, STAG(KEYWORD), APPEND_STR((x)), ETAG)
#define LITERAL_STRING(x)   (SPACER, STAG(LITERAL_STRING), APPEND_STR("\"", (x), "\""), ETAG)
#define LITERAL_INTEGER(x)  (SPACER, STAG(LITERAL_INTEGER), APPEND_FMT("%d", (x)), ETAG)
#define LITERAL_FLOAT(x)    (SPACER, STAG(LITERAL_FLOAT), APPEND_FMT("%f", (double) (x)), ETAG)
#define FORMATTED_ID(x)     (STAG(ID), APPEND_STR((x)), ETAG)
#define ID(x)               (SPACER, STAG(ID), APPEND_STR(spirv_text_format_id(module, (x))), ETAG)
#define TYPE_ID(x)          (SPACER, STAG(TYPE_ID), APPEND_STR(spirv_text_format_type_id(module, (x))), ETAG)

static inline char *spirv_string_opcode_no_result(SPIRV_module *module, SpvOp op) {
    char *result = NULL;
    APPEND_FMT("%16s", "");
    OP(op);
    return result;
}

static inline char *spirv_string_opcode_result_id(SPIRV_module *module, SpvOp op, uint32_t result_id) {
    char *result = NULL;
    const char *s_id = spirv_text_format_id(module, result_id);

    /* try to right align the result-id in the first column */
    int spaces = 13 - (int) arr_len(s_id);
    for (int i = 0; i < spaces; ++i) {
        APPEND_STR(" ");
    }

    FORMATTED_ID(s_id);
    APPEND_STR(" = ");
    OP(op);
    return result;
}

static inline char *spirv_string_opcode_result_type_id(SPIRV_module *module, SpvOp op, uint32_t type_id) {
    char *result = NULL;
    const char *s_id = spirv_text_format_type_id(module, type_id);

    /* try to right align the result-id in the first column */
    int spaces = 13 - (int) arr_len(s_id);
    for (int i = 0; i < spaces; ++i) {
        APPEND_STR(" ");
    }

    FORMATTED_ID(s_id);
    APPEND_STR(" = ");
    OP(op);
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

#define TEXT_FUNC_SPECIAL_BEGIN(kind)       \
    static inline char *spirv_text_##kind(SPIRV_module *module, SPIRV_opcode *opcode) {     \
        assert(module);                     \
        assert(opcode);

#define TEXT_FUNC_SPECIAL_OP_NORES(kind)    \
    TEXT_FUNC_SPECIAL_BEGIN(kind)           \
        char *result = spirv_string_opcode_no_result(module, kind);

#define TEXT_FUNC_SPECIAL_OP_RESID(kind)    \
    TEXT_FUNC_SPECIAL_BEGIN(kind)           \
        char *result = spirv_string_opcode_result_id(module, kind, opcode->optional[1]);

#define TEXT_FUNC_SPECIAL_OP_RESTYPE(kind)  \
    TEXT_FUNC_SPECIAL_BEGIN(kind)           \
        char *result = spirv_string_opcode_result_type_id(module, kind, opcode->optional[0]);

#define TEXT_FUNC_TYPE_NORES(type)          \
    static inline char *spirv_text_##type(SPIRV_module *module, SPIRV_opcode *opcode) { \
        assert(module);                     \
        assert(opcode);                     \
        char *result = spirv_string_opcode_no_result(module, opcode->op.kind);

#define TEXT_FUNC_TYPE_RESID(type)          \
    static inline char *spirv_text_result_##type(SPIRV_module *module, SPIRV_opcode *opcode) { \
        assert(module);                     \
        assert(opcode);                     \
        char *result = spirv_string_opcode_result_id(module, opcode->op.kind, opcode->optional[1]);

#define TEXT_FUNC_TYPE_RESTYPE(type)        \
    static inline char *spirv_text_restype_##type(SPIRV_module *module, SPIRV_opcode *opcode) { \
        assert(module);                     \
        assert(opcode);                     \
        char *result = spirv_string_opcode_result_type_id(module, opcode->op.kind, opcode->optional[0]);

#define TEXT_FUNC_TYPE_N_RESID(type)        \
    static inline char *spirv_text_result_##type(SPIRV_module *module, SPIRV_opcode *opcode, int n) {  \
        assert(module);                     \
        assert(opcode);                     \
        char *result = spirv_string_opcode_result_id(module, opcode->op.kind, opcode->optional[0]);

#define TEXT_FUNC_TYPE_N_NORES(type)        \
    static inline char *spirv_text_##type(SPIRV_module *module, SPIRV_opcode *opcode, int n) {  \
        assert(module);                     \
        assert(opcode);                     \
        char *result = spirv_string_opcode_no_result(module, opcode->op.kind);

#define TEXT_FUNC_END   }


/*
 * text functions for one specific opcode
 */

TEXT_FUNC_SPECIAL_OP_NORES(SpvOpCapability) {
    KEYWORD(spirv_capability_name(opcode->optional[0]));
    return result;
} TEXT_FUNC_END

TEXT_FUNC_SPECIAL_OP_NORES(SpvOpSource) {
    char version[64];
    snprintf(version, 63, "v%d", opcode->optional[1]);
    KEYWORD(spirv_source_language_name(opcode->optional[0]));
    KEYWORD(version);
    // TODO: optional file & source
    return result;
} TEXT_FUNC_END

TEXT_FUNC_SPECIAL_OP_NORES(SpvOpName) {
    ID (opcode->optional[0]);
    LITERAL_STRING((const char *) &opcode->optional[1]);
    return result;
} TEXT_FUNC_END

TEXT_FUNC_SPECIAL_OP_NORES(SpvOpMemberName) {
    ID(opcode->optional[0]);
    LITERAL_INTEGER(opcode->optional[1]);
    LITERAL_STRING((const char *)&opcode->optional[2]);
    return result;
} TEXT_FUNC_END

TEXT_FUNC_SPECIAL_OP_RESID(SpvOpExtInst) {
    ID(opcode->optional[0]);
    ID(opcode->optional[2]);
    LITERAL_INTEGER(opcode->optional[3]);
    for (int idx = 4; idx < opcode->op.length - 1; ++idx) {
        ID(opcode->optional[idx]);
    }
    return result;
} TEXT_FUNC_END

TEXT_FUNC_SPECIAL_OP_NORES(SpvOpMemoryModel) {
    KEYWORD(spirv_addressing_model_name(opcode->optional[0]));
    KEYWORD(spirv_memory_model_name(opcode->optional[1]));
    return result;
} TEXT_FUNC_END

TEXT_FUNC_SPECIAL_OP_NORES(SpvOpEntryPoint) {
    KEYWORD(spirv_execution_model_name(opcode->optional[0]));
    ID(opcode->optional[1]);
    LITERAL_STRING ((const char *) &opcode->optional[2]);

    size_t len = (strlen((const char *) &opcode->optional[2]) + 3) / 4;
    for (size_t idx = 2+len; idx < opcode->op.length - 1; ++idx) {
        ID(opcode->optional[idx]);
    }

    return result;
} TEXT_FUNC_END

TEXT_FUNC_SPECIAL_OP_RESTYPE(SpvOpTypeImage) {
    TYPE_ID(opcode->optional[1]);
    KEYWORD(spirv_dim_name(opcode->optional[2]));
    LITERAL_INTEGER(opcode->optional[3]);
    LITERAL_INTEGER(opcode->optional[4]);
    LITERAL_INTEGER(opcode->optional[5]);
    LITERAL_INTEGER(opcode->optional[6]);
    KEYWORD(spirv_image_format_name(opcode->optional[7])); 
    
    if (opcode->op.length == 10) {
        KEYWORD(spirv_access_qualifier_name(opcode->optional[8]));
    }

    return result;
} TEXT_FUNC_END

TEXT_FUNC_SPECIAL_OP_RESTYPE(SpvOpTypePointer) {
    KEYWORD(spirv_storage_class_name(opcode->optional[1]));
    ID(opcode->optional[2]);
    return result;
} TEXT_FUNC_END

TEXT_FUNC_SPECIAL_OP_RESTYPE(SpvOpTypePipe) {
    KEYWORD(spirv_access_qualifier_name(opcode->optional[1]));
    return result;
} TEXT_FUNC_END

TEXT_FUNC_SPECIAL_OP_NORES(SpvOpTypeForwardPointer) {
    TYPE_ID(opcode->optional[0]);
    KEYWORD(spirv_storage_class_name(opcode->optional[1]));
    return result;
} TEXT_FUNC_END

TEXT_FUNC_SPECIAL_OP_NORES(SpvOpExecutionMode) {
    ID(opcode->optional[0]);
    KEYWORD(spirv_execution_mode_name(opcode->optional[1]));
    return result;
} TEXT_FUNC_END

TEXT_FUNC_SPECIAL_OP_NORES(SpvOpExecutionModeId) {
    ID(opcode->optional[0]);
    KEYWORD(spirv_execution_mode_name(opcode->optional[1]));
    for (int idx=2; idx < opcode->op.length - 1; ++idx) {
        ID(opcode->optional[idx]);
    }
    return result;
} TEXT_FUNC_END

TEXT_FUNC_SPECIAL_OP_RESID(SpvOpConstant) {
    TYPE_ID(opcode->optional[0]);

    Type *res_type = spirv_module_type_by_id(module, opcode->optional[0]);
    
    if (spirv_type_is_float(res_type)) {
        for (int idx=2; idx < opcode->op.length - 1; ++idx) {
            LITERAL_FLOAT(*((float *) &opcode->optional[idx]));
        }
    } else  {
        for (int idx=2; idx < opcode->op.length - 1; ++idx) {
            LITERAL_INTEGER(opcode->optional[idx]);
        }
    }
    return result;
} TEXT_FUNC_END

TEXT_FUNC_SPECIAL_OP_RESID(SpvOpConstantSampler) {
    TYPE_ID(opcode->optional[0]);
    KEYWORD(spirv_sampler_addressing_mode_name(opcode->optional[2]));
    LITERAL_INTEGER(opcode->optional[3]);
    KEYWORD(spirv_sampler_filter_mode_name(opcode->optional[4]));
    return result;
} TEXT_FUNC_END

TEXT_FUNC_SPECIAL_OP_RESID(SpvOpSpecConstantOp) {
    TYPE_ID(opcode->optional[0]);
    KEYWORD(spirv_op_name(opcode->optional[2]));
    for (int idx=3; idx < opcode->op.length - 1; ++idx) {
        ID(opcode->optional[idx]);
    }
    return result;
} TEXT_FUNC_END

TEXT_FUNC_SPECIAL_OP_RESID(SpvOpFunction) {
    TYPE_ID(opcode->optional[0]);
    spirv_string_bitmask(&result, opcode->optional[2], SPIRV_FUNCTION_CONTROL);
    ID(opcode->optional[3]);
    return result;
} TEXT_FUNC_END

TEXT_FUNC_SPECIAL_OP_RESID(SpvOpVariable) {
    TYPE_ID(opcode->optional[0]);
    KEYWORD(spirv_storage_class_name(opcode->optional[2]));
    for (int idx=3; idx < opcode->op.length - 1; ++idx) {
        ID(opcode->optional[idx]);
    }
    return result;
} TEXT_FUNC_END

TEXT_FUNC_SPECIAL_OP_RESID(SpvOpLoad) {
    TYPE_ID(opcode->optional[0]);
    ID(opcode->optional[2]);
    if (opcode->op.length == 5) {
        spirv_string_bitmask(&result, opcode->optional[3], SPIRV_MEMORY_ACCESS);
    }
    return result;
} TEXT_FUNC_END

TEXT_FUNC_SPECIAL_OP_NORES(SpvOpStore) {
    ID(opcode->optional[0]);
    ID(opcode->optional[1]);
    if (opcode->op.length == 4) {
        spirv_string_bitmask(&result, opcode->optional[2], SPIRV_MEMORY_ACCESS);
    }
    return result;
} TEXT_FUNC_END

TEXT_FUNC_SPECIAL_BEGIN(SpvOpCopyMemory) {
    return spirv_text_SpvOpStore(module, opcode);
} TEXT_FUNC_END

TEXT_FUNC_SPECIAL_OP_NORES(SpvOpCopyMemorySized) {
    ID(opcode->optional[0]);
    ID(opcode->optional[1]);
    ID(opcode->optional[2]);
    if (opcode->op.length == 5) {
        spirv_string_bitmask(&result, opcode->optional[3], SPIRV_MEMORY_ACCESS);
    }
    return result;
} TEXT_FUNC_END

TEXT_FUNC_SPECIAL_OP_NORES(SpvOpDecorate) {
    ID(opcode->optional[0]);
    KEYWORD(spirv_decoration_name(opcode->optional[1]));
    for (int idx=2; idx < opcode->op.length - 1; ++idx) {
        if (opcode->optional[1] != SpvDecorationBuiltIn) {
            LITERAL_INTEGER(opcode->optional[idx]);
        } else {
            KEYWORD(spirv_builtin_name(opcode->optional[idx]));
        }
    }
    return result;
} TEXT_FUNC_END

TEXT_FUNC_SPECIAL_OP_NORES(SpvOpDecorateId) {
    ID(opcode->optional[0]);
    KEYWORD(spirv_decoration_name(opcode->optional[1]));
    for (int idx=2; idx < opcode->op.length - 1; ++idx) {
        ID(opcode->optional[idx]);
    }
    return result;
} TEXT_FUNC_END

TEXT_FUNC_SPECIAL_OP_NORES(SpvOpMemberDecorate) {
    TYPE_ID(opcode->optional[0]);
    LITERAL_INTEGER(opcode->optional[1]);
    KEYWORD(spirv_decoration_name(opcode->optional[2]));
    for (int idx=3; idx < opcode->op.length - 1; ++idx) {
        if (opcode->optional[2] != SpvDecorationBuiltIn) {
            LITERAL_INTEGER(opcode->optional[idx]);
        } else {
            KEYWORD(spirv_builtin_name(opcode->optional[idx]));
        }
    }
    return result;
} TEXT_FUNC_END

TEXT_FUNC_SPECIAL_OP_NORES(SpvOpGroupMemberDecorate) {
    ID(opcode->optional[0]);
    for (int idx = 1; idx < opcode->op.length - 1; idx += 2) {
        ID(opcode->optional[idx]);
        LITERAL_INTEGER(opcode->optional[idx+1]);
    }
    return result;
} TEXT_FUNC_END

TEXT_FUNC_SPECIAL_OP_NORES(SpvOpLoopMerge) {
    ID(opcode->optional[0]); 
    ID(opcode->optional[1]);
    spirv_string_bitmask(&result, opcode->optional[2], SPIRV_LOOP_CONTROL);
    return result;
} TEXT_FUNC_END

TEXT_FUNC_SPECIAL_OP_NORES(SpvOpSelectionMerge) {
    ID(opcode->optional[0]);
    spirv_string_bitmask(&result, opcode->optional[1], SPIRV_SELECTION_CONTROL);
    return result;
} TEXT_FUNC_END

TEXT_FUNC_SPECIAL_OP_NORES(SpvOpSwitch) {
    ID(opcode->optional[0]);
    ID(opcode->optional[1]);
    for (int idx = 2; idx < opcode->op.length - 1; idx += 2) {
        ID(opcode->optional[idx]);
        LITERAL_INTEGER(opcode->optional[idx+1]);
    }
    return result;
} TEXT_FUNC_END

TEXT_FUNC_SPECIAL_BEGIN(SpvOpLabel) {
    char *result = spirv_string_opcode_result_id(module, opcode->op.kind, opcode->optional[0]);
    return result;
} TEXT_FUNC_END

TEXT_FUNC_SPECIAL_BEGIN(SpvOpExtInstImport) {
    char *result = spirv_string_opcode_result_id(module, opcode->op.kind, opcode->optional[0]);
    LITERAL_STRING((const char *) &opcode->optional[1]);
    return result;
} TEXT_FUNC_END

/*
 * generic text functions - used by multiple types
 */

TEXT_FUNC_TYPE_NORES(string) {              /* OP "string" */
    LITERAL_STRING((const char *) &opcode->optional[0]);
    return result;
} TEXT_FUNC_END

TEXT_FUNC_TYPE_RESID(string) {              /* %id = OP "string" */
    LITERAL_STRING((const char *) &opcode->optional[1]);
    return result;
} TEXT_FUNC_END

TEXT_FUNC_TYPE_RESTYPE(string) {              /* %id = OP "string" */
    LITERAL_STRING((const char *) &opcode->optional[1]);
    return result;
} TEXT_FUNC_END

TEXT_FUNC_TYPE_RESTYPE(number_list) {       /* %type_id = OP number* */
    for (int idx=1; idx < opcode->op.length - 1; ++idx) {
        LITERAL_INTEGER(opcode->optional[idx]);
    }
    return result;
} TEXT_FUNC_END

TEXT_FUNC_TYPE_RESID(number_list) {         /* %id = OP number* */
    for (int idx=1; idx < opcode->op.length - 1; ++idx) {
        LITERAL_INTEGER(opcode->optional[idx]);
    }
    return result;
} TEXT_FUNC_END

TEXT_FUNC_TYPE_RESTYPE(id_number_list) {      /* %id = OP %id number+ */   
    ID(opcode->optional[1]);        
    for (int idx=2; idx < opcode->op.length - 1; ++idx) {
        LITERAL_INTEGER(opcode->optional[idx]);
    }
    return result;
} TEXT_FUNC_END

TEXT_FUNC_TYPE_RESTYPE(id_list) {             /* %id = OP %id+ */     // XXX: typeid?
    for (int idx=1; idx < opcode->op.length - 1; ++idx) {
        ID(opcode->optional[idx]);
    }
    return result;
} TEXT_FUNC_END

TEXT_FUNC_TYPE_NORES(id_list) {              /* OP %id+ */
    for (int idx=0; idx < opcode->op.length - 1; ++idx) {
        ID(opcode->optional[idx]);
    }
    return result;
} TEXT_FUNC_END

TEXT_FUNC_TYPE_RESID(type_number_list) {     /* %id = OP %type_id number+ */
    ID(opcode->optional[0]);
    for (int idx=2; idx < opcode->op.length - 1; ++idx) {
        LITERAL_INTEGER(opcode->optional[idx]);
    }
    return result;
} TEXT_FUNC_END

TEXT_FUNC_TYPE_RESID(type_id_list) {        /* %id = OP %type_id %id+ */
    TYPE_ID(opcode->optional[0]);
    for (int idx=2; idx < opcode->op.length - 1; ++idx) {
        ID(opcode->optional[idx]);
    }
    return result;
} TEXT_FUNC_END

TEXT_FUNC_TYPE_RESID(type_id_number) {     /* %id = OP %type_id %id number */
    TYPE_ID(opcode->optional[0]);
    ID(opcode->optional[2]);
    LITERAL_INTEGER(opcode->optional[3]);
    return result;
} TEXT_FUNC_END

TEXT_FUNC_TYPE_N_RESID(type_n_id_number_list) {     /* %id = OP %type_id %id*n number+ */
    TYPE_ID(opcode->optional[0]);
    for (int idx = 0; idx < n; ++idx) {
        ID(opcode->optional[2+idx]);
    }
    for (int idx=2+n; idx < opcode->op.length - 1; ++idx) {
        LITERAL_INTEGER(opcode->optional[idx]);
    }
    return result;
} TEXT_FUNC_END

TEXT_FUNC_TYPE_N_NORES(n_id_number_list) {          /* OP %id*n number+ */
    for (int idx = 0; idx < n; ++idx) {
        ID(opcode->optional[idx]);
    }
    for (int idx=n; idx < opcode->op.length - 1; ++idx) {
        LITERAL_INTEGER(opcode->optional[idx]);
    }
    return result;
} TEXT_FUNC_END

TEXT_FUNC_TYPE_N_RESID(type_n_id_imageop_id_list) { /* %id = OP %type_id %id*n <image-operand> %id+ */
    TYPE_ID(opcode->optional[0]);
    for (int idx = 0; idx < n; ++idx) {
        ID(opcode->optional[2+idx]);
    }
    if (opcode->op.length > n + 2) {
        spirv_string_bitmask(&result, opcode->optional[n + 2], SPIRV_IMAGE_OPERANDS);
    }
    for (int idx=3+n; idx < opcode->op.length - 1; ++idx) {
        ID(opcode->optional[idx]);
    }
    return result;
} TEXT_FUNC_END

TEXT_FUNC_TYPE_N_NORES(n_id_imageop_id_list) {      /* OP %id*n <image-operand> %id+ */
    for (int idx = 0; idx < n; ++idx) {
        ID(opcode->optional[idx]);
    }
    if (opcode->op.length > n) {
        spirv_string_bitmask(&result, opcode->optional[n], SPIRV_IMAGE_OPERANDS);
    }
    for (int idx=1+n; idx < opcode->op.length - 1; ++idx) {
        ID(opcode->optional[idx]);
    }
    return result;
} TEXT_FUNC_END

TEXT_FUNC_TYPE_NORES(id_groupop_id) {              /* OP %id <group-operation> %id */
    ID(opcode->optional[0]);
    KEYWORD(spirv_group_operation_name(opcode->optional[1]));
    ID(opcode->optional[2]);
    return result;
} TEXT_FUNC_END

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

void spirv_text_set_flag(struct SPIRV_module *module, SPIRV_text_flag flag, bool value) {
    assert(module);

    if (flag == SPIRV_TEXT_USE_ID_NAMES) {
        module->text->use_id_names = value;
    } else if (flag == SPIRV_TEXT_USE_TYPE_ALIAS) {
        module->text->use_type_alias = value;
    }
}

char *spirv_text_opcode(SPIRV_opcode *opcode, SPIRV_module *module, SPIRV_text_span **spans) {
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

    arr_clear(module->text->spans);
    char *line = NULL;

    switch (opcode->op.kind) {
        // miscellaneous instructions
        OP_DEFAULT(SpvOpNop)
        OP_CASE_TYPE(SpvOpUndef, result_type_id_list)
        OP_CASE_TYPE(SpvOpSizeOf, result_type_id_list)

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
        OP_CASE_SPECIAL(SpvOpExtInstImport)
        OP_CASE_SPECIAL(SpvOpExtInst)

        // mode-setting instructions
        OP_CASE_SPECIAL(SpvOpMemoryModel)
        OP_CASE_SPECIAL(SpvOpEntryPoint)
        OP_CASE_SPECIAL(SpvOpExecutionMode)
        OP_CASE_SPECIAL(SpvOpCapability)
        OP_CASE_SPECIAL(SpvOpExecutionModeId)

        // type-declaration instructions
        OP_CASE_TYPE(SpvOpTypeVoid, restype_number_list)
        OP_CASE_TYPE(SpvOpTypeBool, restype_number_list)
        OP_CASE_TYPE(SpvOpTypeInt, restype_number_list)
        OP_CASE_TYPE(SpvOpTypeFloat, restype_number_list)
        OP_CASE_TYPE(SpvOpTypeVector, restype_id_number_list)
        OP_CASE_TYPE(SpvOpTypeMatrix, restype_id_number_list)
        OP_CASE_SPECIAL(SpvOpTypeImage)
        OP_CASE_TYPE(SpvOpTypeSampler, restype_number_list)
        OP_CASE_TYPE(SpvOpTypeSampledImage, restype_id_list)
        OP_CASE_TYPE(SpvOpTypeArray, restype_id_list)
        OP_CASE_TYPE(SpvOpTypeRuntimeArray, restype_id_list)
        OP_CASE_TYPE(SpvOpTypeStruct, restype_id_list)
        OP_CASE_TYPE(SpvOpTypeOpaque, restype_string)
        OP_CASE_SPECIAL(SpvOpTypePointer)
        OP_CASE_TYPE(SpvOpTypeFunction, restype_id_list)
        OP_CASE_TYPE(SpvOpTypeEvent, restype_number_list)
        OP_CASE_TYPE(SpvOpTypeDeviceEvent, restype_number_list)
        OP_CASE_TYPE(SpvOpTypeReserveId, restype_number_list)
        OP_CASE_TYPE(SpvOpTypeQueue, restype_number_list)
        OP_CASE_SPECIAL(SpvOpTypePipe)
        OP_CASE_SPECIAL(SpvOpTypeForwardPointer)
        OP_CASE_TYPE(SpvOpTypePipeStorage, restype_number_list)
        OP_CASE_TYPE(SpvOpTypeNamedBarrier, restype_number_list)

        // constant-creation instructions
        OP_CASE_TYPE(SpvOpConstantTrue, result_type_number_list)
        OP_CASE_TYPE(SpvOpConstantFalse, result_type_number_list)
        OP_CASE_SPECIAL(SpvOpConstant)
        OP_CASE_TYPE(SpvOpConstantComposite, result_type_id_list)
        OP_CASE_SPECIAL(SpvOpConstantSampler)
        OP_CASE_TYPE(SpvOpConstantNull, result_type_number_list)
        OP_CASE_TYPE(SpvOpSpecConstantTrue, result_type_number_list)
        OP_CASE_TYPE(SpvOpSpecConstantFalse, result_type_number_list)
        OP_CASE_TYPE(SpvOpSpecConstant, result_type_number_list)
        OP_CASE_TYPE(SpvOpSpecConstantComposite, result_type_id_list)
        OP_CASE_SPECIAL(SpvOpSpecConstantOp)

        // function instructions
        OP_CASE_SPECIAL(SpvOpFunction)
        OP_CASE_TYPE(SpvOpFunctionParameter, result_type_number_list)
        OP_DEFAULT(SpvOpFunctionEnd)
        OP_CASE_TYPE(SpvOpFunctionCall, result_type_id_list)

        // memory instructions
        OP_CASE_SPECIAL(SpvOpVariable)
        OP_CASE_TYPE(SpvOpImageTexelPointer, result_type_id_list)
        OP_CASE_SPECIAL(SpvOpLoad)
        OP_CASE_SPECIAL(SpvOpStore)
        OP_CASE_SPECIAL(SpvOpCopyMemory)
        OP_CASE_SPECIAL(SpvOpCopyMemorySized)
        OP_CASE_TYPE(SpvOpAccessChain, result_type_id_list)
        OP_CASE_TYPE(SpvOpInBoundsAccessChain, result_type_id_list)
        OP_CASE_TYPE(SpvOpPtrAccessChain, result_type_id_list)
        OP_CASE_TYPE(SpvOpArrayLength, result_type_id_number)
        OP_CASE_TYPE(SpvOpGenericPtrMemSemantics, result_type_id_list)
        OP_CASE_TYPE(SpvOpInBoundsPtrAccessChain, result_type_id_list)

        // annotation instructions
        OP_CASE_SPECIAL(SpvOpDecorate)
        OP_CASE_SPECIAL(SpvOpMemberDecorate)
        OP_CASE_TYPE(SpvOpDecorationGroup, result_number_list)
        OP_CASE_TYPE(SpvOpGroupDecorate, id_list)
        OP_CASE_SPECIAL(SpvOpGroupMemberDecorate)
        OP_CASE_SPECIAL(SpvOpDecorateId)

        // composite instructions
        OP_CASE_TYPE(SpvOpVectorExtractDynamic, result_type_id_list)
        OP_CASE_TYPE(SpvOpVectorInsertDynamic, result_type_id_list)
        OP_CASE_TYPE_1(SpvOpVectorShuffle, result_type_n_id_number_list, 2)
        OP_CASE_TYPE(SpvOpCompositeConstruct, result_type_id_list)
        OP_CASE_TYPE_1(SpvOpCompositeExtract, result_type_n_id_number_list, 1)
        OP_CASE_TYPE_1(SpvOpCompositeInsert, result_type_n_id_number_list, 2)
        OP_CASE_TYPE(SpvOpCopyObject, result_type_id_list)
        OP_CASE_TYPE(SpvOpTranspose, result_type_id_list)

        // image instructions
        OP_CASE_TYPE(SpvOpSampledImage, result_type_id_list)
        OP_CASE_TYPE_1(SpvOpImageSampleImplicitLod, result_type_n_id_imageop_id_list, 2)
        OP_CASE_TYPE_1(SpvOpImageSampleExplicitLod, result_type_n_id_imageop_id_list, 2)
        OP_CASE_TYPE_1(SpvOpImageSampleDrefImplicitLod, result_type_n_id_imageop_id_list, 3)
        OP_CASE_TYPE_1(SpvOpImageSampleDrefExplicitLod, result_type_n_id_imageop_id_list, 3)
        OP_CASE_TYPE_1(SpvOpImageSampleProjImplicitLod, result_type_n_id_imageop_id_list, 2)
        OP_CASE_TYPE_1(SpvOpImageSampleProjExplicitLod, result_type_n_id_imageop_id_list, 2)
        OP_CASE_TYPE_1(SpvOpImageSampleProjDrefImplicitLod, result_type_n_id_imageop_id_list, 3)
        OP_CASE_TYPE_1(SpvOpImageSampleProjDrefExplicitLod, result_type_n_id_imageop_id_list, 3)
        OP_CASE_TYPE_1(SpvOpImageFetch, result_type_n_id_imageop_id_list, 2)
        OP_CASE_TYPE_1(SpvOpImageGather, result_type_n_id_imageop_id_list, 3)
        OP_CASE_TYPE_1(SpvOpImageDrefGather, result_type_n_id_imageop_id_list, 3)
        OP_CASE_TYPE_1(SpvOpImageRead, result_type_n_id_imageop_id_list, 2)
        OP_CASE_TYPE_1(SpvOpImageWrite, n_id_imageop_id_list, 3)
        OP_CASE_TYPE(SpvOpImage, result_type_id_list)
        OP_CASE_TYPE(SpvOpImageQueryFormat, result_type_id_list)
        OP_CASE_TYPE(SpvOpImageQueryOrder, result_type_id_list)
        OP_CASE_TYPE(SpvOpImageQuerySizeLod, result_type_id_list)
        OP_CASE_TYPE(SpvOpImageQuerySize, result_type_id_list)
        OP_CASE_TYPE(SpvOpImageQueryLod, result_type_id_list)
        OP_CASE_TYPE(SpvOpImageQueryLevels, result_type_id_list)
        OP_CASE_TYPE(SpvOpImageQuerySamples, result_type_id_list)
        OP_CASE_TYPE_1(SpvOpImageSparseSampleImplicitLod, result_type_n_id_imageop_id_list, 2)
        OP_CASE_TYPE_1(SpvOpImageSparseSampleExplicitLod, result_type_n_id_imageop_id_list, 2)
        OP_CASE_TYPE_1(SpvOpImageSparseSampleDrefImplicitLod, result_type_n_id_imageop_id_list, 3)
        OP_CASE_TYPE_1(SpvOpImageSparseSampleDrefExplicitLod, result_type_n_id_imageop_id_list, 3)
        OP_CASE_TYPE_1(SpvOpImageSparseSampleProjImplicitLod, result_type_n_id_imageop_id_list, 2)
        OP_CASE_TYPE_1(SpvOpImageSparseSampleProjExplicitLod, result_type_n_id_imageop_id_list, 2)
        OP_CASE_TYPE_1(SpvOpImageSparseSampleProjDrefImplicitLod, result_type_n_id_imageop_id_list, 3)
        OP_CASE_TYPE_1(SpvOpImageSparseSampleProjDrefExplicitLod, result_type_n_id_imageop_id_list, 3)
        OP_CASE_TYPE_1(SpvOpImageSparseFetch, result_type_n_id_imageop_id_list, 2)
        OP_CASE_TYPE_1(SpvOpImageSparseGather, result_type_n_id_imageop_id_list, 3)
        OP_CASE_TYPE_1(SpvOpImageSparseDrefGather, result_type_n_id_imageop_id_list, 3)
        OP_CASE_TYPE(SpvOpImageSparseTexelsResident, result_type_id_list)
        OP_CASE_TYPE_1(SpvOpImageSparseRead, result_type_n_id_imageop_id_list, 2)

        // conversion instructions
        OP_CASE_TYPE(SpvOpConvertFToU, result_type_id_list)
        OP_CASE_TYPE(SpvOpConvertFToS, result_type_id_list)
        OP_CASE_TYPE(SpvOpConvertSToF, result_type_id_list)
        OP_CASE_TYPE(SpvOpConvertUToF, result_type_id_list)
        OP_CASE_TYPE(SpvOpUConvert, result_type_id_list)
        OP_CASE_TYPE(SpvOpSConvert, result_type_id_list)
        OP_CASE_TYPE(SpvOpFConvert, result_type_id_list)
        OP_CASE_TYPE(SpvOpQuantizeToF16, result_type_id_list)
        OP_CASE_TYPE(SpvOpConvertPtrToU, result_type_id_list)
        OP_CASE_TYPE(SpvOpSatConvertSToU, result_type_id_list)
        OP_CASE_TYPE(SpvOpSatConvertUToS, result_type_id_list)
        OP_CASE_TYPE(SpvOpConvertUToPtr, result_type_id_list)
        OP_CASE_TYPE(SpvOpPtrCastToGeneric, result_type_id_list)
        OP_CASE_TYPE(SpvOpGenericCastToPtr, result_type_id_list)
        OP_CASE_TYPE(SpvOpGenericCastToPtrExplicit, result_type_id_list)
        OP_CASE_TYPE(SpvOpBitcast, result_type_id_list)

        // arithmetic instructions
        OP_CASE_TYPE(SpvOpSNegate, result_type_id_list)
        OP_CASE_TYPE(SpvOpFNegate, result_type_id_list)
        OP_CASE_TYPE(SpvOpIAdd, result_type_id_list)
        OP_CASE_TYPE(SpvOpFAdd, result_type_id_list)
        OP_CASE_TYPE(SpvOpISub, result_type_id_list)
        OP_CASE_TYPE(SpvOpFSub, result_type_id_list)
        OP_CASE_TYPE(SpvOpIMul, result_type_id_list)
        OP_CASE_TYPE(SpvOpFMul, result_type_id_list)
        OP_CASE_TYPE(SpvOpUDiv, result_type_id_list)
        OP_CASE_TYPE(SpvOpSDiv, result_type_id_list)
        OP_CASE_TYPE(SpvOpFDiv, result_type_id_list)
        OP_CASE_TYPE(SpvOpUMod, result_type_id_list)
        OP_CASE_TYPE(SpvOpSRem, result_type_id_list)
        OP_CASE_TYPE(SpvOpSMod, result_type_id_list)
        OP_CASE_TYPE(SpvOpFRem, result_type_id_list)
        OP_CASE_TYPE(SpvOpFMod, result_type_id_list)
        OP_CASE_TYPE(SpvOpVectorTimesScalar, result_type_id_list)
        OP_CASE_TYPE(SpvOpMatrixTimesScalar, result_type_id_list)
        OP_CASE_TYPE(SpvOpVectorTimesMatrix, result_type_id_list)
        OP_CASE_TYPE(SpvOpMatrixTimesVector, result_type_id_list)
        OP_CASE_TYPE(SpvOpMatrixTimesMatrix, result_type_id_list)
        OP_CASE_TYPE(SpvOpOuterProduct, result_type_id_list)
        OP_CASE_TYPE(SpvOpDot, result_type_id_list)
        OP_CASE_TYPE(SpvOpIAddCarry, result_type_id_list)
        OP_CASE_TYPE(SpvOpISubBorrow, result_type_id_list)
        OP_CASE_TYPE(SpvOpUMulExtended, result_type_id_list)
        OP_CASE_TYPE(SpvOpSMulExtended, result_type_id_list)

        // relational and logical instructions
        OP_CASE_TYPE(SpvOpAny, result_type_id_list)
        OP_CASE_TYPE(SpvOpAll, result_type_id_list)
        OP_CASE_TYPE(SpvOpIsNan, result_type_id_list)
        OP_CASE_TYPE(SpvOpIsInf, result_type_id_list)
        OP_CASE_TYPE(SpvOpIsFinite, result_type_id_list)
        OP_CASE_TYPE(SpvOpIsNormal, result_type_id_list)
        OP_CASE_TYPE(SpvOpSignBitSet, result_type_id_list)
        OP_CASE_TYPE(SpvOpLessOrGreater, result_type_id_list)
        OP_CASE_TYPE(SpvOpOrdered, result_type_id_list)
        OP_CASE_TYPE(SpvOpUnordered, result_type_id_list)
        OP_CASE_TYPE(SpvOpLogicalEqual, result_type_id_list)
        OP_CASE_TYPE(SpvOpLogicalNotEqual, result_type_id_list)
        OP_CASE_TYPE(SpvOpLogicalOr, result_type_id_list)
        OP_CASE_TYPE(SpvOpLogicalAnd, result_type_id_list)
        OP_CASE_TYPE(SpvOpLogicalNot, result_type_id_list)
        OP_CASE_TYPE(SpvOpSelect, result_type_id_list)
        OP_CASE_TYPE(SpvOpIEqual, result_type_id_list)
        OP_CASE_TYPE(SpvOpINotEqual, result_type_id_list)
        OP_CASE_TYPE(SpvOpUGreaterThan, result_type_id_list)
        OP_CASE_TYPE(SpvOpSGreaterThan, result_type_id_list)
        OP_CASE_TYPE(SpvOpUGreaterThanEqual, result_type_id_list)
        OP_CASE_TYPE(SpvOpSGreaterThanEqual, result_type_id_list)
        OP_CASE_TYPE(SpvOpULessThan, result_type_id_list)
        OP_CASE_TYPE(SpvOpSLessThan, result_type_id_list)
        OP_CASE_TYPE(SpvOpULessThanEqual, result_type_id_list)
        OP_CASE_TYPE(SpvOpSLessThanEqual, result_type_id_list)
        OP_CASE_TYPE(SpvOpFOrdEqual, result_type_id_list)
        OP_CASE_TYPE(SpvOpFUnordEqual, result_type_id_list)
        OP_CASE_TYPE(SpvOpFOrdNotEqual, result_type_id_list)
        OP_CASE_TYPE(SpvOpFUnordNotEqual, result_type_id_list)
        OP_CASE_TYPE(SpvOpFOrdLessThan, result_type_id_list)
        OP_CASE_TYPE(SpvOpFUnordLessThan, result_type_id_list)
        OP_CASE_TYPE(SpvOpFOrdGreaterThan, result_type_id_list)
        OP_CASE_TYPE(SpvOpFUnordGreaterThan, result_type_id_list)
        OP_CASE_TYPE(SpvOpFOrdLessThanEqual, result_type_id_list)
        OP_CASE_TYPE(SpvOpFUnordLessThanEqual, result_type_id_list)
        OP_CASE_TYPE(SpvOpFOrdGreaterThanEqual, result_type_id_list)
        OP_CASE_TYPE(SpvOpFUnordGreaterThanEqual, result_type_id_list)

        // bit instructions
        OP_CASE_TYPE(SpvOpShiftRightLogical, result_type_id_list)
        OP_CASE_TYPE(SpvOpShiftRightArithmetic, result_type_id_list)
        OP_CASE_TYPE(SpvOpShiftLeftLogical, result_type_id_list)
        OP_CASE_TYPE(SpvOpBitwiseOr, result_type_id_list)
        OP_CASE_TYPE(SpvOpBitwiseXor, result_type_id_list)
        OP_CASE_TYPE(SpvOpBitwiseAnd, result_type_id_list)
        OP_CASE_TYPE(SpvOpNot, result_type_id_list)
        OP_CASE_TYPE(SpvOpBitFieldInsert, result_type_id_list)
        OP_CASE_TYPE(SpvOpBitFieldSExtract, result_type_id_list)
        OP_CASE_TYPE(SpvOpBitFieldUExtract, result_type_id_list)
        OP_CASE_TYPE(SpvOpBitReverse, result_type_id_list)
        OP_CASE_TYPE(SpvOpBitCount, result_type_id_list)

        // derivative instructions
        OP_CASE_TYPE(SpvOpDPdx, result_type_id_list)
        OP_CASE_TYPE(SpvOpDPdy, result_type_id_list)
        OP_CASE_TYPE(SpvOpFwidth, result_type_id_list)
        OP_CASE_TYPE(SpvOpDPdxFine, result_type_id_list)
        OP_CASE_TYPE(SpvOpDPdyFine, result_type_id_list)
        OP_CASE_TYPE(SpvOpFwidthFine, result_type_id_list)
        OP_CASE_TYPE(SpvOpDPdxCoarse, result_type_id_list)
        OP_CASE_TYPE(SpvOpDPdyCoarse, result_type_id_list)
        OP_CASE_TYPE(SpvOpFwidthCoarse, result_type_id_list)

        // primitive instructions
        OP_DEFAULT(SpvOpEmitVertex)
        OP_DEFAULT(SpvOpEndPrimitive)
        OP_CASE_TYPE(SpvOpEmitStreamVertex, id_list)
        OP_CASE_TYPE(SpvOpEndStreamPrimitive, id_list)

        // barrier instructions
        OP_CASE_TYPE(SpvOpControlBarrier, id_list)
        OP_CASE_TYPE(SpvOpMemoryBarrier, id_list)
        OP_CASE_TYPE(SpvOpNamedBarrierInitialize, result_type_id_list)
        OP_CASE_TYPE(SpvOpMemoryNamedBarrier, id_list)

        // atomic instructions
        OP_CASE_TYPE(SpvOpAtomicLoad, result_type_id_list)
        OP_CASE_TYPE(SpvOpAtomicStore, id_list)
        OP_CASE_TYPE(SpvOpAtomicExchange, result_type_id_list)
        OP_CASE_TYPE(SpvOpAtomicCompareExchange, result_type_id_list)
        OP_CASE_TYPE(SpvOpAtomicCompareExchangeWeak, result_type_id_list)
        OP_CASE_TYPE(SpvOpAtomicIIncrement, result_type_id_list)
        OP_CASE_TYPE(SpvOpAtomicIDecrement, result_type_id_list)
        OP_CASE_TYPE(SpvOpAtomicIAdd, result_type_id_list)
        OP_CASE_TYPE(SpvOpAtomicISub, result_type_id_list)
        OP_CASE_TYPE(SpvOpAtomicSMin, result_type_id_list)
        OP_CASE_TYPE(SpvOpAtomicUMin, result_type_id_list)
        OP_CASE_TYPE(SpvOpAtomicSMax, result_type_id_list)
        OP_CASE_TYPE(SpvOpAtomicUMax, result_type_id_list)
        OP_CASE_TYPE(SpvOpAtomicAnd, result_type_id_list)
        OP_CASE_TYPE(SpvOpAtomicOr, result_type_id_list)
        OP_CASE_TYPE(SpvOpAtomicXor, result_type_id_list)
        OP_CASE_TYPE(SpvOpAtomicFlagTestAndSet, result_type_id_list)
        OP_CASE_TYPE(SpvOpAtomicFlagClear, id_list)

        // control-flow instructions
        OP_CASE_TYPE(SpvOpPhi, result_type_id_list)
        OP_CASE_SPECIAL(SpvOpLoopMerge)
        OP_CASE_SPECIAL(SpvOpSelectionMerge)
        OP_CASE_SPECIAL(SpvOpLabel)
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
        OP_CASE_TYPE(SpvOpGroupAsyncCopy, result_type_id_list)
        OP_CASE_TYPE(SpvOpGroupWaitEvents, id_list)
        OP_CASE_TYPE(SpvOpGroupAll, result_type_id_list)
        OP_CASE_TYPE(SpvOpGroupAny, result_type_id_list)
        OP_CASE_TYPE(SpvOpGroupBroadcast, result_type_id_list)
        OP_CASE_TYPE(SpvOpGroupIAdd, id_groupop_id)
        OP_CASE_TYPE(SpvOpGroupFAdd, id_groupop_id)
        OP_CASE_TYPE(SpvOpGroupFMin, id_groupop_id)
        OP_CASE_TYPE(SpvOpGroupUMin, id_groupop_id)
        OP_CASE_TYPE(SpvOpGroupSMin, id_groupop_id)
        OP_CASE_TYPE(SpvOpGroupFMax, id_groupop_id)
        OP_CASE_TYPE(SpvOpGroupUMax, id_groupop_id)
        OP_CASE_TYPE(SpvOpGroupSMax, id_groupop_id)

        // pipe instructions
        OP_CASE_TYPE(SpvOpReadPipe, result_type_id_list)
        OP_CASE_TYPE(SpvOpWritePipe, result_type_id_list)
        OP_CASE_TYPE(SpvOpReservedReadPipe, result_type_id_list)
        OP_CASE_TYPE(SpvOpReservedWritePipe, result_type_id_list)
        OP_CASE_TYPE(SpvOpReserveReadPipePackets, result_type_id_list)
        OP_CASE_TYPE(SpvOpReserveWritePipePackets, result_type_id_list)
        OP_CASE_TYPE(SpvOpCommitReadPipe, id_list)
        OP_CASE_TYPE(SpvOpCommitWritePipe, id_list)
        OP_CASE_TYPE(SpvOpIsValidReserveId, result_type_id_list)
        OP_CASE_TYPE(SpvOpGetNumPipePackets, result_type_id_list)
        OP_CASE_TYPE(SpvOpGetMaxPipePackets, result_type_id_list)
        OP_CASE_TYPE(SpvOpGroupReserveReadPipePackets, result_type_id_list)
        OP_CASE_TYPE(SpvOpGroupReserveWritePipePackets, result_type_id_list)
        OP_CASE_TYPE(SpvOpGroupCommitReadPipe, id_list)
        OP_CASE_TYPE(SpvOpGroupCommitWritePipe, id_list)
        OP_CASE_TYPE(SpvOpConstantPipeStorage, result_type_number_list)
        OP_CASE_TYPE(SpvOpCreatePipeFromPipeStorage, result_type_id_list)

        // device-side enqueue instructions
        OP_CASE_TYPE(SpvOpEnqueueMarker, result_type_id_list)
        OP_CASE_TYPE(SpvOpEnqueueKernel, result_type_id_list)
        OP_CASE_TYPE(SpvOpGetKernelNDrangeSubGroupCount, result_type_id_list)
        OP_CASE_TYPE(SpvOpGetKernelNDrangeMaxSubGroupSize, result_type_id_list)
        OP_CASE_TYPE(SpvOpGetKernelWorkGroupSize, result_type_id_list)
        OP_CASE_TYPE(SpvOpGetKernelPreferredWorkGroupSizeMultiple, result_type_id_list)
        OP_CASE_TYPE(SpvOpRetainEvent, id_list)
        OP_CASE_TYPE(SpvOpReleaseEvent, id_list)
        OP_CASE_TYPE(SpvOpCreateUserEvent, result_type_id_list)
        OP_CASE_TYPE(SpvOpIsValidEvent, result_type_id_list)
        OP_CASE_TYPE(SpvOpSetUserEventStatus, id_list)
        OP_CASE_TYPE(SpvOpCaptureEventProfilingInfo, id_list)
        OP_CASE_TYPE(SpvOpGetDefaultQueue, result_type_id_list)
        OP_CASE_TYPE(SpvOpBuildNDRange, result_type_id_list)
        OP_CASE_TYPE(SpvOpGetKernelLocalSizeForSubgroupCount, result_type_id_list)
        OP_CASE_TYPE(SpvOpGetKernelMaxNumSubgroups, result_type_id_list)

        default :
            line = spirv_string_opcode_no_result(module, opcode->op.kind);
    }

    if (spans) {
        *spans = module->text->spans;
    }

    return line;

#undef OP_CASE_SPECIAL
#undef OP_CASE_TYPE
#undef OP_CASE_TYPE_1
#undef OP_DEFAULT
}
