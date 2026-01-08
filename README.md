# TVM Demo 程序运行逻辑说明

## 整体架构

```
┌─────────────────────────────────────────────────────────────────┐
│                        编译期 (静态)                              │
├─────────────────────────────────────────────────────────────────┤
│  g_graph_nodes[6]          静态图定义 (OpMetadata)              │
│  ├── Node 0: fused_add       (入度=0, 后继=[1])                 │
│  ├── Node 1: fused_subtract  (入度=1, 后继=[5])                 │
│  ├── Node 2: fused_add_1     (入度=0, 后继=[3])                 │
│  ├── Node 3: fused_subtract_1(入度=1, 后继=[4])                 │
│  ├── Node 4: fused_add_2     (入度=1, 后继=[5])                 │
│  └── Node 5: fused_add_3     (入度=2, 后继=[])                  │
└─────────────────────────────────────────────────────────────────┘
                               │
                               ▼
┌─────────────────────────────────────────────────────────────────┐
│                        运行期 (动态)                              │
├─────────────────────────────────────────────────────────────────┤
│  1. 初始化全局参数 (g_args_node0~5)                              │
│  2. 初始化运行时状态 (g_runtime_state, 重置 remaining_deps)      │
│  3. 拓扑排序执行 (Kahn 算法)                                     │
└─────────────────────────────────────────────────────────────────┘
```

---

## 数据结构

### OpMetadata (只读，编译期常量)
```c
typedef struct {
    const char* name;           // 调试用节点名称
    operator_func_t func;       // 算子函数指针
    void* args;                 // 参数指针
    int32_t dep_count;          // 原始入度
    int32_t successor_count;    // 后继节点数量
    int32_t successors[];       // 后继节点索引
} OpMetadata;
```

### RuntimeState (运行时可变，原子类型)
```c
typedef struct {
    _Atomic int32_t remaining_deps;  // 剩余未完成的依赖数
    _Atomic bool is_completed;       // 是否已执行完成
} RuntimeState;
```

---

## 执行流程

### 1. 入口函数 `tvmgen_default___tvm_main__`

```
输入: input_buffer (10.0), output_buffer, 常量/工作空间
      │
      ▼
┌─────────────────────────────────────┐
│ 1. 计算中间缓冲区指针                │
│    sid_1, sid_2, sid_3, sid_4, sid_5│
└─────────────────────────────────────┘
      │
      ▼
┌─────────────────────────────────────┐
│ 2. 初始化全局参数 g_args_node0~5    │
│    填充每个算子的 p0, output 等指针 │
└─────────────────────────────────────┘
      │
      ▼
┌─────────────────────────────────────┐
│ 3. 调用 static_graph_run()          │
└─────────────────────────────────────┘
```

### 2. 拓扑排序执行 `static_graph_run`

```
┌─────────────────────────────────────────────────────────────────┐
│ Step 1: 初始化运行时状态                                         │
│   for each node:                                                │
│     remaining_deps = dep_count (从静态图复制)                    │
│     is_completed = false                                        │
└─────────────────────────────────────────────────────────────────┘
                               │
                               ▼
┌─────────────────────────────────────────────────────────────────┐
│ Step 2: 将入度为 0 的节点入队                                    │
│   Node 0 (fused_add)    → 入队                                  │
│   Node 2 (fused_add_1)  → 入队                                  │
└─────────────────────────────────────────────────────────────────┘
                               │
                               ▼
┌─────────────────────────────────────────────────────────────────┐
│ Step 3: Kahn 算法主循环                                         │
│   while (队列非空):                                              │
│     1. 取出节点 node_id                                         │
│     2. 执行算子: node->func(node->args)                         │
│     3. 标记完成: is_completed = true                            │
│     4. 遍历后继节点:                                             │
│        - remaining_deps--                                       │
│        - 如果 remaining_deps == 0，入队                         │
└─────────────────────────────────────────────────────────────────┘
```

---

## 具体执行顺序

```
执行顺序 (一种可能的拓扑序):

1. Node 0: fused_add      | input (10.0) + 1.0 = 11.0 → sid_1
2. Node 2: fused_add_1    | input (10.0) + 3.0 = 13.0 → sid_3
3. Node 1: fused_subtract | sid_1 (11.0) - 2.0 = 9.0  → sid_2
4. Node 3: fused_subtract_1| sid_3 (13.0) - 4.0 = 9.0 → sid_4
5. Node 4: fused_add_2    | sid_4 (9.0) + 5.0 = 14.0  → sid_5
6. Node 5: fused_add_3    | sid_2 (9.0) + sid_5 (14.0) = 23.0 → output
```

