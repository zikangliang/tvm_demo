#include <stdio.h>
#include "tvmgen_default.h" // 引用上面定义的结构体

// 声明 lib0.c 里的入口函数
int32_t tvmgen_default_run(struct tvmgen_default_inputs* inputs, struct tvmgen_default_outputs* outputs);

int main() {
    // 1. 准备数据
    float input_data[1] = { 10.0f };
    float output_data[1] = { 0.0f };

    // 2. 包装结构体
    struct tvmgen_default_inputs inputs;
    inputs.input = input_data;

    struct tvmgen_default_outputs outputs;
    outputs.output = output_data;

    printf("验证开始...\n");
    printf("输入值: %f\n", input_data[0]);

    // 3. 运行 lib0/lib1
    int32_t ret = tvmgen_default_run(&inputs, &outputs);

    if (ret == 0) {
        printf("执行成功！\n");
        // 预期结果是 23.0 (根据之前的逻辑推算)
        printf("输出值: %f (预期: 23.000000)\n", output_data[0]);
    } else {
        printf("执行出错，错误码: %d\n", ret);
    }

    return 0;
}