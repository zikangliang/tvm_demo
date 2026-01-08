// tvm target: c -keys=cpu 
#define TVM_EXPORTS

// #include "tvm/runtime/c_runtime_api.h"
// #include "tvm/runtime/c_backend_api.h"
#include <math.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <pthread.h>


// 定义队列大小
#define MAX_QUEUE_SIZE 64

// DAG 相关常量
#define MAX_NODES 64       // 最大节点数
#define MAX_DEPS 8         // 每个节点最大依赖数
#define MAX_SUCCESSORS 8   // 每个节点最大后继数

typedef int32_t (*operator_func_t)(void* args);

// 算子任务结构体
typedef struct {
    operator_func_t func;  // 算子函数指针
    void* args;            // 算子参数
} OperatorTask;

// DAG 节点结构体
typedef struct {
    int32_t id;                         // 节点 ID
    OperatorTask task;                  // 算子任务 (复用 OperatorTask)
    
    // 依赖管理 (前驱节点)
    int32_t dep_count;                  // 依赖数量 (入度)
    int32_t deps[MAX_DEPS];             // 依赖的节点 ID 列表
    int32_t remaining_deps;             // 运行时：剩余未完成的依赖数
    
    // 后继管理 (用于通知依赖此节点的其他节点)
    int32_t successor_count;            // 后继节点数量
    int32_t successors[MAX_SUCCESSORS]; // 后继节点 ID 列表
    
    // 状态标记
    bool completed;                     // 是否已执行完成
} DAGNode;

// DAG 图结构体
typedef struct {
    DAGNode nodes[MAX_NODES];           // 节点数组
    int32_t node_count;                 // 当前节点数量
    pthread_mutex_t mutex;              // 互斥锁 (为多线程预留)
} DAG;

// 全局 DAG 实例
static DAG g_dag;

// ============================================================
// DAG 操作函数
// ============================================================

// 初始化 DAG
void dag_init(DAG* dag) {
    dag->node_count = 0;
    pthread_mutex_init(&dag->mutex, NULL);
    // 初始化所有节点
    for (int32_t i = 0; i < MAX_NODES; i++) {
        dag->nodes[i].id = -1;
        dag->nodes[i].task.func = NULL;
        dag->nodes[i].task.args = NULL;
        dag->nodes[i].dep_count = 0;
        dag->nodes[i].remaining_deps = 0;
        dag->nodes[i].successor_count = 0;
        dag->nodes[i].completed = false;
    }
}

// 添加节点，返回节点 ID (-1 表示失败)
int32_t dag_add_node(DAG* dag, operator_func_t func, void* args) {
    if (dag->node_count >= MAX_NODES) {
        printf("Error: DAG node limit reached!\n");
        return -1;
    }
    
    int32_t id = dag->node_count;
    DAGNode* node = &dag->nodes[id];
    
    node->id = id;
    node->task.func = func;
    node->task.args = args;
    node->dep_count = 0;
    node->remaining_deps = 0;
    node->successor_count = 0;
    node->completed = false;
    
    dag->node_count++;
    return id;
}

// 添加依赖边: node_id 依赖于 dep_id (dep_id 必须先执行)
// 返回 0 成功，-1 失败
int32_t dag_add_dep(DAG* dag, int32_t node_id, int32_t dep_id) {
    // 参数校验
    if (node_id < 0 || node_id >= dag->node_count ||
        dep_id < 0 || dep_id >= dag->node_count) {
        printf("Error: Invalid node ID!\n");
        return -1;
    }
    
    DAGNode* node = &dag->nodes[node_id];
    DAGNode* dep = &dag->nodes[dep_id];
    
    // 检查依赖数量限制
    if (node->dep_count >= MAX_DEPS) {
        printf("Error: Node %d dependency limit reached!\n", node_id);
        return -1;
    }
    if (dep->successor_count >= MAX_SUCCESSORS) {
        printf("Error: Node %d successor limit reached!\n", dep_id);
        return -1;
    }
    
    // 添加依赖关系
    node->deps[node->dep_count++] = dep_id;
    node->remaining_deps++;
    
    // 添加后继关系 (反向引用，用于通知)
    dep->successors[dep->successor_count++] = node_id;
    
    return 0;
}

// 销毁 DAG
void dag_destroy(DAG* dag) {
    pthread_mutex_destroy(&dag->mutex);
    dag->node_count = 0;
}

