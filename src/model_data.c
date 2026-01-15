/**
 * @file model_data.c
 * @brief 压力测试模型描述符 - 16算子/8层/8内存槽
 *
 * 本文件实现一个复杂的算子调度图，用于验证:
 * - 内存复用正确性
 * - BSP 调度顺序
 * - 并行执行安全性
 *
 * 输入: 10.0 → 输出: 235.0
 */

#include "tvmrt.h"
#include <stddef.h>

// ============================================================
// 模型特定常量
// ============================================================

#define MODEL_NUM_TENSORS 12 // 使用的 SID 数量
#define MODEL_NUM_OPS 16     // 算子总数
#define MODEL_NUM_LAYERS 9   // 层数 (Layer 8 拆成两层)

// ============================================================
// 参数结构体 (复用 ops.c 中定义的)
// ============================================================

typedef struct {
  float *p0;
  float *output;
  uint8_t *const_ws;
  uint8_t *ws;
} FusedAddArgs;

typedef struct {
  float *p0;
  float *p1;
  float *output;
  uint8_t *const_ws;
  uint8_t *ws;
} FusedAdd3Args;

// ============================================================
// 包装函数前向声明
// ============================================================
extern int32_t wrapped_fused_add(void *args);        // p0 + 1
extern int32_t wrapped_fused_add_1(void *args);      // p0 + 3
extern int32_t wrapped_fused_add_2(void *args);      // p0 + 5
extern int32_t wrapped_fused_add_3(void *args);      // p0 + p1
extern int32_t wrapped_fused_subtract(void *args);   // p0 - 2
extern int32_t wrapped_fused_subtract_1(void *args); // p0 - 4
// Phase 1: 激活函数
extern int32_t wrapped_relu(void *args);    // max(0, x)
extern int32_t wrapped_sigmoid(void *args); // 1/(1+exp(-x))
extern int32_t wrapped_tanh_op(void *args); // tanh(x)
extern int32_t wrapped_relu6(void *args);   // min(max(0,x), 6)
// Phase 2: 基础运算
extern int32_t wrapped_multiply(void *args); // p0 * p1
extern int32_t wrapped_maximum(void *args);  // max(p0, p1)
extern int32_t wrapped_minimum(void *args);  // min(p0, p1)
// Phase 3: 常量乘法
extern int32_t wrapped_mul_2(void *args);    // p0 * 2
extern int32_t wrapped_mul_half(void *args); // p0 * 0.5

// ============================================================
// 张量内存映射表 (8槽, 8字节对齐)
// ============================================================
// M0=ws[0], M1=ws[8], M2=ws[16], M3=ws[24]
// M4=ws[32], M5=ws[40], M6=ws[48], M7=ws[56]

static const tvmrt_tensor_map_entry_t g_model_tensor_map[MODEL_NUM_TENSORS] = {
    // 第一批 SID (Layer 1 产生)
    {.sid = 1, .offset = 0, .size = 4, .align = 4},  // M0
    {.sid = 2, .offset = 8, .size = 4, .align = 4},  // M1
    {.sid = 3, .offset = 16, .size = 4, .align = 4}, // M2
    {.sid = 4,
     .offset = 24,
     .size = 4,
     .align = 4}, // M3
                  // 第二批 SID (Layer 2 产生)
    {.sid = 5, .offset = 32, .size = 4, .align = 4},  // M4
    {.sid = 6, .offset = 40, .size = 4, .align = 4},  // M5
                                                      // 复用的 SID (Layer 3+)
    {.sid = 7, .offset = 0, .size = 4, .align = 4},   // M0 复用
    {.sid = 8, .offset = 8, .size = 4, .align = 4},   // M1 复用
    {.sid = 9, .offset = 48, .size = 4, .align = 4},  // M6
    {.sid = 10, .offset = 16, .size = 4, .align = 4}, // M2 复用
    {.sid = 11, .offset = 24, .size = 4, .align = 4}, // M3 复用
    {.sid = 12, .offset = 56, .size = 4, .align = 4}, // M7
};

// ============================================================
// 算子描述表 (16个算子)
// ============================================================
/*
 * 计算流程:
 * L1: Op0-3: input + [1,3,5,1] → M0-M3 = [11,13,15,11]
 * L2: Op4-5: M0+M1→M4=24, M2+M3→M5=26
 * L3: Op6-7: M4-2→M0=22, M5-4→M1=22
 * L4: Op8:   M0+M1→M6=44
 * L5: Op9-10: M6+3→M2=47, M6+5→M3=49
 * L6: Op11:  M2+M3→M7=96
 * L7: Op12-13: M7-2→M4=94, M6+1→M5=45
 * L8: Op14-15: M4+M5→M0=139, M0+M7→output=235
 */

