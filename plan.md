# TVM Demo 简易测试方案

> 利用现有日志系统，通过打印算子执行信息来验证正确性

---

## 1. 方案概述

**核心思路**：扩展现有日志系统，在算子执行时打印参数内容，通过查看日志验证：
- 每个 worker 拿到的算子
- 算子的输入参数值
- 算子的输出结果
- 执行顺序是否符合预期

---

## 2. 当前日志系统分析

### 2.1 现有结构 (tvmrt.h)

```c
// 日志记录结构
typedef struct {
    int32_t op_id;           // 算子 ID
    const char* op_name;     // 算子名称
    int32_t worker_id;       // Worker ID
    int32_t ret_code;        // 返回码
    tvmrt_log_level_t level; // 日志级别
} tvmrt_log_record_t;

// 现有日志宏
TVMRT_LOG_OP_START(op_id, op_name, worker_id)  // 算子开始
TVMRT_LOG_OP_END(op_id, op_name, worker_id, ret_code)  // 算子结束
```

### 2.2 参数结构

```c
// 单输入算子参数
typedef struct {
    float* p0;               // 输入值
    float* output;           // 输出位置
    uint8_t* const_ws;       // 常量区
    uint8_t* ws;             // workspace
} FusedAddArgs;

// 双输入算子参数
typedef struct {
    float* p0;               // 输入1
    float* p1;               // 输入2
    float* output;           // 输出位置
    uint8_t* const_ws;       // 常量区
    uint8_t* ws;             // workspace
} FusedAdd3Args;
```

---

## 3. 修改方案

### 3.1 扩展日志记录结构 (tvmrt.h)

```c
typedef struct {
    int32_t op_id;
    const char* op_name;
    int32_t worker_id;
    int32_t ret_code;
    tvmrt_log_level_t level;

    // 新增：参数信息
    float p0_value;          // 输入 p0 的值
    float p1_value;          // 输入 p1 的值 (单输入算子为 0)
    float* output_ptr;       // 输出指针
} tvmrt_log_record_t;
```

### 3.2 修改包装函数 (ops.c)

在每个包装函数中添加参数日志：

```c
int32_t wrapped_fused_add(void* args) {
    FusedAddArgs* a = (FusedAddArgs*)args;

    // 执行前记录参数
    tvmrt_log_record_t rec = {
        .op_id = -1,  // 由调用方填充
        .op_name = "fused_add",
        .worker_id = -1,
        .ret_code = 0,
        .level = TVMRT_LOG_DEBUG,
        .p0_value = a->p0 ? *(a->p0) : 0,
        .p1_value = 0,
        .output_ptr = a->output
    };
    tvmrt_log_push(&rec);

    return tvmgen_default_fused_add(a->p0, a->output, a->const_ws, a->ws);
}

int32_t wrapped_fused_add_3(void* args) {
    FusedAdd3Args* a = (FusedAdd3Args*)args;

    tvmrt_log_record_t rec = {
        .op_name = "fused_add_3",
        .level = TVMRT_LOG_DEBUG,
        .p0_value = a->p0 ? *(a->p0) : 0,
        .p1_value = a->p1 ? *(a->p1) : 0,
        .output_ptr = a->output
    };
    tvmrt_log_push(&rec);

    return tvmgen_default_fused_add_3(a->p0, a->p1, a->output, a->const_ws, a->ws);
}
// ... 其他算子类似
```

### 3.3 添加日志打印回调 (main.c)