// ============================================================
// 节点就绪队列 (存储节点 ID，线程安全)
// ============================================================
typedef struct {
    int32_t node_ids[MAX_NODES];        // 存储就绪节点 ID
    int32_t head;
    int32_t tail;
    int32_t count;
    pthread_mutex_t mutex;
} NodeQueue;

// 节点队列操作函数
void node_queue_init(NodeQueue* q) {
    q->head = 0;
    q->tail = 0;
    q->count = 0;
    pthread_mutex_init(&q->mutex, NULL);
}

void node_queue_destroy(NodeQueue* q) {
    pthread_mutex_destroy(&q->mutex);
}

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
    pthread_mutex_unlock(&q->mutex);
    return 0;
}

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

// ============================================================
// DAG 拓扑排序调度 (Kahn 算法 + NodeQueue)
// ============================================================

// 执行 DAG 中所有节点，按拓扑序调度
// 返回 0 成功，-1 失败
int32_t dag_run(DAG* dag, NodeQueue* ready_queue) {
    if (dag->node_count == 0) {
        return 0;  // 空图直接返回
    }
    
    // 初始化：重置所有节点状态，将入度为 0 的节点入队
    for (int32_t i = 0; i < dag->node_count; i++) {
        dag->nodes[i].remaining_deps = dag->nodes[i].dep_count;
        dag->nodes[i].completed = false;
        if (dag->nodes[i].remaining_deps == 0) {
            node_queue_push(ready_queue, i);
        }
    }
    
    int32_t executed_count = 0;
    int32_t node_id;
    
    // Kahn 算法主循环
    while (node_queue_pop(ready_queue, &node_id) == 0) {
        DAGNode* node = &dag->nodes[node_id];
        
        // 执行算子
        int32_t ret = node->task.func(node->task.args);
        if (ret != 0) {
            printf("Error: Node %d operator execution failed with code %d\n", node_id, ret);
            return ret;
        }
        
        node->completed = true;
        executed_count++;
        
        // 通知后继节点：减少其剩余依赖数
        for (int32_t i = 0; i < node->successor_count; i++) {
            int32_t succ_id = node->successors[i];
            DAGNode* succ = &dag->nodes[succ_id];
            succ->remaining_deps--;
            
            // 如果后继节点的所有依赖都已完成，入队
            if (succ->remaining_deps == 0) {
                node_queue_push(ready_queue, succ_id);
            }
        }
    }
    
    // 检查是否所有节点都已执行 (检测循环依赖)
    if (executed_count != dag->node_count) {
        printf("Error: DAG has cycle! Executed %d/%d nodes.\n", executed_count, dag->node_count);
        return -1;
    }
    
    return 0;
}



// 线程安全队列 (保留用于其他场景)
typedef struct {
    OperatorTask tasks[MAX_QUEUE_SIZE];
    int32_t head;
    int32_t tail;
    int32_t count;
    pthread_mutex_t mutex;
} TaskQueue;

// 全局队列和互斥锁
static TaskQueue g_task_queue;

// param q: 队列指针
void queue_init(TaskQueue* q) {
    q->head = 0;
    q->tail = 0;
    q->count = 0;
    pthread_mutex_init(&q->mutex, NULL);
}

// param q: 队列指针
void queue_destroy(TaskQueue* q) {
    pthread_mutex_destroy(&q->mutex);
}

// param q: 队列指针
// param task: 要入队的 Task
int32_t queue_push(TaskQueue* q, OperatorTask task) {
    pthread_mutex_lock(&q->mutex);
    if (q->count >= MAX_QUEUE_SIZE) {
        pthread_mutex_unlock(&q->mutex);
        printf("Error: Queue is full!\n");
        return -1;
    }
    q->tasks[q->tail] = task;
    q->tail = (q->tail + 1) % MAX_QUEUE_SIZE;
    q->count++;
    pthread_mutex_unlock(&q->mutex);
    return 0;
}

// param q: 队列指针
// param out_task: 输出的 Task
int32_t queue_pop(TaskQueue* q, OperatorTask* out_task) {
    pthread_mutex_lock(&q->mutex);
    if (q->count == 0) {
        pthread_mutex_unlock(&q->mutex);
        return -1;  // 队列为空
    }
    *out_task = q->tasks[q->head];
    q->head = (q->head + 1) % MAX_QUEUE_SIZE;
    q->count--;
    pthread_mutex_unlock(&q->mutex);
    return 0;
}