static const tvmrt_op_desc_t g_model_op_descs[MODEL_NUM_OPS] = {
    // Layer 1: 4路分叉
    {.op_id = 0,
     .name = "L1_add_0",
     .backend = TVMRT_BACKEND_CPU,
     .func_entry_id = 0,
     .input_sids = {-1, -1, -1, -1},
     .output_sids = {1, -1},
     .input_count = 1,
     .output_count = 1},
    {.op_id = 1,
     .name = "L1_add_1",
     .backend = TVMRT_BACKEND_CPU,
     .func_entry_id = 1,
     .input_sids = {-1, -1, -1, -1},
     .output_sids = {2, -1},
     .input_count = 1,
     .output_count = 1},
    {.op_id = 2,
     .name = "L1_add_2",
     .backend = TVMRT_BACKEND_CPU,
     .func_entry_id = 2,
     .input_sids = {-1, -1, -1, -1},
     .output_sids = {3, -1},
     .input_count = 1,
     .output_count = 1},
    {.op_id = 3,
     .name = "L1_add_3",
     .backend = TVMRT_BACKEND_CPU,
     .func_entry_id = 0,
     .input_sids = {-1, -1, -1, -1},
     .output_sids = {4, -1},
     .input_count = 1,
     .output_count = 1},

    // Layer 2: 两两合并
    {.op_id = 4,
     .name = "L2_add3_0",
     .backend = TVMRT_BACKEND_CPU,
     .func_entry_id = 3,
     .input_sids = {1, 2, -1, -1},
     .output_sids = {5, -1},
     .input_count = 2,
     .output_count = 1},
    {.op_id = 5,
     .name = "L2_add3_1",
     .backend = TVMRT_BACKEND_CPU,
     .func_entry_id = 3,
     .input_sids = {3, 4, -1, -1},
     .output_sids = {6, -1},
     .input_count = 2,
     .output_count = 1},

    // Layer 3: 变换 (复用 M0, M1)
    {.op_id = 6,
     .name = "L3_sub_0",
     .backend = TVMRT_BACKEND_CPU,
     .func_entry_id = 4,
     .input_sids = {5, -1, -1, -1},
     .output_sids = {7, -1},
     .input_count = 1,
     .output_count = 1},
    {.op_id = 7,
     .name = "L3_sub_1",
     .backend = TVMRT_BACKEND_CPU,
     .func_entry_id = 5,
     .input_sids = {6, -1, -1, -1},
     .output_sids = {8, -1},
     .input_count = 1,
     .output_count = 1},

    // Layer 4: 合并到 M6
    {.op_id = 8,
     .name = "L4_add3",
     .backend = TVMRT_BACKEND_CPU,
     .func_entry_id = 3,
     .input_sids = {7, 8, -1, -1},
     .output_sids = {9, -1},
     .input_count = 2,
     .output_count = 1},

    // Layer 5: 累加链 (复用 M2, M3)
    {.op_id = 9,
     .name = "L5_add1_0",
     .backend = TVMRT_BACKEND_CPU,
     .func_entry_id = 1,
     .input_sids = {9, -1, -1, -1},
     .output_sids = {10, -1},
     .input_count = 1,
     .output_count = 1},
    {.op_id = 10,
     .name = "L5_add2_1",
     .backend = TVMRT_BACKEND_CPU,
     .func_entry_id = 2,
     .input_sids = {9, -1, -1, -1},
     .output_sids = {11, -1},
     .input_count = 1,
     .output_count = 1},

    // Layer 6: 交叉合并到 M7
    {.op_id = 11,
     .name = "L6_add3",
     .backend = TVMRT_BACKEND_CPU,
     .func_entry_id = 3,
     .input_sids = {10, 11, -1, -1},
     .output_sids = {12, -1},
     .input_count = 2,
     .output_count = 1},

    // Layer 7: 最终变换 (复用 M4, M5)
    {.op_id = 12,
     .name = "L7_sub_0",
     .backend = TVMRT_BACKEND_CPU,
     .func_entry_id = 4,
     .input_sids = {12, -1, -1, -1},
     .output_sids = {5, -1},
     .input_count = 1,
     .output_count = 1},
    {.op_id = 13,
     .name = "L7_add_1",
     .backend = TVMRT_BACKEND_CPU,
     .func_entry_id = 0,
     .input_sids = {9, -1, -1, -1},
     .output_sids = {6, -1},
     .input_count = 1,
     .output_count = 1},

    // Layer 8: 输出
    {.op_id = 14,
     .name = "L8_add3_0",
     .backend = TVMRT_BACKEND_CPU,
     .func_entry_id = 3,
     .input_sids = {5, 6, -1, -1},
     .output_sids = {1, -1},
     .input_count = 2,
     .output_count = 1},
    {.op_id = 15,
     .name = "L8_add3_out",
     .backend = TVMRT_BACKEND_CPU,
     .func_entry_id = 3,
     .input_sids = {1, 12, -1, -1},
     .output_sids = {-1, -1},
     .input_count = 2,
     .output_count = 1},
};

