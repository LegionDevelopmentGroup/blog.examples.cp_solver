cmake_minimum_required(VERSION 3.16.9)
set (PROJECT_NAME cpsolver_example_1)

project (${PROJECT_NAME})

set(PROJECT_SOURCE_DIR .)

set(PROJECT_INCLUDE_BASE_DIR .)

if (NOT CMAKE_C_COMPILER)
  set(CMAKE_C_COMPILER "clang")
  set(CMAKE_CXX_COMPILER "clang++")
endif()

add_definitions(-DONLY_C_LOCALE=1)

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    # Support Unix Makefiles and Ninja
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
endif()

set(RegularSource
  ${PROJECT_SOURCE_DIR}/cpsolver_example_1.cpp
)

find_program(CLANGTIDY clang-tidy-15)
if(CLANGTIDY)
set(CMAKE_CXX_CLANG_TIDY ${CLANGTIDY})
else()
message(SEND_ERROR "clang-tidy requested but executable not found")
endif()

find_package(ortools CONFIG REQUIRED)

if(FALSE)
find_program(CPPCHECK cppcheck)
if(CPPCHECK)
set(CMAKE_CXX_CPPCHECK
    ${CPPCHECK}
    --suppress=missingIncludeSystem
    --suppress=unmatchedSuppression
    --enable=all
    --inconclusive
    --output-file=cppcheck.log
    --check-config)
else()
message(SEND_ERROR "cppcheck requested but executable not found")
endif()
endif()

set(CMAKE_CXX_COMPILER "clang++-15")
set(CMAKE_C_COMPILER "clang-15")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -DLLVM_ENABLE_RUNTIMES=libunwind")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}  -lc++abi")

set(RegularInclude
  ${PROJECT_SOURCE_DIR}/json.hpp)

ADD_EXECUTABLE(${PROJECT_NAME} ${RegularSource} ${RegularInclude})

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)

set(ProjectSanitizer "")
set(GENERAL_COMPILER_WARNINGS "-Wall -Wextra -Wshadow -Wnon-virtual-dtor -pedantic -Wold-style-cast -Wcast-align -Wunused -Woverloaded-virtual -Wconversion -Wsign-conversion -Wdouble-promotion -Wformat=2 -Weffc++")

set(GENERAL_COMPILER_FLAGS "-Wfatal-errors ${GENERAL_COMPILER_WARNINGS} -Ofast -ggdb -fno-omit-frame-pointer ${ProjectSanitizer}")

set(LINK_LIBRARIES ortools::ortools pthread)

# set_target_properties(${PROJECT_NAME} PROPERTIES COMPILE_FLAGS "${CMAKE_CXX_FLAGS} ${GENERAL_COMPILER_FLAGS}")
TARGET_LINK_LIBRARIES(${PROJECT_NAME} "${LINK_LIBRARIES}")

target_include_directories(${PROJECT_NAME} PUBLIC
  "/usr/local/include"
  "/usr/include"
)

target_link_directories(${PROJECT_NAME} PUBLIC
  "/usr/local/lib"
)

set_target_properties(${PROJECT_NAME} PROPERTIES COMPILE_FLAGS "${CXX_FLAGS_FORWARD} ${GENERAL_COMPILER_FLAGS}")
