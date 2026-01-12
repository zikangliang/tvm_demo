# TVM Runtime 简化架构文档

## 1. 项目概述

本项目是 TVM Runtime 的精简嵌入式实现，采用 **8 文件架构**，在保持清晰职责分离的同时，最大化代码的可移植性和可维护性。

**核心特性**:
- ✅ 显式任务队列 + Worker 链式唤醒
- ✅ BSP 静态分层调度
- ✅ 保留 lib0/lib1 职责分离
- ✅ Runtime 工具单文件化
- ✅ 零动态内存分配
- ✅ 平台可移植（POSIX / RTOS）

---

## 2. 项目目录结构

```
tvm_demo/
├── include/
│   └── tvmgen_default.h       # TVM 生成的公共头文件
├── src/
│   ├── main.c                 # 测试程序入口
│   ├── default_lib0.c         # Workspace 分配 + 运行入口
│   ├── default_lib1.c         # Runtime 初始化逻辑
│   ├── tvmrt.h                # Runtime 统一头文件
│   ├── tvmrt.c                # Runtime 统一实现
│   ├── tvmrt_port_posix.c     # OS 适配层 (POSIX)
│   ├── model_data.c           # 模型静态描述 (TVM 生成)
│   └── ops.c                  # 算子实现 (TVM 生成)
└── Makefile
```

---

## 3. 各文件内容概述

### 3.1 入口与 Workspace

| 文件 | 大小 | 内容 |
|------|------|------|
| `main.c` | 847 B | 测试程序入口，准备输入输出并调用 `tvmgen_default_run()` |
| `default_lib0.c` | 1.4 KB | 静态分配 workspace 和常量区，提供 `tvmgen_default_run()` |
| `default_lib1.c` | 3.1 KB | Runtime 初始化，算子执行表填充，调用调度引擎 |

### 3.2 Runtime 核心

| 文件 | 大小 | 内容 |
|------|------|------|
| `tvmrt.h` | 11 KB | Runtime 完整 API：类型定义、OS 抽象、日志、引擎、语义转换 |
| `tvmrt.c` | 10 KB | Runtime 完整实现：日志系统、语义转换、BSP 调度引擎 |
| `tvmrt_port_posix.c` | 3.8 KB | POSIX 平台适配：mutex、条件变量、线程、barrier |

### 3.3 模型相关（TVM 生成）

| 文件 | 大小 | 内容 |
|------|------|------|
| `model_data.c` | 9.4 KB | 静态描述表：张量映射、算子描述、调度表、函数表、参数填充 |
| `ops.c` | 3.7 KB | TVM 生成的融合算子 + 包装函数 |

---

## 4. 函数列表

### 4.1 `src/main.c` (测试入口)

| 函数 | 说明 |
|------|------|
| `main()` | 程序入口，准备数据并调用推理 |

### 4.2 `src/default_lib0.c` (Workspace)

| 函数/数据 | 说明 |
|----------|------|
| `global_const_workspace` | 静态分配的常量区 (68 bytes) |
| `global_workspace` | 静态分配的可变 workspace (36 bytes) |
| `tvmgen_default_run()` | 运行入口，调用 `__tvm_main__` |

### 4.3 `src/default_lib1.c` (Runtime 初始化)

| 函数 | 说明 |
|------|------|
| `init_op_execs()` | 初始化算子执行表 |
| `tvmgen_default___tvm_main__()` | TVM 主入口，初始化并运行调度引擎 |

### 4.4 `src/tvmrt.c` (Runtime 核心)

#### 日志系统
| 函数 | 说明 |
|------|------|
| `tvmrt_log_set_callback()` | 设置日志回调 |
| `tvmrt_log_push()` | 压入日志记录 |
| `tvmrt_log_pop()` | 弹出日志记录 |
| `tvmrt_log_clear()` | 清空日志 |
| `tvmrt_log_count()` | 获取日志数量 |

#### 语义转换层
| 函数 | 说明 |
|------|------|
| `tvmrt_semantic_init()` | 初始化运行时上下文 |
| `tvmrt_semantic_resolve_sid()` | 解析 Storage ID 到指针 |

