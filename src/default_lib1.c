// tvm target: c -keys=cpu 
#define TVM_EXPORTS

// #include "tvm/runtime/c_runtime_api.h"
// #include "tvm/runtime/c_backend_api.h"
#include <math.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>  // C11 原子操作
#include <stdio.h>
#include <pthread.h>

// ============================================================
// 常量定义
// ============================================================
#define MAX_QUEUE_SIZE 64
#define MAX_NODES 64
#define MAX_DEPS 8
#define MAX_SUCCESSORS 8
#define NUM_GRAPH_NODES 6    // 当前图的实际节点数

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
// 静态图数据结构 (Step 1: 数据结构重构)
// ============================================================

// OpMetadata: 只读静态图节点描述
typedef struct {
    const char* name;                   // 调试用节点名称
    operator_func_t func;               // 算子函数指针
    void* args;                         // 参数指针
    int32_t dep_count;                  // 原始入度
    int32_t successor_count;            // 后继节点数量
    int32_t successors[MAX_SUCCESSORS]; // 后继节点索引
} OpMetadata;

// RuntimeState: 运行时可变状态 (原子类型)
typedef struct {
    _Atomic int32_t remaining_deps;     // 剩余依赖数
    _Atomic bool is_completed;          // 是否完成
} RuntimeState;

// ============================================================
// 算子参数结构体 (提升到文件级)
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
    {
        .name = "fused_add_0",
        .func = wrapped_fused_add,
        .args = &g_args_node0,
        .dep_count = 0,
        .successor_count = 1,
        .successors = {1}
    },
    // Node 1: fused_subtract(sid_1, sid_2) -> sid_1 - 2.0
    {
        .name = "fused_subtract_0",
        .func = wrapped_fused_subtract,
        .args = &g_args_node1,
        .dep_count = 1,
        .successor_count = 1,
        .successors = {5}
    },
    // Node 2: fused_add_1(input, sid_3) -> input + 3.0
    {
        .name = "fused_add_1",
        .func = wrapped_fused_add_1,
        .args = &g_args_node2,
        .dep_count = 0,
        .successor_count = 1,
        .successors = {3}
    },
    // Node 3: fused_subtract_1(sid_3, sid_4) -> sid_3 - 4.0
    {
        .name = "fused_subtract_1",
        .func = wrapped_fused_subtract_1,
        .args = &g_args_node3,
        .dep_count = 1,
        .successor_count = 1,
        .successors = {4}
    },
    // Node 4: fused_add_2(sid_4, sid_5) -> sid_4 + 5.0
    {
        .name = "fused_add_2",
        .func = wrapped_fused_add_2,
        .args = &g_args_node4,
        .dep_count = 1,
        .successor_count = 1,
        .successors = {5}
    },
    // Node 5: fused_add_3(sid_2, sid_5, output) -> sid_2 + sid_5
    {
        .name = "fused_add_3",
        .func = wrapped_fused_add_3,
        .args = &g_args_node5,
        .dep_count = 2,
        .successor_count = 0,
        .successors = {}
    }
};

// 运行时状态数组
static RuntimeState g_runtime_state[NUM_GRAPH_NODES];

// ============================================================
// 节点就绪队列 (线程安全，支持阻塞等待)
// ============================================================
typedef struct {
    int32_t node_ids[MAX_NODES];        // 存储就绪节点 ID
    int32_t head;
    int32_t tail;
    int32_t count;
    pthread_mutex_t mutex;
    pthread_cond_t cond;                // 条件变量：队列非空时唤醒
    bool shutdown;                      // 停机标志
} NodeQueue;

// 全局就绪队列
static NodeQueue g_ready_queue;

// 初始化队列
void node_queue_init(NodeQueue* q) {
    q->head = 0;
    q->tail = 0;
    q->count = 0;
    q->shutdown = false;
    pthread_mutex_init(&q->mutex, NULL);
    pthread_cond_init(&q->cond, NULL);
}

// 销毁队列
void node_queue_destroy(NodeQueue* q) {
    pthread_mutex_destroy(&q->mutex);
    pthread_cond_destroy(&q->cond);
}

// 入队：添加节点并唤醒等待的 Worker
int32_t node_queue_push(NodeQueue* q, int32_t node_id) {
    pthread_mutex_lock(&q->mutex);
    if (q->count >= MAX_NODES) {
        pthread_mutex_unlock(&q->mutex);
        printf("Error: Node queue is full!\n");
        return -1;
    }
    q->node_ids[q->tail] = node_id;
    q->tail = (q->tail + 1) % MAX_NODES;
    q->count++;
    pthread_cond_signal(&q->cond);  // 唤醒一个等待的 Worker
    pthread_mutex_unlock(&q->mutex);
    return 0;
}

// 非阻塞出队（兼容旧代码）
int32_t node_queue_pop(NodeQueue* q, int32_t* out_node_id) {
    pthread_mutex_lock(&q->mutex);
    if (q->count == 0) {
        pthread_mutex_unlock(&q->mutex);
        return -1;  // 队列为空
    }
    *out_node_id = q->node_ids[q->head];
    q->head = (q->head + 1) % MAX_NODES;
    q->count--;
    pthread_mutex_unlock(&q->mutex);
    return 0;
}