---

## 依赖关系图

```
        input (10.0)
           │
     ┌─────┴─────┐
     ▼           ▼
  Node 0      Node 2
  (+1.0)      (+3.0)
     │           │
     ▼           ▼
  sid_1       sid_3
  (11.0)      (13.0)
     │           │
     ▼           ▼
  Node 1      Node 3
  (-2.0)      (-4.0)
     │           │
     ▼           ▼
  sid_2       sid_4
  (9.0)       (9.0)
     │           │
     │           ▼
     │        Node 4
     │        (+5.0)
     │           │
     │           ▼
     │        sid_5
     │        (14.0)
     │           │
     └─────┬─────┘
           ▼
        Node 5
     (sid_2 + sid_5)
           │
           ▼
       output
       (23.0)
```

---

## 函数调用顺序

```
main() [main.c]
  │
  └──► tvmgen_default_run() [default_lib0.c]
        │
        └──► tvmgen_default___tvm_main__() [default_lib1.c:428]
              │
              ├── 1. 计算中间缓冲区指针 (sid_1 ~ sid_5)
              │
              ├── 2. node_queue_init(&g_ready_queue) [首次调用]
              │
              ├── 3. 初始化全局参数:
              │      g_args_node0 = {input, sid_1, const_ws, ws}
              │      g_args_node1 = {sid_1, sid_2, ...}
              │      g_args_node2 = {input, sid_3, ...}
              │      g_args_node3 = {sid_3, sid_4, ...}
              │      g_args_node4 = {sid_4, sid_5, ...}
              │      g_args_node5 = {sid_2, sid_5, output, ...}
              │
              └──► 4. static_graph_run() [default_lib1.c:280]
                    │
                    ├── node_queue_reset(&g_ready_queue)
                    │
                    ├── static_graph_init_runtime()
                    │     │
                    │     └── for i in 0..5:
                    │           g_runtime_state[i].remaining_deps = g_graph_nodes[i].dep_count
                    │           g_runtime_state[i].is_completed = false
                    │
                    ├── 入队入度=0的节点:
                    │     node_queue_push(0)  // fused_add
                    │     node_queue_push(2)  // fused_add_1
                    │
                    └── Kahn 算法主循环:
                          │
                          ├── node_queue_pop() → node_id=0
                          │     └── g_graph_nodes[0].func(args)
                          │           └── wrapped_fused_add()
                          │                 └── tvmgen_default_fused_add()
                          │     更新后继: node 1 的 remaining_deps--
                          │
                          ├── node_queue_pop() → node_id=2
                          │     └── wrapped_fused_add_1()
                          │           └── tvmgen_default_fused_add_1()
                          │     更新后继: node 3 的 remaining_deps-- → 入队
                          │
                          ├── node_queue_pop() → node_id=1
                          │     └── wrapped_fused_subtract()
                          │           └── tvmgen_default_fused_subtract()
                          │     更新后继: node 5 的 remaining_deps--
                          │
                          ├── node_queue_pop() → node_id=3
                          │     └── wrapped_fused_subtract_1()
                          │           └── tvmgen_default_fused_subtract_1()
                          │     更新后继: node 4 的 remaining_deps-- → 入队
                          │
                          ├── node_queue_pop() → node_id=4
                          │     └── wrapped_fused_add_2()
                          │           └── tvmgen_default_fused_add_2()
                          │     更新后继: node 5 的 remaining_deps-- → 入队
                          │
                          └── node_queue_pop() → node_id=5
                                └── wrapped_fused_add_3()
                                      └── tvmgen_default_fused_add_3()
                                执行完毕，返回 0
```

---

## 关键函数

| 函数 | 作用 |
|------|------|
| `static_graph_init_runtime()` | 重置所有节点的 `remaining_deps` 为静态 `dep_count` |
| `static_graph_run()` | 按拓扑序执行所有节点，返回 0 表示成功 |
| `node_queue_push()` | 将就绪节点入队 |
| `node_queue_pop()` | 取出就绪节点执行 |
| `wrapped_fused_*()` | 包装函数，统一算子接口 |
