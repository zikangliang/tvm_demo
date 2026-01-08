# steps.md — 按 PDF 研究方案三改造你当前 DAG 调度实现（给 Claude Code 的补全清单）

> 目标：把你现在 `default_lib1.c` 的「运行期 Kahn topo + 单线程 NodeQueue」实现，改造成更贴合 PDF 研究方案三（3.1/3.2/3.3）的三层结构：  
> **3.1 语义转换层（实体池 + 张量映射） → 3.2 核心引擎层（静态调度表/按深度分层） → 3.3 并发管理层（全局共享就绪队列 + 多 Worker 阻塞/唤醒）**。  
>
> 同时修正一个关键 correctness 风险：你当前 `sid_4` 与 `sid_1` **指针别名**（复用同一 workspace 地址），一旦引入“同层并行”，会出现 **读写冲突/数据竞争**。

---

## 0. 现状对照（你现在的实现 vs PDF 方案三）

你现在 `default_lib1.c` 里做了：
- DAG：`DAGNode { deps[], successors[], remaining_deps }`
- NodeQueue：mutex 保护的环形队列（非阻塞）
- `dag_run()`：Kahn 算法，执行完一个节点立刻解锁后继并入队（**运行期动态 topo 调度**）
- `tvmgen_default___tvm_main__()`：直接 hardcode 构图、加边、跑 `dag_run`

与 PDF 方案三的表述差别主要在：
- PDF 3.2 强调：**解析编译期静态调度表 → 生成并行就绪集合 → 全局共享就绪队列**（不是每次推理动态 topo）。  
- PDF 1.4 强调：按**依赖深度分层**，**同深度并行、跨深度串行**（深度 barrier），保证确定性执行序。  
- PDF 3.3 强调：多 Worker + 阻塞/唤醒（Block/Take/Execute），队列为空时更新就绪队列。

---

## 1. 文件级改动清单

你主要改两个文件：

### 1.1 `default_lib1.c`
要做的改动：
1) 保留/复用 `wrapped_fused_*` 适配层（不动算子实现）
2) **新增**：实体池/张量映射/静态调度表/执行器/多 worker
3) **替换**：把 `dag_run()` 从“动态 topo”换成“**按 depth level 批量入队 + barrier**”
4) **升级**：`NodeQueue` 改成 **阻塞队列**（mutex + condvar），支持 `take()` 阻塞等待
5) **修正**：消除或显式约束 `sid_1` 与 `sid_4` 的内存别名冲突（见第 5 节）

### 1.2 `default_lib0.c`
可选但建议：
- 若你选择“**不复用 workspace、扩大 workspace**”来换取并行（Option A），需要扩大 `global_workspace[]` 大小。

---

## 2. 分层结构落地：建议新增的数据结构（直接让 Claude 按这个实现）

> 尽量“静态数组 + 上限”，符合嵌入式可预测资源上界。

### 2.1 3.1 语义转换层：可调度实体 & 张量映射

```c
typedef enum { BACKEND_CPU=0, BACKEND_GPU=1, BACKEND_NPU=2 } Backend;

typedef struct {
  void* inputs[/*MAX_IN*/];
  void* outputs[/*MAX_OUT*/];
  void* consts[/*MAX_CONST*/];
  void* workspace;          // 全局 workspace 指针（或分片信息）
} TensorMaps;

/* “封闭可执行实体” */
typedef struct {
  int32_t id;
  Backend backend;
  operator_func_t execute;  // 当前阶段先用 operator_func_t（同步语义）
  void* args;               // args 内部最终应当只引用 TensorMaps 的 index 或解析后的指针
  // 可选：资源亲和性、预计 cost、异步句柄等（后续 GPU/NPU 扩展用）
} SchedEntity;
```

> 说明：PDF 3.1 希望把“算子运行实体 + 张量内存映射”提取出来，组装成“可调度实体池”。  
> 你当前在 `tvm_main` 里直接传裸指针 args；建议迁移为 `build_entities(&maps, &pool)`，并把“映射表”显式化。

### 2.2 3.2 核心引擎层：静态调度表（按深度 level）

```c
#define MAX_LEVELS 64
typedef struct {
  int32_t level_count;
  int32_t level_offsets[MAX_LEVELS+1];  // prefix sum
  int32_t level_nodes[MAX_NODES];       // 存 node_id 或 entity_id
} StaticSchedule;
```

