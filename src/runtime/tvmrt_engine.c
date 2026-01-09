/**
 * @file tvmrt_engine.c
 * @brief 调度引擎实现
 * 
 * 通过 tvmrt_port 实现可选多线程的 BSP 执行。
 */

#include "tvmrt_engine.h"
#include "tvmrt_port.h"
#include "tvmrt_log.h"
#include <string.h>
#include <stdint.h>

// ============================================================
// 线程池状态
// ============================================================

#if TVMRT_NUM_WORKERS > 0

typedef struct {
    tvmrt_thread_t workers[TVMRT_NUM_WORKERS];
    tvmrt_mutex_t pool_mutex;
    tvmrt_cond_t work_cond;
    tvmrt_barrier_t layer_barrier;
    
    // 任务分发状态 (由 pool_mutex 保护)
    tvmrt_context_t* current_ctx;
    const tvmrt_schedule_layer_t* current_layer;
    int32_t next_task_idx;
    
    // 控制标志
    bool shutdown;
    bool initialized;
} engine_state_t;

static engine_state_t g_engine = {0};

// ============================================================
// Worker 线程函数
// ============================================================

static void* worker_func(void* arg) {
    int worker_id = (int)(intptr_t)arg;
    (void)worker_id;  // 用于日志记录
    
    while (1) {
        tvmrt_mutex_lock(&g_engine.pool_mutex);
        
        // 等待任务或停机信号
        while (!g_engine.shutdown && 
               (g_engine.current_layer == NULL || 
                g_engine.next_task_idx >= g_engine.current_layer->count)) {
            tvmrt_cond_wait(&g_engine.work_cond, &g_engine.pool_mutex);
        }
        
        if (g_engine.shutdown) {
            tvmrt_mutex_unlock(&g_engine.pool_mutex);
            break;
        }
        
        // 领取任务
        int32_t task_idx = g_engine.next_task_idx++;
        const tvmrt_schedule_layer_t* layer = g_engine.current_layer;
        tvmrt_context_t* ctx = g_engine.current_ctx;
        
        tvmrt_mutex_unlock(&g_engine.pool_mutex);
        
        // 如果任务有效则执行
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
            
            // 通知完成
            tvmrt_barrier_arrive(&g_engine.layer_barrier);
        }
    }
    
    return NULL;
}

#endif  // TVMRT_NUM_WORKERS > 0

// ============================================================
// 引擎 API 实现
// ============================================================

int tvmrt_engine_init(void) {
#if TVMRT_NUM_WORKERS > 0
    if (g_engine.initialized) {
        return 0;  // 已初始化
    }
    
    // 初始化同步原语
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
    
    // 创建 Worker 线程
    for (int i = 0; i < TVMRT_NUM_WORKERS; i++) {
        if (tvmrt_thread_create(&g_engine.workers[i], worker_func, (void*)(intptr_t)i) != TVMRT_OK) {
            // 失败时清理
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
    
    // 发送停机信号
    tvmrt_mutex_lock(&g_engine.pool_mutex);
    g_engine.shutdown = true;
    tvmrt_cond_broadcast(&g_engine.work_cond);
    tvmrt_mutex_unlock(&g_engine.pool_mutex);
    
    // 等待所有 Worker 完成
    for (int i = 0; i < TVMRT_NUM_WORKERS; i++) {
        tvmrt_thread_join(&g_engine.workers[i]);
    }
    
    // 清理
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
        // 回退到单线程模式
        return tvmrt_engine_run_single(ctx, schedule);
    }
    
    // 逐层执行
    for (int32_t layer_idx = 0; layer_idx < schedule->layer_count; layer_idx++) {
        const tvmrt_schedule_layer_t* layer = &schedule->layers[layer_idx];
        
        if (layer->count == 0) {
            continue;
        }
        
        if (layer->count == 1) {
            // 单任务: 直接执行 (避免线程池开销)
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
            // 多任务: 分发给 Worker
            tvmrt_barrier_reset(&g_engine.layer_barrier, layer->count);
            
            tvmrt_mutex_lock(&g_engine.pool_mutex);
            g_engine.current_ctx = ctx;
            g_engine.current_layer = layer;
            g_engine.next_task_idx = 0;
            tvmrt_cond_broadcast(&g_engine.work_cond);
            tvmrt_mutex_unlock(&g_engine.pool_mutex);
            
            // 等待所有任务完成
            tvmrt_barrier_sync(&g_engine.layer_barrier);
            
            // 清空 layer 指针使 Worker 回到等待状态
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
    
    // 简单串行执行
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
