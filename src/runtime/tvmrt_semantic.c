/**
 * @file tvmrt_semantic.c
 * @brief 语义转换层实现
 */

#include "tvmrt_semantic.h"
#include <string.h>

// ============================================================
// 辅助函数: 将 SID 解析为指针
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
    
    // 线性搜索 (如果 sid == 数组索引，可优化为直接索引)
    for (int32_t i = 0; i < tensor_count; i++) {
        if (tensor_map[i].sid == sid) {
            return workspace + tensor_map[i].offset;
        }
    }
    
    return NULL;
}

// ============================================================
// 主初始化函数
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
    
    // 初始化上下文
    ctx->workspace = workspace;
    ctx->const_workspace = const_workspace;
    ctx->op_execs = op_execs;
    ctx->op_count = model->op_count;
    ctx->args_storage = args_storage;
    
    // 从描述符填充算子条目
    for (int32_t i = 0; i < model->op_count; i++) {
        const tvmrt_op_desc_t* desc = &model->op_descs[i];
        tvmrt_op_exec_t* exec = &op_execs[i];
        
        exec->name = desc->name;
        
        // 根据后端类型绑定函数指针
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
                // 预留给未来实现
                exec->func = NULL;
                break;
        }
        
        // 注意: args 填充是模型特定的，应由生成的
        // model_desc.c 代码或自定义初始化函数完成
        exec->args = NULL;
    }
    
    // 抑制未使用参数警告，留待未来使用
    (void)inputs;
    (void)outputs;
    (void)args_storage;
    
    return 0;
}
