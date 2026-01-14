# 日志系统改动说明

> 添加零开销日志系统，支持通过编译开关启用/禁用日志

---

## 1. 改动概述

本次改动添加了一个基于编译期宏的日志系统，实现：
- 算子执行前打印参数值（输入、输出地址）
- 算子执行后打印计算结果
- 层边界标记显示执行进度
- 零运行时开销的日志开关

---

## 2. 修改的文件

| 文件 | 修改内容 |
|------|----------|
| `src/tvmrt.h` | 扩展 `tvmrt_log_record_t` 结构，添加 `TVMRT_LOG_PARAMS` 和 `TVMRT_LOG_RESULT` 宏 |
| `src/tvmrt.c` | 添加层边界标记，禁用调度引擎日志 |
| `src/ops.c` | 6 个包装函数使用 `TVMRT_LOG_PARAMS` 和 `TVMRT_LOG_RESULT` 宏 |
| `src/main.c` | 添加日志回调函数（条件编译），支持参数和结果两种日志格式 |
| `Makefile` | 添加 `LOG_ENABLE` 开关 |

---

## 3. 新增 API

### 3.1 日志记录结构扩展

```c
typedef struct {
    int32_t op_id;
    const char* op_name;
    int32_t worker_id;
    int32_t ret_code;
    tvmrt_log_level_t level;

    // 参数信息
    float p0_value;     // 输入 p0 的值（或结果值）
    float p1_value;     // 输入 p1 的值 (单输入算子为 0)
    float* output_ptr;  // 输出指针 (NULL 表示结果日志)
} tvmrt_log_record_t;
```

### 3.2 零开销日志宏

```c
// 算子执行前记录参数
#define TVMRT_LOG_PARAMS(name_, p0_, p1_, out_ptr_) \
    do { \
        tvmrt_log_record_t _rec = { \
            .op_id = -1, \
            .op_name = (name_), \
            .worker_id = -1, \
            .ret_code = 0, \
            .level = TVMRT_LOG_DEBUG, \
            .p0_value = (p0_), \
            .p1_value = (p1_), \
            .output_ptr = (out_ptr_) \
        }; \
        tvmrt_log_push(&_rec); \
    } while(0)

// 算子执行后记录结果
#define TVMRT_LOG_RESULT(name_, out_ptr_) \
    do { \
        float _out_val = (out_ptr_) ? *(out_ptr_) : 0.0f; \
        tvmrt_log_record_t _rec = { \
            .op_id = -1, \
            .op_name = (name_), \
            .worker_id = -1, \
            .ret_code = 0, \
            .level = TVMRT_LOG_INFO, \
            .p0_value = _out_val, \
            .p1_value = 0.0f, \
            .output_ptr = NULL \
        }; \
        tvmrt_log_push(&_rec); \
    } while(0)

// TVMRT_LOG_ENABLE=0 时，两个宏都展开为 ((void)0)，完全零开销
```

### 3.3 使用示例

```c
int32_t wrapped_fused_add(void* args) {
    FusedAddArgs* a = (FusedAddArgs*)args;

    // 执行前：记录输入参数
    TVMRT_LOG_PARAMS("fused_add", a->p0 ? *(a->p0) : 0.0f, 0.0f, a->output);

    // 执行算子
    int32_t ret = tvmgen_default_fused_add(a->p0, a->output, a->const_ws, a->ws);

    // 执行后：记录结果
    TVMRT_LOG_RESULT("fused_add", a->output);

    return ret;
}
```

---

## 4. 使用方法

### 4.1 启用日志 (默认)

```bash
make
# 或显式指定
make LOG_ENABLE=1
```

**输出示例**:
```
=== Layer 1 (4 ops) ===
[DEBUG][W-1] fused_add: p0=10.00 → output@0x...050
[INFO][W-1] fused_add → result=11.00
[DEBUG][W-1] fused_add_1: p0=10.00 → output@0x...058
[INFO][W-1] fused_add_1 → result=13.00
...
=== Layer 2 (2 ops) ===
[DEBUG][W-1] fused_add_3: p0=11.00, p1=13.00 → output@0x...070
[INFO][W-1] fused_add_3 → result=24.00
...
```

### 4.2 禁用日志 (零开销)

```bash
make LOG_ENABLE=0
```

**输出示例**:
```
=== Layer 1 (4 ops) ===
=== Layer 2 (2 ops) ===
=== Layer 3 (2 ops) ===
...
```

