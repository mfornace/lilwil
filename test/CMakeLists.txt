
# Either SHARED or OBJECT is fine
add_library(lilwil_test_source OBJECT EXCLUDE_FROM_ALL ${CMAKE_CURRENT_SOURCE_DIR}/Test.cc)

target_link_libraries(lilwil_test_source PRIVATE liblilwil)

# target name, output name, and any libraries that should be linked against
lilwil_module(lilwil_test lilwil_test lilwil_test_source)