#### 调度引擎
| 函数 | 说明 |
|------|------|
| `tvmrt_engine_init()` | 初始化调度引擎（创建线程池，初始化任务队列） |
| `tvmrt_engine_shutdown()` | 关闭调度引擎 |
| `tvmrt_engine_run()` | 执行 BSP 调度（逐层填充任务队列） |
| `tvmrt_engine_run_single()` | 单线程执行 |
| `load_next_layer()` | **辅助函数**：加载下一层任务到队列并唤醒 Worker |
| `worker_func()` | Worker 线程函数（**链式唤醒机制**） |

### 4.5 `src/tvmrt_port_posix.c` (OS 适配)

| 函数 | 说明 |
|------|------|
| `tvmrt_mutex_init/lock/unlock/destroy()` | 互斥锁操作 |
| `tvmrt_cond_init/wait/signal/broadcast/destroy()` | 条件变量操作 |
| `tvmrt_thread_create/join()` | 线程操作 |
| `tvmrt_barrier_init/reset/arrive/sync/destroy()` | 屏障操作 |

### 4.6 `src/model_data.c` (模型描述)

| 函数 | 说明 |
|------|------|
| `model_get_descriptor()` | 获取模型描述符 |
| `model_get_tensor_map()` | 获取 Tensor 映射表 |
| `model_get_op_descs()` | 获取算子描述表 |
| `model_get_schedule()` | 获取静态调度表 |
| `model_fill_args()` | 填充算子参数 |
| `model_get_op_args()` | 获取指定算子的参数指针 |

### 4.7 `src/ops.c` (算子)

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

## 5. 运行流程

### 5.1 调用顺序

```
main()
  ├─ 分配: input_buffer[1], output_buffer[1]
  └─ 调用: tvmgen_default_run(&inputs, &outputs)

tvmgen_default_run() [default_lib0.c]
  ├─ 使用: global_const_workspace, global_workspace
  └─ 调用: tvmgen_default___tvm_main__(input, output, const_ws, ws)

tvmgen_default___tvm_main__() [default_lib1.c]
  ├─ 调用: tvmrt_engine_init()  ← 创建线程池 + 初始化任务队列
  │
  ├─ 调用: init_op_execs()
  │   ├─ model_fill_args()      ← 填充 g_fused_add_args[]
  │   └─ model_get_op_args()    ← 获取参数指针
  │
  ├─ 构造: tvmrt_context_t ctx
  └─ 调用: tvmrt_engine_run(&ctx, schedule)  ← BSP 调度执行

tvmrt_engine_run() [tvmrt.c]
  ├─ 逐层执行:
  │   ├─ load_next_layer()      ← 填充 task_queue 并 signal
  │   └─ tvmrt_barrier_sync()   ← 等待当前层完成
  └─ 返回

worker_func() [Worker 线程, tvmrt.c]
  ├─ lock & wait(队列非空)
  ├─ pop(op_id) & unlock
  ├─ signal() 唤醒下一个 Worker  ← 链式唤醒
  ├─ execute(op_id)
  └─ barrier_arrive()           ← 通知完成
```

### 5.2 BSP 任务队列执行模型

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

### 5.3 数据流

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

## 6. 核心类型定义

### 6.1 数据结构 (定义在 `tvmrt.h`)

| 结构体 | 说明 |
|--------|------|
| `tvmrt_layer_queue_t` | 层级任务队列（存储当前层待执行算子） |
| `tvmrt_context_t` | 运行时上下文（workspace、算子执行表） |
| `tvmrt_op_exec_t` | 可执行算子条目（函数指针 + 参数） |
| `tvmrt_schedule_desc_t` | 静态调度表（层数组） |
| `tvmrt_schedule_layer_t` | 单个调度层（算子 ID 数组） |
| `tvmrt_op_desc_t` | 算子描述（名称、后端、输入输出 SID） |
| `tvmrt_tensor_map_entry_t` | Tensor 内存映射项 |

### 6.2 全局静态参数

#### Workspace (default_lib0.c)

