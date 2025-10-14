cmake_minimum_required(VERSION 3.16)

project(ksysguard)
set(PROJECT_VERSION "6.0.1")

set(KSYSGUARD_VERSION 4.98.0)
set(KSYSGUARD_STRING_VERSION "${KSYSGUARD_VERSION}")

set(KSYSGUARD_SOVERSION 1)

set(QT_MIN_VERSION "6.6.0")
set(KF6_MIN_VERSION "6.0.0")
set(KDE_COMPILERSETTINGS_LEVEL "5.82")

find_package(ECM ${KF6_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH}  ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(CheckIncludeFiles)
include(GenerateExportHeader)
include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(KDEClangFormat)
include(ECMAddTests)
include(ECMInstallIcons)
include(FeatureSummary)
include(ECMConfiguredInstall)
include(ECMGenerateDBusServiceFile)
include(ECMQtDeclareLoggingCategory)

find_package(Qt${QT_MAJOR_VERSION} ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS
    Core
    Widgets
    Test
    Xml
)
find_package(KF6 ${KF6_MIN_VERSION} REQUIRED COMPONENTS
    Config
    CoreAddons
    DBusAddons
    DocTools
    I18n
    IconThemes
    ItemViews
    KIO
    NewStuffCore
    NewStuff
    Auth
    Notifications
    WindowSystem
    ConfigWidgets
    GlobalAccel
    XmlGui
)

find_package(Qt${QT_MAJOR_VERSION}WebEngineWidgets ${QT_MIN_VERSION} CONFIG)
set_package_properties(Qt${QT_MAJOR_VERSION}WebEngineWidgets PROPERTIES
                       URL "git://code.qt.org/qt/qtwebenginewidgets.git"
                       DESCRIPTION "Qt WebEngine module (web browsing engine)"
                       TYPE OPTIONAL
                       PURPOSE "Used by the HTML-based GUI ksysguard library"
                      )
find_package(Qt6WebChannel ${QT_MIN_VERSION} CONFIG)
set_package_properties(Qt${QT_MAJOR_VERSION}WebChannel PROPERTIES
                       URL "git://code.qt.org/qt/qtwebchannel.git"
                       DESCRIPTION "Qt WebChannel module"
                       TYPE OPTIONAL
                       PURPOSE "Used by the HTML-based GUI ksysguard library"
                      )

find_package(ZLIB REQUIRED)
set_package_properties(ZLIB PROPERTIES DESCRIPTION "Support for gzip compressed files and data streams"
                       URL "https://zlib.net/"
                       TYPE REQUIRED
                      )

option(WITH_X11 "Build with X11 support." ON)
if(WITH_X11)
    find_package(X11)
    set_package_properties(X11 PROPERTIES DESCRIPTION "X11 libraries"
                            URL "https://www.x.org"
                            TYPE OPTIONAL
                            PURPOSE "Required for building Process UI with X11 support"
                        )
    if(X11_FOUND)
        if (QT_MAJOR_VERSION STREQUAL "5")
            find_package(Qt5X11Extras REQUIRED)
        elseif (QT_MAJOR_VERSION STREQUAL "6")
            if (Qt6_VERSION VERSION_LESS "6.10.0")
                find_package(Qt6 ${REQUIRED_QT_VERSION} CONFIG REQUIRED Gui) # qtx11extras_p.h
            else()
                find_package(Qt6 ${REQUIRED_QT_VERSION} CONFIG REQUIRED GuiPrivate) # qtx11extras_p.h
            endif()
        endif()
        set(HAVE_X11 1)
        find_library(X11_XRes_LIB XRes ${X11_LIB_SEARCH_PATH})
        find_path(X11_XRes_INCLUDE_PATH X11/extensions/XRes.h ${X11_INC_SEARCH_PATH})

        if(X11_XRes_LIB AND X11_XRes_INCLUDE_PATH)
            set(HAVE_XRES 1)
        endif()
    endif()
else()
    set(HAVE_X11 0)
    set(HAVE_XRES 0)
endif()

set(WEBENGINE_SCRIPTING_ENABLED FALSE)
if(Qt${QT_MAJOR_VERSION}WebEngineWidgets_FOUND AND Qt${QT_MAJOR_VERSION}WebChannel_FOUND)
    set(WEBENGINE_SCRIPTING_ENABLED TRUE)
endif()
add_feature_info("Scripting plugin support" WEBENGINE_SCRIPTING_ENABLED "Support scripting plugins using WebEngine and WebChannel")

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    find_package(NL)
    set_package_properties(NL PROPERTIES
        TYPE REQUIRED
        PURPOSE "Used for gathering socket info via the sock_diag netlink subsystem."
        URL "https://github.com/thom311/libnl/"
    )
    find_package(libpcap)
    set_package_properties(
        libpcap PROPERTIES
        TYPE RECOMMENDED
        PURPOSE "libpcap is used for per-application network usage."
    )

    if(libpcap_FOUND )
        set(BUILD_NETWORK_PLUGIN TRUE)

        find_package(Libcap)
        set_package_properties(Libcap PROPERTIES
            TYPE OPTIONAL
            PURPOSE "Needed for setting capabilities of the per-application network plugin."
        )
    endif()

    find_package(Sensors)
    set_package_properties(Sensors PROPERTIES
        TYPE REQUIRED
        PURPOSE "Used for reading hardware sensors")
endif()

configure_file(config-ksysguard.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-ksysguard.h )

option(ENABLE_KAUTH_HELPER "Build and install ksysguardprocesslist_helper KAuth helper" ON)
add_feature_info(KAUTH_HELPER ${ENABLE_KAUTH_HELPER} "Build and install ksysguardprocesslist_helper KAuth helper")


find_package(KSysGuard REQUIRED)

add_definitions(-DQT_NO_URL_CAST_FROM_STRING)
add_definitions(-DQT_USE_QSTRINGBUILDER)
#add_definitions(-DQT_NO_CAST_FROM_ASCII)
#add_definitions(-DQT_NO_CAST_TO_ASCII)
add_definitions(-DQT_DISABLE_DEPRECATED_BEFORE=0x050f00)
add_definitions(-DKF_DISABLE_DEPRECATED_BEFORE_AND_AT=0x054200)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

find_package(Sensors)
set_package_properties(Sensors PROPERTIES
                       TYPE OPTIONAL
                       PURPOSE "Allows to show sensor information")

include_directories(${CMAKE_CURRENT_BINARY_DIR})
configure_file(config-workspace.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-workspace.h)

add_subdirectory( formatter )
add_subdirectory( processcore )
add_subdirectory( processui )
add_subdirectory( ksgrd )
add_subdirectory( signalplotter )
add_subdirectory( gui )

add_subdirectory( doc )
kdoctools_install(po)

add_subdirectory( pics )
add_subdirectory( example )

add_subdirectory( ksysguardd )

option(WITH_SYSTEMMONITOR "Build with Plasma Dekstop SystemMonitor" ON)
if(WITH_SYSTEMMONITOR)
    add_subdirectory( systemmonitor )
endif()

ki18n_install(po)
# add clang-format target for all our real source files
file(GLOB_RECURSE ALL_CLANG_FORMAT_SOURCE_FILES *.cpp *.h)
kde_clang_format(${ALL_CLANG_FORMAT_SOURCE_FILES})

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)

