/**
 * @file tvmrt_engine.c
 * @brief Scheduling Engine Implementation
 * 
 * BSP execution with optional multi-threading via tvmrt_port.
 */

#include "tvmrt_engine.h"
#include "tvmrt_port.h"
#include "tvmrt_log.h"
#include <string.h>
#include <stdint.h>

// ============================================================
// Thread Pool State
// ============================================================

#if TVMRT_NUM_WORKERS > 0

typedef struct {
    tvmrt_thread_t workers[TVMRT_NUM_WORKERS];
    tvmrt_mutex_t pool_mutex;
    tvmrt_cond_t work_cond;
    tvmrt_barrier_t layer_barrier;
    
    // Task dispatch state (protected by pool_mutex)
    tvmrt_context_t* current_ctx;
    const tvmrt_schedule_layer_t* current_layer;
    int32_t next_task_idx;
    
    // Control flags
    bool shutdown;
    bool initialized;
} engine_state_t;

static engine_state_t g_engine = {0};

// ============================================================
// Worker Thread Function
// ============================================================

static void* worker_func(void* arg) {
    int worker_id = (int)(intptr_t)arg;
    (void)worker_id;  // Used for logging
    
    while (1) {
        tvmrt_mutex_lock(&g_engine.pool_mutex);
        
        // Wait for work or shutdown
        while (!g_engine.shutdown && 
               (g_engine.current_layer == NULL || 
                g_engine.next_task_idx >= g_engine.current_layer->count)) {
            tvmrt_cond_wait(&g_engine.work_cond, &g_engine.pool_mutex);
        }
        
        if (g_engine.shutdown) {
            tvmrt_mutex_unlock(&g_engine.pool_mutex);
            break;
        }
        
        // Claim a task
        int32_t task_idx = g_engine.next_task_idx++;
        const tvmrt_schedule_layer_t* layer = g_engine.current_layer;
        tvmrt_context_t* ctx = g_engine.current_ctx;
        
        tvmrt_mutex_unlock(&g_engine.pool_mutex);
        
        // Execute if valid task
        if (task_idx < layer->count) {
            int32_t op_idx = layer->op_indices[task_idx];
            if (op_idx >= 0 && op_idx < ctx->op_count) {
                tvmrt_op_exec_t* exec = &ctx->op_execs[op_idx];
                if (exec->func) {
                    TVMRT_LOG_OP_START(op_idx, exec->name, worker_id);
                    int32_t ret = exec->func(exec->args);
                    TVMRT_LOG_OP_END(op_idx, exec->name, worker_id, ret);
                }
            }
            
            // Signal completion
            tvmrt_barrier_arrive(&g_engine.layer_barrier);
        }
    }
    
    return NULL;
}

#endif  // TVMRT_NUM_WORKERS > 0

// ============================================================
// Engine API Implementation
// ============================================================

int tvmrt_engine_init(void) {
#if TVMRT_NUM_WORKERS > 0
    if (g_engine.initialized) {
        return 0;  // Already initialized
    }
    
    // Initialize synchronization primitives
    if (tvmrt_mutex_init(&g_engine.pool_mutex) != TVMRT_OK) {
        return -1;
    }
    if (tvmrt_cond_init(&g_engine.work_cond) != TVMRT_OK) {
        tvmrt_mutex_destroy(&g_engine.pool_mutex);
        return -1;
    }
    if (tvmrt_barrier_init(&g_engine.layer_barrier) != TVMRT_OK) {
        tvmrt_cond_destroy(&g_engine.work_cond);
        tvmrt_mutex_destroy(&g_engine.pool_mutex);
        return -1;
    }
    
    g_engine.shutdown = false;
    g_engine.current_layer = NULL;
    g_engine.current_ctx = NULL;
    
    // Create worker threads
    for (int i = 0; i < TVMRT_NUM_WORKERS; i++) {
        if (tvmrt_thread_create(&g_engine.workers[i], worker_func, (void*)(intptr_t)i) != TVMRT_OK) {
            // Cleanup on failure
            g_engine.shutdown = true;
            tvmrt_cond_broadcast(&g_engine.work_cond);
            for (int j = 0; j < i; j++) {
                tvmrt_thread_join(&g_engine.workers[j]);
            }
            tvmrt_barrier_destroy(&g_engine.layer_barrier);
            tvmrt_cond_destroy(&g_engine.work_cond);
            tvmrt_mutex_destroy(&g_engine.pool_mutex);
            return -1;
        }
    }
    
    g_engine.initialized = true;
#endif
    return 0;
}

