# change.md — 与《方案评审1212_v0.8》中“研究方案三”不贴合点 & 代码修改清单

> 目标：你当前实现“写死输入/固定计算顺序/固定算子函数/CPU-only”是可以作为 **phase-0 原型** 的。  
> 这个 change.md 只做一件事：把 **哪些代码点** 与 PDF 中“研究方案三（3.1/3.2/3.3）+ 指标表”不贴合列出来，并给出 **你下一步改代码的落点**（文件/行号/建议结构）。

---

## 0. 当前代码实现概览（便于对照）

### default_lib0.c
- 主要内容：常量区 `global_const_workspace` + 工作区 `global_workspace` 静态分配；提供 `tvmgen_default_run()` 作为入口，调用 `tvmgen_default___tvm_main__()`。

### default_lib1.c
- 主要内容：
  - 以 `OpMetadata{name, func, args}` 表示“可调度实体（雏形）”
  - 用 **BSP 分层静态调度表** `g_static_schedule[]` 固定执行顺序
  - 用 **pthread 线程池 + cond/mutex** 做层内任务并行 + 层间同步（barrier）
  - `__tvm_main__()` 中 **硬编码** workspace offset（sid_1..sid_5）+ 填充全局 args（g_args_node0..5）

---

## 1) 与 PDF「3.1 语义转换层」不贴合：缺少“解析异构编译产物 → 组装可调度实体”的初始化流程

PDF 的 3.1 期望：运行时初始化阶段 **解析编译产物**，抽取：
- 算子运行实体（入口/参数/后端类型）
- 张量内存映射（workspace offset、复用关系、对齐等）
然后转成统一的可调度实体池，支撑确定性调度与工程化部署。

### 1.1 你当前代码的现状
- **没有“产物解析/装配器”**：图、算子、调度、内存映射都写死在 C 代码里。
- 可调度实体仅仅是 `{func,args}`（CPU-only 函数指针），没有“后端类型/可执行文件句柄/资源描述”。

### 1.2 具体不贴合的代码点（文件/行号）
- `default_lib1.c`
  - 写死图节点 & 参数地址绑定：`g_graph_nodes[]`（L88-L101）
  - 写死参数容器：`static FusedAddArgs g_args_node*`（L73-L78）
  - 写死 workspace 映射 + 写死依赖连接：`tvmgen_default___tvm_main__()` 内 `sid_*` 与 `g_args_node*` 填充（L476-L491）

### 1.3 建议修改（不要求你现在支持“文件系统加载”，也能更贴合 PDF）
> **关键思想：把“写死”从“写死在代码逻辑里”迁移到“写死在可解释的表里”，运行时只负责解释表。**  
> 这样即使 CPU-only/固定顺序，也能对齐 PDF 的“解释编译期产物”。

✅ 建议改法（phase-1，仍可保持 CPU-only）：
- 新增 **编译产物描述结构**（可以先是“编译期生成的 const 表”，不需要文件系统）：
  - `model_desc.h/.c`（由编译器/脚本生成）
    - `TensorMapEntry[]`：每个 sid 的 offset/size/align/alias_to
    - `OpDesc[]`：每个 op 的 `op_kind(=CPU)`、entry_id、输入 sid 列表、输出 sid 列表、执行配置
    - `ScheduleDesc[]`：静态调度表（layer 或 ready-set）
- 运行时新增 `semantic_init(ctx, model_desc, input_ptrs, output_ptrs)`：
  - 解析 `TensorMapEntry` 得到 sid 指针（替代 L478-L483 的硬编码）
  - 解析 `OpDesc` 填充 `OpExec`（替代 g_graph_nodes 写死）
  - 填充 args（替代 g_args_node0..5 的全局写死）

---

## 2) 与 PDF「3.2 核心引擎层」不贴合：缺少“加载静态调度图+内存映射表”的通用调度执行器（目前是手写 BSP）

PDF 的 3.2 / 指标表中要求运行时框架：
- **加载静态调度图和内存映射表，实现任务级调度与执行**（对应技术点 3.2）
- 采用“就绪集合/就绪队列 + worker take/execute/block”的工作流（更像通用引擎）

### 2.1 你当前代码的现状
- 调度表 `g_static_schedule[]` 虽然是“静态表”，但它是 **绑定在具体模型/具体 DAG** 上的 BSP 分层表；
- 调度执行器 `static_schedule_run()` 直接 for layer 执行（L342-L380），没有“加载/解释调度图”的接口；
- “就绪队列/就绪集合”在代码中没有体现（只有 layer 内抢任务的 next_task_idx）。

### 2.2 具体不贴合的代码点（文件/行号）
- `default_lib1.c`
  - BSP 静态分层调度表：`g_layer*_tasks` + `g_static_schedule`（L126-L145）
  - 调度执行：`static_schedule_run()`（L342-L380）
  - “任务分发”使用 `next_task_idx`，但没有“就绪队列”抽象（L204-L213, L280-L295）

