cmake_minimum_required(VERSION 3.6.0)
project(qperf VERSION 1.0.0 LANGUAGES C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Set linker flags to use lld
if(DEFINED SANITIZE)
    set(CMAKE_EXE_LINKER_FLAGS "-fuse-ld=lld")
    set(CMAKE_MODULE_LINKER_FLAGS "-fuse-ld=lld")
    set(CMAKE_SHARED_LINKER_FLAGS "-fuse-ld=lld")

    # Set the sanitize options
    set(SANITIZE_FLAGS
     -fsanitize=address
     -fsanitize=undefined
     -fsanitize=signed-integer-overflow
     -fsanitize=shift
     -fsanitize=integer-divide-by-zero
     -fsanitize=array-bounds
     -fsanitize=implicit-conversion
     -fsanitize=nullability
     -fsanitize=integer
     -fsanitize-address-use-after-return=always
     -fsanitize-address-use-after-scope
    )
    set(BETTER_STACKTRACE -g -O1 -fno-omit-frame-pointer -fno-optimize-sibling-calls)
endif()

add_subdirectory(extern)

add_executable(qperf main.c
    client.h client.c
    client_stream.h client_stream.c
    server.h server.c
    server_stream.h server_stream.c
    common.h common.c
    cpu-usage.h cpu-usage.c)

target_link_libraries(qperf PRIVATE quicly ev picotls ${SANITIZE_FLAGS} ${BETTER_STACKTRACE})
target_compile_definitions(qperf PRIVATE QPERF_VERSION="${PROJECT_VERSION}")
target_compile_options(qperf PRIVATE
    -Werror=implicit-function-declaration
    -Werror=incompatible-pointer-types
    -Werror=shift-count-overflow
    ${SANITIZE_FLAGS} ${BETTER_STACKTRACE}
)

