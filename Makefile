# ==========================================
# TVM Runtime Modular Makefile (Step 7)
# Platform: Mac
# ==========================================

# 编译器设置
CC = clang

# 编译选项
CFLAGS = -Isrc -Iinclude -Wno-everything -g -O2

# 目标文件名
TARGET = runner

# ==========================================
# 源文件列表 (模块化架构)
# ==========================================

# 入口文件
ENTRY_SRCS = src/main.c src/default_lib0.c src/default_lib1.c

# Runtime 模块
RUNTIME_SRCS = src/runtime/tvmrt_port_posix.c \
               src/runtime/tvmrt_log.c \
               src/runtime/tvmrt_semantic.c \
               src/runtime/tvmrt_engine.c

# 模型描述 (编译器生成)
MODEL_SRCS = src/model/model_desc.c

# 算子实现
OPS_SRCS = src/ops/default_ops.c

# 所有源文件
SRCS = $(ENTRY_SRCS) $(RUNTIME_SRCS) $(MODEL_SRCS) $(OPS_SRCS)
OBJS = $(SRCS:.c=.o)

# ==========================================
# 规则定义
# ==========================================

# 默认目标：模块化版本
all: $(TARGET)
	@echo "Built MODULAR version (Step 7)"

# 链接步骤
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) -lpthread
	@echo "Build successful! Executable: ./$(TARGET)"

# 编译步骤
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# 清理规则
clean:
	rm -f $(OBJS) $(TARGET) $(TARGET)_legacy
	@echo "Cleaned up."

# 快捷运行
run: $(TARGET)
	@echo "Running $(TARGET)..."
	@echo "--------------------------------"
	@./$(TARGET)

# ==========================================
# Legacy 版本构建 (Step 6 BSP)
# ==========================================
LEGACY_SRCS = src/main.c src/default_lib0.c src/default_lib1_legacy.c
LEGACY_OBJS = $(LEGACY_SRCS:.c=.o)

legacy: $(LEGACY_OBJS)
	$(CC) $(CFLAGS) -o $(TARGET)_legacy $(LEGACY_OBJS) -lpthread
	@echo "Built LEGACY version (Step 6 BSP)"
	@echo "Executable: ./$(TARGET)_legacy"

run_legacy: legacy
	@echo "Running $(TARGET)_legacy..."
	@echo "--------------------------------"
	@./$(TARGET)_legacy

# ==========================================
# 帮助信息
# ==========================================
help:
	@echo "TVM Runtime Build Targets:"
	@echo "  make all     - Build modular version (Step 7, default)"
	@echo "  make run     - Build and run modular version"
	@echo "  make legacy  - Build legacy version (Step 6 BSP)"
	@echo "  make run_legacy - Run legacy version"
	@echo "  make clean   - Remove all build artifacts"
	@echo "  make help    - Show this message"

.PHONY: all clean run legacy run_legacy help