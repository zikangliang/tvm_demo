/**
 * @file model_data.c
 * @brief 模型描述符实现 (编译期生成)
 * 
 * 本文件包含通常由 TVM 编译器自动生成的静态描述表。包括：
 * - 张量内存映射表
 * - 算子描述表
 * - 静态 BSP 调度表
 * - CPU 函数表
 */

#include "tvmrt.h"
#include <stddef.h>

// ============================================================
// 模型特定常量
// ============================================================

#define MODEL_NUM_TENSORS  5
#define MODEL_NUM_OPS      6
#define MODEL_NUM_LAYERS   4

// ============================================================
// 参数结构体 (模型特定)
// ============================================================

/** 单输入 fused_add 算子的参数结构 */
typedef struct {
    float* p0;
    float* output;
    uint8_t* const_ws;
    uint8_t* ws;
} FusedAddArgs;

/** 双输入 fused_add_3 算子的参数结构 */
typedef struct {
    float* p0;
    float* p1;
    float* output;
    uint8_t* const_ws;
    uint8_t* ws;
} FusedAdd3Args;

// ============================================================
// 包装函数前向声明
// ============================================================
extern int32_t wrapped_fused_add(void* args);
extern int32_t wrapped_fused_add_1(void* args);
extern int32_t wrapped_fused_add_2(void* args);
extern int32_t wrapped_fused_add_3(void* args);
extern int32_t wrapped_fused_subtract(void* args);
extern int32_t wrapped_fused_subtract_1(void* args);

// ============================================================
// 张量内存映射表
// ============================================================
// 描述每个存储 ID (sid) 在 workspace 中的位置
// 注意：sid_4 和 sid_5 分别复用 sid_1 和 sid_3 的内存

static const tvmrt_tensor_map_entry_t g_tensor_map[MODEL_NUM_TENSORS] = {
    // sid, offset, size, align
    {.sid = 1, .offset = 16, .size = 4, .align = 4},  // sid_1 在 ws[16]
    {.sid = 2, .offset = 0,  .size = 4, .align = 4},  // sid_2 在 ws[0]
    {.sid = 3, .offset = 32, .size = 4, .align = 4},  // sid_3 在 ws[32]
    {.sid = 4, .offset = 16, .size = 4, .align = 4},  // sid_4 复用 ws[16] (同 sid_1)
    {.sid = 5, .offset = 32, .size = 4, .align = 4}   // sid_5 复用 ws[32] (同 sid_3)
};

// ============================================================
// 算子描述表
// ============================================================
// 描述图中的每个算子

static const tvmrt_op_desc_t g_op_descs[MODEL_NUM_OPS] = {
    // Node 0: fused_add(input -> sid_1)
    {
        .op_id = 0,
        .name = "fused_add_0",
        .backend = TVMRT_BACKEND_CPU,
        .func_entry_id = 0,  // wrapped_fused_add
        .input_sids = {-1, -1, -1, -1},   // 输入是外部的
        .output_sids = {1, -1},            // 产生 sid_1
        .input_count = 1,
        .output_count = 1
    },
    // Node 1: fused_subtract(sid_1 -> sid_2)
    {
        .op_id = 1,
        .name = "fused_subtract_0",
        .backend = TVMRT_BACKEND_CPU,
        .func_entry_id = 4,  // wrapped_fused_subtract
        .input_sids = {1, -1, -1, -1},
        .output_sids = {2, -1},
        .input_count = 1,
        .output_count = 1
    },
    // Node 2: fused_add_1(input -> sid_3)
    {
        .op_id = 2,
        .name = "fused_add_1",
        .backend = TVMRT_BACKEND_CPU,
        .func_entry_id = 1,  // wrapped_fused_add_1
        .input_sids = {-1, -1, -1, -1},
        .output_sids = {3, -1},
        .input_count = 1,
        .output_count = 1
    },
    // Node 3: fused_subtract_1(sid_3 -> sid_4)
    {
        .op_id = 3,
        .name = "fused_subtract_1",
        .backend = TVMRT_BACKEND_CPU,
        .func_entry_id = 5,  // wrapped_fused_subtract_1
        .input_sids = {3, -1, -1, -1},
        .output_sids = {4, -1},
        .input_count = 1,
        .output_count = 1
    },
    // Node 4: fused_add_2(sid_4 -> sid_5)
    {
        .op_id = 4,
        .name = "fused_add_2",
        .backend = TVMRT_BACKEND_CPU,
        .func_entry_id = 2,  // wrapped_fused_add_2
        .input_sids = {4, -1, -1, -1},
        .output_sids = {5, -1},
        .input_count = 1,
        .output_count = 1
    },
    // Node 5: fused_add_3(sid_2, sid_5 -> output)
    {
        .op_id = 5,
        .name = "fused_add_3",
        .backend = TVMRT_BACKEND_CPU,
        .func_entry_id = 3,  // wrapped_fused_add_3
        .input_sids = {2, 5, -1, -1},
        .output_sids = {-1, -1},  // 输出是外部的
        .input_count = 2,
        .output_count = 1
    }
};

// ============================================================
// CPU 函数表
// ============================================================

