cmake_minimum_required(VERSION 3.16)

project(Earie VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Quick Qml QuickControls2 Widgets)

qt_standard_project_setup(REQUIRES 6.8)

qt_add_executable(earie WIN32
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

target_include_directories(earie PRIVATE include)

target_compile_definitions(earie PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)

target_link_libraries(earie PRIVATE
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

set_target_properties(earie PROPERTIES
    WIN32_EXECUTABLE TRUE
)
