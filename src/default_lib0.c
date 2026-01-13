// #include "tvm/runtime/c_runtime_api.h"
#include "tvmgen_default.h" // 包含类型定义，只是暂时的测试可运行。
#ifdef __cplusplus
extern "C" {
#endif
static struct global_const_workspace {
  float fused_constant_4_let[1]
      __attribute__((aligned(16))); // 4 bytes, aligned offset: 0
  float fused_constant_3_let[1]
      __attribute__((packed, aligned(16))); // 4 bytes, aligned offset: 16
  float fused_constant_2_let[1]
      __attribute__((packed, aligned(16))); // 4 bytes, aligned offset: 32
  float fused_constant_1_let[1]
      __attribute__((packed, aligned(16))); // 4 bytes, aligned offset: 48
  float fused_constant_let[1]
      __attribute__((packed, aligned(16))); // 4 bytes, aligned offset: 64
} global_const_workspace = {
    .fused_constant_4_let = {0x1.4p+2},
    .fused_constant_3_let = {0x1p+2},
    .fused_constant_2_let = {0x1.8p+1},
    .fused_constant_1_let = {0x1p+1},
    .fused_constant_let = {0x1p+0},
}; // of total size 68 bytes
__attribute__((aligned(16))) static uint8_t
    global_workspace[64]; // 扩展到 64 bytes, 支持 8 个内存槽

TVM_DLL int32_t tvmgen_default___tvm_main__(
    void *input, void *output0, uint8_t *global_const_workspace_0_var,
    uint8_t *global_workspace_1_var);
int32_t tvmgen_default_run(struct tvmgen_default_inputs *inputs,
                           struct tvmgen_default_outputs *outputs) {
  return tvmgen_default___tvm_main__(inputs->input, outputs->output,
                                     ((uint8_t *)&global_const_workspace),
                                     ((uint8_t *)&global_workspace));
}
#ifdef __cplusplus
}
#endif
;