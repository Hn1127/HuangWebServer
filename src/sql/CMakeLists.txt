project(sqlPool)

add_library(sqlPool
SHARED
sqlConnectionPool.cpp
)

target_include_directories(sqlPool 
    PUBLIC
    ${CMAKE_SOURCE_DIR}/src/locker
)

target_include_directories(sqlPool 
    PUBLIC
    ${CMAKE_SOURCE_DIR}/src/log
)

target_link_directories(sqlPool
    PUBLIC
    log
)

target_link_libraries(sqlPool
    PUBLIC
    libmysqlclient.so
)