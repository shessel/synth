cmake_minimum_required(VERSION 3.12)

set(SOURCE_FILES
		synth.cpp
	)

set(HEADER_FILES
		synth.h
	)

add_executable(synth-main main.cpp)
target_link_libraries(synth-main synth)
target_compile_features(synth-main PUBLIC cxx_std_17)

add_library(synth ${SOURCE_FILES} ${HEADER_FILES})
target_link_libraries(synth Winmm.lib)
target_include_directories(synth
    PUBLIC 
        $<INSTALL_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>    
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)
target_compile_features(synth PUBLIC cxx_std_17)

source_group("Source Files" FILES ${SOURCE_FILES})
source_group("Header Files" FILES ${HEADER_FILES})

# adapted from https://arne-mertz.de/2018/07/cmake-properties-options/
if (MSVC)
    # warning level 4 and all warnings as errors
    target_compile_options(synth-main PRIVATE /W4 /WX /permissive-)
    target_compile_options(synth PRIVATE /W4 /WX /permissive-)
else()
    # lots of warnings and all warnings as errors
    target_compile_options(synth-main PRIVATE -Wall -Wextra -pedantic -Werror)
    target_compile_options(synth PRIVATE -Wall -Wextra -pedantic -Werror)
endif()