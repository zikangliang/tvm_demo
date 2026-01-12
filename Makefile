# ==========================================
# TVM Runtime Simplified Makefile
# 保留 lib0/lib1，Runtime 工具单文件化
# Platform: Mac
# ==========================================

# 编译器设置
CC = clang

# 编译选项
CFLAGS = -Isrc -Iinclude -Wno-everything -g -O2

# 目标文件名
TARGET = runner

# ==========================================
# 简化版本 (8 文件)
# ==========================================

SRCS = src/main.c \
       src/default_lib0.c \
       src/default_lib1.c \
       src/tvmrt.c \
       src/tvmrt_port_posix.c \
       src/model_data.c \
       src/ops.c

OBJS = $(SRCS:.c=.o)

# 默认目标
all: $(TARGET)
	@echo "Built simplified version (8 files)"

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) -lpthread
	@echo "Build successful! Executable: ./$(TARGET)"

# ==========================================
# 模块化参考版本 (18 文件，保留旧结构)
# ==========================================

MODULAR_ENTRY = src/main_test.c src/default_lib0.c src/default_lib1.c
MODULAR_RUNTIME = src/runtime/tvmrt_port_posix.c \
                  src/runtime/tvmrt_log.c \
                  src/runtime/tvmrt_semantic.c \
                  src/runtime/tvmrt_engine.c
MODULAR_MODEL = src/model/model_desc.c
MODULAR_OPS = src/ops/default_ops.c

MODULAR_SRCS = $(MODULAR_ENTRY) $(MODULAR_RUNTIME) $(MODULAR_MODEL) $(MODULAR_OPS)
MODULAR_OBJS = $(patsubst %.c,%.o,$(MODULAR_SRCS))

modular: $(MODULAR_OBJS)
	$(CC) $(CFLAGS) -o $(TARGET)_modular $(MODULAR_OBJS) -lpthread
	@echo "Built modular reference version (18 files)"
	@echo "Executable: ./$(TARGET)_modular"

run_modular: modular
	@echo "Running $(TARGET)_modular..."
	@echo "--------------------------------"
	@./$(TARGET)_modular

# ==========================================
# 通用规则
# ==========================================

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(MODULAR_OBJS) $(TARGET) $(TARGET)_modular
	@echo "Cleaned up."

run: $(TARGET)
	@echo "Running $(TARGET)..."
	@echo "--------------------------------"
	@./$(TARGET)

# ==========================================
# 帮助信息
# ==========================================
help:
	@echo "TVM Runtime Build Targets:"
	@echo "  make all         - Build simplified version (8 files, default)"
	@echo "  make run         - Build and run simplified version"
	@echo "  make modular     - Build modular reference (18 files)"
	@echo "  make run_modular - Build and run modular reference"
	@echo "  make clean       - Remove all build artifacts"
	@echo "  make help        - Show this message"

.PHONY: all modular clean run run_modular help