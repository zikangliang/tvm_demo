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


typedef int32_t (*operator_func_t)(void* args);
typedef struct {
    operator_func_t func;  // 算子函数指针
    void* args;            // 算子参数
} OperatorTask;

// 线程安全队列
typedef struct {
    OperatorTask tasks[MAX_QUEUE_SIZE];
    int head;
    int tail;
    int count;
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
int queue_push(TaskQueue* q, OperatorTask task) {
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
int queue_pop(TaskQueue* q, OperatorTask* out_task) {
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

// __tvm_main__ - 使用队列调度算子
#ifdef __cplusplus
extern "C"
#endif
// 这里该了什么呢？就是先把所有的算子入队一下，然后调用 run() 来执行
int32_t tvmgen_default___tvm_main__(float* input_buffer_var, float* output_buffer_var, uint8_t* global_const_workspace_0_var, uint8_t* global_workspace_1_var) {
    // 分配中间缓冲区指针
    float* sid_1 = (float*)(&(global_workspace_1_var[16]));
    float* sid_2 = (float*)(&(global_workspace_1_var[0]));
    float* sid_3 = (float*)(&(global_workspace_1_var[32]));
    float* sid_4 = (float*)(&(global_workspace_1_var[16]));  // 复用 sid_1
    float* sid_5 = (float*)(&(global_workspace_1_var[32]));  // 复用 sid_3

    // 初始化队列
    queue_init(&g_task_queue);

    // 准备算子参数 (静态分配)
    static FusedAddArgs args1, args2, args3, args4, args5;
    static FusedAdd3Args args6;

    // Task 1: fused_add(input, sid_1) -> input + 1.0
    args1 = (FusedAddArgs){input_buffer_var, sid_1, global_const_workspace_0_var, global_workspace_1_var};
    queue_push(&g_task_queue, (OperatorTask){wrapped_fused_add, &args1});

    // Task 2: fused_subtract(sid_1, sid_2) -> sid_1 - 2.0
    args2 = (FusedAddArgs){sid_1, sid_2, global_const_workspace_0_var, global_workspace_1_var};
    queue_push(&g_task_queue, (OperatorTask){wrapped_fused_subtract, &args2});

    // Task 3: fused_add_1(input, sid_3) -> input + 3.0
    args3 = (FusedAddArgs){input_buffer_var, sid_3, global_const_workspace_0_var, global_workspace_1_var};
    queue_push(&g_task_queue, (OperatorTask){wrapped_fused_add_1, &args3});

    // Task 4: fused_subtract_1(sid_3, sid_4) -> sid_3 - 4.0
    args4 = (FusedAddArgs){sid_3, sid_4, global_const_workspace_0_var, global_workspace_1_var};
    queue_push(&g_task_queue, (OperatorTask){wrapped_fused_subtract_1, &args4});

    // Task 5: fused_add_2(sid_4, sid_5) -> sid_4 + 5.0
    args5 = (FusedAddArgs){sid_4, sid_5, global_const_workspace_0_var, global_workspace_1_var};
    queue_push(&g_task_queue, (OperatorTask){wrapped_fused_add_2, &args5});

    // Task 6: fused_add_3(sid_2, sid_5, output) -> sid_2 + sid_5
    args6 = (FusedAdd3Args){sid_2, sid_5, output_buffer_var, global_const_workspace_0_var, global_workspace_1_var};
    queue_push(&g_task_queue, (OperatorTask){wrapped_fused_add_3, &args6});

    // 执行队列中的所有任务
    int32_t ret = run(&g_task_queue);

    // 清理队列
    queue_destroy(&g_task_queue);

    return ret;
}
