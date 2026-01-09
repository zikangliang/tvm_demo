/**
 * @file tvmrt_semantic.c
 * @brief Semantic Transformation Layer Implementation
 */

#include "tvmrt_semantic.h"
#include <string.h>

// ============================================================
// Helper: Resolve SID to pointer
// ============================================================

void* tvmrt_semantic_resolve_sid(
    const tvmrt_tensor_map_entry_t* tensor_map,
    int32_t tensor_count,
    uint8_t* workspace,
    int32_t sid
) {
    if (!tensor_map || !workspace || sid < 0) {
        return NULL;
    }
    
    // Linear search (could be optimized with direct indexing if sid == array index)
    for (int32_t i = 0; i < tensor_count; i++) {
        if (tensor_map[i].sid == sid) {
            return workspace + tensor_map[i].offset;
        }
    }
    
    return NULL;
}

// ============================================================
// Main Initialization
// ============================================================

int tvmrt_semantic_init(
    tvmrt_context_t* ctx,
    const tvmrt_model_desc_t* model,
    void** inputs,
    void** outputs,
    uint8_t* workspace,
    const uint8_t* const_workspace,
    tvmrt_op_exec_t* op_execs,
    void* args_storage
) {
    if (!ctx || !model || !workspace || !op_execs) {
        return -1;
    }
    
    // Initialize context
    ctx->workspace = workspace;
    ctx->const_workspace = const_workspace;
    ctx->op_execs = op_execs;
    ctx->op_count = model->op_count;
    ctx->args_storage = args_storage;
    
    // Fill operator entries from descriptors
    for (int32_t i = 0; i < model->op_count; i++) {
        const tvmrt_op_desc_t* desc = &model->op_descs[i];
        tvmrt_op_exec_t* exec = &op_execs[i];
        
        exec->name = desc->name;
        
        // Bind function pointer based on backend type
        switch (desc->backend) {
            case TVMRT_BACKEND_CPU:
                if (desc->func_entry_id >= 0 && 
                    desc->func_entry_id < model->cpu_func_count) {
                    exec->func = model->cpu_func_table[desc->func_entry_id];
                } else {
                    exec->func = NULL;
                }
                break;
                
            case TVMRT_BACKEND_NPU:
            case TVMRT_BACKEND_GPU:
                // Reserved for future implementation
                exec->func = NULL;
                break;
        }
        
        // Note: args filling is model-specific and should be done
        // by the generated model_desc.c code or a custom init function
        exec->args = NULL;
    }
    
    // Suppress unused parameter warnings for future use
    (void)inputs;
    (void)outputs;
    (void)args_storage;
    
    return 0;
}
