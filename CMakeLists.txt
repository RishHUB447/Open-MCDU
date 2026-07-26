cmake_minimum_required(VERSION 3.20)
project(A320_MCDU LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE MCDU_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/*.cpp"
)
file(GLOB_RECURSE MCDU_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/*.h"
)

add_executable(A320_MCDU ${MCDU_SOURCES} ${MCDU_HEADERS})
target_include_directories(A320_MCDU PRIVATE ${CMAKE_SOURCE_DIR}/src)

find_package(SFML 3 COMPONENTS Graphics Window System CONFIG REQUIRED)
target_link_libraries(A320_MCDU PRIVATE SFML::Graphics SFML::Window SFML::System)

add_custom_command(TARGET A320_MCDU POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:A320_MCDU>/assets"
)

if(WIN32)
    add_custom_command(TARGET A320_MCDU POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:A320_MCDU> "$<TARGET_FILE_DIR:A320_MCDU>"
        COMMAND_EXPAND_LISTS
    )
endif()
