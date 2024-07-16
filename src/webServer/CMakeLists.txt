project(webServer)

add_library(webServer
SHARED
webServer.cpp http.cpp
)

target_include_directories(webServer 
    PUBLIC
    ${CMAKE_SOURCE_DIR}/src/threadpool
)

target_include_directories(webServer 
    PUBLIC
    ${CMAKE_SOURCE_DIR}/src/Utils
)

target_link_libraries(webServer 
    PUBLIC
    Utils
)