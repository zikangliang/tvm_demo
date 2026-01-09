// tvm target: c -keys=cpu 
#define TVM_EXPORTS

// #include "tvm/runtime/c_runtime_api.h"
// #include "tvm/runtime/c_backend_api.h"
#include <math.h>
#include <stdint.h>
#include <stdbool.h>
// #include <stdatomic.h>
#include <stdio.h>
#include <pthread.h>

// ============================================================
// 常量定义
// ============================================================
#define NUM_GRAPH_NODES 6

// ============================================================
// 类型定义
// ============================================================
typedef int32_t (*operator_func_t)(void* args);

// 算子任务结构体 (兼容旧代码)
typedef struct {
    operator_func_t func;
    void* args;
} OperatorTask;

// ============================================================
// 静态图节点描述
// ============================================================

typedef struct {
    const char* name;                   // 调试用节点名称
    operator_func_t func;               // 算子函数指针
    void* args;                         // 参数指针
} OpMetadata;

// ============================================================
// 算子参数结构体
// ============================================================

// fused_add 参数结构体
typedef struct {
    float* p0;
    float* output;
    uint8_t* const_ws;
    uint8_t* ws;
} FusedAddArgs;

// fused_add_3 参数结构体 (双输入)
typedef struct {
    float* p0;
    float* p1;
    float* output;
    uint8_t* const_ws;
    uint8_t* ws;
} FusedAdd3Args;

// ============================================================
// 包装函数前置声明
// ============================================================
int32_t wrapped_fused_add(void* args);
int32_t wrapped_fused_add_1(void* args);
int32_t wrapped_fused_add_2(void* args);
int32_t wrapped_fused_add_3(void* args);
int32_t wrapped_fused_subtract(void* args);
int32_t wrapped_fused_subtract_1(void* args);

// ============================================================
// 全局参数变量 (静态分配，可在编译期获取地址)
// ============================================================
static FusedAddArgs g_args_node0;   // fused_add
static FusedAddArgs g_args_node1;   // fused_subtract
static FusedAddArgs g_args_node2;   // fused_add_1
static FusedAddArgs g_args_node3;   // fused_subtract_1
static FusedAddArgs g_args_node4;   // fused_add_2
static FusedAdd3Args g_args_node5;  // fused_add_3

// ============================================================
// 静态图表定义 (编译期常量)
// ============================================================
// 依赖关系:
//   n0 (fused_add)     -> n1 (fused_subtract)
//   n2 (fused_add_1)   -> n3 (fused_subtract_1) -> n4 (fused_add_2)
//   n1 + n4            -> n5 (fused_add_3)

static const OpMetadata g_graph_nodes[NUM_GRAPH_NODES] = {
    // Node 0: fused_add(input, sid_1) -> input + 1.0
    {.name = "fused_add_0",      .func = wrapped_fused_add,        .args = &g_args_node0},
    // Node 1: fused_subtract(sid_1, sid_2) -> sid_1 - 2.0
    {.name = "fused_subtract_0", .func = wrapped_fused_subtract,   .args = &g_args_node1},
    // Node 2: fused_add_1(input, sid_3) -> input + 3.0
    {.name = "fused_add_1",      .func = wrapped_fused_add_1,      .args = &g_args_node2},
    // Node 3: fused_subtract_1(sid_3, sid_4) -> sid_3 - 4.0
    {.name = "fused_subtract_1", .func = wrapped_fused_subtract_1, .args = &g_args_node3},
    // Node 4: fused_add_2(sid_4, sid_5) -> sid_4 + 5.0
    {.name = "fused_add_2",      .func = wrapped_fused_add_2,      .args = &g_args_node4},
    // Node 5: fused_add_3(sid_2, sid_5, output) -> sid_2 + sid_5
    {.name = "fused_add_3",      .func = wrapped_fused_add_3,      .args = &g_args_node5}
};

// g_runtime_state 已移除 - BSP 不需要运行时依赖追踪

// ============================================================
// BSP 静态分层调度
// ============================================================

// TaskItem: 单个任务项
typedef struct {
    int32_t op_index;
} TaskItem;

// StaticLayer: 执行层 (层内并行，层间同步)
typedef struct {
    const TaskItem* tasks;
    int32_t task_count;
} StaticLayer;

// 依赖关系:
//   Layer 1: Node 0, Node 2  (可并行)
//   Layer 2: Node 1, Node 3  (可并行)
//   Layer 3: Node 4          (串行)
//   Layer 4: Node 5          (串行)

// Layer 1: 入度为 0 的初始节点，可并行执行
static const TaskItem g_layer1_tasks[] = {{0}, {2}};

// Layer 2: 依赖 Layer 1 的节点，可并行执行
static const TaskItem g_layer2_tasks[] = {{1}, {3}};

