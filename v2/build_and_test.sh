#!/bin/bash

# 构建和运行内存池测试的脚本

# 设置颜色输出
green="\033[0;32m"
red="\033[0;31m"
reset="\033[0m"

# 获取脚本所在目录
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

# 创建构建目录
BUILD_DIR="$SCRIPT_DIR/build"
if [ ! -d "$BUILD_DIR" ]; then
    echo -e "${green}创建构建目录...${reset}"
    mkdir -p "$BUILD_DIR"
fi

# 进入构建目录
cd "$BUILD_DIR"

# 运行CMake
 echo -e "${green}配置项目...${reset}"
cmake $SCRIPT_DIR
if [ $? -ne 0 ]; then
    echo -e "${red}CMake配置失败!${reset}"
    exit 1
fi

# 编译项目
 echo -e "${green}编译项目...${reset}"
make -j$(nproc)
if [ $? -ne 0 ]; then
    echo -e "${red}编译失败!${reset}"
    exit 1
fi

# 运行测试
 echo -e "${green}\n运行测试...${reset}"
./memorypool_v2_test
if [ $? -ne 0 ]; then
    echo -e "${red}测试失败!${reset}"
    exit 1
else
    echo -e "${green}\n所有测试通过!${reset}"
fi

# 清理选项
read -p "是否清理构建文件? (y/n): " choice
if [ "$choice" = "y" ] || [ "$choice" = "Y" ]; then
    echo -e "${green}清理构建文件...${reset}"
    cd ..
    rm -rf "$BUILD_DIR"
    echo -e "${green}构建文件已清理。${reset}"
fi