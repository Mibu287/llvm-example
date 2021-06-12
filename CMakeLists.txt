cmake_minimum_required(VERSION 3.15)

project("GenerateIR" LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

# Sanitizers
include(CheckSourceCompiles)

set(asan_flag "-fsanitize=address")

set(source_code "int main(){}")

set(CMAKE_REQUIRED_FLAGS ${asan_flag})
set(CMAKE_REQUIRED_LINK_OPTIONS ${asan_flag})
check_source_compiles("CXX" ${source_code} has_asan)

if(${has_asan})
    add_compile_options($<$<CONFIG:debug>:${asan_flag}>)
    add_link_options($<$<CONFIG:debug>:${asan_flag}>)
endif()

# Dependencies
include(FetchContent)

FetchContent_Declare(
    llvm
    URL "https://github.com/llvm/llvm-project/archive/refs/tags/llvmorg-12.0.0.tar.gz"
    URL_HASH
        "SHA256=8e6c99e482bb16a450165176c2d881804976a2d770e0445af4375e78a1fbf19c"
)

FetchContent_Populate(llvm)

add_subdirectory("${llvm_SOURCE_DIR}/llvm" EXCLUDE_FROM_ALL)

get_directory_property(
    LLVM_MAIN_INCLUDE_DIR DIRECTORY "${llvm_SOURCE_DIR}/llvm" DEFINITION
                                    LLVM_MAIN_INCLUDE_DIR)

get_directory_property(LLVM_INCLUDE_DIR DIRECTORY "${llvm_SOURCE_DIR}/llvm"
                                                  DEFINITION LLVM_INCLUDE_DIR)

get_directory_property(
    LLVM_TARGETS_TO_BUILD DIRECTORY "${llvm_SOURCE_DIR}/llvm" DEFINITION
                                    LLVM_TARGETS_TO_BUILD)

llvm_map_components_to_libnames(llvm_libs core support target orcjit
                                ${LLVM_TARGETS_TO_BUILD})

# Add compiler warning options
if(${CMAKE_CXX_COMPILER_ID} MATCHES "Clang|GCC")
    add_compile_options("-Wall;-Wextra;-Wpedantic")
elseif(${CMAKE_CXX_COMPILER_ID} EQUAL MSVC)
    add_compile_options("/W4")
endif()

# Add all targets
file(
    GLOB all_source_files
    LIST_DIRECTORIES FALSE
    "${CMAKE_CURRENT_LIST_DIR}/*.cpp")

foreach(source_file IN LISTS all_source_files)
    get_filename_component(executable_file ${source_file} NAME_WLE)

    add_executable(${executable_file} ${source_file})

    foreach(llvm_lib IN LISTS llvm_libs)
        target_link_libraries(${executable_file} PRIVATE ${llvm_lib})
    endforeach()

    target_include_directories(${executable_file}
                               PRIVATE "${CMAKE_CURRENT_LIST_DIR}/include")

    target_include_directories(
        ${executable_file} SYSTEM
        PRIVATE ${LLVM_MAIN_INCLUDE_DIR}
        PRIVATE ${LLVM_INCLUDE_DIR})

endforeach()
