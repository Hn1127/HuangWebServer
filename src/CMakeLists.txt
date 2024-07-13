project(main)

add_subdirectory(echoServer)

add_subdirectory(Utils)

add_executable(server main.cpp)

target_include_directories(server
PUBLIC
threadpool
)

target_include_directories(server
PUBLIC
echoServer
)

target_link_libraries(server
PUBLIC
echoServer
)

