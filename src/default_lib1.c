/**
 * @file default_lib1.c
 * @brief 模块化 TVM Runtime 入口（使用简化 Runtime）
 * 
 * 本文件使用简化的 Runtime 架构（tvmrt.h）。
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Runtime 统一头文件
#include "tvmrt.h"

// 模型数据接口（定义在 model_data.c）
#define MODEL_NUM_OPS 6

extern const tvmrt_model_desc_t* model_get_descriptor(void);
extern const tvmrt_schedule_desc_t* model_get_schedule(void);
extern int model_fill_args(void* args, float* input, float* output, 
                           uint8_t* workspace, const uint8_t* const_workspace);
extern void* model_get_op_args(int32_t op_id);

// ============================================================
// 外部声明
// ============================================================
extern void* model_get_op_args(int32_t op_id);

// ============================================================
// 静态算子执行表
// ============================================================
static tvmrt_op_exec_t g_op_execs[MODEL_NUM_OPS];

// 初始化标志
static bool g_engine_initialized = false;

// ============================================================
// 用正确的函数指针和参数初始化算子条目
// ============================================================
static int init_op_execs(
    float* input,
    float* output,
    uint8_t* workspace,
    const uint8_t* const_workspace
) {
    const tvmrt_model_desc_t* model = model_get_descriptor();
    
    // 先填充参数
    model_fill_args(NULL, input, output, workspace, (uint8_t*)const_workspace);
    
    // 设置执行条目
    for (int32_t i = 0; i < MODEL_NUM_OPS; i++) {
        const tvmrt_op_desc_t* desc = &model->op_descs[i];
        g_op_execs[i].name = desc->name;
        g_op_execs[i].func = model->cpu_func_table[desc->func_entry_id];
        g_op_execs[i].args = model_get_op_args(i);
    }
    
    return 0;
}

// ============================================================
// 主入口 - 使用模块化 Runtime
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
    // 如需初始化引擎
    if (!g_engine_initialized) {
        if (tvmrt_engine_init() != 0) {
            return -1;
        }
        g_engine_initialized = true;
    }
    
    // 初始化算子条目
    init_op_execs(
        input_buffer_var,
        output_buffer_var,
        global_workspace_1_var,
        global_const_workspace_0_var
    );
    
    // 创建运行时上下文
    tvmrt_context_t ctx = {
        .workspace = global_workspace_1_var,
        .const_workspace = global_const_workspace_0_var,
        .op_execs = g_op_execs,
        .op_count = MODEL_NUM_OPS,
        .args_storage = NULL
    };
    
    // 获取调度表并运行
    const tvmrt_schedule_desc_t* schedule = model_get_schedule();
    int32_t ret = tvmrt_engine_run(&ctx, schedule);
    
    return ret;
}
