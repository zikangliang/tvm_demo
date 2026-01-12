

# change.md — 架构对齐修正需求

## 目标

将当前 `Step 7` 的模块化代码，从**“BSP 分层调度 + CPU 函数指针”**架构，严格迁移到 **PDF《方案评审1212_v0.8》第 18-22 页“研究方案三”** 所描述的 **“就绪队列驱动 + 统一异构实体”** 架构。

请重点解决以下三个与 PDF 描述不符的核心架构差异：

---

## 1. 调度驱动模式差异：BSP 分层 vs 就绪队列驱动

### 差异说明

* **现状（当前代码）**：
* 采用 **BSP（Bulk Synchronous Parallel）** 模式。
* **逻辑**：以 `Layer` 为单位执行。所有 Worker 并行执行完 Layer N 后，必须在 `Barrier` 处全员同步等待，才能进入 Layer N+1。
* **数据结构**：依赖 `g_schedule_layers`（分层数组）。


* **PDF 要求（Page 20, 3.2 核心引擎层）**：
* 采用 **动态就绪队列（Ready Queue）** 模式。
* **逻辑**：不存在 Layer 级的全员等待。Worker 的工作流是循环执行 `判断队列为空 -> 取出一个可调度实体 -> 执行 -> 更新后继节点入度 -> 触发新节点入队`。
* **图示**：Page 20 明确画出了“就绪算子队列”和 Worker 的 Loop 流程；Page 21 展示了 Worker 之间的接力唤醒。



### 涉及修改位置

* **`src/runtime/tvmrt_engine.c`**：核心调度循环（`tvmrt_engine_run`）和 Worker 线程函数（`worker_func`）的逻辑流。
* **`src/model/model_desc.c` / `model_desc.h**`：静态调度表的数据结构需要改变，从“层列表”变为支持运行时动态依赖检查的“图结构”（如节点入度表、后继关系表）。

---
 

## 3. Worker 协同与唤醒机制差异：广播 vs 按需唤醒

### 差异说明

* **现状（当前代码）**：
* 使用 `pthread_cond_broadcast`。
* 逻辑是：主线程在 Layer 开始时**广播唤醒所有** Worker，Worker 抢完当前层任务后沉睡。


* **PDF 要求（Page 21, 3.3 并发管理层）**：
* 描述了 **“Worker 唤醒 Worker”** 的细粒度机制。
* **流程图**：展示了 `Worker0` 执行完任务更新队列后，主动 `唤醒堵塞的 Worker1`。这是一种基于任务产出的按需唤醒，而非基于层级的广播。



### 涉及修改位置

* **`src/runtime/tvmrt_engine.c`**：Worker 线程内部的任务获取与同步逻辑（`worker_func`）。
* **`src/runtime/tvmrt_port.*`**：可能需要调整条件变量或信号量的使用方式以配合按需唤醒。

---

## 请求

请基于以上差异，给出**代码修改方案**，使运行时架构完全对齐 PDF 的“方案三”。