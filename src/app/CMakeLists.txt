cmake_minimum_required(VERSION 3.16)
project(WifiCamClient)

set(CMAKE_CXX_STANDARD 17)

find_package(Qt6 REQUIRED COMPONENTS Core Quick Qml Multimedia)

qt_standard_project_setup()

qt_add_executable(qcar
    main.cpp
    MpvItem.cpp
    CameraClient.cpp
    PropParser.cpp
)

qt_add_qml_module(qcar
    URI WifiCam
    VERSION 1.0
    QML_FILES
        qml/Main.qml
)

target_link_libraries(qcar PRIVATE
    Qt6::Core
    Qt6::Quick
    Qt6::Qml
    Qt6::Multimedia
    mpv
)
