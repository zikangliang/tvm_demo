/**
 * @file tvmrt_port_posix.c
 * @brief OS Abstraction Layer - POSIX (pthread) Implementation
 * 
 * This file implements the tvmrt_port.h interface using POSIX pthreads.
 * Use this on Linux, macOS, and other POSIX-compliant systems.
 */

#include "tvmrt_port.h"
#include <string.h>

// Type definitions are now in tvmrt_port.h via conditional compilation

// ============================================================
// Mutex Implementation
// ============================================================

int tvmrt_mutex_init(tvmrt_mutex_t* m) {
    if (!m) return TVMRT_ERR_GENERIC;
    return (pthread_mutex_init(&m->handle, NULL) == 0) ? TVMRT_OK : TVMRT_ERR_GENERIC;
}

int tvmrt_mutex_lock(tvmrt_mutex_t* m) {
    if (!m) return TVMRT_ERR_GENERIC;
    return (pthread_mutex_lock(&m->handle) == 0) ? TVMRT_OK : TVMRT_ERR_GENERIC;
}

int tvmrt_mutex_unlock(tvmrt_mutex_t* m) {
    if (!m) return TVMRT_ERR_GENERIC;
    return (pthread_mutex_unlock(&m->handle) == 0) ? TVMRT_OK : TVMRT_ERR_GENERIC;
}

void tvmrt_mutex_destroy(tvmrt_mutex_t* m) {
    if (m) {
        pthread_mutex_destroy(&m->handle);
    }
}

// ============================================================
// Condition Variable Implementation
// ============================================================

int tvmrt_cond_init(tvmrt_cond_t* c) {
    if (!c) return TVMRT_ERR_GENERIC;
    return (pthread_cond_init(&c->handle, NULL) == 0) ? TVMRT_OK : TVMRT_ERR_GENERIC;
}

int tvmrt_cond_wait(tvmrt_cond_t* c, tvmrt_mutex_t* m) {
    if (!c || !m) return TVMRT_ERR_GENERIC;
    return (pthread_cond_wait(&c->handle, &m->handle) == 0) ? TVMRT_OK : TVMRT_ERR_GENERIC;
}

int tvmrt_cond_signal(tvmrt_cond_t* c) {
    if (!c) return TVMRT_ERR_GENERIC;
    return (pthread_cond_signal(&c->handle) == 0) ? TVMRT_OK : TVMRT_ERR_GENERIC;
}

int tvmrt_cond_broadcast(tvmrt_cond_t* c) {
    if (!c) return TVMRT_ERR_GENERIC;
    return (pthread_cond_broadcast(&c->handle) == 0) ? TVMRT_OK : TVMRT_ERR_GENERIC;
}

void tvmrt_cond_destroy(tvmrt_cond_t* c) {
    if (c) {
        pthread_cond_destroy(&c->handle);
    }
}

// ============================================================
// Thread Implementation
// ============================================================

int tvmrt_thread_create(tvmrt_thread_t* t, tvmrt_thread_func_t func, void* arg) {
    if (!t || !func) return TVMRT_ERR_GENERIC;
    return (pthread_create(&t->handle, NULL, func, arg) == 0) ? TVMRT_OK : TVMRT_ERR_GENERIC;
}

int tvmrt_thread_join(tvmrt_thread_t* t) {
    if (!t) return TVMRT_ERR_GENERIC;
    return (pthread_join(t->handle, NULL) == 0) ? TVMRT_OK : TVMRT_ERR_GENERIC;
}

// ============================================================
// Barrier Implementation (for BSP synchronization)
// ============================================================

int tvmrt_barrier_init(tvmrt_barrier_t* b) {
    if (!b) return TVMRT_ERR_GENERIC;
    
    if (pthread_mutex_init(&b->mutex, NULL) != 0) {
        return TVMRT_ERR_GENERIC;
    }
    if (pthread_cond_init(&b->cond, NULL) != 0) {
        pthread_mutex_destroy(&b->mutex);
        return TVMRT_ERR_GENERIC;
    }
    b->count = 0;
    b->target = 0;
    return TVMRT_OK;
}

void tvmrt_barrier_reset(tvmrt_barrier_t* b, int32_t target) {
    if (!b) return;
    pthread_mutex_lock(&b->mutex);
    b->count = 0;
    b->target = target;
    pthread_mutex_unlock(&b->mutex);
}

void tvmrt_barrier_arrive(tvmrt_barrier_t* b) {
    if (!b) return;
    pthread_mutex_lock(&b->mutex);
    b->count++;
    if (b->count >= b->target) {
        pthread_cond_signal(&b->cond);
    }
    pthread_mutex_unlock(&b->mutex);
}

void tvmrt_barrier_sync(tvmrt_barrier_t* b) {
    if (!b) return;
    pthread_mutex_lock(&b->mutex);
    while (b->count < b->target) {
        pthread_cond_wait(&b->cond, &b->mutex);
    }
    pthread_mutex_unlock(&b->mutex);
}

void tvmrt_barrier_destroy(tvmrt_barrier_t* b) {
    if (b) {
        pthread_mutex_destroy(&b->mutex);
        pthread_cond_destroy(&b->cond);
    }
}
