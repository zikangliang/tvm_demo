/**
 * @file stress_lib0.c
 * @brief 压力测试入口封装层
 *
 * 自包含的常量表和扩展 workspace
 */

#include "tvmgen_default.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// 常量 Workspace (与 default_lib0.c 相同)
// ============================================================
static struct stress_const_workspace {
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
} stress_const_workspace = {
    .fused_constant_4_let =
        {
            0x1.4p+2 // 5.0
        },
    .fused_constant_3_let =
        {
            0x1p+2 // 4.0
        },
    .fused_constant_2_let =
        {
            0x1.8p+1 // 3.0
        },
    .fused_constant_1_let =
        {
            0x1p+1 // 2.0
        },
    .fused_constant_let =
        {
            0x1p+0 // 1.0
        },
}; // 总大小: 68 bytes

// ============================================================
// 扩展的 Workspace (64 bytes, 8 内存槽)
// ============================================================
__attribute__((aligned(16))) static uint8_t stress_workspace[64];

// ============================================================
// 外部声明
// ============================================================

TVM_DLL int32_t stress_tvm_main(void *input, void *output,
                                uint8_t *global_const_workspace_0_var,
                                uint8_t *global_workspace_1_var);

// ============================================================
// 公共入口
// ============================================================

int32_t stress_run(struct tvmgen_default_inputs *inputs,
                   struct tvmgen_default_outputs *outputs) {
  return stress_tvm_main(inputs->input, outputs->output,
                         (uint8_t *)&stress_const_workspace,
                         (uint8_t *)&stress_workspace);
}

#ifdef __cplusplus
}
#endif
