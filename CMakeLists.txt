cmake_minimum_required(VERSION 3.16)
project(english3000 VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

set(ENGLISH3000_BUILD_ID "dev")
if(DEFINED ENV{ENGLISH3000_BUILD_ID}
   AND NOT "$ENV{ENGLISH3000_BUILD_ID}" STREQUAL "")
    set(ENGLISH3000_BUILD_ID "$ENV{ENGLISH3000_BUILD_ID}")
endif()
configure_file(src/build_info.h.in
               ${CMAKE_CURRENT_BINARY_DIR}/build_info.h @ONLY)

find_package(Qt6 REQUIRED COMPONENTS Sql Network)
find_package(Qt6 QUIET COMPONENTS Widgets)

find_package(Qt6 QUIET COMPONENTS Quick QuickControls2)
find_package(Qt6 QUIET COMPONENTS TextToSpeech)
file(GLOB_RECURSE ESPEAK_DATA_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/espeak-ng/data/*)
if(Qt6Quick_FOUND)
    qt_add_executable(english3000_mobile MANUAL_FINALIZATION
        mobile/MobileApp.cpp
        mobile/bridge.cpp
        mobile/bridge.h
        src/ai_probe.cpp
        src/ai_probe.h
    )
    target_include_directories(english3000_mobile PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(english3000_mobile PRIVATE
        core ai Qt6::Quick Qt6::QuickControls2 Qt6::Sql)
    # Android 打包必需:显式导入 SQLite 驱动插件(否则运行时 Driver not loaded)
    qt_import_plugins(english3000_mobile INCLUDE Qt6::QSQLiteDriverPlugin)
    if(Qt6TextToSpeech_FOUND)
        target_link_libraries(english3000_mobile PRIVATE Qt6::TextToSpeech Qt6::Multimedia)
        target_compile_definitions(english3000_mobile PRIVATE
            ENGLISH3000_HAS_TTS=1)
    endif()
    qt_add_resources(english3000_mobile "mobile_resources"
        PREFIX "/"
        FILES
            mobile/qml/main.qml
            mobile/qml/StudyPage.qml
            mobile/qml/ReadingPage.qml
            mobile/qml/WordListsPage.qml
            mobile/qml/TranslatePage.qml
            mobile/qml/SettingsPage.qml
            mobile/qml/StatsPage.qml
            mobile/qml/Theme.qml
            mobile/qml/qmldir
            assets/oxford3000.csv
            assets/lemma.en.txt
            assets/donate/qr1.jpg
            assets/donate/qr2.jpg
            assets/samples/coffee.txt
            assets/samples/linux-files.txt
            assets/samples/morning-routine.txt)
    if(ANDROID)
        set_target_properties(english3000_mobile PROPERTIES
            QT_ANDROID_PACKAGE_SOURCE_DIR
            ${CMAKE_CURRENT_SOURCE_DIR}/mobile/android)
    endif()
    if(ANDROID)
        target_link_libraries(english3000_mobile PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/espeak-ng/lib/arm64-v8a/libespeak-ng.a
            ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/espeak-ng/lib/arm64-v8a/libucd.a
            ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/espeak-ng/lib/arm64-v8a/libspeechPlayer.a)
        target_include_directories(english3000_mobile PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/espeak-ng/include)
    endif()
    qt_add_resources(english3000_mobile "espeak_data"
        PREFIX "/espeak-ng-data"
        BASE ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/espeak-ng/data
        FILES ${ESPEAK_DATA_FILES})
    qt_finalize_executable(english3000_mobile)
endif()

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

if(Qt6Widgets_FOUND)
    add_executable(english3000
        src/main.cpp
        src/mainwindow.cpp
        src/mainwindow.h
        src/update_checker.cpp
        src/update_checker.h
        src/ai_probe.cpp
        src/ai_probe.h
        src/translator_ui.cpp
        src/translator_ui.h
        src/conversation_ui.cpp
        src/conversation_ui.h
        src/wordlist_ui.cpp
        src/wordlist_ui.h
    )
    target_include_directories(english3000 PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR})
    target_link_libraries(english3000 PRIVATE core ai Qt6::Widgets)
    if(NOT ANDROID AND UNIX)
        target_link_libraries(english3000 PRIVATE X11 xcb)
    endif()
    if(WIN32)
        target_link_libraries(english3000 PRIVATE user32)
    endif()
    target_compile_definitions(english3000 PRIVATE
        ENGLISH3000_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets"
    )
endif()

add_executable(test_core tests/test_core.cpp)
target_link_libraries(test_core PRIVATE core ai Qt6::Core Qt6::Sql Qt6::Network)
target_compile_definitions(test_core PRIVATE
    ENGLISH3000_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets"
)

enable_testing()
add_test(NAME test_core COMMAND test_core)
