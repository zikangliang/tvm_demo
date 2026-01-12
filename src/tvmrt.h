/**
 * @file tvmrt.h
 * @brief TVM Runtime 统一头文件
 * 
 * 本头文件包含完整的 TVM Runtime API、类型定义和 OS 抽象层接口。
 * 合并自原模块化结构中的所有头文件。
 */

#ifndef TVMRT_H_
#define TVMRT_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// 配置宏
// ============================================================

/** Worker 线程数 (0 = 单线程模式) */
#ifndef TVMRT_NUM_WORKERS
#define TVMRT_NUM_WORKERS 4
#endif

/** 启用日志 (设为 0 可完全禁用) */
#ifndef TVMRT_LOG_ENABLE
#define TVMRT_LOG_ENABLE 1
#endif

/** Ring buffer 大小 (记录条数) */
#ifndef TVMRT_LOG_BUFFER_SIZE
#define TVMRT_LOG_BUFFER_SIZE 64
#endif

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
// OS 抽象层 - 错误码
// ============================================================

#define TVMRT_OK           0
#define TVMRT_ERR_GENERIC  (-1)
#define TVMRT_ERR_TIMEOUT  (-2)

// ============================================================
// OS 抽象层 - 平台检测与类型定义
// ============================================================

#if defined(TVMRT_PORT_POSIX) || (!defined(TVMRT_PORT_SINGLE) && (defined(__unix__) || defined(__APPLE__)))
// POSIX 平台 (Linux, macOS 等)
#include <pthread.h>

typedef struct {
    pthread_mutex_t handle;
} tvmrt_mutex_t;

typedef struct {
    pthread_cond_t handle;
} tvmrt_cond_t;

typedef struct {
    pthread_t handle;
} tvmrt_thread_t;

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int32_t count;
    int32_t target;
} tvmrt_barrier_t;

#elif defined(TVMRT_PORT_SINGLE)
// 单线程 / 无 OS 平台

typedef struct {
    int dummy;
} tvmrt_mutex_t;

typedef struct {
    int dummy;
} tvmrt_cond_t;

typedef struct {
    int dummy;
} tvmrt_thread_t;

typedef struct {
    int32_t count;
    int32_t target;
} tvmrt_barrier_t;

#else
// 默认: POSIX
#include <pthread.h>

typedef struct {
    pthread_mutex_t handle;
} tvmrt_mutex_t;

typedef struct {
    pthread_cond_t handle;
} tvmrt_cond_t;

typedef struct {
    pthread_t handle;
} tvmrt_thread_t;

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int32_t count;
    int32_t target;
} tvmrt_barrier_t;

#endif

// ============================================================
// OS 抽象层 - API
// ============================================================

/** 线程函数签名 */
typedef void* (*tvmrt_thread_func_t)(void* arg);

// Mutex API
int tvmrt_mutex_init(tvmrt_mutex_t* m);
int tvmrt_mutex_lock(tvmrt_mutex_t* m);
int tvmrt_mutex_unlock(tvmrt_mutex_t* m);
void tvmrt_mutex_destroy(tvmrt_mutex_t* m);

// 条件变量 API
int tvmrt_cond_init(tvmrt_cond_t* c);
int tvmrt_cond_wait(tvmrt_cond_t* c, tvmrt_mutex_t* m);
int tvmrt_cond_signal(tvmrt_cond_t* c);
int tvmrt_cond_broadcast(tvmrt_cond_t* c);
void tvmrt_cond_destroy(tvmrt_cond_t* c);

// 线程 API
int tvmrt_thread_create(tvmrt_thread_t* t, tvmrt_thread_func_t func, void* arg);
int tvmrt_thread_join(tvmrt_thread_t* t);

// 屏障 API (用于 BSP 同步)
int tvmrt_barrier_init(tvmrt_barrier_t* b);
void tvmrt_barrier_reset(tvmrt_barrier_t* b, int32_t target);
void tvmrt_barrier_arrive(tvmrt_barrier_t* b);
void tvmrt_barrier_sync(tvmrt_barrier_t* b);
void tvmrt_barrier_destroy(tvmrt_barrier_t* b);

// ============================================================
// 日志系统 - 类型定义
// ============================================================

typedef enum {
    TVMRT_LOG_DEBUG = 0,
    TVMRT_LOG_INFO  = 1,
    TVMRT_LOG_WARN  = 2,
    TVMRT_LOG_ERROR = 3
} tvmrt_log_level_t;

typedef struct {
    int32_t op_id;
    const char* op_name;
    int32_t worker_id;
    int32_t ret_code;
    tvmrt_log_level_t level;
} tvmrt_log_record_t;

// ============================================================
// 日志系统 - API
// ============================================================

typedef void (*tvmrt_log_callback_t)(const tvmrt_log_record_t* rec, void* user);

void tvmrt_log_set_callback(tvmrt_log_callback_t cb, void* user);
void tvmrt_log_push(const tvmrt_log_record_t* rec);
int tvmrt_log_pop(tvmrt_log_record_t* rec);
void tvmrt_log_clear(void);
int32_t tvmrt_log_count(void);

// ============================================================
// 日志系统 - 便捷宏
// ============================================================

#if TVMRT_LOG_ENABLE

#define TVMRT_LOG_OP(op_id_, op_name_, worker_id_, ret_code_, level_) \
    do { \
        tvmrt_log_record_t _rec = { \
            .op_id = (op_id_), \
            .op_name = (op_name_), \
            .worker_id = (worker_id_), \
            .ret_code = (ret_code_), \
            .level = (level_) \
        }; \
        tvmrt_log_push(&_rec); \
    } while(0)

