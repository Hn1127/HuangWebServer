project(timer)

add_library(timer
SHARED
lst_timer.cpp
)

target_include_directories(timer
    PUBLIC
    ${CMAKE_SOURCE_DIR}/src/log
)

target_link_libraries(timer
    PUBLIC
    log
)