### 2.3 建议修改（phase-1 可保持 BSP；phase-2 对齐 ready-queue）
**phase-1（最小改动，仍能对齐“加载/解释静态调度图”）：**
- 把 `g_static_schedule` 从 “写死在 runtime 文件里” 移到 `model_desc.c` 作为 `const ScheduleDesc`；
- 让 `static_schedule_run(ctx, schedule_desc)` 成为 **通用解释器**，而不是依赖 `NUM_GRAPH_NODES/NUM_LAYERS` 的硬编码宏。

**phase-2（更贴合 PDF 的 ready-queue 表述）：**
- 引入 `ReadyQueue`：
  - 编译期产物给出：每个节点的 `successor_list[]`、初始 `indegree[]` 或 “就绪算子索引集合”
  - 运行时：worker 从 `ReadyQueue` take → execute → 对后继做 indegree--，为 0 则 push
- 为保证确定性：当多个 op 同时 ready，可使用固定优先级（按 op_index 升序）入队。

---

## 3) 与 PDF「3.3 并发管理层」不贴合：缺少“异构资源协调/统一调度多个后端可执行文件”（当前 CPU-only）

PDF 的 3.3 / 指标表要求运行时框架：
- **支持统一调度多个后端可执行文件**（对应技术点 3.3）
- 并发管理不仅是“多 worker 并行”，还包括“跨后端交互、资源互斥、同步/唤醒、数据搬运/同步点”等。

### 3.1 你当前代码的现状
- `OpMetadata.func` 只有 CPU 函数指针，不支持 NPU/GPU 子图实体；
- 没有 `BackendModule` 概念（例如 CPU 函数表、NPU 引擎句柄等）；
- 没有跨后端同步点/数据搬运描述（即使后续还不实现，也需要接口占位才能贴合 PDF）。

### 3.2 具体不贴合的代码点（文件/行号）
- `default_lib1.c`
  - 可执行实体只支持 CPU：`operator_func_t` / `OpMetadata.func`（L21-L37）
  - 调用处直接 `node->func(node->args)`（L289-L292）
  - `__tvm_main__` 直接把所有节点绑定到 `wrapped_fused_*`（L485-L490）

### 3.3 建议修改（phase-1：接口占位；phase-2：真的上异构）
✅ phase-1（先贴合“统一调度”结构，仍只实现 CPU）：
- 定义 `OpExec`：
  - `exec_kind = CPU_FUNC / NPU_SUBGRAPH / GPU_KERNEL ...`
  - `union { cpu.func_ptr; npu.handle; ... }`
- 定义 `BackendModule[]`（数组即可，不需要动态加载）：
  - `backend_id` + `dispatch()` 接口
- 调度执行器永远只调用 `dispatch(exec, args)`，不直接调用函数指针。

---

## 4) 与指标表“运行时执行日志记录（3.1）”不贴合：目前没有日志/trace

### 4.1 你当前代码的现状
- `OpMetadata.name` 存在（L33-L37），但执行过程中没有任何日志/trace 记录。
- 还引入了 `<stdio.h>`（L10），但并未形成可控、可移植的日志机制。

### 4.2 建议修改
- 不建议直接 `printf`（嵌入式/RTOS 上经常不可用或代价高）。
- 建议引入可裁剪日志接口：
  - `tvmrt_log.h`
    - `typedef void (*tvmrt_log_cb)(const tvmrt_log_record_t* rec, void* user);`
    - 或者固定大小 ring buffer `LOG_BUF_SIZE`（纯静态内存）
  - 记录项建议包含：`op_id/op_name`、start/end timestamp（如有）、worker_id、返回码、backend_kind。

---

## 5) “通用平台/通用 OS 依赖”的不适配性（RTOS 迁移阻碍点）

PDF 明确指出：通用推理框架依赖文件系统/内存管理/解释器/通用并行库等，难以满足 RTOS 确定性与资源可预测。

你当前代码虽然避免了 malloc 和文件系统，但仍有明显 POSIX 依赖与移植风险。

### 5.1 不适配点清单（文件/行号）
- `default_lib1.c`
  - 直接依赖 POSIX pthread：`#include <pthread.h>`（L11）
  - 使用 `pthread_mutex_t / pthread_cond_t / pthread_create / join`（L152-L335）
  - barrier/thread pool 初始化是“运行时创建线程”（L303-L316），在不少 RTOS 上不可用或不建议
  - 头文件 `<stdio.h>`（L10）/ `<math.h>`（L6）在嵌入式裁剪环境中可能不可用或引入体积
- `default_lib0.c`
  - GCC/Clang attribute：`__attribute__((aligned))`, `__attribute__((packed))`（多处）
    - 若目标编译器是 ARMCC/IAR/MSVC，需要做宏适配

