/**
 * @file tvmrt_log.h
 * @brief Lightweight Logging Interface
 * 
 * Features:
 * - Zero dynamic allocation (static ring buffer)
 * - Compile-time enable/disable via TVMRT_LOG_ENABLE
 * - Callback mode for custom logging backends
 * - Ring buffer mode for post-mortem analysis
 */

#ifndef TVMRT_LOG_H_
#define TVMRT_LOG_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// Configuration
// ============================================================

/** Enable logging (set to 0 to completely disable) */
#ifndef TVMRT_LOG_ENABLE
#define TVMRT_LOG_ENABLE 1
#endif

/** Ring buffer size (number of records) */
#ifndef TVMRT_LOG_BUFFER_SIZE
#define TVMRT_LOG_BUFFER_SIZE 64
#endif

// ============================================================
// Log Levels
// ============================================================

typedef enum {
    TVMRT_LOG_DEBUG = 0,
    TVMRT_LOG_INFO  = 1,
    TVMRT_LOG_WARN  = 2,
    TVMRT_LOG_ERROR = 3
} tvmrt_log_level_t;

// ============================================================
// Log Record Structure
// ============================================================

typedef struct {
    int32_t op_id;              // Operator ID
    const char* op_name;        // Operator name (static string, not copied)
    int32_t worker_id;          // Worker thread ID (-1 for main thread)
    int32_t ret_code;           // Return code from operator
    tvmrt_log_level_t level;    // Log level
} tvmrt_log_record_t;

// ============================================================
// Callback Mode API
// ============================================================

/** Log callback function type */
typedef void (*tvmrt_log_callback_t)(const tvmrt_log_record_t* rec, void* user);

/**
 * @brief Set custom log callback
 * @param cb Callback function (NULL to disable)
 * @param user User data passed to callback
 */
void tvmrt_log_set_callback(tvmrt_log_callback_t cb, void* user);

// ============================================================
// Ring Buffer Mode API
// ============================================================

/**
 * @brief Push a log record to the ring buffer
 * @param rec Pointer to log record
 * 
 * If the buffer is full, the oldest record is overwritten.
 */
void tvmrt_log_push(const tvmrt_log_record_t* rec);

/**
 * @brief Pop a log record from the ring buffer
 * @param rec Pointer to receive the log record
 * @return 0 on success, -1 if buffer is empty
 */
int tvmrt_log_pop(tvmrt_log_record_t* rec);

/**
 * @brief Clear all records from the ring buffer
 */
void tvmrt_log_clear(void);

/**
 * @brief Get number of records in the buffer
 * @return Current record count
 */
int32_t tvmrt_log_count(void);

// ============================================================
// Convenience Macros
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
