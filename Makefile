# ==========================================
# TVM Runtime Simplified Makefile
# 8 文件精简架构
# Platform: Mac
# ==========================================

# 编译器设置
CC = clang

# 编译选项
CFLAGS = -Isrc -Iinclude -Wno-everything -g -O2

# 目标文件名
TARGET = runner
STRESS_TARGET = stress_runner

# ==========================================
# 源文件列表 (原版 8 文件)
# ==========================================

SRCS = src/main.c \
       src/default_lib0.c \
       src/default_lib1.c \
       src/tvmrt.c \
       src/tvmrt_port_posix.c \
       src/model_data.c \
       src/ops.c

OBJS = $(SRCS:.c=.o)

# ==========================================
# 压力测试源文件
# ==========================================

STRESS_SRCS = src/stress_main.c \
              src/stress_lib0.c \
              src/stress_lib1.c \
              src/stress_model_data.c \
              src/tvmrt.c \
              src/tvmrt_port_posix.c \
              src/ops.c

STRESS_OBJS = $(STRESS_SRCS:.c=.o)

# ==========================================
# 构建规则
# ==========================================

all: $(TARGET)
	@echo "Built successfully (8 files)"

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) -lpthread
	@echo "Executable: ./$(TARGET)"

# 压力测试构建
stress_test: $(STRESS_TARGET)
	@echo "Built stress test successfully"

$(STRESS_TARGET): $(STRESS_OBJS)
	$(CC) $(CFLAGS) -o $@ $(STRESS_OBJS) -lpthread
	@echo "Executable: ./$(STRESS_TARGET)"

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f src/*.o $(TARGET) $(STRESS_TARGET)
	@echo "Cleaned up."

run: $(TARGET)
	@echo "Running $(TARGET)..."
	@echo "--------------------------------"
	@./$(TARGET)

stress_run: $(STRESS_TARGET)
	@echo "Running $(STRESS_TARGET)..."
	@echo "--------------------------------"
	@./$(STRESS_TARGET)

# ==========================================
# 帮助信息
# ==========================================
help:
	@echo "TVM Runtime Build Targets:"
	@echo "  make all        - Build simplified version (8 files)"
	@echo "  make run        - Build and run original model"
	@echo "  make stress_test- Build stress test (16 ops, 8 layers)"
	@echo "  make stress_run - Build and run stress test"
	@echo "  make clean      - Remove build artifacts"
	@echo "  make help       - Show this message"

.PHONY: all clean run help stress_test stress_run