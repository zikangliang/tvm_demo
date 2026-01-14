/**
 * @file main.c
 * @brief TVM Runtime 测试主程序
 *
 * 16 算子 / 9 层 / 8 内存槽 模型
 * 输入: 10.0 → 预期输出: 235.0
 */

#include "tvmgen_default.h"
#include "tvmrt.h"
#include <math.h>
#include <stdio.h>

// 声明入口函数
int32_t tvmgen_default_run(struct tvmgen_default_inputs *inputs,
                           struct tvmgen_default_outputs *outputs);

// ============================================================
// 日志回调函数 (仅当日志启用时编译)
// ============================================================
#if TVMRT_LOG_ENABLE
/**
 * @brief 日志回调，打印算子执行信息
 *
 * 输出格式:
 * - 参数: [DEBUG][W-1] fused_add: p0=10.00 → output@0x...
 * - 结果: [INFO][W-1] fused_add → result=11.00
 */
static void log_callback(const tvmrt_log_record_t* rec, void* user) {
    (void)user;

    const char* level_str = "";
    switch (rec->level) {
        case TVMRT_LOG_DEBUG: level_str = "DEBUG"; break;
        case TVMRT_LOG_INFO:  level_str = "INFO"; break;
        case TVMRT_LOG_WARN:  level_str = "WARN"; break;
        case TVMRT_LOG_ERROR: level_str = "ERROR"; break;
        default: level_str = "?"; break;
    }

    // output_ptr == NULL 表示这是结果日志 (TVMRT_LOG_RESULT)
    if (rec->output_ptr == NULL) {
        printf("[%s][W%d] %s → result=%.2f\n",
               level_str, rec->worker_id, rec->op_name, rec->p0_value);
    } else if (rec->p1_value == 0.0f) {
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
#endif  // TVMRT_LOG_ENABLE

int main(void) {
#if TVMRT_LOG_ENABLE
  // 设置日志回调
  tvmrt_log_set_callback(log_callback, NULL);
#endif

  // 1. 准备数据
  float input_data[1] = {10.0f};
  float output_data[1] = {0.0f};
  float expected = 235.0f;

  // 2. 包装结构体
  struct tvmgen_default_inputs inputs = {.input = input_data};
  struct tvmgen_default_outputs outputs = {.output = output_data};

  printf("========================================\n");
  printf("  TVM Runtime: 16算子 / 9层 / 8内存槽\n");
  printf("========================================\n");
  printf("输入值: %.1f\n", input_data[0]);
  printf("预期输出: %.1f\n\n", expected);

  // 3. 运行推理
  printf("执行中...\n");
  int32_t ret = tvmgen_default_run(&inputs, &outputs);

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
