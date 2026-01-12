/**
 * @file tvmrt_port.h
 * @brief OS 抽象层 - 平台无关的线程原语
 * 
 * 本头文件定义了 mutex、条件变量和线程操作的抽象，
 * 使 Runtime 可以在 POSIX、RTOS 和单线程环境中移植。
 * 
 * 注意：对于嵌入式/RTOS 目标，请替换为平台特定的实现。
 */

#ifndef TVMRT_PORT_H_
#define TVMRT_PORT_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// 错误码
// ============================================================
#define TVMRT_OK           0
#define TVMRT_ERR_GENERIC  (-1)
#define TVMRT_ERR_TIMEOUT  (-2)

// ============================================================
// 平台检测与类型定义
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
// 默认: POSIX (同上)
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
// 互斥锁 API
// ============================================================

/**
 * @brief 初始化互斥锁
 * @param m 互斥锁指针
 * @return 成功返回 TVMRT_OK，失败返回错误码
 */
int tvmrt_mutex_init(tvmrt_mutex_t* m);

/**
 * @brief 加锁 (阻塞)
 * @param m 互斥锁指针
 * @return 成功返回 TVMRT_OK
 */
int tvmrt_mutex_lock(tvmrt_mutex_t* m);

/**
 * @brief 解锁
 * @param m 互斥锁指针
 * @return 成功返回 TVMRT_OK
 */
int tvmrt_mutex_unlock(tvmrt_mutex_t* m);

/**
 * @brief 销毁互斥锁并释放资源
 * @param m 互斥锁指针
 */
void tvmrt_mutex_destroy(tvmrt_mutex_t* m);

// ============================================================
// 条件变量 API
// ============================================================

/**
 * @brief 初始化条件变量
 * @param c 条件变量指针
 * @return 成功返回 TVMRT_OK
 */
int tvmrt_cond_init(tvmrt_cond_t* c);

/**
 * @brief 等待条件变量
 * @param c 条件变量指针
 * @param m 关联的互斥锁指针 (必须已加锁)
 * @return 成功返回 TVMRT_OK
 */
int tvmrt_cond_wait(tvmrt_cond_t* c, tvmrt_mutex_t* m);

/**
 * @brief 唤醒一个等待线程
 * @param c 条件变量指针
 * @return 成功返回 TVMRT_OK
 */
int tvmrt_cond_signal(tvmrt_cond_t* c);

/**
 * @brief 唤醒所有等待线程
 * @param c 条件变量指针
 * @return 成功返回 TVMRT_OK
 */
int tvmrt_cond_broadcast(tvmrt_cond_t* c);

/**
 * @brief 销毁条件变量
 * @param c 条件变量指针
 */
void tvmrt_cond_destroy(tvmrt_cond_t* c);

// ============================================================
// 线程 API
// ============================================================

/** 线程函数签名 */
typedef void* (*tvmrt_thread_func_t)(void* arg);

/**
 * @brief 创建并启动新线程
 * @param t 线程句柄指针
 * @param func 线程入口函数
 * @param arg 传递给 func 的参数
 * @return 成功返回 TVMRT_OK
 */
int tvmrt_thread_create(tvmrt_thread_t* t, tvmrt_thread_func_t func, void* arg);

/**
 * @brief 等待线程完成
 * @param t 线程句柄指针
 * @return 成功返回 TVMRT_OK
 */
int tvmrt_thread_join(tvmrt_thread_t* t);

// ============================================================
// 屏障 API (用于 BSP 同步)
// ============================================================

/**
 * @brief 初始化屏障
 * @param b 屏障指针
 * @return 成功返回 TVMRT_OK
 */
int tvmrt_barrier_init(tvmrt_barrier_t* b);

/**
 * @brief 重置屏障并设置新的目标计数
 * @param b 屏障指针
 * @param target 需要等待的线程数
 */
void tvmrt_barrier_reset(tvmrt_barrier_t* b, int32_t target);

/**
 * @brief Worker 通知完成 (计数+1)
 * @param b 屏障指针
 */
void tvmrt_barrier_arrive(tvmrt_barrier_t* b);

/**
 * @brief 主线程等待所有 Worker 到达
 * @param b 屏障指针
 */
void tvmrt_barrier_sync(tvmrt_barrier_t* b);

/**
 * @brief 销毁屏障
 * @param b 屏障指针
 */
void tvmrt_barrier_destroy(tvmrt_barrier_t* b);

#ifdef __cplusplus
}
#endif

#endif  // TVMRT_PORT_H_
