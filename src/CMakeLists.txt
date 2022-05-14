include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR})

find_package(BISON REQUIRED)
find_package(FLEX REQUIRED)

bison_target(yacc parser/yacc.y ${CMAKE_CURRENT_BINARY_DIR}/yacc.tab.cpp
        DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/yacc.tab.h)
flex_target(lex parser/lex.l ${CMAKE_CURRENT_BINARY_DIR}/lex.yy.cpp)
add_flex_bison_dependency(lex yacc)

add_library(redbase-cpp STATIC
        pf/pf_manager.cpp pf/pf_pager.cpp
        rm/rm_manager.cpp rm/rm_scan.cpp rm/rm_file_handle.cpp
        ix/ix_manager.cpp ix/ix_index_handle.cpp ix/ix_scan.cpp
        sm/sm_manager.cpp
        ql/ql_manager.cpp ql/ql_node.cpp
        parser/ast.cpp ${BISON_yacc_OUTPUT_SOURCE} ${FLEX_lex_OUTPUTS})

add_executable(rawcli rawcli.cpp)
target_link_libraries(rawcli redbase-cpp)

add_executable(redbase redbase.cpp)
target_link_libraries(redbase redbase-cpp readline)

if (REDBASE_ENABLE_TEST)
    file(GLOB_RECURSE REDBASE_TEST_FILES *_test.cpp)
    foreach (REDBASE_TEST_FILE ${REDBASE_TEST_FILES})
        get_filename_component(REDBASE_TEST_NAME ${REDBASE_TEST_FILE} NAME_WE)
        add_executable(${REDBASE_TEST_NAME} ${REDBASE_TEST_FILE})
        target_link_libraries(${REDBASE_TEST_NAME} redbase-cpp gtest_main)
        gtest_discover_tests(${REDBASE_TEST_NAME})
    endforeach ()
endif ()
