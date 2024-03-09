include_directories(${CMAKE_CURRENT_BINARY_DIR}/.. ${CMAKE_CURRENT_SOURCE_DIR}/..)

add_library(ProcessPlugin SHARED ProcessPlugin.cpp ProcessPlugin.h ProcessEnums.cpp ProcessEnums.h)

target_link_libraries(ProcessPlugin Qt::Qml ProcessCore)

install(TARGETS ProcessPlugin DESTINATION ${KDE_INSTALL_QMLDIR}/org/kde/ksysguard/process)
install(FILES qmldir DESTINATION ${KDE_INSTALL_QMLDIR}/org/kde/ksysguard/process)
