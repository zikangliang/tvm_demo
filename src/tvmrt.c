/**
 * @file tvmrt.c
 * @brief TVM Runtime 统一实现
 * 
 * 本文件包含完整的 TVM Runtime 实现，合并自：
 * - tvmrt_log.c (日志系统)
 * - tvmrt_semantic.c (语义转换层)
 * - tvmrt_engine.c (调度引擎)
 */

#include "tvmrt.h"
#include <string.h>
#include <stdint.h>

// ============================================================
// 日志系统实现
// ============================================================

#if TVMRT_LOG_ENABLE

static tvmrt_log_record_t g_log_buffer[TVMRT_LOG_BUFFER_SIZE];
static int32_t g_log_head = 0;
static int32_t g_log_tail = 0;
static int32_t g_log_count = 0;

static tvmrt_log_callback_t g_log_callback = NULL;
static void* g_log_callback_user = NULL;

void tvmrt_log_set_callback(tvmrt_log_callback_t cb, void* user) {
    g_log_callback = cb;
    g_log_callback_user = user;
}

void tvmrt_log_push(const tvmrt_log_record_t* rec) {
    if (!rec) return;
    
    if (g_log_callback) {
        g_log_callback(rec, g_log_callback_user);
    }
    
    g_log_buffer[g_log_head] = *rec;
    g_log_head = (g_log_head + 1) % TVMRT_LOG_BUFFER_SIZE;
    
    if (g_log_count < TVMRT_LOG_BUFFER_SIZE) {
        g_log_count++;
    } else {
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

// ============================================================
// 语义转换层实现
// ============================================================

void* tvmrt_semantic_resolve_sid(
    uint8_t* workspace,
    const tvmrt_tensor_map_entry_t* tensor_map,
    int32_t tensor_count,
    int32_t sid
) {
    if (!tensor_map || !workspace || sid < 0) {
        return NULL;
    }
    
    for (int32_t i = 0; i < tensor_count; i++) {
        if (tensor_map[i].sid == sid) {
            return workspace + tensor_map[i].offset;
        }
    }
    
    return NULL;
}

int tvmrt_semantic_init(
    tvmrt_context_t* ctx,
    const tvmrt_model_desc_t* model
) {
    if (!ctx || !model) {
        return -1;
    }
    
    // 基本初始化（具体参数填充由 model_data.c 中的函数完成）
    ctx->op_count = model->op_count;
    
    return 0;
}

// ============================================================
// 调度引擎实现
// ============================================================

#if TVMRT_NUM_WORKERS > 0

typedef struct {
    tvmrt_thread_t workers[TVMRT_NUM_WORKERS];
    tvmrt_layer_queue_t task_queue;
    tvmrt_barrier_t layer_barrier;
    
    tvmrt_context_t* current_ctx;
    const tvmrt_schedule_desc_t* current_schedule;
    int32_t current_layer_idx;
    
    bool shutdown;
    bool initialized;
} engine_state_t;

static engine_state_t g_engine = {0};

// Worker 线程函数
static void* worker_func(void* arg) {
    int worker_id = (int)(intptr_t)arg;
    
    while (1) {
        int32_t op_id = -1;
        
        // 阻塞获取任务
        tvmrt_mutex_lock(&g_engine.task_queue.mutex);
        
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
        
        // 链式唤醒下一个 Worker
        if (g_engine.task_queue.count > 0) {
            tvmrt_cond_signal(&g_engine.task_queue.cond);
        }
        
        tvmrt_mutex_unlock(&g_engine.task_queue.mutex);
        
        // 执行算子（在锁外）
        tvmrt_context_t* ctx = g_engine.current_ctx;
        if (op_id >= 0 && op_id < ctx->op_count) {
            tvmrt_op_exec_t* exec = &ctx->op_execs[op_id];
            if (exec->func) {
                // 调度引擎日志已禁用，由包装函数中的参数日志替代
                // TVMRT_LOG_OP_START(op_id, exec->name, worker_id);
                int32_t ret = exec->func(exec->args);
                // TVMRT_LOG_OP_END(op_id, exec->name, worker_id, ret);
                (void)ret;  // 避免未使用变量警告
            }
        }
        
        // 通知完成
        tvmrt_barrier_arrive(&g_engine.layer_barrier);
    }
    
    return NULL;
}

#endif  // TVMRT_NUM_WORKERS > 0

// 引擎 API 实现
int tvmrt_engine_init(void) {
#if TVMRT_NUM_WORKERS > 0
    if (g_engine.initialized) {
        return 0;
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

#if TVMRT_NUM_WORKERS > 0
// 辅助函数：加载下一层任务到队列
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
#endif

int tvmrt_engine_run(
    tvmrt_context_t* ctx,
    const tvmrt_schedule_desc_t* schedule
) {
    if (!ctx || !schedule) {
        return -1;
    }
    
#if TVMRT_NUM_WORKERS > 0
    if (!g_engine.initialized) {
        return tvmrt_engine_run_single(ctx, schedule);
    }
    
    // 设置全局状态
    g_engine.current_ctx = ctx;
    g_engine.current_schedule = schedule;
    g_engine.current_layer_idx = 0;
    
    // 逐层执行
    for (int32_t layer_idx = 0; layer_idx < schedule->layer_count; layer_idx++) {
        const tvmrt_schedule_layer_t* layer = &schedule->layers[layer_idx];

        // 层边界标记
        printf("=== Layer %d (%d op%s) ===\n",
               layer_idx + 1, layer->count, layer->count == 1 ? "" : "s");

        if (layer->count == 0) {
            continue;
        }

        if (layer->count == 1) {
            // 单任务: 直接执行
            int32_t op_idx = layer->op_indices[0];
            if (op_idx >= 0 && op_idx < ctx->op_count) {
                tvmrt_op_exec_t* exec = &ctx->op_execs[op_idx];
                if (exec->func) {
                    // 调度引擎日志已禁用，由包装函数中的参数日志替代
                    // TVMRT_LOG_OP_START(op_idx, exec->name, -1);
                    int32_t ret = exec->func(exec->args);
                    // TVMRT_LOG_OP_END(op_idx, exec->name, -1, ret);
                    if (ret != 0) return ret;
                }
            }
        } else {
            // 多任务: 填充队列并等待完成
            tvmrt_barrier_reset(&g_engine.layer_barrier, layer->count);
            load_next_layer();
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

        // 层边界标记
        printf("=== Layer %d (%d op%s) ===\n",
               layer_idx + 1, layer->count, layer->count == 1 ? "" : "s");

        for (int32_t task_idx = 0; task_idx < layer->count; task_idx++) {
            int32_t op_idx = layer->op_indices[task_idx];
            if (op_idx >= 0 && op_idx < ctx->op_count) {
                tvmrt_op_exec_t* exec = &ctx->op_execs[op_idx];
                if (exec->func) {
                    // 调度引擎日志已禁用，由包装函数中的参数日志替代
                    // TVMRT_LOG_OP_START(op_idx, exec->name, -1);
                    int32_t ret = exec->func(exec->args);
                    // TVMRT_LOG_OP_END(op_idx, exec->name, -1, ret);
                    if (ret != 0) return ret;
                }
            }
        }
    }

    return 0;
}
