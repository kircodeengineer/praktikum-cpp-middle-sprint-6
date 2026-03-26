set(target thread_pool_test)

add_executable(${target}
    thread_pool.cpp
)

target_link_libraries(${target}
    PRIVATE
        GTest::GTest
        GTest::Main
        thread_pool
        queue
)

add_test(NAME ${target} COMMAND ${target})