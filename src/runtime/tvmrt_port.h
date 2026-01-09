/**
 * @file tvmrt_port.h
 * @brief OS Abstraction Layer - Platform-independent threading primitives
 * 
 * This header defines abstractions for mutex, condition variable, and thread
 * operations, allowing the runtime to be portable across POSIX, RTOS, and
 * single-threaded environments.
 * 
 * Note: For embedded/RTOS targets, replace this header with platform-specific
 * implementations.
 */

#ifndef TVMRT_PORT_H_
#define TVMRT_PORT_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// Error Codes
// ============================================================
#define TVMRT_OK           0
#define TVMRT_ERR_GENERIC  (-1)
#define TVMRT_ERR_TIMEOUT  (-2)

// ============================================================
// Platform Detection and Type Definitions
// ============================================================

#if defined(TVMRT_PORT_POSIX) || (!defined(TVMRT_PORT_SINGLE) && (defined(__unix__) || defined(__APPLE__)))
// POSIX platform (Linux, macOS, etc.)
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
// Single-threaded / no-OS platform

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
// Default: POSIX (same as above)
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
// Mutex API
// ============================================================

/**
 * @brief Initialize a mutex
 * @param m Pointer to mutex
 * @return TVMRT_OK on success, error code on failure
 */
int tvmrt_mutex_init(tvmrt_mutex_t* m);

/**
 * @brief Lock a mutex (blocking)
 * @param m Pointer to mutex
 * @return TVMRT_OK on success
 */
int tvmrt_mutex_lock(tvmrt_mutex_t* m);

/**
 * @brief Unlock a mutex
 * @param m Pointer to mutex
 * @return TVMRT_OK on success
 */
int tvmrt_mutex_unlock(tvmrt_mutex_t* m);

/**
 * @brief Destroy a mutex and release resources
 * @param m Pointer to mutex
 */
void tvmrt_mutex_destroy(tvmrt_mutex_t* m);

// ============================================================
// Condition Variable API
// ============================================================

/**
 * @brief Initialize a condition variable
 * @param c Pointer to condition variable
 * @return TVMRT_OK on success
 */
int tvmrt_cond_init(tvmrt_cond_t* c);

/**
 * @brief Wait on a condition variable
 * @param c Pointer to condition variable
 * @param m Pointer to associated mutex (must be locked)
 * @return TVMRT_OK on success
 */
int tvmrt_cond_wait(tvmrt_cond_t* c, tvmrt_mutex_t* m);

/**
 * @brief Signal one waiting thread
 * @param c Pointer to condition variable
 * @return TVMRT_OK on success
 */
int tvmrt_cond_signal(tvmrt_cond_t* c);

/**
 * @brief Signal all waiting threads
 * @param c Pointer to condition variable
 * @return TVMRT_OK on success
 */
int tvmrt_cond_broadcast(tvmrt_cond_t* c);

/**
 * @brief Destroy a condition variable
 * @param c Pointer to condition variable
 */
void tvmrt_cond_destroy(tvmrt_cond_t* c);

// ============================================================
// Thread API
// ============================================================

/** Thread function signature */
typedef void* (*tvmrt_thread_func_t)(void* arg);

/**
 * @brief Create and start a new thread
 * @param t Pointer to thread handle
 * @param func Thread entry function
 * @param arg Argument passed to func
 * @return TVMRT_OK on success
 */
int tvmrt_thread_create(tvmrt_thread_t* t, tvmrt_thread_func_t func, void* arg);

/**
 * @brief Wait for a thread to complete
 * @param t Pointer to thread handle
 * @return TVMRT_OK on success
 */
int tvmrt_thread_join(tvmrt_thread_t* t);

// ============================================================
// Barrier API (for BSP synchronization)
// ============================================================

/**
 * @brief Initialize a barrier
 * @param b Pointer to barrier
 * @return TVMRT_OK on success
 */
int tvmrt_barrier_init(tvmrt_barrier_t* b);

/**
 * @brief Reset barrier with new target count
 * @param b Pointer to barrier
 * @param target Number of threads to wait for
 */
void tvmrt_barrier_reset(tvmrt_barrier_t* b, int32_t target);

/**
 * @brief Worker signals completion (count++)
 * @param b Pointer to barrier
 */
void tvmrt_barrier_arrive(tvmrt_barrier_t* b);

/**
 * @brief Main thread waits for all arrivals
 * @param b Pointer to barrier
 */
void tvmrt_barrier_sync(tvmrt_barrier_t* b);

/**
 * @brief Destroy a barrier
 * @param b Pointer to barrier
 */
void tvmrt_barrier_destroy(tvmrt_barrier_t* b);

#ifdef __cplusplus
}
#endif

#endif  // TVMRT_PORT_H_
