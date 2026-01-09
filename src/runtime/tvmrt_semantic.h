/**
 * @file tvmrt_semantic.h
 * @brief 语义转换层接口
 * 
 * 本层"解析"编译期生成的描述符，
 * 并组装运行时可执行的算子条目。
 * 
 * 对应 PDF 3.1 节: "语义转换层"
 */

#ifndef TVMRT_SEMANTIC_H_
#define TVMRT_SEMANTIC_H_

#include "tvmrt_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// 模型描述符 (编译期生成)
// ============================================================

/**
 * @brief 编译器生成的完整模型描述符
 */
typedef struct {
    const tvmrt_tensor_map_entry_t* tensor_map;  /**< 张量内存映射 */
    int32_t tensor_count;
    
    const tvmrt_op_desc_t* op_descs;             /**< 算子描述 */
    int32_t op_count;
    
    const tvmrt_schedule_desc_t* schedule;       /**< 静态调度表 */
    
    /** CPU 算子函数表 */
    const tvmrt_op_func_t* cpu_func_table;
    int32_t cpu_func_count;
} tvmrt_model_desc_t;

// ============================================================
// 语义层 API
// ============================================================

/**
 * @brief 从模型描述符初始化运行时上下文
 * 
 * 本函数执行以下操作：
 * 1. 使用 tensor_map 将张量 SID 解析为实际指针
 * 2. 填充算子参数结构
 * 3. 从函数表绑定函数指针
 * 
 * @param ctx 待初始化的运行时上下文
 * @param model 模型描述符
 * @param inputs 输入张量指针数组
 * @param outputs 输出张量指针数组
 * @param workspace 可变 workspace 缓冲区
 * @param const_workspace 常量数据缓冲区
 * @param op_execs 预分配的算子条目数组 (大小 >= model->op_count)
 * @param args_storage 预分配的参数结构存储
 * @return 成功返回 0，错误返回负数
 */
int tvmrt_semantic_init(
    tvmrt_context_t* ctx,
    const tvmrt_model_desc_t* model,
    void** inputs,
    void** outputs,
    uint8_t* workspace,
    const uint8_t* const_workspace,
    tvmrt_op_exec_t* op_execs,
    void* args_storage
);

/**
 * @brief 将存储 ID 解析为指针
 * 
 * @param tensor_map 张量映射表
 * @param tensor_count 表中的条目数
 * @param workspace workspace 基地址
 * @param sid 要解析的存储 ID
 * @return 张量数据指针，未找到返回 NULL
 */
void* tvmrt_semantic_resolve_sid(
    const tvmrt_tensor_map_entry_t* tensor_map,
    int32_t tensor_count,
    uint8_t* workspace,
    int32_t sid
);

#ifdef __cplusplus
}
#endif

#endif  // TVMRT_SEMANTIC_H_
