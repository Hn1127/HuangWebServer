project(echoServer)

add_library(echoServer
    SHARED 
    echoServer.cpp echo.cpp
)

target_include_directories(echoServer 
    PUBLIC
    ${CMAKE_SOURCE_DIR}/src/threadpool
)

target_include_directories(echoServer
    PUBLIC
    ${CMAKE_SOURCE_DIR}/src/log
)

target_link_libraries(echoServer
    PUBLIC
    log
)

