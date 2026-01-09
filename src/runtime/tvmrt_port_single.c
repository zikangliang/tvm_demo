/**
 * @file tvmrt_port_single.c
 * @brief OS 抽象层 - 单线程 (无 OS) 实现
 * 
 * 本文件提供 tvmrt_port.h 的空操作实现，用于：
 * - 在单线程主机上调试
 * - 裸机或无线程支持的最小 RTOS 环境
 * - 在无并发情况下验证逻辑
 * 
 * 所有线程原语都是空操作存根。
 * 线程不会实际创建 - 线程函数将内联执行。
 */

#include "tvmrt_port.h"

// ============================================================
// 内部结构定义 (空存根)
// ============================================================

// 类型定义现在在 tvmrt_port.h 中条件编译提供

// ============================================================
// 互斥锁实现 (空操作)
// ============================================================

int tvmrt_mutex_init(tvmrt_mutex_t* m) {
    (void)m;
    return TVMRT_OK;
}

int tvmrt_mutex_lock(tvmrt_mutex_t* m) {
    (void)m;
    return TVMRT_OK;
}

int tvmrt_mutex_unlock(tvmrt_mutex_t* m) {
    (void)m;
    return TVMRT_OK;
}

void tvmrt_mutex_destroy(tvmrt_mutex_t* m) {
    (void)m;
}

// ============================================================
// 条件变量实现 (空操作)
// ============================================================

int tvmrt_cond_init(tvmrt_cond_t* c) {
    (void)c;
    return TVMRT_OK;
}

int tvmrt_cond_wait(tvmrt_cond_t* c, tvmrt_mutex_t* m) {
    // 单线程模式下，wait 是空操作
    // (没有其他线程来唤醒我们)
    (void)c;
    (void)m;
    return TVMRT_OK;
}

int tvmrt_cond_signal(tvmrt_cond_t* c) {
    (void)c;
    return TVMRT_OK;
}

int tvmrt_cond_broadcast(tvmrt_cond_t* c) {
    (void)c;
    return TVMRT_OK;
}

void tvmrt_cond_destroy(tvmrt_cond_t* c) {
    (void)c;
}

// ============================================================
// 线程实现 (内联执行)
// ============================================================

int tvmrt_thread_create(tvmrt_thread_t* t, tvmrt_thread_func_t func, void* arg) {
    // 单线程模式下，我们不实际创建线程
    // 调用者 (线程池) 应检测单线程模式，
    // 直接在主线程中执行任务
    (void)t;
    (void)func;
    (void)arg;
    return TVMRT_OK;
}

int tvmrt_thread_join(tvmrt_thread_t* t) {
    // 单线程模式下没有线程需要 join
    (void)t;
    return TVMRT_OK;
}

// ============================================================
// 屏障实现 (简单计数)
// ============================================================

int tvmrt_barrier_init(tvmrt_barrier_t* b) {
    if (!b) return TVMRT_ERR_GENERIC;
    b->count = 0;
    b->target = 0;
    return TVMRT_OK;
}

void tvmrt_barrier_reset(tvmrt_barrier_t* b, int32_t target) {
    if (!b) return;
    b->count = 0;
    b->target = target;
}

void tvmrt_barrier_arrive(tvmrt_barrier_t* b) {
    if (!b) return;
    b->count++;
}

void tvmrt_barrier_sync(tvmrt_barrier_t* b) {
    // 单线程模式下，如果 count < target，说明任务
    // 还未执行。这是开发阶段应捕获的逻辑错误。
    // 生产环境中，我们简单地继续执行。
    (void)b;
}

void tvmrt_barrier_destroy(tvmrt_barrier_t* b) {
    (void)b;
}
