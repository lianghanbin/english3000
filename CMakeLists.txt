cmake_minimum_required(VERSION 3.16)
project(english3000 VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Sql Network)

add_library(core STATIC
    src/core.cpp
    src/core.h
)
target_include_directories(core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(core PUBLIC Qt6::Core Qt6::Sql)

add_library(ai STATIC
    src/ai_client.cpp
    src/ai_client.h
)
target_include_directories(ai PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(ai PUBLIC Qt6::Core Qt6::Network)

add_executable(english3000
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
    src/translator_ui.cpp
    src/translator_ui.h
    src/conversation_ui.cpp
    src/conversation_ui.h
    src/wordlist_ui.cpp
    src/wordlist_ui.h
)
target_link_libraries(english3000 PRIVATE core ai Qt6::Widgets)
if(NOT ANDROID AND UNIX)
    target_link_libraries(english3000 PRIVATE X11 xcb)
endif()
target_compile_definitions(english3000 PRIVATE
    ENGLISH3000_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets"
)

add_executable(test_core tests/test_core.cpp)
target_link_libraries(test_core PRIVATE core ai Qt6::Core Qt6::Sql Qt6::Network)
target_compile_definitions(test_core PRIVATE
    ENGLISH3000_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets"
)

enable_testing()
add_test(NAME test_core COMMAND test_core)
