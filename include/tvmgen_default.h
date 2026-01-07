// tvmgen_default.h
#ifndef TVMGEN_DEFAULT_H
#define TVMGEN_DEFAULT_H

#include <stdint.h>

#ifndef TVM_DLL
#define TVM_DLL
#endif

// 定义 lib0.c 需要的输入结构体
struct tvmgen_default_inputs {
    void* input;  // 名字必须叫 input，因为 lib0 代码里写了 inputs->input
};

// 定义 lib0.c 需要的输出结构体
struct tvmgen_default_outputs {
    void* output; // 名字必须叫 output
};

// 声明 lib1.c 里的核心函数 (防止编译警告)
int32_t tvmgen_default___tvm_main__(void* input, void* output, uint8_t* const_ws, uint8_t* ws);

#endif