// Layer 3: 依赖 Node 3，单独执行
static const TaskItem g_layer3_tasks[] = {{4}};

// Layer 4: 依赖 Node 1 + Node 4，最终输出
static const TaskItem g_layer4_tasks[] = {{5}};

// 完整静态调度表
#define NUM_LAYERS 4
static const StaticLayer g_static_schedule[NUM_LAYERS] = {
    {g_layer1_tasks, 2},  // Layer 1: Node 0, 2 并行
    {g_layer2_tasks, 2},  // Layer 2: Node 1, 3 并行
    {g_layer3_tasks, 1},  // Layer 3: Node 4 串行
    {g_layer4_tasks, 1}   // Layer 4: Node 5 串行
};

// ============================================================
// ============================================================
// Sync Barrier
// ============================================================

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int32_t count;   // 当前已完成的任务数
    int32_t target;  // 目标任务数 (该层并行度)
} Barrier;

// 全局 Barrier
static Barrier g_barrier;

// 初始化 Barrier
void barrier_init(Barrier* b) {
    pthread_mutex_init(&b->mutex, NULL);
    pthread_cond_init(&b->cond, NULL);
    b->count = 0;
    b->target = 0;
}

// 重置 Barrier (设置新的目标任务数)
void barrier_reset(Barrier* b, int32_t target) {
    pthread_mutex_lock(&b->mutex);
    b->count = 0;
    b->target = target;
    pthread_mutex_unlock(&b->mutex);
}

// Worker 调用：任务完成，计数+1，唤醒主线程
void barrier_wait(Barrier* b) {
    pthread_mutex_lock(&b->mutex);
    b->count++;
    if (b->count >= b->target) {
        pthread_cond_signal(&b->cond); // 唤醒主线程
    }
    pthread_mutex_unlock(&b->mutex);
}

// 主线程调用：等待直到所有任务完成
void barrier_sync(Barrier* b) {
    pthread_mutex_lock(&b->mutex);
    while (b->count < b->target) {
        pthread_cond_wait(&b->cond, &b->mutex);
    }
    pthread_mutex_unlock(&b->mutex);
}

// ============================================================
// ============================================================
// Worker Thread Pool
// ============================================================

#define NUM_WORKERS 4

typedef struct {
    pthread_t threads[NUM_WORKERS];
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    bool shutdown;
    
    // 任务分发状态
    const StaticLayer* current_layer;
    int32_t next_task_idx;   // 下一个待领取的任务索引
} ThreadPool;

static ThreadPool g_pool;
static bool g_pool_initialized = false;

// Worker 线程函数
void* worker_thread_func(void* arg) {
    while (1) {
        pthread_mutex_lock(&g_pool.mutex);
        
        // 等待任务或停机信号
        while ((g_pool.current_layer == NULL) && !g_pool.shutdown) {
            pthread_cond_wait(&g_pool.cond, &g_pool.mutex);
        }
        
        if (g_pool.shutdown) {
            pthread_mutex_unlock(&g_pool.mutex);
            break;
        }
        
        // 尝试领取任务
        const StaticLayer* layer = g_pool.current_layer;
        int32_t task_idx = -1;
        
        if (g_pool.next_task_idx < layer->task_count) {
            task_idx = g_pool.next_task_idx++;
        }
        
        pthread_mutex_unlock(&g_pool.mutex);
        
        // 如果领到了任务，执行它
        if (task_idx >= 0) {
            int32_t op_idx = layer->tasks[task_idx].op_index;
            const OpMetadata* node = &g_graph_nodes[op_idx];
            
            // 执行算子
            node->func(node->args);
            
            // 任务完成，通知 Barrier
            barrier_wait(&g_barrier);
        } else {
            // 当前层任务已被领完，Worker 继续等待下一轮
            // 注意：这里需要再次加锁检查，避免忙等待，实际逻辑是回到外层循环 wait
            // 但为了简单，我们让它回到 wait，直到 current_layer 被置为 NULL 后再次被唤醒
            // 实际上 worker 在一层中可能执行多个任务，所以需要在 while(1) 中不断尝试
            // 修正逻辑：Worker 应该在一次唤醒中尽可能多地执行任务
        }
    }
    return NULL;
}

