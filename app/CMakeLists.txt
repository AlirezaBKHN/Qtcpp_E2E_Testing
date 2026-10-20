cmake_minimum_required(VERSION 3.24)
project(IranSignalTracker LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Charts Sql Test)
qt_standard_project_setup()

qt_add_executable(IranSignalTracker WIN32
    src/main.cpp
    src/mainwindow.h
    src/mainwindow.cpp
    src/signin_dialog.h
    src/signin_dialog.cpp
    src/dashboard_view.h
    src/dashboard_view.cpp
    src/users_view.h
    src/users_view.cpp
    src/db.h
    src/db.cpp
)
target_link_libraries(IranSignalTracker PRIVATE Qt6::Widgets Qt6::Charts Qt6::Sql)

qt_add_executable(app_smoke
    tests/smoke_main.cpp
    src/mainwindow.h
    src/mainwindow.cpp
    src/signin_dialog.h
    src/signin_dialog.cpp
    src/dashboard_view.h
    src/dashboard_view.cpp
    src/users_view.h
    src/users_view.cpp
    src/db.h
    src/db.cpp
)
target_include_directories(app_smoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(app_smoke PRIVATE Qt6::Widgets Qt6::Charts Qt6::Sql Qt6::Test)

qt_add_executable(app_dbtest
    tests/db_test.cpp
    src/db.h
    src/db.cpp
)
target_include_directories(app_dbtest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(app_dbtest PRIVATE Qt6::Sql)

# Generates the committed database/signaltracker.db artifact.
qt_add_executable(app_dbgen tools/dbgen.cpp src/db.h src/db.cpp)
target_include_directories(app_dbgen PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(app_dbgen PRIVATE Qt6::Sql)

enable_testing()
add_test(NAME smoke COMMAND app_smoke)
add_test(NAME db_test COMMAND app_dbtest)
