/**
 * @file main.c
 * @brief TVM Runtime 统一入口
 * 
 * 合并自 main.c + default_lib0.c + default_lib1.c
 */

#include "tvmrt.h"
#include "tvmgen_default.h"
#include <stdio.h>
#include <stdbool.h>

// ============================================================
// Workspace 分配 (原 default_lib0.c)
// ============================================================

static struct global_const_workspace {
    float fused_constant_4_let[1] __attribute__((aligned(16)));
    float fused_constant_3_let[1] __attribute__((packed, aligned(16)));
    float fused_constant_2_let[1] __attribute__((packed, aligned(16)));
    float fused_constant_1_let[1] __attribute__((packed, aligned(16)));
    float fused_constant_let[1] __attribute__((packed, aligned(16)));
} global_const_workspace = {
    .fused_constant_4_let = {0x1.4p+2},  // 5.0
    .fused_constant_3_let = {0x1p+2},    // 4.0
    .fused_constant_2_let = {0x1.8p+1},  // 3.0
    .fused_constant_1_let = {0x1p+1},    // 2.0
    .fused_constant_let = {0x1p+0},      // 1.0
};

__attribute__((aligned(16)))
static uint8_t global_workspace[36];

// ============================================================
// 模型数据接口声明 (定义在 model_data.c)
// ============================================================

#define MODEL_NUM_OPS 6

extern const tvmrt_model_desc_t* model_get_descriptor(void);
extern const tvmrt_schedule_desc_t* model_get_schedule(void);
extern int model_fill_args(void* args, float* input, float* output, 
                           uint8_t* workspace, const uint8_t* const_workspace);
extern void* model_get_op_args(int32_t op_id);

// ============================================================
// Runtime 初始化 (原 default_lib1.c)
// ============================================================

static tvmrt_op_exec_t g_op_execs[MODEL_NUM_OPS];
static bool g_engine_initialized = false;

static int init_op_execs(
    float* input,
    float* output,
    uint8_t* workspace,
    const uint8_t* const_workspace
) {
    const tvmrt_model_desc_t* model = model_get_descriptor();
    
    // 填充参数
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

int32_t tvmgen_default___tvm_main__(
    void* input_buffer_var,
    void* output_buffer_var,
    uint8_t* global_const_workspace_0_var,
    uint8_t* global_workspace_1_var
) {
    // 初始化引擎
    if (!g_engine_initialized) {
        if (tvmrt_engine_init() != 0) {
            return -1;
        }
        g_engine_initialized = true;
    }
    
    // 初始化算子条目
    init_op_execs(
        (float*)input_buffer_var,
        (float*)output_buffer_var,
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

int32_t tvmgen_default_run(
    struct tvmgen_default_inputs* inputs,
    struct tvmgen_default_outputs* outputs
) {
    return tvmgen_default___tvm_main__(
        inputs->input,
        outputs->output,
        (uint8_t*)&global_const_workspace,
        (uint8_t*)&global_workspace
    );
}

// ============================================================
// 测试入口 (原 main.c)
// ============================================================

int main(void) {
    // 准备数据
    float input_data[1] = {10.0f};
    float output_data[1] = {0.0f};
    
    struct tvmgen_default_inputs inputs = {.input = input_data};
    struct tvmgen_default_outputs outputs = {.output = output_data};
    
    printf("验证开始...\\n");
    printf("输入值: %f\\n", input_data[0]);
    
    // 运行推理
    int32_t ret = tvmgen_default_run(&inputs, &outputs);
    
    if (ret == 0) {
        printf("执行成功！\\n");
        printf("输出值: %f (预期: 23.000000)\\n", output_data[0]);
    } else {
        printf("执行出错，错误码: %d\\n", ret);
    }
    
    return 0;
}