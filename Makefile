# ==========================================
# TVM Runtime Makefile
# 16算子/9层/8内存槽 模型
# Platform: Mac
# ==========================================

# 编译器设置
CC = clang

# 编译选项
CFLAGS = -Isrc -Iinclude -Wno-everything -g -O2

# 日志开关 (设为 0 禁用日志，实现零运行时开销)
LOG_ENABLE ?= 1
CFLAGS += -DTVMRT_LOG_ENABLE=$(LOG_ENABLE)

# 目标文件名
TARGET = runner

# ==========================================
# 源文件列表
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
# 构建规则
# ==========================================

all: $(TARGET)
	@echo "Built successfully"

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) -lpthread
	@echo "Executable: ./$(TARGET)"

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	@echo "Running $(TARGET)..."
	@echo "--------------------------------"
	@./$(TARGET)

# ==========================================
# 单元测试
# ==========================================
TEST_SRCS = src/test_new_ops.c src/ops.c src/tvmrt.c src/tvmrt_port_posix.c
TEST_TARGET = test_new_ops

test: $(TEST_TARGET)
	@echo "Running unit tests..."
	@./$(TEST_TARGET)

$(TEST_TARGET): $(TEST_SRCS)
	@echo "Building unit tests..."
	$(CC) -Isrc -Iinclude -Wno-everything -g -O2 -DTVMRT_LOG_ENABLE=0 \
		$(TEST_SRCS) -o $(TEST_TARGET) -lm -lpthread

clean: clean-test
	rm -f src/*.o $(TARGET)
	@echo "Cleaned up."

clean-test:
	rm -f $(TEST_TARGET)

# ==========================================
# 帮助信息
# ==========================================
help:
	@echo "TVM Runtime Build Targets:"
	@echo "  make all   - Build the model"
	@echo "  make run   - Build and run"
	@echo "  make test  - Build and run unit tests"
	@echo "  make clean - Remove build artifacts"
	@echo "  make help  - Show this message"

.PHONY: all clean clean-test run help test