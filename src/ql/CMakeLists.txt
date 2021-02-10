set(SOURCES ql_manager.cpp)
add_library(ql STATIC ${SOURCES})
target_link_libraries(ql sm)

add_executable(test_ql test_ql.cpp)
target_link_libraries(test_ql ql parser)
add_test(NAME test_ql COMMAND test_ql
        WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