#define TVMRT_LOG_OP_START(op_id, op_name, worker_id) \
    TVMRT_LOG_OP((op_id), (op_name), (worker_id), 0, TVMRT_LOG_DEBUG)

#define TVMRT_LOG_OP_END(op_id, op_name, worker_id, ret_code) \
    TVMRT_LOG_OP((op_id), (op_name), (worker_id), (ret_code), \
                 ((ret_code) == 0) ? TVMRT_LOG_INFO : TVMRT_LOG_ERROR)

#else

#define TVMRT_LOG_OP(op_id_, op_name_, worker_id_, ret_code_, level_) ((void)0)
#define TVMRT_LOG_OP_START(op_id, op_name, worker_id) ((void)0)
#define TVMRT_LOG_OP_END(op_id, op_name, worker_id, ret_code) ((void)0)

#endif  // TVMRT_LOG_ENABLE

// ============================================================
// Runtime 核心类型 - 后端类型
// ============================================================

typedef enum {
    TVMRT_BACKEND_CPU = 0,
    TVMRT_BACKEND_NPU = 1,
    TVMRT_BACKEND_GPU = 2
} tvmrt_backend_kind_t;

// ============================================================
// Runtime 核心类型 - 张量内存映射
// ============================================================

typedef struct {
    int32_t sid;
    int32_t offset;
    int32_t size;
    int32_t align;
} tvmrt_tensor_map_entry_t;

// ============================================================
// Runtime 核心类型 - 算子描述
// ============================================================

typedef struct {
    int32_t op_id;
    const char* name;
    tvmrt_backend_kind_t backend;
    int32_t func_entry_id;
    int32_t input_sids[TVMRT_MAX_OP_INPUTS];
    int32_t output_sids[TVMRT_MAX_OP_OUTPUTS];
    int32_t input_count;
    int32_t output_count;
} tvmrt_op_desc_t;

// ============================================================
// Runtime 核心类型 - 静态调度描述
// ============================================================

typedef struct {
    const int32_t* op_indices;
    int32_t count;
} tvmrt_schedule_layer_t;

typedef struct {
    const tvmrt_schedule_layer_t* layers;
    int32_t layer_count;
} tvmrt_schedule_desc_t;

// ============================================================
// Runtime 核心类型 - 算子函数类型
// ============================================================

typedef int32_t (*tvmrt_op_func_t)(void* args);

typedef struct {
    const char* name;
    tvmrt_op_func_t func;
    void* args;
} tvmrt_op_exec_t;

// ============================================================
// Runtime 核心类型 - 运行时上下文
// ============================================================

typedef struct {
    uint8_t* workspace;
    const uint8_t* const_workspace;
    
    tvmrt_op_exec_t* op_execs;
    int32_t op_count;
    
    void* args_storage;
} tvmrt_context_t;

// ============================================================
// Runtime 核心类型 - 层级任务队列
// ============================================================

typedef struct {
    int32_t tasks[TVMRT_MAX_OPS_PER_LAYER];
    int32_t head;
    int32_t tail;
    int32_t count;
    tvmrt_mutex_t mutex;
    tvmrt_cond_t cond;
} tvmrt_layer_queue_t;

// ============================================================
// Runtime 核心类型 - 模型描述符
// ============================================================

typedef struct {
    const tvmrt_tensor_map_entry_t* tensor_map;
    int32_t tensor_count;
    
    const tvmrt_op_desc_t* op_descs;
    int32_t op_count;
    
    const tvmrt_schedule_desc_t* schedule;
    
    const tvmrt_op_func_t* cpu_func_table;
    int32_t cpu_func_count;
} tvmrt_model_desc_t;

// ============================================================
// 调度引擎 API
// ============================================================

/**
 * @brief 初始化执行引擎
 * 
 * 创建线程池和同步原语。应在启动时调用一次。
 * @return 成功返回 0
 */
int tvmrt_engine_init(void);

/**
 * @brief 关闭执行引擎
 * 
 * 销毁线程池并释放资源。
 */
void tvmrt_engine_shutdown(void);

/**
 * @brief 按静态调度表执行模型
 * 
 * @param ctx 已填充算子的运行时上下文
 * @param schedule 静态调度描述符
 * @return 成功返回 0，错误返回负数
 */
int tvmrt_engine_run(
    tvmrt_context_t* ctx,
    const tvmrt_schedule_desc_t* schedule
);

/**
 * @brief 单线程模式执行模型 (不使用线程池)
 * 
 * 适用于调试或不支持线程的环境。
 * @param ctx 已填充算子的运行时上下文
 * @param schedule 静态调度描述符
 * @return 成功返回 0，错误返回负数
 */
int tvmrt_engine_run_single(
    tvmrt_context_t* ctx,
    const tvmrt_schedule_desc_t* schedule
);

// ============================================================
// 语义转换层 API
// ============================================================

/**
 * @brief 根据模型描述填充匹配的运行时上下文
 * 
 * 暂未使用（已通过 model_fill_args 简化）
 */
int tvmrt_semantic_init(tvmrt_context_t* ctx, const tvmrt_model_desc_t* desc);

/**
 * @brief 根据 SID 解析为 workspace 指针
 * 
 * @param workspace workspace 基地址
 * @param tensor_map 张量映射表
 * @param tensor_count 张量数量
 * @param sid 存储 ID
 * @return 对应指针，未找到返回 NULL
 */
void* tvmrt_semantic_resolve_sid(
    uint8_t* workspace,
    const tvmrt_tensor_map_entry_t* tensor_map,
    int32_t tensor_count,
    int32_t sid
);

#ifdef __cplusplus
}
#endif

#endif  // TVMRT_H_
