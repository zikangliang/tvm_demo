/**
 * @file tvmrt_port_posix.c
 * @brief OS 抽象层 - POSIX (pthread) 实现
 * 
 * 本文件使用 POSIX pthreads 实现 tvmrt_port.h 接口。
 * 适用于 Linux、macOS 等 POSIX 兼容系统。
 */

#include "tvmrt.h"
#include <string.h>

// 类型定义现在通过条件编译在 tvmrt_port.h 中提供

// ============================================================
// 互斥锁实现
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
// 条件变量实现
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
// 线程实现
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
// 屏障实现 (用于 BSP 同步)
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
// 锁是用来保证线程安全的， 条件变量是worker线程等待的， 屏障是主线程等待的
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
