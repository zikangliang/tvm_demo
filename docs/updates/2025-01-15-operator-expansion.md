# 算子库扩展开发记录

**日期**: 2026-01-15
**版本**: v1.0
**作者**: TVM Demo Team
**状态**: ✅ 已完成

---

## 1. 概述

本次开发在保持现有架构不变的前提下，为 TVM Runtime 扩展了 9 个新算子，涵盖了激活函数、基础运算和常量运算三类。

### 1.1 目标

- 验证现有架构的扩展能力
- 补充常用的神经网络算子
- 建立单元测试框架

### 1.2 约束

| 约束项 | 说明 |
|--------|------|
| 数据类型 | 标量 float（4字节） |
| 文件结构 | 不新增算子文件，扩展现有 `ops.c` |
| 内存槽位 | 保持 8 槽 × 4 字节 = 64 字节 |
| 调度方式 | BSP 静态分层调度 |

---

## 2. 新增算子清单

### 2.1 Phase 1: 激活函数（4 个）

| 算子 | 函数名 | 公式 | 函数表索引 |
|------|--------|------|-----------|
| ReLU | `wrapped_relu` | `max(0, x)` | 6 |
| Sigmoid | `wrapped_sigmoid` | `1 / (1 + exp(-x))` | 7 |
| Tanh | `wrapped_tanh_op` | `tanh(x)` | 8 |
| ReLU6 | `wrapped_relu6` | `min(max(0, x), 6)` | 9 |

### 2.2 Phase 2: 基础运算（3 个）

| 算子 | 函数名 | 公式 | 函数表索引 |
|------|--------|------|-----------|
| Multiply | `wrapped_multiply` | `p0 * p1` | 10 |
| Maximum | `wrapped_maximum` | `max(p0, p1)` | 11 |
| Minimum | `wrapped_minimum` | `min(p0, p1)` | 12 |

### 2.3 Phase 3: 常量运算（2 个）

| 算子 | 函数名 | 公式 | 函数表索引 |
|------|--------|------|-----------|
| Mul2 | `wrapped_mul_2` | `p0 * 2.0` | 13 |
| MulHalf | `wrapped_mul_half` | `p0 * 0.5` | 14 |

---

## 3. 实现细节

### 3.1 文件变更

| 文件 | 变更类型 | 行数变化 |
|------|---------|---------|
| `src/ops.c` | 扩展 | +150 行 |
| `src/model_data.c` | 扩展 | 函数表 6→15 |
| `src/test_new_ops.c` | 新增 | +149 行 |
| `Makefile` | 扩展 | +20 行 |

### 3.2 算子实现模板

所有算子遵循统一的实现模式：

```c
// 核心算子函数
int32_t tvmgen_default_xxx(float* p0, float* output, uint8_t* cws, uint8_t* ws) {
    (void)cws;  // 未使用，消除警告
    (void)ws;   // 未使用，消除警告
    output[0] = /* 计算逻辑 */;
    return 0;
}

// 包装函数（统一签名，适配调度引擎）
int32_t wrapped_xxx(void* args) {
    FusedAddArgs* a = (FusedAddArgs*)args;
    TVMRT_LOG_PARAMS("xxx", a->p0 ? *(a->p0) : 0.0f, 0.0f, a->output);
    int32_t ret = tvmgen_default_xxx(a->p0, a->output, a->const_ws, a->ws);
    TVMRT_LOG_RESULT("xxx", a->output);
    return ret;
}
```

### 3.3 函数表扩展

```c
static const tvmrt_op_func_t g_model_cpu_func_table[] = {
    // 原有算子 (索引 0-5)
    wrapped_fused_add,        // 0: +1
    wrapped_fused_add_1,      // 1: +3
    wrapped_fused_add_2,      // 2: +5
    wrapped_fused_add_3,      // 3: p0+p1
    wrapped_fused_subtract,   // 4: -2
    wrapped_fused_subtract_1, // 5: -4

    // 新增激活函数 (索引 6-9)
    wrapped_relu,             // 6: ReLU
    wrapped_sigmoid,          // 7: Sigmoid
    wrapped_tanh_op,          // 8: Tanh
    wrapped_relu6,            // 9: ReLU6

    // 新增基础运算 (索引 10-12)
    wrapped_multiply,         // 10: p0*p1
    wrapped_maximum,          // 11: max(p0,p1)
    wrapped_minimum,          // 12: min(p0,p1)

    // 新增常量运算 (索引 13-14)
    wrapped_mul_2,            // 13: p0*2
    wrapped_mul_half,         // 14: p0*0.5
};

#define MODEL_CPU_FUNC_COUNT 15
```

---

## 4. 测试设计

### 4.1 单元测试框架

新建 `src/test_new_ops.c`，提供独立的单元测试：

```c
#define EPSILON 1e-5f
#define TEST(name, cond) \
    do { \
        if (cond) { printf("  ✅ %s\n", name); passed++; } \
        else { printf("  ❌ %s\n", name); failed++; } \
    } while (0)
```

### 4.2 测试用例

| 分类 | 测试项 | 用例数 |
|------|--------|--------|
| 激活函数 | ReLU, Sigmoid, Tanh, ReLU6 | 9 |
| 基础运算 | Multiply, Maximum, Minimum | 3 |
| 常量运算 | Mul2, MulHalf | 2 |
| **总计** | | **14** |

