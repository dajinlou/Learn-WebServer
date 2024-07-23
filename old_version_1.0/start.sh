#!/bin/bash
cd ./build

# 清除之前的文件
rm * -rf

# 生成Makefile
cmake ..

# 执行make
make

if [ "$*" = "" ];then
    echo "empty"
else
    cd .. &&
    ./bin/server
fi