// 修正后的 Worker 逻辑：一次唤醒，由 Loop 处理所有任务
void* worker_thread_func_optimized(void* arg) {
    while (1) {
        pthread_mutex_lock(&g_pool.mutex);
        
        // 等待新的一层开始
        while ((g_pool.current_layer == NULL) && !g_pool.shutdown) {
            pthread_cond_wait(&g_pool.cond, &g_pool.mutex);
        }
        
        if (g_pool.shutdown) {
            pthread_mutex_unlock(&g_pool.mutex);
            break;
        }
        
        // 开始竞争执行当前层的所有任务
        while (g_pool.current_layer != NULL && g_pool.next_task_idx < g_pool.current_layer->task_count) {
             int32_t task_idx = g_pool.next_task_idx++;
             const StaticLayer* layer = g_pool.current_layer;
             
             pthread_mutex_unlock(&g_pool.mutex);
             
             // 执行任务
             int32_t op_idx = layer->tasks[task_idx].op_index;
             const OpMetadata* node = &g_graph_nodes[op_idx];
             node->func(node->args);
             
             // 任务完成，通知 Barrier
             barrier_wait(&g_barrier);
             
             pthread_mutex_lock(&g_pool.mutex);
        }
        
        // 当前层任务全部被领取完，Worker 回去等待 null -> new layer 转换
        pthread_mutex_unlock(&g_pool.mutex);
    }
    return NULL;
}

// 初始化线程池
void thread_pool_init(void) {
    if (g_pool_initialized) return;
    
    pthread_mutex_init(&g_pool.mutex, NULL);
    pthread_cond_init(&g_pool.cond, NULL);
    g_pool.shutdown = false;
    g_pool.current_layer = NULL;
    g_pool.next_task_idx = 0;
    
    for (int i = 0; i < NUM_WORKERS; i++) {
        pthread_create(&g_pool.threads[i], NULL, worker_thread_func_optimized, NULL);
    }
    g_pool_initialized = true;
}

// 销毁线程池
void thread_pool_destroy(void) {
    if (!g_pool_initialized) return;
    
    pthread_mutex_lock(&g_pool.mutex);
    g_pool.shutdown = true;
    pthread_cond_broadcast(&g_pool.cond);
    pthread_mutex_unlock(&g_pool.mutex);
    
    for (int i = 0; i < NUM_WORKERS; i++) {
        pthread_join(g_pool.threads[i], NULL);
    }
    
    pthread_mutex_destroy(&g_pool.mutex);
    pthread_cond_destroy(&g_pool.cond);
    g_pool_initialized = false;
}

// ============================================================
// ============================================================
// BSP 执行函数
// ============================================================

int32_t static_schedule_run(void) {
    // 确保线程池已初始化
    if (!g_pool_initialized) {
        thread_pool_init();
    }

    for (int32_t layer = 0; layer < NUM_LAYERS; layer++) {
        const StaticLayer* current = &g_static_schedule[layer];
        
        if (current->task_count == 1) {
            // 单任务：直接在主线程执行 (低开销)
            int32_t op_idx = current->tasks[0].op_index;
            int32_t ret = g_graph_nodes[op_idx].func(g_graph_nodes[op_idx].args);
            if (ret != 0) return ret;
        } else {
            // 多任务：分发给 Worker
            
            // 1. 设置 Barrier 目标 (等待 task_count 个任务完成)
            barrier_reset(&g_barrier, current->task_count);
            
            // 2. 设置线程池工作状态
            pthread_mutex_lock(&g_pool.mutex);
            g_pool.current_layer = current;
            g_pool.next_task_idx = 0;
            pthread_cond_broadcast(&g_pool.cond); // 唤醒所有 Worker
            pthread_mutex_unlock(&g_pool.mutex);
            
            // 3. 主线程等待本层所有任务完成
            barrier_sync(&g_barrier);
            
            // 4. 清理状态 (可选，防止 Worker 误判)
            pthread_mutex_lock(&g_pool.mutex);
            g_pool.current_layer = NULL;
            pthread_mutex_unlock(&g_pool.mutex);
        }
    }
    
    return 0;
}

// ============================================================
// 原始 fused 算子实现
// ============================================================
// 注: FusedAddArgs 和 FusedAdd3Args 已在文件前部定义

#ifdef __cplusplus
extern "C"
#endif
int32_t tvmgen_default_fused_add(float* p0, float* T_add, uint8_t* global_const_workspace_2_var, uint8_t* global_workspace_3_var) {
    void* fused_constant_let = (&(global_const_workspace_2_var[64]));
    T_add[0] = (p0[0] + ((float*)fused_constant_let)[0]);
    return 0;
}

#ifdef __cplusplus
extern "C"
#endif
int32_t tvmgen_default_fused_add_1(float* p0, float* T_add, uint8_t* global_const_workspace_6_var, uint8_t* global_workspace_7_var) {
    void* fused_constant_2_let = (&(global_const_workspace_6_var[32]));
    T_add[0] = (p0[0] + ((float*)fused_constant_2_let)[0]);
    return 0;
}

