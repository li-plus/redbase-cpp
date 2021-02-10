set(SOURCES sm_manager.cpp)
add_library(sm STATIC ${SOURCES})
target_link_libraries(sm ix rm)

add_executable(test_sm test_sm.cpp)
target_link_libraries(test_sm sm)
add_test(NAME test_sm COMMAND test_sm
        WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
