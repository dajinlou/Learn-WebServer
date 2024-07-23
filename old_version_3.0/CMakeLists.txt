# 指定最低版本号
cmake_minimum_required(VERSION 3.0)

#项目名称
project(WebServer)

# 设置C++
set(CMAKE_CXX_STANDARD 11)

add_definitions(-DDEBUG)

# 禁用转换警告
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-write-strings")  

# 搜索源文件
aux_source_directory(${PROJECT_SOURCE_DIR} SRC)

# 设置输出路径
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

# 生成可执行文件
add_executable(server ${SRC})

# 生成调试文件
target_compile_options(server PRIVATE -g)

target_link_libraries(server pthread opencv_core opencv_imgproc opencv_highgui)