// ============================================================
// CPU 函数表
// ============================================================

static const tvmrt_op_func_t g_model_cpu_func_table[] = {
    // 原有算子 (索引 0-5)
    wrapped_fused_add,        // 索引 0: +1
    wrapped_fused_add_1,      // 索引 1: +3
    wrapped_fused_add_2,      // 索引 2: +5
    wrapped_fused_add_3,      // 索引 3: p0+p1
    wrapped_fused_subtract,   // 索引 4: -2
    wrapped_fused_subtract_1, // 索引 5: -4
    // Phase 1: 激活函数 (索引 6-9)
    wrapped_relu,    // 索引 6: ReLU
    wrapped_sigmoid, // 索引 7: Sigmoid
    wrapped_tanh_op, // 索引 8: Tanh
    wrapped_relu6,   // 索引 9: ReLU6
    // Phase 2: 基础运算 (索引 10-12)
    wrapped_multiply, // 索引 10: p0*p1
    wrapped_maximum,  // 索引 11: max(p0,p1)
    wrapped_minimum,  // 索引 12: min(p0,p1)
    // Phase 3: 常量乘法 (索引 13-14)
    wrapped_mul_2,    // 索引 13: p0*2
    wrapped_mul_half, // 索引 14: p0*0.5
};

#define MODEL_CPU_FUNC_COUNT 15

// ============================================================
// 静态 BSP 调度表
// ============================================================

static const int32_t g_model_layer1_ops[] = {0, 1, 2, 3}; // 4并行
static const int32_t g_model_layer2_ops[] = {4, 5};       // 2并行
static const int32_t g_model_layer3_ops[] = {6, 7};       // 2并行
static const int32_t g_model_layer4_ops[] = {8};          // 串行
static const int32_t g_model_layer5_ops[] = {9, 10};      // 2并行
static const int32_t g_model_layer6_ops[] = {11};         // 串行
static const int32_t g_model_layer7_ops[] = {12, 13};     // 2并行
static const int32_t g_model_layer8_ops[] = {14};         // 串行: M4+M5→M0
static const int32_t g_model_layer9_ops[] = {
    15}; // 串行: M0+M7→output (依赖 Op14)

static const tvmrt_schedule_layer_t g_model_schedule_layers[MODEL_NUM_LAYERS] =
    {
        {.op_indices = g_model_layer1_ops, .count = 4},
        {.op_indices = g_model_layer2_ops, .count = 2},
        {.op_indices = g_model_layer3_ops, .count = 2},
        {.op_indices = g_model_layer4_ops, .count = 1},
        {.op_indices = g_model_layer5_ops, .count = 2},
        {.op_indices = g_model_layer6_ops, .count = 1},
        {.op_indices = g_model_layer7_ops, .count = 2},
        {.op_indices = g_model_layer8_ops, .count = 1},
        {.op_indices = g_model_layer9_ops, .count = 1},
};

static const tvmrt_schedule_desc_t g_model_schedule = {
    .layers = g_model_schedule_layers, .layer_count = MODEL_NUM_LAYERS};

// ============================================================
// 完整模型描述符
// ============================================================

static const tvmrt_model_desc_t g_model_desc = {
    .tensor_map = g_model_tensor_map,
    .tensor_count = MODEL_NUM_TENSORS,
    .op_descs = g_model_op_descs,
    .op_count = MODEL_NUM_OPS,
    .schedule = &g_model_schedule,
    .cpu_func_table = g_model_cpu_func_table,
    .cpu_func_count = MODEL_CPU_FUNC_COUNT};