---

## 5. 日志输出格式

### 5.1 参数日志 (DEBUG 级别)

```
[DEBUG][W-1] fused_add: p0=10.00 → output@0x...
```

| 字段 | 含义 |
|------|------|
| `[DEBUG]` | 日志级别 |
| `[W-1]` | Worker ID (-1 表示单线程) |
| `fused_add` | 算子名称 |
| `p0=10.00` | 输入参数值 |
| `output@0x...` | 输出地址 |

### 5.2 结果日志 (INFO 级别)

```
[INFO][W-1] fused_add → result=11.00
```

| 字段 | 含义 |
|------|------|
| `[INFO]` | 日志级别 |
| `fused_add` | 算子名称 |
| `result=11.00` | 计算结果 |

### 5.3 双输入算子

```
[DEBUG][W-1] fused_add_3: p0=11.00, p1=13.00 → output@0x...
[INFO][W-1] fused_add_3 → result=24.00
```

---

## 6. 验证内容

通过日志可以验证：

| 验证项 | 方法 |
|--------|------|
| **执行顺序** | 检查日志中算子名称的顺序是否符合调度表 |
| **参数正确性** | 检查 p0/p1 的输入值是否符合预期 |
| **计算正确性** | 检查 result 值是否符合计算预期 |
| **内存复用** | 检查相同 output@ 被多次写入时，结果值的变化 |

### 示例：完整验证

```
=== Layer 1 (4 ops) ===
[DEBUG][W-1] fused_add: p0=10.00 → output@0x...050    # 输入 10
[INFO][W-1] fused_add → result=11.00                   # 结果 10+1=11
...
=== Layer 3 (2 ops) ===
[DEBUG][W-1] fused_subtract: p0=24.00 → output@0x...050  # 输入 24，复用 M0
[INFO][W-1] fused_subtract → result=22.00                 # 结果 24-2=22
...
=== Layer 8 (1 op) ===
[DEBUG][W-1] fused_add_3: p0=94.00, p1=45.00 → output@0x...050  # 复用 M0
[INFO][W-1] fused_add_3 → result=139.00                         # 结果 94+45=139
```

**验证 M0 槽位 (@...050) 的变化**:
- Layer 1: `10 + 1 = 11`
- Layer 3: `24 - 2 = 22`
- Layer 8: `94 + 45 = 139`

每次复用前，前一层的计算都已完成，证明内存复用安全。

---

## 7. 零开销保证

### 7.1 禁用时的代码展开

```c
// 源代码
int32_t wrapped_fused_add(void* args) {
    FusedAddArgs* a = (FusedAddArgs*)args;
    TVMRT_LOG_PARAMS("fused_add", a->p0 ? *(a->p0) : 0.0f, 0.0f, a->output);
    int32_t ret = tvmgen_default_fused_add(a->p0, a->output, a->const_ws, a->ws);
    TVMRT_LOG_RESULT("fused_add", a->output);
    return ret;
}

// TVMRT_LOG_ENABLE=0 时展开为
int32_t wrapped_fused_add(void* args) {
    FusedAddArgs* a = (FusedAddArgs*)args;
    ((void)0);  // 完全被优化器移除
    int32_t ret = tvmgen_default_fused_add(a->p0, a->output, a->const_ws, a->ws);
    ((void)0);  // 完全被优化器移除
    return ret;
}
```

### 7.2 禁用时的编译输出

```bash
$ make LOG_ENABLE=0 all
clang ... -DTVMRT_LOG_ENABLE=0 -c src/main.c -o src/main.o
clang ... -DTVMRT_LOG_ENABLE=0 -c src/ops.c -o src/ops.o
...
```

生成的汇编代码**完全不含**：
- 变量声明
- 函数调用
- 内存操作

---

## 8. 移植到 RTOS

如需在 RTOS 平台使用，确保 `tvmrt.h` 中的配置：

```c
// 在 tvmrt.h 或编译选项中定义
#define TVMRT_LOG_ENABLE 0  // 生产环境设为 0
```

或通过 Makefile:

```makefile
# RTOS 生产环境
CFLAGS += -DTVMRT_LOG_ENABLE=0
```

---

**文档版本**: v1.1
**创建日期**: 2026-01-14
**更新日期**: 2026-01-14 (添加结果日志)
**相关文件**: `plan.md`, `src/tvmrt.h`, `src/ops.c`, `src/main.c`
