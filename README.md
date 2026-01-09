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
| `tvmrt_types.h` | 公共类型定义（后端类型、Tensor 映射、算子描述、调度层、运行时上下文） |
| `tvmrt_port.h` | OS 抽象层接口（mutex、cond、thread、barrier） |
| `tvmrt_port_posix.c` | POSIX pthread 实现 |
| `tvmrt_port_single.c` | 单线程空实现 |
| `tvmrt_log.h/c` | 日志机制（Ring Buffer + 回调模式） |
| `tvmrt_semantic.h/c` | 语义转换层（解析模型描述符，组装可执行算子） |
| `tvmrt_engine.h/c` | BSP 调度引擎（层间屏障同步，层内并行执行） |

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
| `tvmrt_engine_init()` | 初始化调度引擎（创建线程池） |
| `tvmrt_engine_shutdown()` | 关闭调度引擎 |
| `tvmrt_engine_run()` | 执行 BSP 调度 |
| `tvmrt_engine_run_single()` | 单线程执行 |
| `worker_func()` | Worker 线程函数 |

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

### 4.2 BSP 执行模型

```
┌─────────────────────────────────────────────────────────┐
│                    Layer 1 (并行)                       │
│   ┌─────────────┐    ┌─────────────┐                   │
│   │   Node 0    │    │   Node 2    │                   │
│   │ fused_add_0 │    │ fused_add_1 │                   │
│   └─────────────┘    └─────────────┘                   │
├─────────────────────────────────────────────────────────┤
│                    ═══ BARRIER ═══                      │
├─────────────────────────────────────────────────────────┤
│                    Layer 2 (并行)                       │
│   ┌───────────────┐  ┌─────────────────┐               │
│   │    Node 1     │  │     Node 3      │               │
│   │fused_subtract │  │fused_subtract_1 │               │
│   └───────────────┘  └─────────────────┘               │
├─────────────────────────────────────────────────────────┤
│                    ═══ BARRIER ═══                      │
├─────────────────────────────────────────────────────────┤
│                    Layer 3 (串行)                       │
│   ┌─────────────┐                                      │
│   │   Node 4    │                                      │
│   │ fused_add_2 │                                      │
│   └─────────────┘                                      │
├─────────────────────────────────────────────────────────┤
│                    ═══ BARRIER ═══                      │
├─────────────────────────────────────────────────────────┤
│                    Layer 4 (串行)                       │
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

## 5. 编译与运行

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
