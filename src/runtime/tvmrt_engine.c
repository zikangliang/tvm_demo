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
    tvmrt_layer_queue_t task_queue;          // 任务队列 (替代 pool_mutex + work_cond)
    tvmrt_barrier_t layer_barrier;
    
    // 任务分发状态
    tvmrt_context_t* current_ctx;
    const tvmrt_schedule_desc_t* current_schedule;  // 完整调度表
    int32_t current_layer_idx;                      // 当前层索引
    
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
    
    while (1) {
        int32_t op_id = -1;
        
        // === 阻塞获取任务 ===
        tvmrt_mutex_lock(&g_engine.task_queue.mutex);
        
        // 等待队列非空或停机信号
        while (g_engine.task_queue.count == 0 && !g_engine.shutdown) {
            tvmrt_cond_wait(&g_engine.task_queue.cond, &g_engine.task_queue.mutex);
        }
        
        if (g_engine.shutdown) {
            tvmrt_mutex_unlock(&g_engine.task_queue.mutex);
            break;
        }
        
        // 从队列头取任务
        op_id = g_engine.task_queue.tasks[g_engine.task_queue.head];
        g_engine.task_queue.head++;
        g_engine.task_queue.count--;
        
        // === 链式唤醒：唤醒下一个 Worker ===
        if (g_engine.task_queue.count > 0) {
            tvmrt_cond_signal(&g_engine.task_queue.cond);
        }
        
        tvmrt_mutex_unlock(&g_engine.task_queue.mutex);
        
        // === 执行算子（在锁外） ===
        tvmrt_context_t* ctx = g_engine.current_ctx;
        if (op_id >= 0 && op_id < ctx->op_count) {
            tvmrt_op_exec_t* exec = &ctx->op_execs[op_id];
            if (exec->func) {
                TVMRT_LOG_OP_START(op_id, exec->name, worker_id);
                int32_t ret = exec->func(exec->args);
                TVMRT_LOG_OP_END(op_id, exec->name, worker_id, ret);
            }
        }
        
        // === 通知完成 ===
        tvmrt_barrier_arrive(&g_engine.layer_barrier);
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
    
    // 初始化任务队列
    if (tvmrt_mutex_init(&g_engine.task_queue.mutex) != TVMRT_OK) {
        return -1;
    }
    if (tvmrt_cond_init(&g_engine.task_queue.cond) != TVMRT_OK) {
        tvmrt_mutex_destroy(&g_engine.task_queue.mutex);
        return -1;
    }
    g_engine.task_queue.head = 0;
    g_engine.task_queue.tail = 0;
    g_engine.task_queue.count = 0;
    
    // 初始化 barrier
    if (tvmrt_barrier_init(&g_engine.layer_barrier) != TVMRT_OK) {
        tvmrt_cond_destroy(&g_engine.task_queue.cond);
        tvmrt_mutex_destroy(&g_engine.task_queue.mutex);
        return -1;
    }
    
    g_engine.shutdown = false;
    g_engine.current_schedule = NULL;
    g_engine.current_ctx = NULL;
    
    // 创建 Worker 线程
    for (int i = 0; i < TVMRT_NUM_WORKERS; i++) {
        if (tvmrt_thread_create(&g_engine.workers[i], worker_func, (void*)(intptr_t)i) != TVMRT_OK) {
            // 失败时清理
            g_engine.shutdown = true;
            tvmrt_cond_broadcast(&g_engine.task_queue.cond);
            for (int j = 0; j < i; j++) {
                tvmrt_thread_join(&g_engine.workers[j]);
            }
            tvmrt_barrier_destroy(&g_engine.layer_barrier);
            tvmrt_cond_destroy(&g_engine.task_queue.cond);
            tvmrt_mutex_destroy(&g_engine.task_queue.mutex);
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
    tvmrt_mutex_lock(&g_engine.task_queue.mutex);
    g_engine.shutdown = true;
    tvmrt_cond_broadcast(&g_engine.task_queue.cond);
    tvmrt_mutex_unlock(&g_engine.task_queue.mutex);
    
    // 等待所有 Worker 完成
    for (int i = 0; i < TVMRT_NUM_WORKERS; i++) {
        tvmrt_thread_join(&g_engine.workers[i]);
    }
    
    // 清理
    tvmrt_barrier_destroy(&g_engine.layer_barrier);
    tvmrt_cond_destroy(&g_engine.task_queue.cond);
    tvmrt_mutex_destroy(&g_engine.task_queue.mutex);
    
    g_engine.initialized = false;
#endif
}

// ============================================================
// 辅助函数：加载下一层任务到队列
// ============================================================

static void load_next_layer(void) {
    const tvmrt_schedule_layer_t* layer = 
        &g_engine.current_schedule->layers[g_engine.current_layer_idx];
    
    tvmrt_mutex_lock(&g_engine.task_queue.mutex);
    
    // 填充队列
    g_engine.task_queue.head = 0;
    g_engine.task_queue.tail = layer->count;
    g_engine.task_queue.count = layer->count;
    for (int32_t i = 0; i < layer->count; i++) {
        g_engine.task_queue.tasks[i] = layer->op_indices[i];
    }
    
    // 唤醒一个 Worker 开始链式执行
    if (g_engine.task_queue.count > 0) {
        tvmrt_cond_signal(&g_engine.task_queue.cond);
    }
    
    tvmrt_mutex_unlock(&g_engine.task_queue.mutex);
    
    g_engine.current_layer_idx++;
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
    
    // 设置全局状态
    g_engine.current_ctx = ctx;
    g_engine.current_schedule = schedule;
    g_engine.current_layer_idx = 0;
    
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
            // 多任务: 填充队列并等待完成
            tvmrt_barrier_reset(&g_engine.layer_barrier, layer->count);
            
            load_next_layer();
            
            // 等待所有任务完成
            tvmrt_barrier_sync(&g_engine.layer_barrier);
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