```c
// 日志回调函数
void log_callback(const tvmrt_log_record_t* rec, void* user) {
    (void)user;

    const char* level_str = "";
    switch (rec->level) {
        case TVMRT_LOG_DEBUG: level_str = "DEBUG"; break;
        case TVMRT_LOG_INFO:  level_str = "INFO"; break;
        case TVMRT_LOG_WARN:  level_str = "WARN"; break;
        case TVMRT_LOG_ERROR: level_str = "ERROR"; break;
    }

    if (rec->p1_value == 0) {
        // 单输入算子
        printf("[%s][W%d] %s: p0=%.2f → output@%p\n",
               level_str, rec->worker_id, rec->op_name,
               rec->p0_value, (void*)rec->output_ptr);
    } else {
        // 双输入算子
        printf("[%s][W%d] %s: p0=%.2f, p1=%.2f → output@%p\n",
               level_str, rec->worker_id, rec->op_name,
               rec->p0_value, rec->p1_value, (void*)rec->output_ptr);
    }
}

int main(void) {
    // 设置日志回调
    tvmrt_log_set_callback(log_callback, NULL);

    // ... 原有代码 ...
}
```

---

## 4. 预期日志输出

### 4.1 单线程模式输出

```
[DEBUG][W-1] L1_add_0: p0=10.00 → output@0x...
[DEBUG][W-1] L1_add_1: p0=10.00 → output@0x...
[DEBUG][W-1] L1_add_2: p0=10.00 → output@0x...
[DEBUG][W-1] L1_add_3: p0=10.00 → output@0x...
[DEBUG][W-1] L2_add3_0: p0=11.00, p1=13.00 → output@0x...
[DEBUG][W-1] L2_add3_1: p0=15.00, p1=11.00 → output@0x...
...
```

### 4.2 多线程模式输出

```
[DEBUG][W0] L1_add_0: p0=10.00 → output@0x...
[DEBUG][W1] L1_add_1: p0=10.00 → output@0x...
[DEBUG][W2] L1_add_2: p0=10.00 → output@0x...
[DEBUG][W3] L1_add_3: p0=10.00 → output@0x...
[BARRIER]
[DEBUG][W0] L2_add3_0: p0=11.00, p1=13.00 → output@0x...
[DEBUG][W1] L2_add3_1: p0=15.00, p1=11.00 → output@0x...
[BARRIER]
...
```

---

## 5. 验证检查清单

通过日志可以验证：

| 检查项 | 验证方法 |
|--------|----------|
| **算子执行顺序** | 检查日志中算子名称的顺序是否符合调度表 |
| **输入参数正确** | 检查 p0/p1 的值是否符合预期 |
| **内存复用正确** | 检查相同 output_ptr 被多次写入时，前一层已完成 |
| **并行执行** | 多线程模式下，同层算子由不同 worker 执行 |
| **层间同步** | 检查下一层算子开始前，上一层所有算子已完成 |

---

## 6. 可选增强

### 6.1 添加输出值打印

```c
// 在包装函数执行后再次记录输出值
tvmrt_log_record_t rec_out = {
    .op_name = "fused_add",
    .level = TVMRT_LOG_INFO,
    .p0_value = 0,
    .p1_value = a->output ? *(a->output) : 0,  // 用 p1 存输出
    .output_ptr = a->output
};
tvmrt_log_push(&rec_out);
```

### 6.2 添加层边界标记

```c
// 在 tvmrt_engine_run_single 中
for (int32_t layer_idx = 0; layer_idx < schedule->layer_count; layer_idx++) {
    printf("=== Layer %d ===\n", layer_idx);
    // ... 执行算子 ...
}
```

---

## 7. 修改文件清单

| 文件 | 修改内容 |
|------|----------|
| `src/tvmrt.h` | 扩展 `tvmrt_log_record_t` 结构 |
| `src/ops.c` | 6 个包装函数中添加参数日志 |
| `src/main.c` | 添加 `log_callback` 函数和调用 |

**预计工作量**: 1-2 小时

---

## 8. 方案优势

| 优势 | 说明 |
|------|------|
| **无外部依赖** | 使用现有日志系统 |
| **修改量小** | 只需修改 3 个文件 |
| **即时反馈** | 运行即可看到日志 |
| **易于调试** | 参数值直接可见 |
| **支持多线程** | worker_id 清晰标识执行者 |

---

**文档版本**: v2.0 (简化版)
**创建日期**: 2026-01-14
**预计工作量**: 1-2 小时
