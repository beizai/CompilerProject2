file(GLOB test_src "*.cc")
file(GLOB kernel_src "./kernels/*.cc")
file(GLOB solution1_src "./solution/my_solution1.cc")
file(GLOB solution2_src "./solution/my_solution2.cc")
file(GLOB clean_src "./clean/*.cc")

include_directories("../include")

add_executable(test2 ${test_src} ${kernel_src})
add_executable(solution2_1 ${solution1_src})
add_executable(solution2_2 ${solution2_src})
target_link_libraries(solution2_1 ${LIB_NAME})
target_link_libraries(solution2_2 ${LIB_NAME})
add_executable(cleanf2 ${clean_src})

add_custom_target(solution_run2_1
  COMMAND solution2_1
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMENT "run generated solution in ${CMAKE_CURRENT_SOURCE_DIR}"
  SOURCES ${solution1_src}
)
add_custom_target(solution_run2_2
  COMMAND solution2_2
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMENT "run generated solution in ${CMAKE_CURRENT_SOURCE_DIR}"
  SOURCES ${solution2_src}
)

add_custom_target(clean_run2
  COMMAND cleanf2
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMENT "run generated clean in ${CMAKE_CURRENT_SOURCE_DIR}"
  SOURCES ${clean_src}
)
add_dependencies(solution_run2_1 clean_run2)
add_dependencies(solution_run2_2 solution_run2_1)
add_dependencies(test2 solution_run2_2)
