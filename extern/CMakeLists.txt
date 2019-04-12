
add_library(picotls
    ${CMAKE_CURRENT_SOURCE_DIR}/quicly/deps/picotls/lib/openssl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/quicly/deps/picotls/lib/pembase64.c
    ${CMAKE_CURRENT_SOURCE_DIR}/quicly/deps/picotls/lib/picotls.c)
target_include_directories(picotls PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/quicly/deps/picotls/include)
FIND_PACKAGE(OpenSSL REQUIRED)
target_link_libraries(picotls PRIVATE OpenSSL::SSL)


add_subdirectory(quicly EXCLUDE_FROM_ALL)
target_include_directories(quicly PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/quicly/include)