```c
static struct global_const_workspace {
    float fused_constant_4_let[1];  // 5.0
    float fused_constant_3_let[1];  // 4.0
    float fused_constant_2_let[1];  // 3.0
    float fused_constant_1_let[1];  // 2.0
    float fused_constant_let[1];    // 1.0
} global_const_workspace = {...};

static uint8_t global_workspace[36];
```

#### 模型描述参数 (model_data.c)

| 参数 | 类型 | 说明 |
|------|------|------|
| `g_tensor_map[]` | `tvmrt_tensor_map_entry_t[]` | 张量内存映射表（5个 SID） |
| `g_op_descs[]` | `tvmrt_op_desc_t[]` | 算子描述表（6个节点） |
| `g_cpu_func_table[]` | `tvmrt_op_func_t[]` | CPU 函数指针表 |
| `g_schedule_layers[]` | `tvmrt_schedule_layer_t[]` | 静态调度表（4层） |
| `g_fused_add_args[5]` | `FusedAddArgs[]` | 单输入算子参数 |
| `g_fused_add3_args` | `FusedAdd3Args` | 双输入算子参数 |

#### 引擎状态 (tvmrt.c)

| 参数 | 类型 | 说明 |
|------|------|------|
| `g_engine` | `engine_state_t` | 全局引擎状态 |
| `g_engine.task_queue` | `tvmrt_layer_queue_t` | 当前层任务队列 |
| `g_engine.workers[4]` | `tvmrt_thread_t[]` | Worker 线程数组 |
| `g_engine.layer_barrier` | `tvmrt_barrier_t` | 层间同步屏障 |

---

## 7. 编译与运行

### 7.1 快速开始

```bash
# 编译
make all

# 运行
make run

# 清理
make clean
```

### 7.2 预期输出

```
Running runner...
--------------------------------
验证开始...
输入值: 10.000000
执行成功！
输出值: 23.000000 (预期: 23.000000)
```

### 7.3 编译选项

```makefile
# Makefile 配置
CC = clang
CFLAGS = -Isrc -Iinclude -Wno-everything -g -O2

# Worker 线程数配置（在 tvmrt.h 中）
#define TVMRT_NUM_WORKERS 4  // 可设为 0 启用单线程模式

# 日志开关（在 tvmrt.h 中）
#define TVMRT_LOG_ENABLE 1   // 设为 0 禁用日志
```

---

## 8. 架构优势

### 8.1 职责清晰

| 组件 | 职责 | 文件 |
|------|------|------|
| **入口层** | 测试与外部接口 | `main.c` |
| **Workspace 层** | 内存分配与生命周期 | `default_lib0.c` |
| **初始化层** | Runtime 对接与初始化 | `default_lib1.c` |
| **Runtime 层** | 调度引擎与工具函数 | `tvmrt.h/c` |
| **OS 适配层** | 平台抽象 | `tvmrt_port_posix.c` |
| **模型层** | 编译器生成的描述 | `model_data.c` |
| **算子层** | 编译器生成的实现 | `ops.c` |

### 8.2 文件精简度

| 对比维度 | 模块化版 | 简化版 | 改善 |
|---------|---------|--------|------|
| 文件数量 | 18 个 | 8 个 | **-56%** |
| 目录层级 | 3 层 | 1 层 | **扁平化** |
| 头文件数 | 9 个 | 1 个 | **-89%** |
| Runtime 文件 | 10 个 | 2 个 | **-80%** |

### 8.3 可移植性

- ✅ **单一 Runtime API**：只需 `#include "tvmrt.h"`
- ✅ **OS 适配分离**：替换 `tvmrt_port_*.c` 即可移植
- ✅ **职责清晰**：lib0 负责内存，lib1 负责初始化
- ✅ **零外部依赖**：除 pthread 外无其他依赖
- ✅ **静态内存**：零动态分配，适合嵌入式

### 8.4 集成便利性

对于新项目集成：
1. 复制 `src/` 下的 8 个文件
2. Include `tvmrt.h`
3. 调用 `tvmgen_default_run()`
4. 替换 `model_data.c` 和 `ops.c` 为你的模型

---

## 9. 技术特性

### 9.1 任务队列 + 链式唤醒

**优势**:
- 减少不必要的上下文切换
- 按需唤醒 Worker，降低 CPU 开销
- 清晰表达任务分发语义

