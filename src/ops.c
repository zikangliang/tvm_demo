/**
 * @file default_ops.c
 * @brief TVM 生成的融合算子
 * 
 * 本文件包含 TVM 生成的实际算子实现，
 * 以及将它们适配为统一签名的包装函数。
 */

#include "tvmrt.h"

// ============================================================
// 参数结构体 (模型特定)
// ============================================================

typedef struct {
    float* p0;
    float* output;
    uint8_t* const_ws;
    uint8_t* ws;
} FusedAddArgs;

typedef struct {
    float* p0;
    float* p1;
    float* output;
    uint8_t* const_ws;
    uint8_t* ws;
} FusedAdd3Args;

#ifdef __cplusplus
extern "C"
#endif
int32_t tvmgen_default_fused_add(float* p0, float* T_add, 
    uint8_t* global_const_workspace_2_var, uint8_t* global_workspace_3_var) 
{
    void* fused_constant_let = (&(global_const_workspace_2_var[64]));
    T_add[0] = (p0[0] + ((float*)fused_constant_let)[0]);
    return 0;
}

#ifdef __cplusplus
extern "C"
#endif
int32_t tvmgen_default_fused_add_1(float* p0, float* T_add, 
    uint8_t* global_const_workspace_6_var, uint8_t* global_workspace_7_var) 
{
    void* fused_constant_2_let = (&(global_const_workspace_6_var[32]));
    T_add[0] = (p0[0] + ((float*)fused_constant_2_let)[0]);
    return 0;
}

#ifdef __cplusplus
extern "C"
#endif
int32_t tvmgen_default_fused_add_2(float* p0, float* T_add, 
    uint8_t* global_const_workspace_10_var, uint8_t* global_workspace_11_var) 
{
    void* fused_constant_4_let = (&(global_const_workspace_10_var[0]));
    T_add[0] = (p0[0] + ((float*)fused_constant_4_let)[0]);
    return 0;
}

#ifdef __cplusplus
extern "C"
#endif
int32_t tvmgen_default_fused_add_3(float* p0, float* p1, float* T_add, 
    uint8_t* global_const_workspace_12_var, uint8_t* global_workspace_13_var) 
{
    T_add[0] = (p0[0] + p1[0]);
    return 0;
}

#ifdef __cplusplus
extern "C"
#endif
int32_t tvmgen_default_fused_subtract(float* p0, float* T_subtract, 
    uint8_t* global_const_workspace_4_var, uint8_t* global_workspace_5_var) 
{
    void* fused_constant_1_let = (&(global_const_workspace_4_var[48]));
    T_subtract[0] = (p0[0] - ((float*)fused_constant_1_let)[0]);
    return 0;
}

#ifdef __cplusplus
extern "C"
#endif
int32_t tvmgen_default_fused_subtract_1(float* p0, float* T_subtract, 
    uint8_t* global_const_workspace_8_var, uint8_t* global_workspace_9_var) 
{
    void* fused_constant_3_let = (&(global_const_workspace_8_var[16]));
    T_subtract[0] = (p0[0] - ((float*)fused_constant_3_let)[0]);
    return 0;
}

// ============================================================
// 包装函数 (统一签名: int32_t func(void* args))
// ============================================================
// 将 TVM 生成的算子适配为 Runtime 的统一
// operator_func_t 签名以便调度。
//
// TVMRT_LOG_PARAMS 宏在 TVMRT_LOG_ENABLE=0 时完全展开为空，
// 实现零运行时开销。

int32_t wrapped_fused_add(void* args) {
    FusedAddArgs* a = (FusedAddArgs*)args;
    TVMRT_LOG_PARAMS("fused_add", a->p0 ? *(a->p0) : 0.0f, 0.0f, a->output);
    int32_t ret = tvmgen_default_fused_add(a->p0, a->output, a->const_ws, a->ws);
    TVMRT_LOG_RESULT("fused_add", a->output);
    return ret;
}

int32_t wrapped_fused_add_1(void* args) {
    FusedAddArgs* a = (FusedAddArgs*)args;
    TVMRT_LOG_PARAMS("fused_add_1", a->p0 ? *(a->p0) : 0.0f, 0.0f, a->output);
    int32_t ret = tvmgen_default_fused_add_1(a->p0, a->output, a->const_ws, a->ws);
    TVMRT_LOG_RESULT("fused_add_1", a->output);
    return ret;
}

int32_t wrapped_fused_add_2(void* args) {
    FusedAddArgs* a = (FusedAddArgs*)args;
    TVMRT_LOG_PARAMS("fused_add_2", a->p0 ? *(a->p0) : 0.0f, 0.0f, a->output);
    int32_t ret = tvmgen_default_fused_add_2(a->p0, a->output, a->const_ws, a->ws);
    TVMRT_LOG_RESULT("fused_add_2", a->output);
    return ret;
}

int32_t wrapped_fused_add_3(void* args) {
    FusedAdd3Args* a = (FusedAdd3Args*)args;
    TVMRT_LOG_PARAMS("fused_add_3", a->p0 ? *(a->p0) : 0.0f, a->p1 ? *(a->p1) : 0.0f, a->output);
    int32_t ret = tvmgen_default_fused_add_3(a->p0, a->p1, a->output, a->const_ws, a->ws);
    TVMRT_LOG_RESULT("fused_add_3", a->output);
    return ret;
}

int32_t wrapped_fused_subtract(void* args) {
    FusedAddArgs* a = (FusedAddArgs*)args;
    TVMRT_LOG_PARAMS("fused_subtract", a->p0 ? *(a->p0) : 0.0f, 0.0f, a->output);
    int32_t ret = tvmgen_default_fused_subtract(a->p0, a->output, a->const_ws, a->ws);
    TVMRT_LOG_RESULT("fused_subtract", a->output);
    return ret;
}

int32_t wrapped_fused_subtract_1(void* args) {
    FusedAddArgs* a = (FusedAddArgs*)args;
    TVMRT_LOG_PARAMS("fused_subtract_1", a->p0 ? *(a->p0) : 0.0f, 0.0f, a->output);
    int32_t ret = tvmgen_default_fused_subtract_1(a->p0, a->output, a->const_ws, a->ws);
    TVMRT_LOG_RESULT("fused_subtract_1", a->output);
    return ret;
}
