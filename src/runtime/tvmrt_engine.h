/**
 * @file tvmrt_engine.h
 * @brief 调度引擎接口
 * 
 * 实现 BSP (Bulk Synchronous Parallel) 执行模型：
 * - 逐层执行，层间有屏障同步
 * - 层内并行执行
 * 
 * 对应 PDF 3.2 节: "核心引擎层"
 */

#ifndef TVMRT_ENGINE_H_
#define TVMRT_ENGINE_H_

#include "tvmrt_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// 引擎配置
// ============================================================

/** Worker 线程数 (0 = 单线程模式) */
#ifndef TVMRT_NUM_WORKERS
#define TVMRT_NUM_WORKERS 4
#endif

// ============================================================
// 引擎 API
// ============================================================

/**
 * @brief 初始化执行引擎
 * 
 * 创建线程池和同步原语。
 * 应在启动时调用一次。
 * 
 * @return 成功返回 0
 */
int tvmrt_engine_init(void);

/**
 * @brief 关闭执行引擎
 * 
 * 销毁线程池并释放资源。
 */
void tvmrt_engine_shutdown(void);

/**
 * @brief 按静态调度表执行模型
 * 
 * @param ctx 已填充算子的运行时上下文
 * @param schedule 静态调度描述符
 * @return 成功返回 0，错误返回负数
 */
int tvmrt_engine_run(
    tvmrt_context_t* ctx,
    const tvmrt_schedule_desc_t* schedule
);

/**
 * @brief 单线程模式执行模型 (不使用线程池)
 * 
 * 适用于调试或不支持线程的环境。
 * 
 * @param ctx 已填充算子的运行时上下文
 * @param schedule 静态调度描述符
 * @return 成功返回 0，错误返回负数
 */
int tvmrt_engine_run_single(
    tvmrt_context_t* ctx,
    const tvmrt_schedule_desc_t* schedule
);

#ifdef __cplusplus
}
#endif

#endif  // TVMRT_ENGINE_H_
