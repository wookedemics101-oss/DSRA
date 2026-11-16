cmake_minimum_required(VERSION 3.16)
project(titan_spsc_ring_buffer LANGUAGES CXX)

option(TITAN_ENABLE_TSAN "Build tests with ThreadSanitizer" OFF)

add_executable(spsc_ring_buffer_test tests/spsc_ring_buffer_test.cpp)
target_include_directories(spsc_ring_buffer_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(spsc_ring_buffer_test PRIVATE cxx_std_17)

if (MSVC)
    target_compile_options(spsc_ring_buffer_test PRIVATE /W4 /WX)
else()
    target_compile_options(spsc_ring_buffer_test PRIVATE
        -Wall -Wextra -Wpedantic -Werror)
    if (TITAN_ENABLE_TSAN)
        target_compile_options(spsc_ring_buffer_test PRIVATE
            -fsanitize=thread -fno-omit-frame-pointer -g)
        target_link_options(spsc_ring_buffer_test PRIVATE -fsanitize=thread)
    endif()
endif()

enable_testing()
add_test(NAME spsc_ring_buffer_test COMMAND spsc_ring_buffer_test)
