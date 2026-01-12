# TVM Runtime 模块化架构文档

## 1. 项目目录结构

```
tvm_demo/
├── include/
│   └── tvmgen_default.h       # TVM 生成的公共头文件
├── src/
│   ├── main.c                 # 程序入口
│   ├── default_lib0.c         # Workspace 分配 + 运行入口
│   ├── default_lib1.c         # 模块化入口 (调用 Runtime 模块)
│   ├── default_lib1_legacy.c  # [备份] 原始 BSP 实现
│   ├── runtime/               # Runtime 核心模块
│   │   ├── tvmrt_types.h      # 公共类型定义
│   │   ├── tvmrt_port.h       # OS 抽象层接口
│   │   ├── tvmrt_port_posix.c # POSIX 实现
│   │   ├── tvmrt_port_single.c# 单线程 fallback
│   │   ├── tvmrt_log.h        # 日志接口
│   │   ├── tvmrt_log.c        # 日志实现
│   │   ├── tvmrt_semantic.h   # 语义转换层接口
│   │   ├── tvmrt_semantic.c   # 语义转换层实现
│   │   ├── tvmrt_engine.h     # 调度引擎接口
│   │   └── tvmrt_engine.c     # BSP 调度引擎实现
│   ├── model/                 # 模型描述模块
│   │   ├── model_desc.h       # 模型描述接口
│   │   └── model_desc.c       # 静态描述表 (编译器生成)
│   └── ops/                   # 算子模块
│       └── default_ops.c      # TVM 生成的 fused 算子
└── Makefile
```

---

## 2. 各文件内容概述

### 2.1 入口文件

| 文件 | 内容 |
|------|------|
| `main.c` | 测试程序入口，准备输入输出调用 `tvmgen_default_run()` |
| `default_lib0.c` | 静态分配 workspace + 常量区，提供 `tvmgen_default_run()` |
| `default_lib1.c` | 模块化入口，调用 Runtime 模块执行模型 |

### 2.2 Runtime 模块

| 文件 | 内容 |
|------|------|
| `tvmrt_types.h` | 公共类型定义（后端类型、Tensor 映射、算子描述、调度层、运行时上下文、**层级任务队列**） |
| `tvmrt_port.h` | OS 抽象层接口（mutex、cond、thread、barrier） |
| `tvmrt_port_posix.c` | POSIX pthread 实现 |
| `tvmrt_port_single.c` | 单线程空实现 |
| `tvmrt_log.h/c` | 日志机制（Ring Buffer + 回调模式） |
| `tvmrt_semantic.h/c` | 语义转换层（解析模型描述符，组装可执行算子） |
| `tvmrt_engine.h/c` | BSP 调度引擎（**任务队列 + 链式唤醒**，层间屏障同步，层内并行执行） |

### 2.3 Model 模块

| 文件 | 内容 |
|------|------|
| `model_desc.h` | 模型描述接口 + 参数结构体定义 |
| `model_desc.c` | 静态描述表（Tensor 映射、算子描述、调度表、函数表） |

### 2.4 Ops 模块

| 文件 | 内容 |
|------|------|
| `default_ops.c` | TVM 生成的 fused 算子 + 包装函数 |

---

## 3. 各文件函数列表

### 3.1 `src/main.c`

| 函数 | 说明 |
|------|------|
| `main()` | 程序入口，准备数据并调用推理 |

### 3.2 `src/default_lib0.c`

| 函数 | 说明 |
|------|------|
| `tvmgen_default_run()` | 运行入口，调用 `__tvm_main__` |

### 3.3 `src/default_lib1.c` (模块化入口)

| 函数 | 说明 |
|------|------|
| `init_op_execs()` | 初始化算子执行表 |
| `tvmgen_default___tvm_main__()` | TVM 主入口，初始化并运行调度引擎 |

### 3.4 `src/runtime/tvmrt_port_posix.c`

