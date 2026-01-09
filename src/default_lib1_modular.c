/**
 * @file default_lib1_modular.c
 * @brief Modular TVM Runtime Entry Point (Step 7)
 * 
 * This file replaces the monolithic default_lib1.c with a modular version
 * that uses the new runtime architecture.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Runtime modules
#include "runtime/tvmrt_types.h"
#include "runtime/tvmrt_engine.h"
#include "runtime/tvmrt_log.h"

// Model descriptor
#include "model/model_desc.h"

// ============================================================
// External declarations
// ============================================================
extern void* model_get_op_args(int32_t op_id);

// ============================================================
// Static operator execution entries
// ============================================================
static tvmrt_op_exec_t g_op_execs[MODEL_NUM_OPS];

// Flag to track initialization
static bool g_engine_initialized = false;

// ============================================================
// Initialize operator entries with correct function pointers and args
// ============================================================
static int init_op_execs(
    float* input,
    float* output,
    uint8_t* workspace,
    const uint8_t* const_workspace
) {
    const tvmrt_model_desc_t* model = model_get_descriptor();
    
    // Fill args first
    model_fill_args(NULL, input, output, workspace, (uint8_t*)const_workspace);
    
    // Set up execution entries
    for (int32_t i = 0; i < MODEL_NUM_OPS; i++) {
        const tvmrt_op_desc_t* desc = &model->op_descs[i];
        g_op_execs[i].name = desc->name;
        g_op_execs[i].func = model->cpu_func_table[desc->func_entry_id];
        g_op_execs[i].args = model_get_op_args(i);
    }
    
    return 0;
}

// ============================================================
// Main entry point - uses modular runtime
// ============================================================
#ifdef __cplusplus
extern "C"
#endif
int32_t tvmgen_default___tvm_main__(
    float* input_buffer_var,
    float* output_buffer_var,
    uint8_t* global_const_workspace_0_var,
    uint8_t* global_workspace_1_var
) {
    // Initialize engine if needed
    if (!g_engine_initialized) {
        if (tvmrt_engine_init() != 0) {
            return -1;
        }
        g_engine_initialized = true;
    }
    
    // Initialize operator entries
    init_op_execs(
        input_buffer_var,
        output_buffer_var,
        global_workspace_1_var,
        global_const_workspace_0_var
    );
    
    // Create runtime context
    tvmrt_context_t ctx = {
        .workspace = global_workspace_1_var,
        .const_workspace = global_const_workspace_0_var,
        .op_execs = g_op_execs,
        .op_count = MODEL_NUM_OPS,
        .args_storage = NULL
    };
    
    // Get schedule and run
    const tvmrt_schedule_desc_t* schedule = model_get_schedule();
    int32_t ret = tvmrt_engine_run(&ctx, schedule);
    
    return ret;
}
