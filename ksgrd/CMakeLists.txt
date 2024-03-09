


########### next target ###############

set(ksgrd_LIB_SRCS
   SensorAgent.cpp
   SensorManager.cpp
   SensorShellAgent.cpp
   SensorSocketAgent.cpp
)

set(ksgrd_LIB_HEADERS
    SensorAgent.h
    SensorClient.h
    SensorManager.h
    SensorShellAgent.h
    SensorSocketAgent.h
)

ecm_qt_declare_logging_category(ksgrd_LIB_SRCS HEADER ksgrd_debug.h IDENTIFIER LIBKSYSGUARD_KSGRD CATEGORY_NAME org.kde.libksysguard.ksgrd
    DESCRIPTION "libksysguard (ksgrd)" EXPORT LIBKSYSGUARD)
add_definitions(-DTRANSLATION_DOMAIN=\"ksgrd\")

remove_definitions(-DQT_NO_CAST_FROM_ASCII -DQT_STRICT_ITERATORS -DQT_NO_CAST_FROM_BYTEARRAY -DQT_NO_KEYWORDS)

add_library(ksgrd STATIC ${ksgrd_LIB_SRCS} ${ksgrd_LIB_HEADERS})
add_library(SysGuard ALIAS ksgrd)
target_include_directories(ksgrd
    PUBLIC
        "$<BUILD_INTERFACE:${libksysguard_SOURCE_DIR}>"
        "$<INSTALL_INTERFACE:${KDE_INSTALL_INCLUDEDIR}/ksysguard/>"
)

target_link_libraries(ksgrd
    PUBLIC
        Qt::Core
        Qt::Network # QTcpSocket in SensorSocketAgent.h
        KF6::ConfigCore # KConfigGroup in SensorManager.h
    PRIVATE
        KF6::I18n
        KF6::CoreAddons
        ProcessCore
)

#set_target_properties(ksgrd PROPERTIES
#    VERSION ${KSYSGUARD_VERSION}
#    SOVERSION ${KSYSGUARD_SOVERSION}
#    EXPORT_NAME SysGuard
#)
#install(TARGETS ksgrd EXPORT libksysguardLibraryTargets ${KDE_INSTALL_TARGETS_DEFAULT_ARGS} )

#install(FILES
#    ${ksgrd_LIB_HEADERS}
#    DESTINATION ${KDE_INSTALL_INCLUDEDIR}/ksysguard/ksgrd
#    COMPONENT Devel
#)