| 函数 | 说明 |
|------|------|
| `tvmrt_mutex_init/lock/unlock/destroy()` | 互斥锁操作 |
| `tvmrt_cond_init/wait/signal/broadcast/destroy()` | 条件变量操作 |
| `tvmrt_thread_create/join()` | 线程操作 |
| `tvmrt_barrier_init/reset/arrive/sync/destroy()` | 屏障操作 |

### 3.5 `src/runtime/tvmrt_log.c`

| 函数 | 说明 |
|------|------|
| `tvmrt_log_set_callback()` | 设置日志回调 |
| `tvmrt_log_push()` | 压入日志记录 |
| `tvmrt_log_pop()` | 弹出日志记录 |
| `tvmrt_log_clear()` | 清空日志 |
| `tvmrt_log_count()` | 获取日志数量 |

### 3.6 `src/runtime/tvmrt_semantic.c`

| 函数 | 说明 |
|------|------|
| `tvmrt_semantic_init()` | 从模型描述初始化运行时上下文 |
| `tvmrt_semantic_resolve_sid()` | 解析 Storage ID 到指针 |

### 3.7 `src/runtime/tvmrt_engine.c`

| 函数 | 说明 |
|------|------|
| `tvmrt_engine_init()` | 初始化调度引擎（创建线程池，初始化任务队列） |
| `tvmrt_engine_shutdown()` | 关闭调度引擎 |
| `tvmrt_engine_run()` | 执行 BSP 调度（逐层填充任务队列） |
| `tvmrt_engine_run_single()` | 单线程执行 |
| `load_next_layer()` | **辅助函数**：加载下一层任务到队列并唤醒 Worker |
| `worker_func()` | Worker 线程函数（**链式唤醒机制**） |

### 3.8 `src/model/model_desc.c`

| 函数 | 说明 |
|------|------|
| `model_get_descriptor()` | 获取模型描述符 |
| `model_get_tensor_map()` | 获取 Tensor 映射表 |
| `model_get_op_descs()` | 获取算子描述表 |
| `model_get_schedule()` | 获取静态调度表 |
| `model_fill_args()` | 填充算子参数 |
| `model_get_op_args()` | 获取指定算子的参数指针 |

### 3.9 `src/ops/default_ops.c`

| 函数 | 说明 |
|------|------|
| `tvmgen_default_fused_add()` | 加法算子 (input+1.0) |
| `tvmgen_default_fused_add_1()` | 加法算子 (input+3.0) |
| `tvmgen_default_fused_add_2()` | 加法算子 (+5.0) |
| `tvmgen_default_fused_add_3()` | 双输入加法 (sid_2+sid_5) |
| `tvmgen_default_fused_subtract()` | 减法算子 (-2.0) |
| `tvmgen_default_fused_subtract_1()` | 减法算子 (-4.0) |
| `wrapped_fused_*()` | 包装函数，适配统一签名 |

---

## 4. 运行流程

### 4.1 调用顺序

```
main()
  │
  └──▶ tvmgen_default_run()              [default_lib0.c]
        │
        └──▶ tvmgen_default___tvm_main__()  [default_lib1.c]
              │
              ├── tvmrt_engine_init()         ← 初始化线程池
              │
              ├── init_op_execs()             ← 填充算子参数和函数指针
              │     ├── model_fill_args()
              │     └── model_get_op_args()
              │
              └── tvmrt_engine_run()          ← BSP 调度执行
                    │
                    ├── Layer 1: Node 0, Node 2 (并行)
                    │     barrier_sync()
                    ├── Layer 2: Node 1, Node 3 (并行)
                    │     barrier_sync()
                    ├── Layer 3: Node 4 (串行)
                    └── Layer 4: Node 5 (串行)
```

### 4.2 BSP 任务队列执行模型

**核心机制**: 显式任务队列 + Worker 链式唤醒

