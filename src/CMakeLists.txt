project(main)

add_subdirectory(echoServer)

add_subdirectory(log)

add_subdirectory(webServer)

add_subdirectory(sql)

add_executable(server main.cpp)

# 包含echoServer头文件
target_include_directories(server
PUBLIC
echoServer
)

# 链接echoServer库
target_link_libraries(server
PUBLIC
echoServer
)

# 包含webServer头文件
target_include_directories(server
PUBLIC
webServer
)

# 链接webServer头文件
target_link_libraries(server
PUBLIC
webServer
)

