# Each subfolder is a sample, built as an executable named after the folder.
function(add_sample dir)
    file(GLOB_RECURSE sources CONFIGURE_DEPENDS ${dir}/*.cpp ${dir}/*.h)
    add_executable(${dir} ${sources})
    target_link_libraries(${dir} PRIVATE imgui::stack)

    if(MSVC)
        target_compile_options(${dir} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${dir} PRIVATE -Wall -Wextra -Wpedantic -Wshadow)
    endif()
endfunction()

file(GLOB children RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*)
set(samples)
foreach(child ${children})
    if(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${child})
        add_sample(${child})
        list(APPEND samples ${child})
    endif()
endforeach()

set(SAMPLES ${samples} PARENT_SCOPE)
