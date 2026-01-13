/**
 * @file default_lib0.c
 * @brief TVM Runtime 入口封装层
 *
 * 常量表和 workspace 定义
 * 16算子/9层/8内存槽 压力测试模型
 */

#include "tvmgen_default.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// 常量 Workspace
// ============================================================
static struct global_const_workspace {
  float fused_constant_4_let[1]
      __attribute__((aligned(16))); // offset: 0,  值: 5.0
  float fused_constant_3_let[1]
      __attribute__((packed, aligned(16))); // offset: 16, 值: 4.0
  float fused_constant_2_let[1]
      __attribute__((packed, aligned(16))); // offset: 32, 值: 3.0
  float fused_constant_1_let[1]
      __attribute__((packed, aligned(16))); // offset: 48, 值: 2.0
  float fused_constant_let[1]
      __attribute__((packed, aligned(16))); // offset: 64, 值: 1.0
} global_const_workspace = {
    .fused_constant_4_let = {0x1.4p+2}, // 5.0
    .fused_constant_3_let = {0x1p+2},   // 4.0
    .fused_constant_2_let = {0x1.8p+1}, // 3.0
    .fused_constant_1_let = {0x1p+1},   // 2.0
    .fused_constant_let = {0x1p+0},     // 1.0
}; // 总大小: 68 bytes

// ============================================================
// Workspace (64 bytes, 8 内存槽)
// ============================================================
__attribute__((aligned(16))) static uint8_t global_workspace[64];

// ============================================================
// 外部声明
// ============================================================

TVM_DLL int32_t tvmgen_default___tvm_main__(
    void *input, void *output, uint8_t *global_const_workspace_0_var,
    uint8_t *global_workspace_1_var);

// ============================================================
// 公共入口
// ============================================================

int32_t tvmgen_default_run(struct tvmgen_default_inputs *inputs,
                           struct tvmgen_default_outputs *outputs) {
  return tvmgen_default___tvm_main__(inputs->input, outputs->output,
                                     (uint8_t *)&global_const_workspace,
                                     (uint8_t *)&global_workspace);
}

#ifdef __cplusplus
}
#endif