**对比**:

| 机制 | 唤醒方式 | 上下文切换 | 适用场景 |
|------|----------|-----------|---------|
| 广播唤醒 | `broadcast` 唤醒所有 Worker | 高 | 简单实现 |
| **链式唤醒** | `signal` 逐个唤醒 | **低** | **高效调度** |

### 9.2 BSP 静态分层

**优势**:
- 保证内存安全（TVM 静态内存规划）
- 层间 Barrier 确保依赖正确
- 编译期生成，零运行时开销

### 9.3 零动态分配

**实现**:
- Workspace 静态分配（`default_lib0.c`）
- 调度表编译期确定（`model_data.c`）
- 线程池启动时创建（`tvmrt_engine_init`）

**适用场景**: 嵌入式、实时系统

---

## 10. 扩展与定制

### 10.1 替换 OS 适配层

创建 `tvmrt_port_rtos.c` 实现 RTOS 线程接口：

```c
// 实现以下函数即可
int tvmrt_mutex_init(tvmrt_mutex_t* m);
int tvmrt_cond_wait(tvmrt_cond_t* c, tvmrt_mutex_t* m);
// ... 其他接口
```

### 10.2 禁用多线程

```c
// 在 tvmrt.h 中设置
#define TVMRT_NUM_WORKERS 0
```

自动回退到单线程模式，无需修改代码。

### 10.3 更换模型

1. 用 TVM 编译新模型
2. 替换 `model_data.c`（描述表）
3. 替换 `ops.c`（算子实现）
4. 更新 `default_lib0.c` 中的 workspace 大小

---

## 11. 文件依赖关系

```
main.c
  └─▶ tvmgen_default.h
  └─▶ default_lib0.c
        └─▶ tvmgen_default.h
        └─▶ default_lib1.c
              └─▶ tvmrt.h
              └─▶ model_data.c
                    └─▶ tvmrt.h
                    └─▶ ops.c
                          └─▶ tvmrt.h
                          └─▶ tvmrt.c
                                └─▶ tvmrt.h
                                └─▶ tvmrt_port_posix.c
                                      └─▶ tvmrt.h
```

**依赖原则**:
- 所有 Runtime 相关代码只需 include `tvmrt.h`
- 入口层使用 TVM 公共头文件 `tvmgen_default.h`
- 无循环依赖

---

## 12. 常见问题

**Q: 如何调整 Worker 线程数？**  
A: 修改 `tvmrt.h` 中的 `TVMRT_NUM_WORKERS`

**Q: 如何启用日志？**  
A: 设置 `TVMRT_LOG_ENABLE 1` 并重新编译

**Q: 支持 GPU 算子吗？**  
A: 当前仅支持 CPU，GPU 支持需扩展 `tvmrt_backend_kind_t`

**Q: lib0 和 lib1 的区别？**  
A: 
- `lib0`: 负责 **内存分配**（workspace）和 **外部接口**（`tvmgen_default_run`）
- `lib1`: 负责 **Runtime 初始化**和 **调度执行**（`tvmgen_default___tvm_main__`）

**Q: 为什么保留 lib0/lib1 分离？**  
A: 
- 清晰的职责边界
- lib0 可独立替换内存分配策略
- lib1 可专注于 Runtime 对接逻辑
- 符合 TVM 生成代码的惯例

---

## 13. 代码统计

```
总计: ~1800 行代码（含注释）

入口层:
- main.c:           30 行
- default_lib0.c:   40 行  
- default_lib1.c:  100 行

Runtime 层:
- tvmrt.h:         500 行
- tvmrt.c:         430 行
- tvmrt_port_posix.c: 200 行

模型层:
- model_data.c:    280 行
- ops.c:           130 行
```

---

## 14. 许可与引用

本项目为 TVM Runtime 的精简嵌入式实现示例。

**参考资料**:
- TVM 官方文档: https://tvm.apache.org/
- BSP 模型: Bulk Synchronous Parallel

---

**最后更新**: 2026-01-12  
**架构版本**: 8 文件精简版 v1.0  
**核心特性**: 保留 lib0/lib1 职责分离 + Runtime 工具单文件化