#ifdef __cplusplus
extern "C"
#endif
int32_t tvmgen_default_fused_add_2(float* p0, float* T_add, uint8_t* global_const_workspace_10_var, uint8_t* global_workspace_11_var) {
    void* fused_constant_4_let = (&(global_const_workspace_10_var[0]));
    T_add[0] = (p0[0] + ((float*)fused_constant_4_let)[0]);
    return 0;
}

#ifdef __cplusplus
extern "C"
#endif
int32_t tvmgen_default_fused_add_3(float* p0, float* p1, float* T_add, uint8_t* global_const_workspace_12_var, uint8_t* global_workspace_13_var) {
    T_add[0] = (p0[0] + p1[0]);
    return 0;
}

#ifdef __cplusplus
extern "C"
#endif
int32_t tvmgen_default_fused_subtract(float* p0, float* T_subtract, uint8_t* global_const_workspace_4_var, uint8_t* global_workspace_5_var) {
    void* fused_constant_1_let = (&(global_const_workspace_4_var[48]));
    T_subtract[0] = (p0[0] - ((float*)fused_constant_1_let)[0]);
    return 0;
}

#ifdef __cplusplus
extern "C"
#endif
int32_t tvmgen_default_fused_subtract_1(float* p0, float* T_subtract, uint8_t* global_const_workspace_8_var, uint8_t* global_workspace_9_var) {
    void* fused_constant_3_let = (&(global_const_workspace_8_var[16]));
    T_subtract[0] = (p0[0] - ((float*)fused_constant_3_let)[0]);
    return 0;
}

// 包装函数 - 将 fused 函数适配为统一的 operator_func_t 签名
int32_t wrapped_fused_add(void* args) {
    FusedAddArgs* a = (FusedAddArgs*)args;
    return tvmgen_default_fused_add(a->p0, a->output, a->const_ws, a->ws);
}

int32_t wrapped_fused_add_1(void* args) {
    FusedAddArgs* a = (FusedAddArgs*)args;
    return tvmgen_default_fused_add_1(a->p0, a->output, a->const_ws, a->ws);
}

int32_t wrapped_fused_add_2(void* args) {
    FusedAddArgs* a = (FusedAddArgs*)args;
    return tvmgen_default_fused_add_2(a->p0, a->output, a->const_ws, a->ws);
}

int32_t wrapped_fused_add_3(void* args) {
    FusedAdd3Args* a = (FusedAdd3Args*)args;
    return tvmgen_default_fused_add_3(a->p0, a->p1, a->output, a->const_ws, a->ws);
}

int32_t wrapped_fused_subtract(void* args) {
    FusedAddArgs* a = (FusedAddArgs*)args;
    return tvmgen_default_fused_subtract(a->p0, a->output, a->const_ws, a->ws);
}

int32_t wrapped_fused_subtract_1(void* args) {
    FusedAddArgs* a = (FusedAddArgs*)args;
    return tvmgen_default_fused_subtract_1(a->p0, a->output, a->const_ws, a->ws);
}

// __tvm_main__ - BSP 静态分层调度入口
#ifdef __cplusplus
extern "C"
#endif
// 运行时只需初始化参数并执行静态图
int32_t tvmgen_default___tvm_main__(float* input_buffer_var, float* output_buffer_var, uint8_t* global_const_workspace_0_var, uint8_t* global_workspace_1_var) {
    // 分配中间缓冲区指针
    float* sid_1 = (float*)(&(global_workspace_1_var[16]));
    float* sid_2 = (float*)(&(global_workspace_1_var[0]));
    float* sid_3 = (float*)(&(global_workspace_1_var[32]));
    float* sid_4 = (float*)(&(global_workspace_1_var[16]));  // 复用 sid_1
    float* sid_5 = (float*)(&(global_workspace_1_var[32]));  // 复用 sid_3

    // 初始化全局参数（运行时填充缓冲区指针）
    g_args_node0 = (FusedAddArgs){input_buffer_var, sid_1, global_const_workspace_0_var, global_workspace_1_var};
    g_args_node1 = (FusedAddArgs){sid_1, sid_2, global_const_workspace_0_var, global_workspace_1_var};
    g_args_node2 = (FusedAddArgs){input_buffer_var, sid_3, global_const_workspace_0_var, global_workspace_1_var};
    g_args_node3 = (FusedAddArgs){sid_3, sid_4, global_const_workspace_0_var, global_workspace_1_var};
    g_args_node4 = (FusedAddArgs){sid_4, sid_5, global_const_workspace_0_var, global_workspace_1_var};
    g_args_node5 = (FusedAdd3Args){sid_2, sid_5, output_buffer_var, global_const_workspace_0_var, global_workspace_1_var};

    // 执行 BSP 静态分层调度
    int32_t ret = static_schedule_run();

    return ret;
}
