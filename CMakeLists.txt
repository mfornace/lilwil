cmake_minimum_required(VERSION 3.8)
project(lilwil VERSION 0.0.0 LANGUAGES CXX)

################################################################################

set(LILWIL_PYTHON "python" CACHE STRING "Python executable to use in deducing include directory")
# set(LILWIL_PYTHON_INCLUDE "" CACHE STRING "Include directory containing Python.h")
set(LILWIL_MIN_PYTHON 2.7 CACHE STRING "Minimum python version to look for (if LILWIL_PYTHON and LILWIL_PYTHON_INCLUDE not defined)")

################################################################################

if (${LILWIL_PYTHON_INCLUDE})
    message("-- Using pre-specified Python include ${LILWIL_PYTHON_INCLUDE}")
elseif (LILWIL_PYTHON)
    execute_process(
        COMMAND ${LILWIL_PYTHON} -c "import sys, sysconfig; sys.stdout.write(sysconfig.get_paths()['include'])"
        RESULT_VARIABLE python_stat OUTPUT_VARIABLE python_include
    )
    if (python_stat)
        message(FATAL_ERROR "Failed to deduce include directory from '${LILWIL_PYTHON}' executable.\nMaybe specify LILWIL_PYTHON_INCLUDE directly.")
    endif()
    message("-- Using Python include ${python_include} deduced from ${LILWIL_PYTHON}")
    set (LILWIL_PYTHON_INCLUDE ${python_include} CACHE STRING "Python include directory for lilwil")
else()
    find_package(PythonLibs ${LILWIL_MIN_PYTHON} REQUIRED)
    set (LILWIL_PYTHON_INCLUDE ${PYTHON_INCLUDE_DIR} CACHE STRING "Python include directory for lilwil")
endif()

################################################################################

add_library(lilwil_interface INTERFACE)
target_compile_features(lilwil_interface INTERFACE cxx_std_17)
target_include_directories(lilwil_interface INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
message("whaahttt ${CMAKE_CURRENT_SOURCE_DIR}/include")

################################################################################

# Maybe change in future to user provided interface library?
add_library(liblilwil STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/source/lilwil/Source.cc
)
set_target_properties(liblilwil PROPERTIES OUTPUT_NAME lilwil ${LILWIL_STATIC_PROPERTIES})
target_link_libraries(liblilwil PUBLIC lilwil_interface)

################################################################################

# Module.cc has to be recompiled based on the exported module name
# Could just build Python.cc as its own library, but here it's built together with Module.cc
set_property(GLOBAL PROPERTY lilwil_module_files
    ${CMAKE_CURRENT_SOURCE_DIR}/source/lilwil/Python.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/source/lilwil/Module.cc
)

function(lilwil_module module_name)
    get_property(files GLOBAL PROPERTY lilwil_module_files)
    add_library(${module_name} MODULE ${files})
    set_target_properties(${module_name} PROPERTIES PREFIX "")

    if(${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
        set_target_properties(${module_name} PROPERTIES LINK_FLAGS "-undefined,dynamic_lookup")
    else()
        set_target_properties(${module_name} PROPERTIES LINK_FLAGS "-undefined dynamic_lookup")
    endif()

    target_compile_definitions(${module_name} PRIVATE LILWIL_MODULE=${module_name})
    message("hmm ${module_name} ")
    target_link_libraries(${module_name} PRIVATE lilwil_interface ${ARGN})
    target_include_directories(${module_name} PRIVATE ${LILWIL_PYTHON_INCLUDE})
endfunction(lilwil_module)

################################################################################

add_library(testlilwil SHARED EXCLUDE_FROM_ALL source/Test.cc)
set_property(TARGET testlilwil PROPERTY OUTPUT_NAME test)
target_link_libraries(testlilwil PRIVATE liblilwil)
lilwil_module(mymodule testlilwil)