// ============================================================
// 访问函数
// ============================================================

const tvmrt_model_desc_t *model_get_descriptor(void) { return &g_model_desc; }

const tvmrt_schedule_desc_t *model_get_schedule(void) {
  return &g_model_schedule;
}

// ============================================================
// 参数存储 (静态分配)
// ============================================================

static FusedAddArgs g_model_single_args[16]; // 单输入算子参数
static FusedAdd3Args g_model_dual_args[16];  // 双输入算子参数

// ============================================================
// 参数填充
// ============================================================

int model_fill_args(void *args, float *input, float *output, uint8_t *workspace,
                    const uint8_t *const_workspace) {
  (void)args;

  // 内存槽指针
  float *M0 = (float *)(workspace + 0);
  float *M1 = (float *)(workspace + 8);
  float *M2 = (float *)(workspace + 16);
  float *M3 = (float *)(workspace + 24);
  float *M4 = (float *)(workspace + 32);
  float *M5 = (float *)(workspace + 40);
  float *M6 = (float *)(workspace + 48);
  float *M7 = (float *)(workspace + 56);

  // Layer 1: input + const → M0-M3
  g_model_single_args[0] =
      (FusedAddArgs){input, M0, (uint8_t *)const_workspace, workspace};
  g_model_single_args[1] =
      (FusedAddArgs){input, M1, (uint8_t *)const_workspace, workspace};
  g_model_single_args[2] =
      (FusedAddArgs){input, M2, (uint8_t *)const_workspace, workspace};
  g_model_single_args[3] =
      (FusedAddArgs){input, M3, (uint8_t *)const_workspace, workspace};

  // Layer 2: M0+M1→M4, M2+M3→M5
  g_model_dual_args[4] =
      (FusedAdd3Args){M0, M1, M4, (uint8_t *)const_workspace, workspace};
  g_model_dual_args[5] =
      (FusedAdd3Args){M2, M3, M5, (uint8_t *)const_workspace, workspace};

  // Layer 3: M4-2→M0, M5-4→M1
  g_model_single_args[6] =
      (FusedAddArgs){M4, M0, (uint8_t *)const_workspace, workspace};
  g_model_single_args[7] =
      (FusedAddArgs){M5, M1, (uint8_t *)const_workspace, workspace};

  // Layer 4: M0+M1→M6
  g_model_dual_args[8] =
      (FusedAdd3Args){M0, M1, M6, (uint8_t *)const_workspace, workspace};

  // Layer 5: M6+3→M2, M6+5→M3
  g_model_single_args[9] =
      (FusedAddArgs){M6, M2, (uint8_t *)const_workspace, workspace};
  g_model_single_args[10] =
      (FusedAddArgs){M6, M3, (uint8_t *)const_workspace, workspace};

  // Layer 6: M2+M3→M7
  g_model_dual_args[11] =
      (FusedAdd3Args){M2, M3, M7, (uint8_t *)const_workspace, workspace};

  // Layer 7: M7-2→M4, M6+1→M5
  g_model_single_args[12] =
      (FusedAddArgs){M7, M4, (uint8_t *)const_workspace, workspace};
  g_model_single_args[13] =
      (FusedAddArgs){M6, M5, (uint8_t *)const_workspace, workspace};

  // Layer 8: M4+M5→M0, M0+M7→output
  g_model_dual_args[14] =
      (FusedAdd3Args){M4, M5, M0, (uint8_t *)const_workspace, workspace};
  g_model_dual_args[15] =
      (FusedAdd3Args){M0, M7, output, (uint8_t *)const_workspace, workspace};

  return 0;
}

// ============================================================
// 获取指定算子的参数指针
// ============================================================

void *model_get_op_args(int32_t op_id) {
  if (op_id < 0 || op_id >= MODEL_NUM_OPS) {
    return NULL;
  }

  // 根据算子类型返回对应参数
  switch (op_id) {
  // 单输入算子
  case 0:
  case 1:
  case 2:
  case 3: // Layer 1
  case 6:
  case 7: // Layer 3
  case 9:
  case 10: // Layer 5
  case 12:
  case 13: // Layer 7
    return &g_model_single_args[op_id];

  // 双输入算子
  case 4:
  case 5:  // Layer 2
  case 8:  // Layer 4
  case 11: // Layer 6
  case 14:
  case 15: // Layer 8
    return &g_model_dual_args[op_id];

  default:
    return NULL;
  }
}