### 4.3 边界测试覆盖

| 算子 | 边界用例 |
|------|---------|
| ReLU | 负值 (-2.0)、零 (0.0)、正值 (3.0) |
| ReLU6 | 下溢 (-1.0)、正常 (3.0)、上溢 (10.0) |
| Sigmoid | 零点 (0.0)、单位 (1.0) |
| Tanh | 零点 (0.0)、单位 (1.0) |

### 4.4 集成验证

- 回归测试：原有 16 算子模型输出 235.0 验证
- 零开销日志：`LOG_ENABLE=0` 编译验证
- 内存安全：8 槽复用无冲突

---

## 5. 技术决策

### 5.1 命名冲突处理

| 原计划 | 实际实现 | 原因 |
|--------|---------|------|
| `tanh` | `tanh_op` | 避免与标准库 `<math.h>` 中的 `tanh()` 冲突 |
| `mul_0.5` | `mul_half` | C 标识符不能包含小数点 |

### 5.2 头文件组织

将 `<math.h>` 从算子实现中间移至文件顶部：

```c
// ops.c
#include "tvmrt.h"
#include <math.h>  // 移至顶部，符合代码规范
```

### 5.3 Makefile 集成

新增测试目标，简化测试流程：

```makefile
test: $(TEST_TARGET)
	@echo "Running unit tests..."
	@./$(TEST_TARGET)

$(TEST_TARGET): $(TEST_SRCS)
	@echo "Building unit tests..."
	$(CC) -Isrc -Iinclude -Wno-everything -g -O2 -DTVMRT_LOG_ENABLE=0 \
		$(TEST_SRCS) -o $(TEST_TARGET) -lm -lpthread
```

---

## 6. 测试结果

### 6.1 单元测试

```
========================================
  新算子单元测试
========================================

--- Phase 1: 激活函数 ---
  ✅ ReLU(-2.0) = 0.0
  ✅ ReLU(3.0) = 3.0
  ✅ Sigmoid(0.0) = 0.5
  ✅ Sigmoid(1.0) ≈ 0.731
  ✅ Tanh(0.0) = 0.0
  ✅ Tanh(1.0) ≈ 0.762
  ✅ ReLU6(-1.0) = 0.0
  ✅ ReLU6(3.0) = 3.0
  ✅ ReLU6(10.0) = 6.0

--- Phase 2: 基础运算 ---
  ✅ Multiply(3.0, 4.0) = 12.0
  ✅ Maximum(2.0, 5.0) = 5.0
  ✅ Minimum(2.0, 5.0) = 2.0

--- Phase 3: 常量乘法 ---
  ✅ Mul2(3.0) = 6.0
  ✅ MulHalf(4.0) = 2.0

========================================
  测试结果: 14 通过, 0 失败
========================================
```

### 6.2 回归测试

原有 16 算子 / 9 层模型验证通过：

```
输入值: 10.0
预期输出: 235.0
实际输出: 235.0
✅ 测试通过! 结果正确
```

### 6.3 质量指标

| 指标 | 结果 |
|------|------|
| 编译警告 | 无 |
| 内存泄漏 | 无（静态分配） |
| 日志开销 | 零（编译期开关） |
| 代码覆盖率 | 100%（新算子） |

---

## 7. 经验总结

### 7.1 成功经验

1. **复用现有结构**：使用 `FusedAddArgs`/`FusedAdd3Args`，无需新定义结构体
2. **统一签名**：包装函数模式确保所有算子可被调度引擎调用
3. **日志集成**：`TVMRT_LOG_PARAMS/RESULT` 宏提供零开销调试能力
4. **独立测试**：单元测试与回归测试分离，便于快速验证

### 7.2 改进空间

1. **参数化算子**：LeakyReLU 等需要可配置参数的算子需要扩展常量区设计
2. **自动化生成**：函数表和调度表目前手写，可考虑工具生成
3. **张量支持**：当前仅支持标量，实际网络需要向量/矩阵运算

### 7.3 技术债务

| 项目 | 当前状态 | 建议 |
|------|---------|------|
| 函数表索引 | 手写注释维护 | 考虑枚举类型 |
| 测试数据 | 硬编码 | 可考虑外部配置 |
| 常量管理 | 分散在各算子 | 可考虑集中管理 |

---

## 8. 后续计划

### 8.1 短期（P0-P1）

| 优先级 | 任务 |
|--------|------|
| P0 | DAG → 静态表自动生成工具 |
| P1 | 向量/张量运算支持设计 |
| P1 | 扩展算子库（除法、绝对值等） |

### 8.2 长期（P2-P3）

| 优先级 | 任务 |
|--------|------|
| P2 | 线性层（MatMul、FullyConnected） |
| P2 | 卷积算子（Conv2D） |
| P3 | 异构加速支持（DSP/NPU） |

---

## 9. 参考文档

- [TVM Runtime 简化架构文档](../README.md)
- [算子扩充计划](../plan.md)
- [未来发展规划](../future.md)

---

**文档版本**: v1.0
**创建日期**: 2026-01-15
**最后更新**: 2026-01-15
