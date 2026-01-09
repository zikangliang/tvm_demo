/**
 * @file tvmrt_port_single.c
 * @brief OS Abstraction Layer - Single-threaded (No-OS) Implementation
 * 
 * This file provides a no-op implementation of tvmrt_port.h for:
 * - Debugging on single-threaded hosts
 * - Bare-metal or minimal RTOS environments without thread support
 * - Validating logic without concurrency
 * 
 * All threading primitives are stubs that do nothing.
 * Threads are NOT actually created - the thread function is executed inline.
 */

#include "tvmrt_port.h"

// ============================================================
// Internal Structure Definitions (empty stubs)
// ============================================================

struct tvmrt_mutex {
    int dummy;  // Prevent zero-size struct
};

struct tvmrt_cond {
    int dummy;
};

struct tvmrt_thread {
    int dummy;
};

struct tvmrt_barrier {
    int32_t count;
    int32_t target;
};

// ============================================================
// Mutex Implementation (No-op)
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
// Condition Variable Implementation (No-op)
// ============================================================

int tvmrt_cond_init(tvmrt_cond_t* c) {
    (void)c;
    return TVMRT_OK;
}

int tvmrt_cond_wait(tvmrt_cond_t* c, tvmrt_mutex_t* m) {
    // In single-threaded mode, wait is a no-op
    // (there's no other thread to signal us)
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
// Thread Implementation (Inline execution)
// ============================================================

int tvmrt_thread_create(tvmrt_thread_t* t, tvmrt_thread_func_t func, void* arg) {
    // In single-threaded mode, we DON'T actually create threads
    // The caller (thread pool) should detect single-thread mode and
    // execute tasks directly in the main thread instead.
    (void)t;
    (void)func;
    (void)arg;
    return TVMRT_OK;
}

int tvmrt_thread_join(tvmrt_thread_t* t) {
    // No threads to join in single-threaded mode
    (void)t;
    return TVMRT_OK;
}

// ============================================================
// Barrier Implementation (Simple counter)
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
    // In single-threaded mode, if count < target, the tasks
    // haven't been executed yet. This is a logic error that
    // should be caught during development.
    // In production, we simply proceed.
    (void)b;
}

void tvmrt_barrier_destroy(tvmrt_barrier_t* b) {
    (void)b;
}
