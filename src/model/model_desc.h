/**
 * @file model_desc.h
 * @brief 模型描述符接口
 * 
 * 本头文件提供对编译期生成的模型描述符的访问。
 * 在实际部署中，这将由 TVM 编译器自动生成。
 */

#ifndef MODEL_DESC_H_
#define MODEL_DESC_H_

#include "../runtime/tvmrt_types.h"
#include "../runtime/tvmrt_semantic.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// 模型特定常量
// ============================================================

#define MODEL_NUM_TENSORS  5
#define MODEL_NUM_OPS      6
#define MODEL_NUM_LAYERS   4

// ============================================================
// 模型描述符访问
// ============================================================

/**
 * @brief 获取模型描述符
 * @return 静态模型描述符指针
 */
const tvmrt_model_desc_t* model_get_descriptor(void);

/**
 * @brief 获取张量映射表
 */
const tvmrt_tensor_map_entry_t* model_get_tensor_map(void);

/**
 * @brief 获取算子描述表
 */
const tvmrt_op_desc_t* model_get_op_descs(void);

/**
 * @brief 获取静态调度表
 */
const tvmrt_schedule_desc_t* model_get_schedule(void);

// ============================================================
// 参数结构体 (模型特定)
// ============================================================

/** 单输入 fused_add 算子的参数结构 */
typedef struct {
    float* p0;          // 输入张量
    float* output;      // 输出张量
    uint8_t* const_ws;  // 常量 workspace
    uint8_t* ws;        // 可变 workspace
} FusedAddArgs;

/** 双输入 fused_add_3 算子的参数结构 */
typedef struct {
    float* p0;          // 第一输入张量
    float* p1;          // 第二输入张量
    float* output;      // 输出张量
    uint8_t* const_ws;  // 常量 workspace
    uint8_t* ws;        // 可变 workspace
} FusedAdd3Args;

// ============================================================
// 模型初始化辅助函数
// ============================================================

/**
 * @brief 根据输入/输出和 workspace 填充算子参数
 * 
 * 本函数根据以下信息设置所有算子的参数：
 * - 外部输入/输出指针
 * - 解析后的 workspace SID 指针
 * 
 * @param args 参数结构数组 (大小 >= MODEL_NUM_OPS)
 * @param input 外部输入张量
 * @param output 外部输出张量
 * @param workspace 可变 workspace 缓冲区
 * @param const_workspace 常量数据缓冲区
 * @return 成功返回 0
 */
int model_fill_args(
    void* args,
    float* input,
    float* output,
    uint8_t* workspace,
    const uint8_t* const_workspace
);

#ifdef __cplusplus
}
#endif

#endif  // MODEL_DESC_H_
