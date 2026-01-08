# 静态图 + 多线程 Worker 池重构计划

## 任务目标

将当前的单线程、动态构建图的 DAG 执行引擎，重构为符合"嵌入式轻量化运行时"要求的**静态图数据驱动 + 多线程 Worker 池**架构。

## 核心原则

1. **完全静态化 (Static Allocation)**：移除运行时 `dag_add_node` / `dag_add_dep`，所有节点依赖关系在编译期定义为 `const` 全局数组
2. **Worker 池模型 (Worker Pool Pattern)**：固定数量线程的 Worker Pool，协同消费就绪队列
3. **读写分离 (Separation of Concern)**：图拓扑结构（只读）与节点运行状态（可变）分离
4. **原子操作 (Atomic Operations)**：使用 `_Atomic` 类型保证多线程安全

---

## Step 1: 数据结构重构

将 `DAGNode` 拆分为**静态描述**和**运行时状态**两部分。

### 1.1 定义 `OpMetadata` (只读，const)

```c
typedef struct {
    const char* name;               // 调试用，如 "fused_add_0"
    operator_func_t func;           // 算子函数指针
    void* args;                     // 参数指针
    int32_t dep_count;              // 原始入度
    int32_t successor_count;        // 后继节点数量
    int32_t successors[MAX_SUCCESSORS]; // 后继节点索引
} OpMetadata;
```

### 1.2 定义 `RuntimeState` (可变，原子类型)

```c
typedef struct {
    _Atomic int32_t remaining_deps; // 剩余依赖数（原子）
    _Atomic bool is_completed;      // 是否完成（原子）
} RuntimeState;
```

### 1.3 全局静态表与参数提升

1.  **参数提升 (Global Args)**：将原 `tvmgen_default___tvm_main__` 中的 `static FusedAddArgs args1` 等变量移至函数外部，定义为**文件级静态变量** (`static FusedAddArgs g_args_0;` 等)，以便获取编译期常量地址。
2.  **静态图定义**：使用这些全局参数地址初始化图节点。

```c
// 1. 参数结构体定义（移出 main 函数）
static FusedAddArgs g_args_node0 = { ... };
static FusedAddArgs g_args_node1 = { ... };
// ...

// 2. 编译期静态图定义
static const OpMetadata g_graph_nodes[NUM_NODES] = {
    {
        .name = "fused_add_0",
        .func = wrapped_fused_add,
        .args = &g_args_node0,  // 现在可以合法获取地址了
        .dep_count = 0,
        .successor_count = 1,
        .successors = {1}
    },
    // ...
};

static RuntimeState g_runtime_state[NUM_NODES];

---

## Step 2: 线程安全就绪队列

增强 `NodeQueue` 支持多线程竞争和等待机制。

### 2.1 队列结构

```c
typedef struct {
    int32_t node_ids[MAX_NODES];
    int32_t head, tail, count;
    pthread_mutex_t mutex;
    pthread_cond_t cond;            // 条件变量
    bool shutdown;                  // 停机标志
} NodeQueue;
```

### 2.2 API 更新

- `node_queue_push`: 入队后 `pthread_cond_signal` 唤醒 Worker
- `node_queue_pop_blocking`: 队列空时 `pthread_cond_wait` 挂起，收到 `shutdown` 时退出
- `node_queue_shutdown`: 设置 `shutdown=true` 并 `broadcast` 唤醒所有 Worker

---

## Step 3: Worker 线程实现

### 3.1 执行上下文

```c
typedef struct {
    _Atomic int32_t nodes_completed; // 已完成节点数
    int32_t total_nodes;             // 总节点数
    pthread_cond_t done_cond;        // 完成信号
    pthread_mutex_t done_mutex;
} ExecutionContext;
```

### 3.2 Worker 函数

```c
void* worker_thread_func(void* arg) {
    while (1) {
        int32_t node_id;
        if (node_queue_pop_blocking(&ready_queue, &node_id) < 0) {
            break;  // 收到退出信号
        }
        
        // 执行算子
        g_graph_nodes[node_id].func(g_graph_nodes[node_id].args);
        
        // 原子更新后继节点
        for (int i = 0; i < g_graph_nodes[node_id].successor_count; i++) {
            int32_t succ_id = g_graph_nodes[node_id].successors[i];
            if (atomic_fetch_sub(&g_runtime_state[succ_id].remaining_deps, 1) == 1) {
                node_queue_push(&ready_queue, succ_id);
            }
        }
        
        // 检查全部完成
        if (atomic_fetch_add(&ctx.nodes_completed, 1) + 1 == ctx.total_nodes) {
            pthread_cond_signal(&ctx.done_cond);
        }
    }
    return NULL;
}
```

### 3.3 线程池管理

```c
#define NUM_WORKERS 4

void thread_pool_init();    // 创建线程（首次调用时）
void thread_pool_start();   // 唤醒线程开始执行
void thread_pool_wait();    // 等待任务完成
void thread_pool_destroy(); // 销毁线程池
```

---

## Step 4: 主入口重写

### 4.1 初始化阶段

```c
int32_t tvmgen_default___tvm_main__(...) {
    // 1. 重置运行时状态
    for (int i = 0; i < NUM_NODES; i++) {
        atomic_store(&g_runtime_state[i].remaining_deps, g_graph_nodes[i].dep_count);
        atomic_store(&g_runtime_state[i].is_completed, false);
    }
    
    // 2. 将入度为 0 的节点入队
    for (int i = 0; i < NUM_NODES; i++) {
        if (g_graph_nodes[i].dep_count == 0) {
            node_queue_push(&ready_queue, i);
        }
    }
    
    // 3. 启动执行（唤醒 Worker）
    thread_pool_start();
    
    // 4. 等待完成
    thread_pool_wait();
    
    return 0;
}
```

---

## Step 5: (可选) 异构扩展预留

```c
typedef enum {
    DEVICE_CPU = 0,
    DEVICE_GPU = 1,
    DEVICE_NPU = 2
} DeviceType;

typedef struct {
    const char* name;
    operator_func_t func;
    void* args;
    DeviceType device;          // 设备类型
    int32_t priority;           // 调度优先级
    // ...
} OpMetadata;
```

---

## 实现顺序

1. [x] Step 1: 数据结构重构 (`OpMetadata`, `RuntimeState`)
2. [x] Step 2: 就绪队列增强 (条件变量, 停机信号)
3. [ ] Step 3: Worker 线程池实现
4. [ ] Step 4: 主入口重写
5. [ ] Step 5: (可选) 异构扩展

---

**保持 `fused_*` 算子实现不变，只重写 DAG 管理和调度部分。**