// 阻塞出队：等待直到队列非空或收到停机信号
// 返回: 0=成功取出, -1=收到停机信号
int32_t node_queue_pop_blocking(NodeQueue* q, int32_t* out_node_id) {
    pthread_mutex_lock(&q->mutex);
    
    // 等待队列非空或停机
    while (q->count == 0 && !q->shutdown) {
        pthread_cond_wait(&q->cond, &q->mutex);
    }
    
    // 检查是否是停机信号
    if (q->shutdown && q->count == 0) {
        pthread_mutex_unlock(&q->mutex);
        return -1;  // 停机退出
    }
    
    // 正常出队
    *out_node_id = q->node_ids[q->head];
    q->head = (q->head + 1) % MAX_NODES;
    q->count--;
    pthread_mutex_unlock(&q->mutex);
    return 0;
}

// 发送停机信号：唤醒所有等待的 Worker
void node_queue_shutdown(NodeQueue* q) {
    pthread_mutex_lock(&q->mutex);
    q->shutdown = true;
    pthread_cond_broadcast(&q->cond);  // 唤醒所有等待的线程
    pthread_mutex_unlock(&q->mutex);
}

// 重置队列（用于下一次推理）
void node_queue_reset(NodeQueue* q) {
    pthread_mutex_lock(&q->mutex);
    q->head = 0;
    q->tail = 0;
    q->count = 0;
    q->shutdown = false;
    pthread_mutex_unlock(&q->mutex);
}

// ============================================================
// 静态图拓扑排序执行 (单线程版，使用 NodeQueue)
// ============================================================

// 初始化运行时状态
void static_graph_init_runtime(void) {
    for (int32_t i = 0; i < NUM_GRAPH_NODES; i++) {
        atomic_store(&g_runtime_state[i].remaining_deps, g_graph_nodes[i].dep_count);
        atomic_store(&g_runtime_state[i].is_completed, false);
    }
}

// 执行静态图，按拓扑序调度
// 返回 0 成功，-1 失败
int32_t static_graph_run(void) {
    // 重置就绪队列
    node_queue_reset(&g_ready_queue);
    
    // 初始化运行时状态
    static_graph_init_runtime();
    
    // 将所有入度为 0 的节点入队
    for (int32_t i = 0; i < NUM_GRAPH_NODES; i++) {
        if (g_graph_nodes[i].dep_count == 0) {
            node_queue_push(&g_ready_queue, i);
        }
    }
    
    int32_t executed_count = 0;
    int32_t node_id;
    
    // Kahn 算法主循环
    while (node_queue_pop(&g_ready_queue, &node_id) == 0) {
        const OpMetadata* node = &g_graph_nodes[node_id];
        
        // 执行算子
        int32_t ret = node->func(node->args);
        if (ret != 0) {
            printf("Error: Node %d (%s) execution failed with code %d\n", 
                   node_id, node->name, ret);
            return ret;
        }
        
        atomic_store(&g_runtime_state[node_id].is_completed, true);
        executed_count++;
        
        // 通知后继节点：减少其剩余依赖数
        for (int32_t i = 0; i < node->successor_count; i++) {
            int32_t succ_id = node->successors[i];
            int32_t remaining = atomic_fetch_sub(&g_runtime_state[succ_id].remaining_deps, 1) - 1;
            
            // 如果后继节点的所有依赖都已完成，入队
            if (remaining == 0) {
                node_queue_push(&g_ready_queue, succ_id);
            }
        }
    }
    
    // 检查是否所有节点都已执行 (检测循环依赖)
    if (executed_count != NUM_GRAPH_NODES) {
        printf("Error: Graph has cycle! Executed %d/%d nodes.\n", executed_count, NUM_GRAPH_NODES);
        return -1;
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

// __tvm_main__ - 使用静态图拓扑排序调度算子
#ifdef __cplusplus
extern "C"
#endif
// 使用编译期静态图，运行时只需初始化参数并执行
int32_t tvmgen_default___tvm_main__(float* input_buffer_var, float* output_buffer_var, uint8_t* global_const_workspace_0_var, uint8_t* global_workspace_1_var) {
    // 分配中间缓冲区指针
    float* sid_1 = (float*)(&(global_workspace_1_var[16]));
    float* sid_2 = (float*)(&(global_workspace_1_var[0]));
    float* sid_3 = (float*)(&(global_workspace_1_var[32]));
    float* sid_4 = (float*)(&(global_workspace_1_var[16]));  // 复用 sid_1
    float* sid_5 = (float*)(&(global_workspace_1_var[32]));  // 复用 sid_3

    // 初始化就绪队列（只需首次调用）
    static bool initialized = false;
    if (!initialized) {
        node_queue_init(&g_ready_queue);
        initialized = true;
    }

    // 初始化全局参数（运行时填充缓冲区指针）
    g_args_node0 = (FusedAddArgs){input_buffer_var, sid_1, global_const_workspace_0_var, global_workspace_1_var};
    g_args_node1 = (FusedAddArgs){sid_1, sid_2, global_const_workspace_0_var, global_workspace_1_var};
    g_args_node2 = (FusedAddArgs){input_buffer_var, sid_3, global_const_workspace_0_var, global_workspace_1_var};
    g_args_node3 = (FusedAddArgs){sid_3, sid_4, global_const_workspace_0_var, global_workspace_1_var};
    g_args_node4 = (FusedAddArgs){sid_4, sid_5, global_const_workspace_0_var, global_workspace_1_var};
    g_args_node5 = (FusedAdd3Args){sid_2, sid_5, output_buffer_var, global_const_workspace_0_var, global_workspace_1_var};

    // 执行静态图
    int32_t ret = static_graph_run();

    return ret;
}
