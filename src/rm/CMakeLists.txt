set(SOURCES rm_file_handle.cpp rm_scan.cpp)
add_library(rm STATIC ${SOURCES})
target_link_libraries(rm pf)

add_executable(test_bitmap test_bitmap.cpp)
add_test(NAME test_bitmap COMMAND test_bitmap
        WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

add_executable(test_rm test_rm.cpp)
target_link_libraries(test_rm rm)
add_test(NAME test_rm COMMAND test_rm
        WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
