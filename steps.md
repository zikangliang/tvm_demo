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
3. [x] Step 3: Worker 线程池实现 (单线程版完成，多线程待后续)
4. [x] Step 4: 主入口重写
5. [ ] Step 5: (可选) 异构扩展
6. [x] Step 6: 静态分层调度 (BSP) 重构
7. [ ] Step 7: 模块化代码重构

---

**保持 `fused_*` 算子实现不变，只重写 DAG 管理和调度部分。**

---

## Step 6: 静态分层调度 (BSP) 重构

> [!CAUTION]
> **关键问题**: 当前动态调度 (Kahn 算法) + TVM 静态内存规划 = 潜在数据竞争
> 
> TVM 编译器复用内存地址 (如 `sid_1` 与 `sid_4` 共用 `workspace[16]`)，在多线程动态调度下可能导致内存冲突。

### 6.1 删除旧代码

| 删除项 | 位置 |
|--------|------|
| `RuntimeState` 结构体 | 第 47-51 行 |
| `g_runtime_state` 数组 | 第 160 行 |
| `NodeQueue` 结构体及所有 `node_queue_*` 函数 | 第 165-264 行 |
| `static_graph_init_runtime()` 函数 | 第 271-276 行 |
| `static_graph_run()` 函数 | 第 280-331 行 |
| `OpMetadata` 中的 `dep_count`, `successor_count`, `successors` 字段 | 第 42-44 行 |
| `<stdatomic.h>` 头文件 | 第 9 行 |

### 6.2 添加新数据结构

```c
// 单个任务项
typedef struct {
    int32_t op_index;  // g_graph_nodes 中的索引
} TaskItem;

// 一个执行层 - 层内所有任务可以安全并行
typedef struct {
    const TaskItem* tasks;  // 任务数组
    int32_t task_count;     // 任务数量
} StaticLayer;
```

### 6.3 定义静态调度表

根据依赖关系，将节点分为 4 层：

```
Layer 1: Node 0, Node 2  (入度=0，可并行)
         ↓ BARRIER ↓
Layer 2: Node 1, Node 3  (Layer 1 完成后就绪，可并行)
         ↓ BARRIER ↓
Layer 3: Node 4          (依赖 Node 3)
         ↓ BARRIER ↓
Layer 4: Node 5          (依赖 Node 1 + Node 4)
```

```c
// Layer 任务定义
static const TaskItem g_layer1_tasks[] = {{0}, {2}};
static const TaskItem g_layer2_tasks[] = {{1}, {3}};
static const TaskItem g_layer3_tasks[] = {{4}};
static const TaskItem g_layer4_tasks[] = {{5}};

// 完整静态调度表
#define NUM_LAYERS 4
static const StaticLayer g_static_schedule[NUM_LAYERS] = {
    {g_layer1_tasks, 2},  // Layer 1: 并行
    {g_layer2_tasks, 2},  // Layer 2: 并行
    {g_layer3_tasks, 1},  // Layer 3: 串行
    {g_layer4_tasks, 1}   // Layer 4: 串行
};
```

### 6.4 实现 BSP 执行函数

```c
int32_t static_schedule_run(void) {
    for (int32_t layer = 0; layer < NUM_LAYERS; layer++) {
        const StaticLayer* current = &g_static_schedule[layer];
        
        if (current->task_count == 1) {
            // 单任务：直接在当前线程执行（避免线程池开销）
            int32_t op_idx = current->tasks[0].op_index;
            int32_t ret = g_graph_nodes[op_idx].func(g_graph_nodes[op_idx].args);
            if (ret != 0) return ret;
        } else {
            // 多任务：分发到线程池并行执行
            dispatch_to_workers(current->tasks, current->task_count);
            
            // ========== BARRIER ==========
            // 等待所有任务完成后才进入下一层
            wait_for_all_workers();
        }
    }
    return 0;
}
```

### 6.5 实现简化的 Barrier

```c
typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int32_t count;   // 当前已完成的任务数
    int32_t target;  // 目标任务数
} Barrier;

void barrier_wait(Barrier* b);    // Worker 调用：完成计数+1
void barrier_sync(Barrier* b);    // 主线程调用：等待全部完成
void barrier_reset(Barrier* b);   // 重置计数器
```

### 6.6 更新主入口

