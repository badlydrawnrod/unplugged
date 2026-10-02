cmake_minimum_required(VERSION 3.25)
project(unplugged LANGUAGES CXX)

include(CTest)
include(FetchContent)

FetchContent_Declare(
    Microsoft_GSL
    GIT_REPOSITORY https://github.com/microsoft/GSL.git
    GIT_TAG        v4.1.0 # Replace with the latest or preferred version tag
)
FetchContent_MakeAvailable(Microsoft_GSL)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(EDITOR_SOURCES
    editor.cpp
    document.cpp
    gap_buffer.cpp
    gap_loader.cpp
    key.cpp
    line_starts.cpp
    logical_line_range.cpp
    raw_mode.cpp
    read_key.cpp
    terminal.cpp
    wrapped_row_range.cpp
)

set(DOCUMENT_MODEL_SOURCES
    document.cpp
    gap_buffer.cpp
    line_starts.cpp
    logical_line_range.cpp
)

set(WRAPPED_ROW_SOURCES
    ${DOCUMENT_MODEL_SOURCES}
    wrapped_row_range.cpp
)

add_executable(editor ${EDITOR_SOURCES})
target_compile_options(editor PRIVATE -Wall -Wextra -Wpedantic -Werror)
target_link_libraries(editor PRIVATE Microsoft.GSL::GSL)

# Enable exceptions for contract violations in debug/test builds only.
set(CONTRACT_EXCEPTIONS_DEF "")
set(CONTRACT_EXCEPTIONS_DEF "$<$<CONFIG:Debug>:CONTRACT_EXCEPTIONS>")

if(BUILD_TESTING)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.15.2
    )
    FetchContent_MakeAvailable(googletest)

    function(add_unplugged_test target)
        add_executable(${target} ${ARGN})
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror -UNDEBUG)
        target_compile_definitions(${target} PRIVATE ${CONTRACT_EXCEPTIONS_DEF})
        target_link_libraries(${target} PRIVATE GTest::gtest_main Microsoft.GSL::GSL)
    endfunction()

    add_unplugged_test(gap_buffer_tests
        gap_buffer_tests.cpp
        gap_buffer.cpp
    )

    add_unplugged_test(line_starts_tests
        line_starts_tests.cpp
        line_starts.cpp
    )

    add_unplugged_test(document_tests
        document_tests.cpp
        ${DOCUMENT_MODEL_SOURCES}
    )

    add_unplugged_test(document_view_tests
        document_view_tests.cpp
        ${DOCUMENT_MODEL_SOURCES}
    )

    add_unplugged_test(logical_line_range_tests
        logical_line_range_tests.cpp
        ${DOCUMENT_MODEL_SOURCES}
    )

    add_unplugged_test(wrapped_row_range_tests
        wrapped_row_range_tests.cpp
        ${WRAPPED_ROW_SOURCES}
    )

    include(GoogleTest)
    gtest_discover_tests(gap_buffer_tests)
    gtest_discover_tests(line_starts_tests)
    gtest_discover_tests(document_tests)
    gtest_discover_tests(document_view_tests)
    gtest_discover_tests(logical_line_range_tests)
    gtest_discover_tests(wrapped_row_range_tests)
endif()
