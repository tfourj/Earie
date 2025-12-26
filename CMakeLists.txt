cmake_minimum_required(VERSION 3.16)

project(Earie VERSION 1.0.0 LANGUAGES CXX)

set(TARGET_NAME Earie)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Quick Qml QuickControls2 Widgets)

qt_standard_project_setup(REQUIRES 6.8)

qt_add_executable(${TARGET_NAME} WIN32
    main.cpp

    src/AppController.cpp
    src/AudioBackend.cpp
    src/AudioDevice.cpp
    src/AudioSession.cpp
    src/AudioWorker.cpp
    src/ComInit.cpp
    src/ConfigStore.cpp
    src/DeviceListModel.cpp
    src/IconCache.cpp
    src/SessionListModel.cpp
    src/UpdateCoalescer.cpp
    src/WinAcrylic.cpp
    src/WinTrayPositioner.cpp
    resources.qrc
    app.rc

    include/AppController.h
    include/AudioBackend.h
    include/AudioDevice.h
    include/AudioSession.h
    include/AudioWorker.h
    include/ComInit.h
    include/ConfigStore.h
    include/DeviceListModel.h
    include/IconCache.h
    include/SessionListModel.h
    include/UpdateCoalescer.h
    include/WinAcrylic.h
    include/WinTrayPositioner.h
    include/win/ComPtr.h
    include/win/Hr.h
    include/win/Utf.h
)

target_include_directories(${TARGET_NAME} PRIVATE include)

target_compile_definitions(${TARGET_NAME} PRIVATE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    EARIE_VERSION="${PROJECT_VERSION}"
)

target_link_libraries(${TARGET_NAME} PRIVATE
    Qt6::Quick
    Qt6::Qml
    Qt6::QuickControls2
    Qt6::Widgets
    dwmapi
    ole32
    uuid
    mmdevapi
    shell32
    user32
    gdi32
    shlwapi
)

set_target_properties(${TARGET_NAME} PROPERTIES
    WIN32_EXECUTABLE TRUE
)

if (WIN32 AND CMAKE_BUILD_TYPE STREQUAL "Release")
    get_target_property(_qt_qmake_executable Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(_qt_bin_dir "${_qt_qmake_executable}" DIRECTORY)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_qt_bin_dir}")
    if (WINDEPLOYQT_EXECUTABLE)
        set(_deploy_dir "${CMAKE_SOURCE_DIR}/release")
        add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${_deploy_dir}"
            COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:${TARGET_NAME}>" "${_deploy_dir}/$<TARGET_FILE_NAME:${TARGET_NAME}>"
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                "${_deploy_dir}/$<TARGET_FILE_NAME:${TARGET_NAME}>"
                --qmldir "${CMAKE_SOURCE_DIR}/qml"
                --no-compiler-runtime
                --no-translations
                --no-system-d3d-compiler
                --no-opengl-sw
            COMMAND cmd /c if exist "${CMAKE_SOURCE_DIR}/installer/clean_release.bat" call "${CMAKE_SOURCE_DIR}/installer/clean_release.bat" "${_deploy_dir}"
            COMMAND cmd /c if exist "${CMAKE_SOURCE_DIR}/installer/user_post_deploy.bat" call "${CMAKE_SOURCE_DIR}/installer/user_post_deploy.bat" "${_deploy_dir}" "${PROJECT_VERSION}"
            COMMENT "Running windeployqt for ${TARGET_NAME} (release folder)"
        )
    else()
        message(WARNING "windeployqt not found; skipping Qt deployment step")
    endif()
endif()
