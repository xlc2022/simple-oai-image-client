cmake_minimum_required(VERSION 3.21)

project(SimpleImageClient VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Widgets Network)
qt_standard_project_setup()

# Everything except main.cpp lives in a static library so tests can link it.
add_library(imageclient_core STATIC
    src/appsettings.cpp
    src/appsettings.h
    src/historypanel.cpp
    src/historypanel.h
    src/historystore.cpp
    src/historystore.h
    src/imageactions.cpp
    src/imageactions.h
    src/imagepanel.cpp
    src/imagepanel.h
    src/imageprovider.cpp
    src/imageprovider.h
    src/imageview.cpp
    src/imageview.h
    src/maskcanvas.cpp
    src/maskcanvas.h
    src/openaiprovider.cpp
    src/openaiprovider.h
    src/openrouterprovider.cpp
    src/openrouterprovider.h
    src/settingsdialog.cpp
    src/settingsdialog.h
    src/mainwindow.cpp
    src/mainwindow.h
)
target_include_directories(imageclient_core PUBLIC src)
target_link_libraries(imageclient_core PUBLIC Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network)

qt_add_executable(SimpleImageClient WIN32 src/main.cpp)
target_link_libraries(SimpleImageClient PRIVATE imageclient_core)
target_compile_definitions(SimpleImageClient PRIVATE PROJECT_VERSION="${PROJECT_VERSION}")

include(GNUInstallDirs)
install(TARGETS SimpleImageClient
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

option(BUILD_TESTING "Build tests" ON)
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()
