project(webServer)

add_library(webServer
SHARED
webServer.cpp http.cpp
)

add_subdirectory(timer)

target_include_directories(webServer 
    PUBLIC
    ${CMAKE_SOURCE_DIR}/src/threadpool
)

target_include_directories(webServer
    PUBLIC
    ${CMAKE_SOURCE_DIR}/src/webServer/timer
)

target_include_directories(webServer
    PUBLIC
    ${CMAKE_SOURCE_DIR}/src/log
)

target_include_directories(webServer
    PUBLIC
    ${CMAKE_SOURCE_DIR}/src/sql
)

target_link_libraries(webServer
    PUBLIC
    log
)

target_link_libraries(webServer
    PUBLIC
    timer
)

target_link_libraries(webServer
    PUBLIC
    sqlPool
)