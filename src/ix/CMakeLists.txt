set(SOURCES ix_index_handle.cpp ix_scan.cpp)
add_library(ix STATIC ${SOURCES})
target_link_libraries(ix pf)

add_executable(test_ix test_ix.cpp)
target_link_libraries(test_ix ix)
add_test(NAME test_ix COMMAND test_ix
        WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
