include(ECMQtDeclareLoggingCategory)

set(nvidiaplugin_SRCS nvidia.cpp)

ecm_qt_declare_logging_category(nvidiaplugin_SRCS
    HEADER nvidialogging.h
    IDENTIFIER KSYSGUARD_PLUGIN_NVIDIA
    CATEGORY_NAME org.kde.ksysguard.plugin.nvidia
)

add_library(ksysguard_plugin_nvidia MODULE ${nvidiaplugin_SRCS})
target_link_libraries(ksysguard_plugin_nvidia Qt::Core Qt::Widgets Qt::Xml ProcessCore KF6::I18n KF6::CoreAddons KF6::ConfigCore)

install(TARGETS ksysguard_plugin_nvidia DESTINATION ${KDE_INSTALL_PLUGINDIR}/ksysguard6/process)
