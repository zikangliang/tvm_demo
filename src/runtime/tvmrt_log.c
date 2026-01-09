/**
 * @file tvmrt_log.c
 * @brief 轻量级日志实现
 * 
 * 静态 ring buffer 实现，零动态分配。
 */

#include "tvmrt_log.h"
#include <string.h>

#if TVMRT_LOG_ENABLE

// ============================================================
// 静态 Ring Buffer
// ============================================================

static tvmrt_log_record_t g_log_buffer[TVMRT_LOG_BUFFER_SIZE];
static int32_t g_log_head = 0;  // 下一个写入位置
static int32_t g_log_tail = 0;  // 下一个读取位置
static int32_t g_log_count = 0; // 当前记录数

// ============================================================
// 回调存储
// ============================================================

static tvmrt_log_callback_t g_log_callback = NULL;
static void* g_log_callback_user = NULL;

// ============================================================
// API 实现
// ============================================================

void tvmrt_log_set_callback(tvmrt_log_callback_t cb, void* user) {
    g_log_callback = cb;
    g_log_callback_user = user;
}

void tvmrt_log_push(const tvmrt_log_record_t* rec) {
    if (!rec) return;
    
    // 如果设置了回调，调用它
    if (g_log_callback) {
        g_log_callback(rec, g_log_callback_user);
    }
    
    // 将记录复制到 ring buffer
    g_log_buffer[g_log_head] = *rec;
    g_log_head = (g_log_head + 1) % TVMRT_LOG_BUFFER_SIZE;
    
    if (g_log_count < TVMRT_LOG_BUFFER_SIZE) {
        g_log_count++;
    } else {
        // 缓冲区已满，推进 tail (覆盖最旧的记录)
        g_log_tail = (g_log_tail + 1) % TVMRT_LOG_BUFFER_SIZE;
    }
}

int tvmrt_log_pop(tvmrt_log_record_t* rec) {
    if (!rec || g_log_count == 0) {
        return -1;
    }
    
    *rec = g_log_buffer[g_log_tail];
    g_log_tail = (g_log_tail + 1) % TVMRT_LOG_BUFFER_SIZE;
    g_log_count--;
    
    return 0;
}

void tvmrt_log_clear(void) {
    g_log_head = 0;
    g_log_tail = 0;
    g_log_count = 0;
}

int32_t tvmrt_log_count(void) {
    return g_log_count;
}

#else  // TVMRT_LOG_ENABLE == 0

// 日志禁用时的存根实现
void tvmrt_log_set_callback(tvmrt_log_callback_t cb, void* user) {
    (void)cb;
    (void)user;
}

void tvmrt_log_push(const tvmrt_log_record_t* rec) {
    (void)rec;
}

int tvmrt_log_pop(tvmrt_log_record_t* rec) {
    (void)rec;
    return -1;
}

void tvmrt_log_clear(void) {}

int32_t tvmrt_log_count(void) {
    return 0;
}

#endif  // TVMRT_LOG_ENABLE
