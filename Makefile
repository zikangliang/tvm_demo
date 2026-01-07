# ==========================================
# TVM Runtime Simple Makefile
# Platform: Mac
# ==========================================

# 编译器设置 (Mac 下 gcc 通常也是 clang 的别名，直接用 clang 更明确)
CC = clang

# 编译选项
# -Iinclude       : 在 include 目录寻找头文件 (tvmgen_default.h)
# -Wno-everything : 忽略所有警告 (因为这是AOT生成的代码，且我们手动修改过，警告会很多)
# -g              : 生成调试信息 (方便你以后用 lldb 调试)
# -O2             : 开启二级优化
CFLAGS = -Iinclude -Wno-everything -g -O2

# 目标文件名
TARGET = runner

# 源文件列表
SRCS = src/main.c src/default_lib0.c src/default_lib1.c

# 根据源文件列表自动生成 .o 文件列表
OBJS = $(SRCS:.c=.o)

# ==========================================
# 规则定义
# ==========================================

# 默认目标：只编译链接
all: $(TARGET)

# 链接步骤：把所有的 .o 文件合并成可执行文件
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) -lpthread
	@echo "Build successful! Executable created: ./$(TARGET)"

# 编译步骤：把每个 .c 文件编译成 .o 文件
# $< 代表源文件， $@ 代表目标文件
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# 清理规则：删除生成的中间文件和可执行文件
clean:
	rm -f $(OBJS) $(TARGET)
	@echo "Cleaned up."

# 快捷运行规则：编译并直接运行
run: $(TARGET)
	@echo "Running $(TARGET)..."
	@echo "--------------------------------"
	@./$(TARGET)