修改 `tvmgen_default___tvm_main__` 调用 `static_schedule_run()` 替代 `static_graph_run()`。

### 6.7 简化 OpMetadata

```c
typedef struct {
    const char* name;         // 调试用节点名称
    operator_func_t func;     // 算子函数指针
    void* args;               // 参数指针
    // 移除: dep_count, successor_count, successors
    // 依赖关系现在由 g_static_schedule 隐式表达
} OpMetadata;
```

---

## Step 6 实施检查清单

### Phase 1: 基础架构与单线程实现 (✅ 已完成)
- [x] **6.1** 删除旧代码 (RuntimeState, NodeQueue, Atomic)
- [x] **6.2** 添加新数据结构 (TaskItem, StaticLayer)
- [x] **6.3** 定义静态调度表 (g_static_schedule)
- [x] **6.4a** 实现 `static_schedule_run` (单线程版)
- [x] **6.6a** 验证单线程正确性

### Phase 2: 多线程并发支持 (✅ 已完成)
- [x] **6.5** 实现同步原语 (Barrier)
  - [x] 定义 `Barrier` 结构体
  - [x] 实现 `barrier_wait`, `barrier_sync`
- [x] **6.4b** 升级为多线程执行
  - [x] 实现 Worker 线程池分发机制
  - [x] 更新 `static_schedule_run` 支持并行分发
- [x] **6.6b** 验证多线程并发正确性

---

## 附：关键约束

| 约束 | 说明 |
|-----|------|
| ❌ 无动态依赖追踪 | 不再使用原子计数器，调度顺序编译期固定 |
| ✅ 层间隐式 Barrier | for 循环 + wait 保证层间同步 |
| ✅ 零动态分配 | 所有调度数组必须是 `static const` |
| ✅ 内存安全 | 同层节点不会访问相同复用内存 |

---

## Step 7: 模块化代码重构

> [!IMPORTANT]
> **目标**: 将当前单文件实现重构为模块化架构，对齐 PDF「研究方案三」的语义转换层、核心引擎层、并发管理层三级设计。

### 7.0 目标架构

```
src/
├── runtime/                    # 运行时框架核心
│   ├── tvmrt_types.h          # 公共类型定义
│   ├── tvmrt_port.h           # OS 抽象层接口
│   ├── tvmrt_port_posix.c     # POSIX 实现
│   ├── tvmrt_port_single.c    # 单线程 fallback
│   ├── tvmrt_log.h            # 日志接口
│   ├── tvmrt_log.c            # 日志实现（ring buffer）
│   ├── tvmrt_engine.h         # 调度引擎接口
│   ├── tvmrt_engine.c         # 通用调度执行器
│   ├── tvmrt_semantic.h       # 语义转换层接口
│   └── tvmrt_semantic.c       # 语义解释/装配器
├── model/                      # 编译产物描述（编译器生成）
│   ├── model_desc.h           # 模型描述结构
│   └── model_desc.c           # 静态描述数据
├── ops/                        # TVM 生成的算子
│   └── default_ops.c          # fused_* 内核实现
└── entry/                      # 入口文件
    ├── default_lib0.c         # workspace 分配
    └── default_lib1.c         # 入口 + 胶水代码
```

### 7.1 OS 抽象层 (`tvmrt_port`)

新增 OS 无关的抽象接口，使 runtime 可移植到 RTOS：

```c
// tvmrt_port.h - 互斥锁/条件变量/线程抽象
typedef struct tvmrt_mutex tvmrt_mutex_t;
int tvmrt_mutex_init(tvmrt_mutex_t* m);
int tvmrt_mutex_lock(tvmrt_mutex_t* m);
int tvmrt_mutex_unlock(tvmrt_mutex_t* m);

typedef struct tvmrt_cond tvmrt_cond_t;
int tvmrt_cond_wait(tvmrt_cond_t* c, tvmrt_mutex_t* m);
int tvmrt_cond_signal(tvmrt_cond_t* c);

typedef struct tvmrt_thread tvmrt_thread_t;
int tvmrt_thread_create(tvmrt_thread_t* t, void* (*func)(void*), void* arg);
int tvmrt_thread_join(tvmrt_thread_t* t);
```

提供两套实现：
- `tvmrt_port_posix.c`: 使用 pthread（当前逻辑迁移）
- `tvmrt_port_single.c`: 单线程空实现（用于调试）

