cmake_minimum_required(VERSION 3.16)
project(cs2_skinchanger CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT WIN32)
    message(WARNING "This project targets Windows (cs2.exe). Cross-compile with a MinGW toolchain file.")
endif()

add_executable(cs2-skinchanger
    src/main.cpp
    src/changer.hpp
    src/config.hpp
    src/memory.hpp
    src/offsets.hpp
    src/paintkits.hpp
)

target_include_directories(cs2-skinchanger PRIVATE src)

if(MSVC)
    target_compile_options(cs2-skinchanger PRIVATE /W4 /O2)
else()
    target_compile_options(cs2-skinchanger PRIVATE -Wall -Wextra -O2)
endif()

# Ship the config templates next to the binary.
add_custom_command(TARGET cs2-skinchanger POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:cs2-skinchanger>/config
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/config/offsets.cfg
            ${CMAKE_SOURCE_DIR}/config/skins.cfg
            $<TARGET_FILE_DIR:cs2-skinchanger>/config)
