cmake_minimum_required(VERSION 3.6.0)
project(qperf VERSION 1.0.0 LANGUAGES C)

add_subdirectory(extern)

add_executable(qperf main.c
    client.h client.c
    client_stream.h client_stream.c
    server.h server.c
    server_stream.h server_stream.c
    common.h common.c)

target_link_libraries(qperf PRIVATE quicly ev picotls)
target_compile_definitions(qperf PRIVATE QPERF_VERSION="${PROJECT_VERSION}")
target_compile_options(qperf PRIVATE
    -Werror=implicit-function-declaration
    -Werror=incompatible-pointer-types
    -Werror=shift-count-overflow
)