```
主线程                          Worker1                Worker2
  │
  ├─ load_next_layer(Layer1)   
  │  ├─ 填充队列: [Node0, Node2]
  │  └─ signal() ────────────▶ 唤醒 Worker1
  │                              │
  │                              ├─ lock & pop(Node0)
  │                              ├─ signal() ──────▶ 唤醒 Worker2
  │                              ├─ unlock()          │
  │                              ├─ execute(Node0)    ├─ lock & pop(Node2)
  │                              │                    ├─ unlock()
  │                              │                    ├─ execute(Node2)
  │                              ├─ barrier_arrive()  │
  │                              │                    ├─ barrier_arrive()
  ├─ barrier_sync() ◀────────────┴────────────────────┘
  │ (Layer1 完成)
  │
  ├─ load_next_layer(Layer2)
  │  └─ 重复上述流程...
```

**层级划分**:

```
┌─────────────────────────────────────────────────────────┐
│                    Layer 1 (队列: [0, 2])               │
│   ┌─────────────┐    ┌─────────────┐                   │
│   │   Node 0    │    │   Node 2    │                   │
│   │ fused_add_0 │    │ fused_add_1 │                   │
│   └─────────────┘    └─────────────┘                   │
├─────────────────────────────────────────────────────────┤
│                    ═══ BARRIER ═══                      │
├─────────────────────────────────────────────────────────┤
│                    Layer 2 (队列: [1, 3])               │
│   ┌───────────────┐  ┌─────────────────┐               │
│   │    Node 1     │  │     Node 3      │               │
│   │fused_subtract │  │fused_subtract_1 │               │
│   └───────────────┘  └─────────────────┘               │
├─────────────────────────────────────────────────────────┤
│                    ═══ BARRIER ═══                      │
├─────────────────────────────────────────────────────────┤
│                    Layer 3 (单任务)                     │
│   ┌─────────────┐                                      │
│   │   Node 4    │                                      │
│   │ fused_add_2 │                                      │
│   └─────────────┘                                      │
├─────────────────────────────────────────────────────────┤
│                    ═══ BARRIER ═══                      │
├─────────────────────────────────────────────────────────┤
│                    Layer 4 (单任务)                     │
│   ┌─────────────┐                                      │
│   │   Node 5    │                                      │
│   │ fused_add_3 │                                      │
│   └─────────────┘                                      │
└─────────────────────────────────────────────────────────┘
```

### 4.3 数据流

```
input (10.0)
    │
    ├──▶ Node 0: + 1.0 ──▶ sid_1 (11.0)
    │                          │
    │                          └──▶ Node 1: - 2.0 ──▶ sid_2 (9.0)
    │
    └──▶ Node 2: + 3.0 ──▶ sid_3 (13.0)
                               │
                               └──▶ Node 3: - 4.0 ──▶ sid_4 (9.0)
                                                         │
                                                         └──▶ Node 4: + 5.0 ──▶ sid_5 (14.0)

    sid_2 (9.0) + sid_5 (14.0) ──▶ Node 5 ──▶ output (23.0)
```

---

## 5. 参数定义与初始化位置

### 5.1 核心数据结构定义

| 结构体/类型 | 定义位置 | 说明 |
|------------|---------|------|
| `tvmrt_layer_queue_t` | `src/runtime/tvmrt_types.h:150-166` | 层级任务队列（存储当前层待执行算子） |
| `tvmrt_context_t` | `src/runtime/tvmrt_types.h:139-147` | 运行时上下文（workspace、算子执行表） |
| `tvmrt_op_exec_t` | `src/runtime/tvmrt_types.h:123-127` | 可执行算子条目（函数指针 + 参数） |
| `tvmrt_schedule_desc_t` | `src/runtime/tvmrt_types.h:108-111` | 静态调度表（层数组） |
| `tvmrt_schedule_layer_t` | `src/runtime/tvmrt_types.h:100-103` | 单个调度层（算子 ID 数组） |
| `tvmrt_op_desc_t` | `src/runtime/tvmrt_types.h:82-91` | 算子描述（名称、后端、输入输出 SID） |
| `tvmrt_tensor_map_entry_t` | `src/runtime/tvmrt_types.h:66-71` | Tensor 内存映射项 |

### 5.2 全局静态参数

#### 模型描述参数（`src/model/model_desc.c`）

