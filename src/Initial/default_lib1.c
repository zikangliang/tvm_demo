// tvm target: c -keys=cpu 
#define TVM_EXPORTS
#include "tvm/runtime/c_runtime_api.h"
#include "tvm/runtime/c_backend_api.h"
#include <math.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C"
#endif
TVM_DLL int32_t tvmgen_default_fused_add(float* p0, float* T_add, uint8_t* global_const_workspace_2_var, uint8_t* global_workspace_3_var);
#ifdef __cplusplus
extern "C"
#endif
TVM_DLL int32_t tvmgen_default_fused_add_1(float* p0, float* T_add, uint8_t* global_const_workspace_6_var, uint8_t* global_workspace_7_var);
#ifdef __cplusplus
extern "C"
#endif
TVM_DLL int32_t tvmgen_default_fused_add_2(float* p0, float* T_add, uint8_t* global_const_workspace_10_var, uint8_t* global_workspace_11_var);
#ifdef __cplusplus
extern "C"
#endif
TVM_DLL int32_t tvmgen_default_fused_add_3(float* p0, float* p1, float* T_add, uint8_t* global_const_workspace_12_var, uint8_t* global_workspace_13_var);
#ifdef __cplusplus
extern "C"
#endif
TVM_DLL int32_t tvmgen_default_fused_subtract(float* p0, float* T_subtract, uint8_t* global_const_workspace_4_var, uint8_t* global_workspace_5_var);
#ifdef __cplusplus
extern "C"
#endif
TVM_DLL int32_t tvmgen_default_fused_subtract_1(float* p0, float* T_subtract, uint8_t* global_const_workspace_8_var, uint8_t* global_workspace_9_var);
#ifdef __cplusplus
extern "C"
#endif
TVM_DLL int32_t tvmgen_default___tvm_main__(float* input_buffer_var, float* output_buffer_var, uint8_t* global_const_workspace_0_var, uint8_t* global_workspace_1_var);
#ifdef __cplusplus
extern "C"
#endif
TVM_DLL int32_t tvmgen_default_fused_add(float* p0, float* T_add, uint8_t* global_const_workspace_2_var, uint8_t* global_workspace_3_var) {
  void* fused_constant_let = (&(global_const_workspace_2_var[64]));
  T_add[0] = (p0[0] + ((float*)fused_constant_let)[0]);
  return 0;
}

#ifdef __cplusplus
extern "C"
#endif
TVM_DLL int32_t tvmgen_default_fused_add_1(float* p0, float* T_add, uint8_t* global_const_workspace_6_var, uint8_t* global_workspace_7_var) {
  void* fused_constant_2_let = (&(global_const_workspace_6_var[32]));
  T_add[0] = (p0[0] + ((float*)fused_constant_2_let)[0]);
  return 0;
}

#ifdef __cplusplus
extern "C"
#endif
TVM_DLL int32_t tvmgen_default_fused_add_2(float* p0, float* T_add, uint8_t* global_const_workspace_10_var, uint8_t* global_workspace_11_var) {
  void* fused_constant_4_let = (&(global_const_workspace_10_var[0]));
  T_add[0] = (p0[0] + ((float*)fused_constant_4_let)[0]);
  return 0;
}

#ifdef __cplusplus
extern "C"
#endif
TVM_DLL int32_t tvmgen_default_fused_add_3(float* p0, float* p1, float* T_add, uint8_t* global_const_workspace_12_var, uint8_t* global_workspace_13_var) {
  T_add[0] = (p0[0] + p1[0]);
  return 0;
}

#ifdef __cplusplus
extern "C"
#endif
TVM_DLL int32_t tvmgen_default_fused_subtract(float* p0, float* T_subtract, uint8_t* global_const_workspace_4_var, uint8_t* global_workspace_5_var) {
  void* fused_constant_1_let = (&(global_const_workspace_4_var[48]));
  T_subtract[0] = (p0[0] - ((float*)fused_constant_1_let)[0]);
  return 0;
}

#ifdef __cplusplus
extern "C"
#endif
TVM_DLL int32_t tvmgen_default_fused_subtract_1(float* p0, float* T_subtract, uint8_t* global_const_workspace_8_var, uint8_t* global_workspace_9_var) {
  void* fused_constant_3_let = (&(global_const_workspace_8_var[16]));
  T_subtract[0] = (p0[0] - ((float*)fused_constant_3_let)[0]);
  return 0;
}

#ifdef __cplusplus
extern "C"
#endif
TVM_DLL int32_t tvmgen_default___tvm_main__(float* input_buffer_var, float* output_buffer_var, uint8_t* global_const_workspace_0_var, uint8_t* global_workspace_1_var) {
  void* sid_2_let = (&(global_workspace_1_var[0]));
  void* sid_1_let = (&(global_workspace_1_var[16]));
  void* sid_3_let = (&(global_workspace_1_var[32]));
  void* sid_4_let = (&(global_workspace_1_var[16]));
  void* sid_5_let = (&(global_workspace_1_var[32]));
  if (tvmgen_default_fused_add(input_buffer_var, sid_1_let, global_const_workspace_0_var, global_workspace_1_var) != 0 ) return -1;
  if (tvmgen_default_fused_subtract(sid_1_let, sid_2_let, global_const_workspace_0_var, global_workspace_1_var) != 0 ) return -1;
  if (tvmgen_default_fused_add_1(input_buffer_var, sid_3_let, global_const_workspace_0_var, global_workspace_1_var) != 0 ) return -1;
  if (tvmgen_default_fused_subtract_1(sid_3_let, sid_4_let, global_const_workspace_0_var, global_workspace_1_var) != 0 ) return -1;
  if (tvmgen_default_fused_add_2(sid_4_let, sid_5_let, global_const_workspace_0_var, global_workspace_1_var) != 0 ) return -1;
  if (tvmgen_default_fused_add_3(sid_2_let, sid_5_let, output_buffer_var, global_const_workspace_0_var, global_workspace_1_var) != 0 ) return -1;
  return 0;
}

