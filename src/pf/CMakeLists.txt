set(SOURCES pf_manager.cpp)
add_library(pf STATIC ${SOURCES})

add_executable(test_pf test_pf.cpp)
target_link_libraries(test_pf pf)
add_test(NAME test_pf COMMAND test_pf
        WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
