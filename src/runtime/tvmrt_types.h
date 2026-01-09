/**
 * @file tvmrt_types.h
 * @brief TVM Runtime 公共类型定义
 * 
 * 本头文件定义了所有 Runtime 模块共享的类型。
 */

#ifndef TVMRT_TYPES_H_
#define TVMRT_TYPES_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// 配置限制
// ============================================================

/** 每个算子的最大输入张量数 */
#ifndef TVMRT_MAX_OP_INPUTS
#define TVMRT_MAX_OP_INPUTS 4
#endif

/** 每个算子的最大输出张量数 */
#ifndef TVMRT_MAX_OP_OUTPUTS
#define TVMRT_MAX_OP_OUTPUTS 2
#endif

/** 模型中的最大算子数 */
#ifndef TVMRT_MAX_OPS
#define TVMRT_MAX_OPS 64
#endif

/** 静态调度中的最大层数 */
#ifndef TVMRT_MAX_LAYERS
#define TVMRT_MAX_LAYERS 32
#endif

/** 每层的最大算子数 */
#ifndef TVMRT_MAX_OPS_PER_LAYER
#define TVMRT_MAX_OPS_PER_LAYER 16
#endif

// ============================================================
// 后端类型 (用于异构调度)
// ============================================================

typedef enum {
    TVMRT_BACKEND_CPU = 0,     /**< CPU 函数指针 */
    TVMRT_BACKEND_NPU = 1,     /**< NPU 子图 (预留) */
    TVMRT_BACKEND_GPU = 2      /**< GPU 内核 (预留) */
} tvmrt_backend_kind_t;

// ============================================================
// 张量内存映射
// ============================================================

/**
 * @brief 描述张量在 workspace 中的内存位置
 * 
 * 这是编译期生成的模型描述符的一部分。
 */
typedef struct {
    int32_t sid;        /**< 存储 ID (来自 TVM 编译器) */
    int32_t offset;     /**< workspace 中的字节偏移 */
    int32_t size;       /**< 字节大小 */
    int32_t align;      /**< 对齐要求 */
} tvmrt_tensor_map_entry_t;

// ============================================================
// 算子描述
// ============================================================

/**
 * @brief 描述模型图中的一个算子
 * 
 * 由编译器生成，供语义层使用。
 */
typedef struct {
    int32_t op_id;                               /**< 唯一算子 ID */
    const char* name;                            /**< 调试名称 */
    tvmrt_backend_kind_t backend;                /**< 执行后端 */
    int32_t func_entry_id;                       /**< 函数表索引 */
    int32_t input_sids[TVMRT_MAX_OP_INPUTS];     /**< 输入张量 SID (-1 表示未使用) */
    int32_t output_sids[TVMRT_MAX_OP_OUTPUTS];   /**< 输出张量 SID (-1 表示未使用) */
    int32_t input_count;                         /**< 有效输入数量 */
    int32_t output_count;                        /**< 有效输出数量 */
} tvmrt_op_desc_t;

// ============================================================
// 静态调度描述
// ============================================================

/**
 * @brief BSP 调度中的单个层 (层内所有算子可并行执行)
 */
typedef struct {
    const int32_t* op_indices;   /**< 要执行的算子 ID 数组 */
    int32_t count;               /**< 该层的算子数量 */
} tvmrt_schedule_layer_t;

/**
 * @brief 模型的完整静态调度表
 */
typedef struct {
    const tvmrt_schedule_layer_t* layers;  /**< 层数组 */
    int32_t layer_count;                   /**< 层数量 */
} tvmrt_schedule_desc_t;

// ============================================================
// 算子函数类型
// ============================================================

/** 通用算子函数签名 (用于统一调度) */
typedef int32_t (*tvmrt_op_func_t)(void* args);

/**
 * @brief 可执行算子条目 (运行时填充)
 */
typedef struct {
    const char* name;           /**< 调试名称 */
    tvmrt_op_func_t func;       /**< 函数指针 */
    void* args;                 /**< 预填充的参数结构 */
} tvmrt_op_exec_t;

// ============================================================
// 运行时上下文
// ============================================================

/**
 * @brief 模型实例的运行时上下文
 * 
 * 该结构持有一次执行所需的所有状态，
 * 使多个模型实例能够并发运行。
 */
typedef struct {
    uint8_t* workspace;                 /**< 可变 workspace 指针 */
    const uint8_t* const_workspace;     /**< 常量数据 workspace */
    
    tvmrt_op_exec_t* op_execs;          /**< 已填充的算子条目 */
    int32_t op_count;                   /**< 算子数量 */
    
    void* args_storage;                 /**< 参数结构存储 */
} tvmrt_context_t;

#ifdef __cplusplus
}
#endif

#endif  // TVMRT_TYPES_H_
