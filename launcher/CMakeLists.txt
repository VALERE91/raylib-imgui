# The sample list is baked into the launcher at configure time. The description
# shown in the menu is the first comment line of the sample's main.cpp.
set(entries "")
foreach(sample ${SAMPLES})
    set(description "")
    if(EXISTS ${PROJECT_SOURCE_DIR}/samples/${sample}/main.cpp)
        file(STRINGS ${PROJECT_SOURCE_DIR}/samples/${sample}/main.cpp lines REGEX "^//" LIMIT_COUNT 1)
        string(REGEX REPLACE "^// ?" "" description "${lines}")
    endif()
    string(APPEND entries "    {\"${sample}\", R\"(${description})\", R\"($<TARGET_FILE:${sample}>)\"},\n")
endforeach()

file(GENERATE
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/samples.inc
    CONTENT "${entries}"
)

add_executable(launcher main.cpp)
target_include_directories(launcher PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>)
target_link_libraries(launcher PRIVATE ftxui::component)
add_dependencies(launcher ${SAMPLES})

if(MSVC)
    target_compile_options(launcher PRIVATE /W4 /permissive-)
else()
    target_compile_options(launcher PRIVATE -Wall -Wextra -Wpedantic -Wshadow)
endif()
