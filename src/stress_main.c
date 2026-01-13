/**
 * @file stress_main.c
 * @brief 压力测试主程序
 *
 * 验证 16 算子 / 8 层 / 8 内存槽的复杂调度
 * 输入: 10.0 → 预期输出: 235.0
 */

#include "tvmgen_default.h"
#include <math.h>
#include <stdio.h>

// 声明 stress_lib0.c 的入口函数
int32_t stress_run(struct tvmgen_default_inputs *inputs,
                   struct tvmgen_default_outputs *outputs);

int main(void) {
  // 1. 准备数据
  float input_data[1] = {10.0f};
  float output_data[1] = {0.0f};
  float expected = 235.0f;

  // 2. 包装结构体
  struct tvmgen_default_inputs inputs = {.input = input_data};
  struct tvmgen_default_outputs outputs = {.output = output_data};

  printf("========================================\n");
  printf("  压力测试: 16算子 / 8层 / 8内存槽\n");
  printf("========================================\n");
  printf("输入值: %.1f\n", input_data[0]);
  printf("预期输出: %.1f\n\n", expected);

  // 3. 运行推理
  printf("执行中...\n");
  int32_t ret = stress_run(&inputs, &outputs);

  // 4. 验证结果
  printf("\n--- 结果 ---\n");
  if (ret == 0) {
    printf("执行状态: 成功\n");
    printf("实际输出: %.1f\n", output_data[0]);

    if (fabs(output_data[0] - expected) < 0.001f) {
      printf("\n✅ 测试通过! 结果正确\n");
    } else {
      printf("\n❌ 测试失败! 结果不匹配\n");
      printf("   差值: %.6f\n", output_data[0] - expected);
    }
  } else {
    printf("❌ 执行出错，错误码: %d\n", ret);
  }

  printf("========================================\n");
  return (ret == 0 && fabs(output_data[0] - expected) < 0.001f) ? 0 : 1;
}
