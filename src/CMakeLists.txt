include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(pf)
add_subdirectory(rm)
add_subdirectory(ix)
add_subdirectory(sm)
add_subdirectory(ql)
add_subdirectory(parser)

add_executable(rawcli rawcli.cpp)
target_link_libraries(rawcli parser ql)

add_executable(redbase redbase.cpp)
target_link_libraries(redbase parser ql readline)