- `level_nodes[level_offsets[l] ... level_offsets[l+1)-1]` 是同深度可并行集合。
- 需要一个构建函数 `build_static_schedule_from_graph(...)`（见第 3 节）。

### 2.3 3.3 并发管理层：阻塞就绪队列 + worker 池

把你现在的 `NodeQueue` 升级为阻塞队列：

```c
typedef struct {
  int32_t items[MAX_NODES];
  int32_t head, tail, count;
  bool shutdown;
  pthread_mutex_t mu;
  pthread_cond_t  cv;
} ReadyQueue;
```

API 建议：
- `readyq_init/destroy`
- `readyq_push_batch(int32_t* items, int n)`：一次锁住批量入队，`pthread_cond_broadcast`
- `readyq_take_blocking(int32_t* out)`：队列空则 `cond_wait`，shutdown 则返回 -1
- `readyq_shutdown()`：置 `shutdown=true`，broadcast 唤醒全部 worker 退出

---

## 3. 从“DAG”到“静态调度表”：你必须补的一段构建逻辑（对应 PDF 1.4 + 3.2）

### 3.1 先保留 DAG（但不要用 Kahn 动态跑）
你可以保留你现在的 `dag_add_node / dag_add_dep`，用于描述依赖。

### 3.2 新增：计算依赖深度 depth，并按 depth 分桶
实现一个函数：

```c
int build_depth_levels(
    DAG* dag,
    int32_t depth_out[MAX_NODES],
    StaticSchedule* sched_out
);
```

要求：
1) 以 DAG deps 为基础计算 `depth[node] = max(depth[pred]) + 1`（PDF “前驱最大深度原则”）。
2) 按 depth 分桶生成 level 列表（level=1..maxDepth）。
3) 同一个 level 内节点顺序固定（建议按 node id 升序，保证确定性）。

> ⚠️ 仅基于数据依赖算出的 depth，**在你现在这份 demo 上会产生严重问题**：  
> `n1` 与 `n3` 都会落在同一深度，但它们因为 `sid_1`/`sid_4` 内存别名产生冲突（见第 5 节）。  
> 所以你还需要 3.3 的“内存/资源约束 → 额外顺序边”，或者直接修内存规划。

---

## 4. 运行期执行器：按 level 批量入队 + barrier（贴合 PDF 3.2/3.3）

### 4.1 ExecutorContext（执行上下文）
```c
typedef struct {
  SchedEntity* entities;
  int32_t entity_count;

  StaticSchedule sched;
  ReadyQueue readyq;

  int32_t cur_level;
  int32_t remaining_in_level;       // 用原子或 mu 保护
  pthread_mutex_t level_mu;
  pthread_cond_t  level_cv;
} ExecutorContext;
```

### 4.2 worker 线程函数（Block/Take/Execute）
```c
void* worker_main(void* arg) {
  ExecutorContext* ctx = (ExecutorContext*)arg;
  for (;;) {
    int32_t id;
    if (readyq_take_blocking(&ctx->readyq, &id) != 0) break; // shutdown

    // 执行实体（当前阶段用同步语义）
    int32_t ret = ctx->entities[id].execute(ctx->entities[id].args);
    if (ret != 0) { /* 记录错误并触发 shutdown */ }

    // level 完成计数
    pthread_mutex_lock(&ctx->level_mu);
    ctx->remaining_in_level--;
    if (ctx->remaining_in_level == 0) {
      pthread_cond_signal(&ctx->level_cv);
    }
    pthread_mutex_unlock(&ctx->level_mu);
  }
  return NULL;
}
```

### 4.3 主线程调度：逐 level 推进（跨深度串行）
实现 `executor_run(ctx, num_workers)`：

1) 启动 N 个 worker（pthread_create）
2) for level in [0..sched.level_count):
   - 取出该 level 的节点列表（entity_id）
   - `ctx->remaining_in_level = level_size`
   - `readyq_push_batch(...)` 一次性批量入队（降低锁开销）
   - 等待 `remaining_in_level==0`（cond_wait）
3) 调 `readyq_shutdown`，join workers
4) 返回错误码（如果 worker 执行失败要能传播）

> 这一步是把你现在的“每个节点完成就解锁后继”改成“**编译期静态调度表驱动、就绪集合批量分发**”。

---