### 5.2 必改的正确性问题（P0 Bug）
- **Barrier 没有初始化就被使用**
  - `barrier_init()` 定义了（L163-L168），但全文件未调用；
  - `static_schedule_run()` 会调用 `barrier_reset/barrier_sync`（L360-L371）
  - 在 POSIX 上这是未定义行为；在 RTOS 上更容易直接挂。

✅ 立刻改法：
- 在 `thread_pool_init()` 或 `static_schedule_run()` 最开始调用 `barrier_init(&g_barrier)`，并用布尔标记保证只初始化一次。

### 5.3 建议改法（让 runtime 能“跨平台/RTOS”）
✅ phase-1（最小移植层：先让代码“可替换 pthread”）
- 新增 OS 抽象层：`tvmrt_port.h/.c`
  - `tvmrt_mutex_*`, `tvmrt_cond_*`, `tvmrt_thread_*`, `tvmrt_sleep/yield`, `tvmrt_time_now()`
- `default_lib1.c` 中把 pthread 全部替换成 tvmrt_port API
- 提供两个实现：
  1) `tvmrt_port_posix.c`（你现在的 pthread 逻辑）
  2) `tvmrt_port_single.c`（无多线程：所有 op 在主线程串行执行，保证能跑）

✅ phase-2（更 RTOS-friendly & 确定性）
- 避免运行时创建线程：由系统启动阶段静态创建 worker/task（RTOS 常见做法）
- 避免 cond/mutex 复杂依赖：用 RTOS 原语（semaphore/event group）或编译期固定的“worker->task list”
- 若坚持多核确定性：考虑“编译期固定映射”
  - schedule 里直接给出：每个 layer 每个 worker 要执行的 task 列表
  - 运行时只需要 barrier（或事件）同步，不需要抢占式队列与锁竞争

---

## 6) 可重入/多实例能力：全局静态变量会阻碍“工程化部署/多模型”表述

### 6.1 现状
- `g_args_node0..5`、`g_pool`、`g_barrier` 都是全局静态（L73-L78, L160-L216）
- 若未来需要：同一进程/同一系统中跑多个 model 实例、或并发调用，会互相覆盖状态。

### 6.2 建议修改
- 引入 `RuntimeContext ctx`：
  - 包含：workspace 指针、const_ws 指针、op_exec 数组、args 存储、thread pool 句柄（可选共享）
- `tvmgen_default___tvm_main__()` 变成：
  - `tvmrt_run(&ctx, model_desc, inputs, outputs)`
- CPU-only 阶段也建议至少把 `g_args_node*` 放进 `ctx`，消除全局可变状态。

---

## 7) 建议的优先级（你可以按这个顺序改）

### P0（必须修，避免跑不起来/移植炸）
- [ ] barrier_init 未调用（未定义行为）
- [ ] worker 空转/忙等风险（任务领完但 current_layer 还没置 NULL 时会反复循环）
- [ ] 移除/裁剪无用头文件：`math.h/stdio.h`（若不用）

### P1（让“表述贴合 PDF”：解释产物 + 通用执行器）
- [ ] 把 schedule/memory map/op desc 抽到 `model_desc.c`（编译期产物）
- [ ] runtime 只解释 `model_desc`，不再手写 sid_* offset/写死 g_graph_nodes
- [ ] 加入可裁剪日志机制（ring buffer 或 callback）

### P2（对齐 3.3：统一调度多后端）
- [ ] 引入 `OpExec` + `BackendModule` 抽象（CPU-only 先占位）
- [ ] 调度器走统一 `dispatch()` 路径
- [ ] 后续再接 NPU/GPU 子图实体与同步/搬运

---

## 8) 你改完后，代码应该长成什么样（一个参考目录）

```
runtime/
  tvmrt_port.h
  tvmrt_port_posix.c         # 你现在的 pthread 实现
  tvmrt_port_single.c        # 无线程 fallback
  tvmrt_log.h / tvmrt_log.c  # 可裁剪日志
  tvmrt_engine.h/.c          # 通用调度执行器（解释 schedule_desc）
  tvmrt_semantic.h/.c        # 语义转换层（解释 op/tensor desc）
model_artifacts/
  model_desc.h
  model_desc.c               # 编译期生成：op/tensor/schedule/memmap
generated/
  default_lib0.c             # 只保留入口 & 常量区/工作区（可继续存在）
  default_ops.c              # tvmgen fused 内核（CPU）
```

---

## 附：你这版 CPU-only 原型已经“对齐的点”（别删掉）
- 静态 workspace/常量区（default_lib0.c）✅
- 算子封装为统一签名（wrapped_fused_*）✅
- 静态调度思想（g_static_schedule）✅

> 你要做的不是推倒重来，而是把“写死在逻辑里”变成“写死在可解释表里”，并把 pthread 换成可移植的 port 层。