### 7.2 日志机制 (`tvmrt_log`)

可裁剪日志接口，支持回调或 ring buffer 模式：

```c
typedef struct {
    int32_t op_id;
    const char* op_name;
    int32_t worker_id;
    int32_t ret_code;
} tvmrt_log_record_t;

void tvmrt_log_push(const tvmrt_log_record_t* rec);
int tvmrt_log_pop(tvmrt_log_record_t* rec);
```

### 7.3 语义转换层 (`tvmrt_semantic`)

对齐 PDF 3.1 "解析编译产物 → 组装可调度实体"：

```c
// Tensor 内存映射项
typedef struct {
    int32_t sid;       // storage id
    int32_t offset;    // workspace 偏移
    int32_t size;
} TensorMapEntry;

// 算子描述
typedef enum { OP_KIND_CPU_FUNC, OP_KIND_NPU_SUBGRAPH } OpKind;

typedef struct {
    int32_t op_id;
    const char* name;
    OpKind kind;
    int32_t input_sids[4];
    int32_t output_sids[2];
} OpDesc;

// 运行时上下文（支持多实例）
typedef struct {
    uint8_t* workspace;
    const uint8_t* const_workspace;
    void* op_args;
} RuntimeContext;

int tvmrt_semantic_init(RuntimeContext* ctx, ...);
```

### 7.4 调度引擎 (`tvmrt_engine`)

通用 BSP 执行器，解释 `ScheduleDesc`：

```c
typedef struct {
    const int32_t* op_indices;
    int32_t count;
} ScheduleLayer;

typedef struct {
    const ScheduleLayer* layers;
    int32_t layer_count;
} ScheduleDesc;

int tvmrt_engine_run(RuntimeContext* ctx, const ScheduleDesc* schedule, const OpMetadata* ops);
```

### 7.5 编译产物描述 (`model_desc`)

将当前写死的 `g_graph_nodes`、`g_static_schedule`、`sid_*` 提取为静态描述表：

```c
// model_desc.c - 编译器生成的静态表
static const TensorMapEntry g_tensor_map[] = {
    {.sid = 1, .offset = 0,  .size = 4},
    {.sid = 2, .offset = 4,  .size = 4},
    // ...
};

static const OpDesc g_op_descs[] = { ... };
static const ScheduleDesc g_schedule = { ... };
```

### 7.6 算子提取 (`default_ops`)

将 `tvmgen_default_fused_*` 函数从 `default_lib1.c` 提取到 `ops/default_ops.c`。

### 7.7 入口简化

修改 `default_lib1.c`，调用新模块：

```c
#include "runtime/tvmrt_engine.h"
#include "runtime/tvmrt_semantic.h"
#include "model/model_desc.h"

int32_t tvmgen_default___tvm_main__(...) {
    RuntimeContext ctx;
    tvmrt_semantic_init(&ctx, ...);
    return tvmrt_engine_run(&ctx, &g_schedule, g_op_descs);
}
```

---

## Step 7 实施检查清单

### Phase 1: 基础设施层
- [x] **7.1** OS 抽象层
  - [x] 创建 `tvmrt_port.h` 接口
  - [x] 实现 `tvmrt_port_posix.c`
  - [x] 实现 `tvmrt_port_single.c`

- [x] **7.2** 日志机制
  - [x] 创建 `tvmrt_log.h` 接口
  - [x] 实现 `tvmrt_log.c` (ring buffer)

### Phase 2: 核心框架层
- [x] **7.3** 语义转换层
  - [x] 创建 `tvmrt_semantic.h` 接口
  - [x] 实现 `tvmrt_semantic.c`

- [x] **7.4** 调度引擎
  - [x] 创建 `tvmrt_engine.h` 接口
  - [x] 实现 `tvmrt_engine.c`

### Phase 3: 模型描述层
- [x] **7.5** 编译产物描述
  - [x] 创建 `model_desc.h`
  - [x] 创建 `model_desc.c` (提取静态表)

### Phase 4: 代码迁移与集成
- [x] **7.6** 算子提取
  - [x] 创建 `ops/default_ops.c`

- [x] **7.7** 入口简化与验证
  - [x] 重构 `default_lib1.c` (98行模块化入口)
  - [x] 更新 `Makefile`
  - [x] 验证编译通过
  - [x] 验证输出正确 (23.000000)