## 5. 必须修的 correctness：`sid_1` 与 `sid_4` 复用导致并行冲突（两种处理方案二选一）

你当前 `tvmgen_default___tvm_main__` 里：
```c
float* sid_1 = (float*)(&(global_workspace_1_var[16]));
float* sid_4 = (float*)(&(global_workspace_1_var[16]));  // 复用 sid_1
```

一旦你按 PDF 1.4 做“同深度并行”，在 depth=2 时：
- `n1` 读取 `sid_1`（作为输入）
- `n3` 写 `sid_4`（等价于写 `sid_1`）
会出现 **读写冲突**，导致结果不确定甚至错误。

### Option A（推荐，利于并行）：调整内存规划，取消别名复用
- 改 workspace 划分：给 `sid_4` 分配一个不同地址（不与 `sid_1` 重叠）；`sid_5` 同理。
- 若 workspace 不够，扩大 `default_lib0.c` 的 `global_workspace[]`。

验收：depth=2 的 `n1` 与 `n3` 可以安全并行。

### Option B（保守，保持内存复用）：把“内存冲突”转换为额外顺序约束（等价于静态调度表约束）
- 保持 `sid_4` 复用 `sid_1`
- 在构建静态调度表时，加入一个额外依赖边：`n3` 必须在 `n1` 之后执行（或反过来，但要与原 TVM 顺序一致）
- 这样 `n1` 与 `n3` 就不会出现在同一 level

#### 建议实现成通用机制（给 Claude 的实现任务）
1) 给每个节点标注 `reads[]` / `writes[]` 的 BufferId（最小化实现：只标注 sid_1..sid_5 + output）
2) 维护 alias 组（例如 `sid_1` 与 `sid_4` 属于同一 alias group）
3) 给一个“原始静态顺序”数组 `static_order[]`（来自 TVM 原线性顺序：n0,n1,n2,n3,n4,n5）
4) 遍历 `static_order` 的所有 i<j 对，如果两节点在 alias group 上存在 RAW/WAR/WAW 冲突，则加边 `i -> j`
5) 再计算 depth 分层

验收：即使多 worker 并行，同一 level 内无内存冲突。

---

## 6. 把 `tvmgen_default___tvm_main__` 变薄（对齐 PDF 3.1 分层）

现在 `tvm_main` 里 hardcode 构图/加边/执行。改为：

1) `prepare_tensor_maps(&maps, input, output, const_ws, workspace)`
2) `build_entities(&maps, &entity_pool)`（3.1）
3) `build_graph_deps(&dag)`（3.1/3.2：依赖描述）
4) `build_static_schedule(&dag, &sched)`（3.2：静态调度表/level）
5) `executor_run(&ctx, NUM_WORKERS)`（3.3：并发管理）

> 注意：args 建议不要用 `static`，至少改成“每次调用独立”的对象（栈上或 ctx 内），避免未来并发调用互相踩。

---

## 7. 队列性能优化点（按 PDF 3.2 的“就绪队列更新”思路）

你现在 push/pop 都是单元素锁一次。按新结构应当：
- `readyq_push_batch()`：一次加锁塞一整个 level（显著减少锁次数）
- worker `take` 使用 condvar 阻塞，避免忙等
- （可选）如果 worker 数很大，可用 `cond_signal` + “队列里还有任务就不 broadcast”，但先实现正确版本

---

## 8. 最小验收标准（Claude 补完后你要怎么判断改对了）

### Correctness（必须）
- 仍然能跑通 `tvmgen_default_run`
- 多 worker（>=2）时输出与单线程一致
- 对于当前 demo：输出应满足 `output = 2*input + 3`（你可以用几组输入快速回归）

### Determinism（对齐 PDF）
- 每次运行同一输入，输出一致
- 同一 level 内执行顺序即使不同，也不影响结果（必须满足：无数据依赖 + 无内存冲突）

### 架构对齐（对齐 PDF 3.1/3.2/3.3）
- `tvm_main` 只负责 maps 绑定与调用 executor，不直接写调度逻辑
- 存在明确的：实体池（3.1）、静态调度表（3.2）、并发管理/worker/就绪队列（3.3）

---

## 9. 备注（关于文件“过期”）
如果你后续要我继续对齐更多算子/更大模型，记得把最新源码和 PDF 版本再上传一次（系统可能会让旧文件过期）。
