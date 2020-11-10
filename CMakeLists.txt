cmake_minimum_required(VERSION 3.8)
project(lilwil VERSION 0.0.0 LANGUAGES CXX)

################################################################################

set(LILWIL_PYTHON "python" CACHE STRING "Specified Python executable used to deduce include directory")
set(LILWIL_PYTHON_INCLUDE "" CACHE STRING "Specified include directory containing Python.h")

################################################################################

if (${LILWIL_PYTHON_INCLUDE})
    message("-- Using specified Python include")
    set_property(GLOBAL PROPERTY lilwil_python_include ${LILWIL_PYTHON_INCLUDE})
else()
    execute_process(
        COMMAND ${LILWIL_PYTHON} -c "import sys, sysconfig; sys.stdout.write(sysconfig.get_paths()['include'])"
        RESULT_VARIABLE python_stat OUTPUT_VARIABLE python_include
    )
    if (python_stat)
        message(FATAL_ERROR "Failed to deduce include directory from '${LILWIL_PYTHON}' executable.\nMaybe specify LILWIL_PYTHON_INCLUDE directly.")
    endif()
    message("-- Using Python include directory deduced from LILWIL_PYTHON=${LILWIL_PYTHON}")
    set_property(GLOBAL PROPERTY lilwil_python_include ${python_include})
endif()

message("-- Using Python include directory ${python_include}")

################################################################################

add_library(lilwil_interface INTERFACE)
target_compile_features(lilwil_interface INTERFACE cxx_std_17)
target_include_directories(lilwil_interface INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

################################################################################

# Maybe change in future to user provided interface library?
add_library(liblilwil STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/source/Source.cc
)
set_target_properties(liblilwil PROPERTIES OUTPUT_NAME lilwil ${LILWIL_STATIC_PROPERTIES})
target_link_libraries(liblilwil PUBLIC lilwil_interface)

################################################################################

# Module.cc has to be recompiled based on the exported module name
# Could just build Python.cc as its own library, but here it's built together with Module.cc
set_property(GLOBAL PROPERTY lilwil_module_files
    ${CMAKE_CURRENT_SOURCE_DIR}/source/Python.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/source/Module.cc
)

set_property(GLOBAL PROPERTY lilwil_source_dir ${CMAKE_CURRENT_SOURCE_DIR})
set_property(GLOBAL PROPERTY lilwil_binary_dir ${CMAKE_CURRENT_BINARY_DIR})

function(lilwil_module target_name output_name)
    get_property(files GLOBAL PROPERTY lilwil_module_files)
    get_property(python_include GLOBAL PROPERTY lilwil_python_include)
    get_property(source_dir GLOBAL PROPERTY lilwil_source_dir)
    get_property(binary_dir GLOBAL PROPERTY lilwil_binary_dir)

    add_library(${target_name} MODULE ${files})
    set_target_properties(${target_name} PROPERTIES PREFIX "lib"
        LIBRARY_OUTPUT_DIRECTORY ${binary_dir} OUTPUT_NAME ${output_name}_lilwil)

    if(${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
        set_target_properties(${target_name} PROPERTIES LINK_FLAGS "-Wl,-flat_namespace,-undefined,dynamic_lookup")
    else()
        set_target_properties(${target_name} PROPERTIES LINK_FLAGS "-undefined dynamic_lookup")
    endif()

    target_compile_definitions(${target_name} PRIVATE LILWIL_MODULE=lib${output_name}_lilwil)
    target_link_libraries(${target_name} PRIVATE lilwil_interface ${ARGN})
    target_include_directories(${target_name} PRIVATE ${python_include})

    # following not quite right in all cases...
    set(LILWIL_DEFAULT_LIBRARY_NAME lib${output_name}_lilwil)
    set(LILWIL_DEFAULT_PYTHON_PATH ${source_dir})
    set(LILWIL_DEFAULT_LIBRARY_PATH ${binary_dir})
    configure_file(${source_dir}/source/test.py ${CMAKE_CURRENT_BINARY_DIR}/${output_name}.py)
endfunction(lilwil_module)

################################################################################

set(LILWIL_TEST OFF CACHE BOOL "Build unit tests of lilwil")
if(LILWIL_TEST)
    add_subdirectory(test)
endif()

