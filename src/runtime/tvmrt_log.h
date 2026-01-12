/**
 * @file tvmrt_log.h
 * @brief 轻量级日志接口
 * 
 * 特性：
 * - 零动态分配 (静态 ring buffer)
 * - 可通过 TVMRT_LOG_ENABLE 编译期开关
 * - 回调模式支持自定义日志后端
 * - Ring buffer 模式支持事后分析
 */

#ifndef TVMRT_LOG_H_
#define TVMRT_LOG_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// 配置
// ============================================================

/** 启用日志 (设为 0 可完全禁用) */
#ifndef TVMRT_LOG_ENABLE
#define TVMRT_LOG_ENABLE 1
#endif

/** Ring buffer 大小 (记录条数) */
#ifndef TVMRT_LOG_BUFFER_SIZE
#define TVMRT_LOG_BUFFER_SIZE 64
#endif

// ============================================================
// 日志级别
// ============================================================

typedef enum {
    TVMRT_LOG_DEBUG = 0,
    TVMRT_LOG_INFO  = 1,
    TVMRT_LOG_WARN  = 2,
    TVMRT_LOG_ERROR = 3
} tvmrt_log_level_t;

// ============================================================
// 日志记录结构
// ============================================================

typedef struct {
    int32_t op_id;              // 算子 ID
    const char* op_name;        // 算子名称 (静态字符串，不复制)
    int32_t worker_id;          // Worker 线程 ID (-1 表示主线程)
    int32_t ret_code;           // 算子返回码
    tvmrt_log_level_t level;    // 日志级别
} tvmrt_log_record_t;

// ============================================================
// 回调模式 API
// ============================================================

/** 日志回调函数类型 */
typedef void (*tvmrt_log_callback_t)(const tvmrt_log_record_t* rec, void* user);

/**
 * @brief 设置自定义日志回调
 * @param cb 回调函数 (NULL 表示禁用)
 * @param user 传递给回调的用户数据
 */
void tvmrt_log_set_callback(tvmrt_log_callback_t cb, void* user);

// ============================================================
// Ring Buffer 模式 API
// ============================================================

/**
 * @brief 将日志记录压入 ring buffer
 * @param rec 日志记录指针
 * 
 * 如果缓冲区已满，最旧的记录将被覆盖。
 */
void tvmrt_log_push(const tvmrt_log_record_t* rec);

/**
 * @brief 从 ring buffer 弹出日志记录
 * @param rec 接收日志记录的指针
 * @return 成功返回 0，缓冲区为空返回 -1
 */
int tvmrt_log_pop(tvmrt_log_record_t* rec);

/**
 * @brief 清空 ring buffer 中的所有记录
 */
void tvmrt_log_clear(void);

/**
 * @brief 获取缓冲区中的记录数量
 * @return 当前记录数
 */
int32_t tvmrt_log_count(void);

// ============================================================
// 便捷宏
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

#ifdef __cplusplus
}
#endif

#endif  // TVMRT_LOG_H_
