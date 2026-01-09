/**
 * @file tvmrt_engine.h
 * @brief Scheduling Engine Interface
 * 
 * Implements BSP (Bulk Synchronous Parallel) execution model:
 * - Layer-by-layer execution with barriers between layers
 * - Parallel execution within each layer
 * 
 * Aligns with PDF Section 3.2: "核心引擎层"
 */

#ifndef TVMRT_ENGINE_H_
#define TVMRT_ENGINE_H_

#include "tvmrt_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// Engine Configuration
// ============================================================

/** Number of worker threads (0 = single-threaded mode) */
#ifndef TVMRT_NUM_WORKERS
#define TVMRT_NUM_WORKERS 4
#endif

// ============================================================
// Engine API
// ============================================================

/**
 * @brief Initialize the execution engine
 * 
 * Creates thread pool and synchronization primitives.
 * Should be called once at startup.
 * 
 * @return 0 on success
 */
int tvmrt_engine_init(void);

/**
 * @brief Shutdown the execution engine
 * 
 * Destroys thread pool and releases resources.
 */
void tvmrt_engine_shutdown(void);

/**
 * @brief Execute model according to static schedule
 * 
 * @param ctx Runtime context with filled operators
 * @param schedule Static schedule descriptor
 * @return 0 on success, negative on error
 */
int tvmrt_engine_run(
    tvmrt_context_t* ctx,
    const tvmrt_schedule_desc_t* schedule
);

/**
 * @brief Execute model in single-threaded mode (no thread pool)
 * 
 * Useful for debugging or environments without thread support.
 * 
 * @param ctx Runtime context with filled operators
 * @param schedule Static schedule descriptor
 * @return 0 on success, negative on error
 */
int tvmrt_engine_run_single(
    tvmrt_context_t* ctx,
    const tvmrt_schedule_desc_t* schedule
);

#ifdef __cplusplus
}
#endif

#endif  // TVMRT_ENGINE_H_