| 参数 | 定义位置 | 类型 | 说明 |
|------|---------|------|------|
| `g_tensor_map` | L31-38 | `tvmrt_tensor_map_entry_t[]` | 张量内存映射表（5个 SID） |
| `g_op_descs` | L45-112 | `tvmrt_op_desc_t[]` | 算子描述表（6个节点） |
| `g_cpu_func_table` | L118-125 | `tvmrt_op_func_t[]` | CPU 函数指针表 |
| `g_schedule_layers` | L145-150 | `tvmrt_schedule_layer_t[]` | 静态调度表（4层） |
| `g_layer1_ops` | L134 | `int32_t[]` | Layer 1 算子索引: `{0, 2}` |
| `g_layer2_ops` | L137 | `int32_t[]` | Layer 2 算子索引: `{1, 3}` |
| `g_layer3_ops` | L140 | `int32_t[]` | Layer 3 算子索引: `{4}` |
| `g_layer4_ops` | L143 | `int32_t[]` | Layer 4 算子索引: `{5}` |

#### 算子参数存储（`src/model/model_desc.c`）

| 参数 | 定义位置 | 类型 | 初始化位置 |
|------|---------|------|-----------|
| `g_fused_add_args[5]` | L196 (声明) | `FusedAddArgs[]` | `model_fill_args()` L217-254 |
| `g_fused_add3_args` | L197 (声明) | `FusedAdd3Args` | `model_fill_args()` L257-263 |

> **注**: 这些参数在 `tvmgen_default___tvm_main__()` 调用 `init_op_execs()` 时被填充。

#### 引擎状态（`src/runtime/tvmrt_engine.c`）

| 参数 | 定义位置 | 类型 | 说明 |
|------|---------|------|------|
| `g_engine` | L36 (声明) | `engine_state_t` | 全局引擎状态（线程池、任务队列、Barrier） |
| `g_engine.task_queue` | 成员变量 | `tvmrt_layer_queue_t` | 当前层任务队列 |
| `g_engine.workers[4]` | 成员变量 | `tvmrt_thread_t[]` | Worker 线程数组 |
| `g_engine.layer_barrier` | 成员变量 | `tvmrt_barrier_t` | 层间同步屏障 |

### 5.3 函数参数传递流程

```
main()
  ├─ 分配: input_buffer, output_buffer
  └─ 调用: tvmgen_default_run(input, output)

tvmgen_default_run() [default_lib0.c]
  ├─ 分配: global_workspace, global_const_workspace
  └─ 调用: tvmgen_default___tvm_main__(input, output, const_ws, ws)

tvmgen_default___tvm_main__() [default_lib1.c]
  ├─ 调用: init_op_execs(input, output, ws, const_ws)
  │   ├─ 调用: model_fill_args(NULL, input, output, ws, const_ws)
  │   │   └─ 填充: g_fused_add_args[], g_fused_add3_args
  │   └─ 调用: model_get_op_args(op_id)
  │       └─ 返回: &g_fused_add_args[op_id] 或 &g_fused_add3_args
  │
  ├─ 构造: tvmrt_context_t ctx = {.workspace=ws, .op_execs=g_op_execs, ...}
  └─ 调用: tvmrt_engine_run(&ctx, schedule)

tvmrt_engine_run() [tvmrt_engine.c]
  ├─ 设置: g_engine.current_ctx = ctx
  ├─ 逐层执行:
  │   ├─ 调用: load_next_layer()
  │   │   └─ 填充 g_engine.task_queue.tasks[] 并唤醒 Worker
  │   └─ 调用: tvmrt_barrier_sync() (等待当前层完成)
  └─ 返回

worker_func() [Worker 线程]
  ├─ 从 g_engine.task_queue 获取 op_id
  ├─ 获取: exec = &ctx->op_execs[op_id]
  ├─ 调用: exec->func(exec->args)  // 执行算子
  └─ 调用: tvmrt_barrier_arrive()  // 通知完成
```

---

## 6. 编译与运行

```bash
# 编译模块化版本
make all

# 运行
make run

# 输出
验证开始...
输入值: 10.000000
执行成功！
输出值: 23.000000 (预期: 23.000000)
```