static const tvmrt_op_func_t g_cpu_func_table[] = {
    wrapped_fused_add,       // 索引 0
    wrapped_fused_add_1,     // 索引 1
    wrapped_fused_add_2,     // 索引 2
    wrapped_fused_add_3,     // 索引 3
    wrapped_fused_subtract,  // 索引 4
    wrapped_fused_subtract_1 // 索引 5
};

#define CPU_FUNC_COUNT (sizeof(g_cpu_func_table) / sizeof(g_cpu_func_table[0]))

// ============================================================
// 静态 BSP 调度表
// ============================================================

// 第1层: Node 0, Node 2 (并行，无依赖)
static const int32_t g_layer1_ops[] = {0, 2};

// 第2层: Node 1, Node 3 (并行，依赖第1层)
static const int32_t g_layer2_ops[] = {1, 3};

// 第3层: Node 4 (串行，依赖 Node 3)
static const int32_t g_layer3_ops[] = {4};

// 第4层: Node 5 (串行，依赖 Node 1 和 Node 4)
static const int32_t g_layer4_ops[] = {5};

static const tvmrt_schedule_layer_t g_schedule_layers[MODEL_NUM_LAYERS] = {
    {.op_indices = g_layer1_ops, .count = 2},
    {.op_indices = g_layer2_ops, .count = 2},
    {.op_indices = g_layer3_ops, .count = 1},
    {.op_indices = g_layer4_ops, .count = 1}
};

static const tvmrt_schedule_desc_t g_schedule = {
    .layers = g_schedule_layers,
    .layer_count = MODEL_NUM_LAYERS
};

// ============================================================
// 完整模型描述符
// ============================================================

static const tvmrt_model_desc_t g_model_desc = {
    .tensor_map = g_tensor_map,
    .tensor_count = MODEL_NUM_TENSORS,
    .op_descs = g_op_descs,
    .op_count = MODEL_NUM_OPS,
    .schedule = &g_schedule,
    .cpu_func_table = g_cpu_func_table,
    .cpu_func_count = CPU_FUNC_COUNT
};

// ============================================================
// 访问函数
// ============================================================

const tvmrt_model_desc_t* model_get_descriptor(void) {
    return &g_model_desc;
}

const tvmrt_tensor_map_entry_t* model_get_tensor_map(void) {
    return g_tensor_map;
}

const tvmrt_op_desc_t* model_get_op_descs(void) {
    return g_op_descs;
}

const tvmrt_schedule_desc_t* model_get_schedule(void) {
    return &g_schedule;
}

// ============================================================
// 参数填充辅助函数
// ============================================================

// 算子参数存储 (静态分配)
static FusedAddArgs g_fused_add_args[5];    // 用于算子 0-4
static FusedAdd3Args g_fused_add3_args;     // 用于算子 5

int model_fill_args(
    void* args,
    float* input,
    float* output,
    uint8_t* workspace,
    const uint8_t* const_workspace
) {
    (void)args;  // 使用静态存储代替
    
    // 解析 SID 指针
    float* sid_1 = (float*)(workspace + 16);
    float* sid_2 = (float*)(workspace + 0);
    float* sid_3 = (float*)(workspace + 32);
    float* sid_4 = (float*)(workspace + 16);  // 复用 sid_1
    float* sid_5 = (float*)(workspace + 32);  // 复用 sid_3
    
    // 为每个算子填充参数
    // Node 0: fused_add(input -> sid_1)
    g_fused_add_args[0] = (FusedAddArgs){
        .p0 = input,
        .output = sid_1,
        .const_ws = (uint8_t*)const_workspace,
        .ws = workspace
    };
    
    // Node 1: fused_subtract(sid_1 -> sid_2)
    g_fused_add_args[1] = (FusedAddArgs){
        .p0 = sid_1,
        .output = sid_2,
        .const_ws = (uint8_t*)const_workspace,
        .ws = workspace
    };
    
    // Node 2: fused_add_1(input -> sid_3)
    g_fused_add_args[2] = (FusedAddArgs){
        .p0 = input,
        .output = sid_3,
        .const_ws = (uint8_t*)const_workspace,
        .ws = workspace
    };
    
    // Node 3: fused_subtract_1(sid_3 -> sid_4)
    g_fused_add_args[3] = (FusedAddArgs){
        .p0 = sid_3,
        .output = sid_4,
        .const_ws = (uint8_t*)const_workspace,
        .ws = workspace
    };
    
    // Node 4: fused_add_2(sid_4 -> sid_5)
    g_fused_add_args[4] = (FusedAddArgs){
        .p0 = sid_4,
        .output = sid_5,
        .const_ws = (uint8_t*)const_workspace,
        .ws = workspace
    };
    
    // Node 5: fused_add_3(sid_2, sid_5 -> output)
    g_fused_add3_args = (FusedAdd3Args){
        .p0 = sid_2,
        .p1 = sid_5,
        .output = output,
        .const_ws = (uint8_t*)const_workspace,
        .ws = workspace
    };
    
    return 0;
}

// ============================================================
// 获取指定算子的参数指针
// ============================================================

void* model_get_op_args(int32_t op_id) {
    if (op_id < 0 || op_id >= MODEL_NUM_OPS) {
        return NULL;
    }
    // 0 - 4 号算子为 fused_add 有 4 个参数
    // 5 号算子为 fused_add3 有 5 个参数
    if (op_id < 5) {
        return &g_fused_add_args[op_id];
    } else {
        return &g_fused_add3_args;
    }
}