void tvmrt_engine_shutdown(void) {
#if TVMRT_NUM_WORKERS > 0
    if (!g_engine.initialized) {
        return;
    }
    
    // Signal shutdown
    tvmrt_mutex_lock(&g_engine.pool_mutex);
    g_engine.shutdown = true;
    tvmrt_cond_broadcast(&g_engine.work_cond);
    tvmrt_mutex_unlock(&g_engine.pool_mutex);
    
    // Join all workers
    for (int i = 0; i < TVMRT_NUM_WORKERS; i++) {
        tvmrt_thread_join(&g_engine.workers[i]);
    }
    
    // Cleanup
    tvmrt_barrier_destroy(&g_engine.layer_barrier);
    tvmrt_cond_destroy(&g_engine.work_cond);
    tvmrt_mutex_destroy(&g_engine.pool_mutex);
    
    g_engine.initialized = false;
#endif
}

int tvmrt_engine_run(
    tvmrt_context_t* ctx,
    const tvmrt_schedule_desc_t* schedule
) {
    if (!ctx || !schedule) {
        return -1;
    }
    
#if TVMRT_NUM_WORKERS > 0
    if (!g_engine.initialized) {
        // Fallback to single-threaded mode
        return tvmrt_engine_run_single(ctx, schedule);
    }
    
    // Execute layer by layer
    for (int32_t layer_idx = 0; layer_idx < schedule->layer_count; layer_idx++) {
        const tvmrt_schedule_layer_t* layer = &schedule->layers[layer_idx];
        
        if (layer->count == 0) {
            continue;
        }
        
        if (layer->count == 1) {
            // Single task: execute directly (avoid thread pool overhead)
            int32_t op_idx = layer->op_indices[0];
            if (op_idx >= 0 && op_idx < ctx->op_count) {
                tvmrt_op_exec_t* exec = &ctx->op_execs[op_idx];
                if (exec->func) {
                    TVMRT_LOG_OP_START(op_idx, exec->name, -1);
                    int32_t ret = exec->func(exec->args);
                    TVMRT_LOG_OP_END(op_idx, exec->name, -1, ret);
                    if (ret != 0) return ret;
                }
            }
        } else {
            // Multiple tasks: dispatch to workers
            tvmrt_barrier_reset(&g_engine.layer_barrier, layer->count);
            
            tvmrt_mutex_lock(&g_engine.pool_mutex);
            g_engine.current_ctx = ctx;
            g_engine.current_layer = layer;
            g_engine.next_task_idx = 0;
            tvmrt_cond_broadcast(&g_engine.work_cond);
            tvmrt_mutex_unlock(&g_engine.pool_mutex);
            
            // Wait for all tasks to complete
            tvmrt_barrier_sync(&g_engine.layer_barrier);
            
            // Clear layer pointer so workers go back to waiting
            tvmrt_mutex_lock(&g_engine.pool_mutex);
            g_engine.current_layer = NULL;
            tvmrt_mutex_unlock(&g_engine.pool_mutex);
        }
    }
    
    return 0;
#else
    return tvmrt_engine_run_single(ctx, schedule);
#endif
}

int tvmrt_engine_run_single(
    tvmrt_context_t* ctx,
    const tvmrt_schedule_desc_t* schedule
) {
    if (!ctx || !schedule) {
        return -1;
    }
    
    // Simple sequential execution
    for (int32_t layer_idx = 0; layer_idx < schedule->layer_count; layer_idx++) {
        const tvmrt_schedule_layer_t* layer = &schedule->layers[layer_idx];
        
        for (int32_t task_idx = 0; task_idx < layer->count; task_idx++) {
            int32_t op_idx = layer->op_indices[task_idx];
            if (op_idx >= 0 && op_idx < ctx->op_count) {
                tvmrt_op_exec_t* exec = &ctx->op_execs[op_idx];
                if (exec->func) {
                    TVMRT_LOG_OP_START(op_idx, exec->name, -1);
                    int32_t ret = exec->func(exec->args);
                    TVMRT_LOG_OP_END(op_idx, exec->name, -1, ret);
                    if (ret != 0) return ret;
                }
            }
        }
    }
    
    return 0;
}