// run() 函数 - 执行队列中的所有任务
// param q: 队列指针
int32_t run(TaskQueue* q) {
    OperatorTask task;
    while (queue_pop(q, &task) == 0) { // 还能取出 Task 就执行
        int32_t ret = task.func(task.args);
        if (ret != 0) {
            printf("Error: Operator execution failed with code %d\n", ret);
            return ret;
        }
    }
    return 0;
}

// ============================================================
// 原始 fused 算子实现
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

// __tvm_main__ - 使用 DAG 拓扑排序调度算子
#ifdef __cplusplus
extern "C"
#endif
// 使用 DAG 构建算子依赖图，通过拓扑排序确定执行顺序
int32_t tvmgen_default___tvm_main__(float* input_buffer_var, float* output_buffer_var, uint8_t* global_const_workspace_0_var, uint8_t* global_workspace_1_var) {
    // 分配中间缓冲区指针
    float* sid_1 = (float*)(&(global_workspace_1_var[16]));
    float* sid_2 = (float*)(&(global_workspace_1_var[0]));
    float* sid_3 = (float*)(&(global_workspace_1_var[32]));
    float* sid_4 = (float*)(&(global_workspace_1_var[16]));  // 复用 sid_1
    float* sid_5 = (float*)(&(global_workspace_1_var[32]));  // 复用 sid_3

    // 初始化 DAG 和就绪队列
    dag_init(&g_dag);
    static NodeQueue ready_queue;
    node_queue_init(&ready_queue);

    // 准备算子参数 (静态分配)
    static FusedAddArgs args1, args2, args3, args4, args5;
    static FusedAdd3Args args6;

    // 构建 DAG: 添加节点
    // Node 0: fused_add(input, sid_1) -> input + 1.0
    args1 = (FusedAddArgs){input_buffer_var, sid_1, global_const_workspace_0_var, global_workspace_1_var};
    int32_t n0 = dag_add_node(&g_dag, wrapped_fused_add, &args1);

    // Node 1: fused_subtract(sid_1, sid_2) -> sid_1 - 2.0
    args2 = (FusedAddArgs){sid_1, sid_2, global_const_workspace_0_var, global_workspace_1_var};
    int32_t n1 = dag_add_node(&g_dag, wrapped_fused_subtract, &args2);

    // Node 2: fused_add_1(input, sid_3) -> input + 3.0
    args3 = (FusedAddArgs){input_buffer_var, sid_3, global_const_workspace_0_var, global_workspace_1_var};
    int32_t n2 = dag_add_node(&g_dag, wrapped_fused_add_1, &args3);

    // Node 3: fused_subtract_1(sid_3, sid_4) -> sid_3 - 4.0
    args4 = (FusedAddArgs){sid_3, sid_4, global_const_workspace_0_var, global_workspace_1_var};
    int32_t n3 = dag_add_node(&g_dag, wrapped_fused_subtract_1, &args4);

    // Node 4: fused_add_2(sid_4, sid_5) -> sid_4 + 5.0
    args5 = (FusedAddArgs){sid_4, sid_5, global_const_workspace_0_var, global_workspace_1_var};
    int32_t n4 = dag_add_node(&g_dag, wrapped_fused_add_2, &args5);

    // Node 5: fused_add_3(sid_2, sid_5, output) -> sid_2 + sid_5
    args6 = (FusedAdd3Args){sid_2, sid_5, output_buffer_var, global_const_workspace_0_var, global_workspace_1_var};
    int32_t n5 = dag_add_node(&g_dag, wrapped_fused_add_3, &args6);

    // 构建 DAG: 添加依赖边
    // n1 依赖 n0: fused_subtract 需要 fused_add 的输出 (sid_1)
    dag_add_dep(&g_dag, n1, n0);
    
    // n3 依赖 n2: fused_subtract_1 需要 fused_add_1 的输出 (sid_3)
    dag_add_dep(&g_dag, n3, n2);
    
    // n4 依赖 n3: fused_add_2 需要 fused_subtract_1 的输出 (sid_4)
    dag_add_dep(&g_dag, n4, n3);
    
    // n5 依赖 n1 和 n4: fused_add_3 需要 sid_2 和 sid_5
    dag_add_dep(&g_dag, n5, n1);
    dag_add_dep(&g_dag, n5, n4);

    // 使用拓扑排序执行 DAG
    int32_t ret = dag_run(&g_dag, &ready_queue);

    // 清理
    node_queue_destroy(&ready_queue);
    dag_destroy(&g_dag);

    return ret;
}
