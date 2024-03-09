include_directories(${CMAKE_CURRENT_BINARY_DIR}/.. ${CMAKE_CURRENT_SOURCE_DIR}/..)

add_library(FormatterPlugin STATIC FormatterPlugin.cpp FormatterPlugin.h FormatterWrapper.cpp FormatterWrapper.h)

target_link_libraries(FormatterPlugin Qt::Qml Formatter)

#install(TARGETS FormatterPlugin DESTINATION ${KDE_INSTALL_QMLDIR}/org/kde/ksysguard/formatter)
#install(FILES qmldir DESTINATION ${KDE_INSTALL_QMLDIR}/org/kde/ksysguard/formatter)
