

add_library(lilwil_test_source SHARED EXCLUDE_FROM_ALL ${CMAKE_CURRENT_SOURCE_DIR}/Test.cc)

set_property(TARGET lilwil_test_source PROPERTY OUTPUT_NAME test)

target_link_libraries(lilwil_test_source PRIVATE liblilwil)

lilwil_module(lilwil_test lilwil_test lilwil_test_source)
