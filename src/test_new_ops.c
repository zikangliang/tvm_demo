/**
 * @file test_new_ops.c
 * @brief 新算子单元测试
 *
 * 验证 Phase 1-3 添加的 9 个新算子的正确性
 */

#include <math.h>
#include <stdint.h>
#include <stdio.h>

// 直接调用算子函数进行测试
extern int32_t tvmgen_default_relu(float *p0, float *output, uint8_t *cws,
                                   uint8_t *ws);
extern int32_t tvmgen_default_sigmoid(float *p0, float *output, uint8_t *cws,
                                      uint8_t *ws);
extern int32_t tvmgen_default_tanh_op(float *p0, float *output, uint8_t *cws,
                                      uint8_t *ws);
extern int32_t tvmgen_default_relu6(float *p0, float *output, uint8_t *cws,
                                    uint8_t *ws);
extern int32_t tvmgen_default_multiply(float *p0, float *p1, float *output,
                                       uint8_t *cws, uint8_t *ws);
extern int32_t tvmgen_default_maximum(float *p0, float *p1, float *output,
                                      uint8_t *cws, uint8_t *ws);
extern int32_t tvmgen_default_minimum(float *p0, float *p1, float *output,
                                      uint8_t *cws, uint8_t *ws);
extern int32_t tvmgen_default_mul_2(float *p0, float *output, uint8_t *cws,
                                    uint8_t *ws);
extern int32_t tvmgen_default_mul_half(float *p0, float *output, uint8_t *cws,
                                       uint8_t *ws);

#define EPSILON 1e-5f
#define TEST(name, cond)                                                       \
  do {                                                                         \
    if (cond) {                                                                \
      printf("  ✅ %s\n", name);                                               \
      passed++;                                                                \
    } else {                                                                   \
      printf("  ❌ %s\n", name);                                               \
      failed++;                                                                \
    }                                                                          \
  } while (0)

int main(void) {
  int passed = 0, failed = 0;
  float in, in2, out;

  printf("========================================\n");
  printf("  新算子单元测试\n");
  printf("========================================\n\n");

  // Phase 1: 激活函数测试
  printf("--- Phase 1: 激活函数 ---\n");

  // ReLU
  in = -2.0f;
  out = 0.0f;
  tvmgen_default_relu(&in, &out, NULL, NULL);
  TEST("ReLU(-2.0) = 0.0", fabsf(out - 0.0f) < EPSILON);

  in = 3.0f;
  out = 0.0f;
  tvmgen_default_relu(&in, &out, NULL, NULL);
  TEST("ReLU(3.0) = 3.0", fabsf(out - 3.0f) < EPSILON);

  // Sigmoid
  in = 0.0f;
  out = 0.0f;
  tvmgen_default_sigmoid(&in, &out, NULL, NULL);
  TEST("Sigmoid(0.0) = 0.5", fabsf(out - 0.5f) < EPSILON);

  in = 1.0f;
  out = 0.0f;
  tvmgen_default_sigmoid(&in, &out, NULL, NULL);
  TEST("Sigmoid(1.0) ≈ 0.731", fabsf(out - 0.7310586f) < EPSILON);

  // Tanh
  in = 0.0f;
  out = 0.0f;
  tvmgen_default_tanh_op(&in, &out, NULL, NULL);
  TEST("Tanh(0.0) = 0.0", fabsf(out - 0.0f) < EPSILON);

  in = 1.0f;
  out = 0.0f;
  tvmgen_default_tanh_op(&in, &out, NULL, NULL);
  TEST("Tanh(1.0) ≈ 0.762", fabsf(out - 0.7615942f) < EPSILON);

  // ReLU6
  in = -1.0f;
  out = 0.0f;
  tvmgen_default_relu6(&in, &out, NULL, NULL);
  TEST("ReLU6(-1.0) = 0.0", fabsf(out - 0.0f) < EPSILON);

  in = 3.0f;
  out = 0.0f;
  tvmgen_default_relu6(&in, &out, NULL, NULL);
  TEST("ReLU6(3.0) = 3.0", fabsf(out - 3.0f) < EPSILON);

  in = 10.0f;
  out = 0.0f;
  tvmgen_default_relu6(&in, &out, NULL, NULL);
  TEST("ReLU6(10.0) = 6.0", fabsf(out - 6.0f) < EPSILON);

  // Phase 2: 基础运算测试
  printf("\n--- Phase 2: 基础运算 ---\n");

  // Multiply
  in = 3.0f;
  in2 = 4.0f;
  out = 0.0f;
  tvmgen_default_multiply(&in, &in2, &out, NULL, NULL);
  TEST("Multiply(3.0, 4.0) = 12.0", fabsf(out - 12.0f) < EPSILON);

  // Maximum
  in = 2.0f;
  in2 = 5.0f;
  out = 0.0f;
  tvmgen_default_maximum(&in, &in2, &out, NULL, NULL);
  TEST("Maximum(2.0, 5.0) = 5.0", fabsf(out - 5.0f) < EPSILON);

  // Minimum
  in = 2.0f;
  in2 = 5.0f;
  out = 0.0f;
  tvmgen_default_minimum(&in, &in2, &out, NULL, NULL);
  TEST("Minimum(2.0, 5.0) = 2.0", fabsf(out - 2.0f) < EPSILON);

  // Phase 3: 常量乘法测试
  printf("\n--- Phase 3: 常量乘法 ---\n");

  // Mul2
  in = 3.0f;
  out = 0.0f;
  tvmgen_default_mul_2(&in, &out, NULL, NULL);
  TEST("Mul2(3.0) = 6.0", fabsf(out - 6.0f) < EPSILON);

  // MulHalf
  in = 4.0f;
  out = 0.0f;
  tvmgen_default_mul_half(&in, &out, NULL, NULL);
  TEST("MulHalf(4.0) = 2.0", fabsf(out - 2.0f) < EPSILON);

  // 汇总
  printf("\n========================================\n");
  printf("  测试结果: %d 通过, %d 失败\n", passed, failed);
  printf("========================================\n");

  return failed > 0 ? 1 : 0;
}
