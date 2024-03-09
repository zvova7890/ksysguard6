include_directories(
   SensorDisplayLib/
)


########### next target ###############

add_definitions(-DTRANSLATION_DOMAIN=\"ksysguard\")

set(libsensordisplays_SRCS
   	SensorDisplayLib/SensorDisplay.cpp SensorDisplayLib/SensorDisplay.h
   	SensorDisplayLib/BarGraph.cpp SensorDisplayLib/BarGraph.h
   	SensorDisplayLib/DancingBars.cpp SensorDisplayLib/DancingBars.h
   	SensorDisplayLib/DancingBarsSettings.cpp SensorDisplayLib/DancingBarsSettings.h
   	SensorDisplayLib/DummyDisplay.cpp SensorDisplayLib/DummyDisplay.h
   	SensorDisplayLib/FancyPlotter.cpp SensorDisplayLib/FancyPlotter.h
   	SensorDisplayLib/FancyPlotterSettings.cpp SensorDisplayLib/FancyPlotterSettings.h
   	SensorDisplayLib/ListView.cpp SensorDisplayLib/ListView.h
   	SensorDisplayLib/LogFile.cpp SensorDisplayLib/LogFile.h
   	SensorDisplayLib/MultiMeter.cpp SensorDisplayLib/MultiMeter.h
   	SensorDisplayLib/MultiMeterSettings.cpp SensorDisplayLib/MultiMeterSettings.h
   	SensorDisplayLib/ProcessController.cpp SensorDisplayLib/ProcessController.h
   	SensorDisplayLib/SensorLogger.cpp SensorDisplayLib/SensorLogger.h
   	SensorDisplayLib/SensorLoggerDlg.cpp SensorDisplayLib/SensorLoggerDlg.h
   	SensorDisplayLib/SensorLoggerSettings.cpp SensorDisplayLib/SensorLoggerSettings.h
   	SensorDisplayLib/SensorModel.cpp SensorDisplayLib/SensorModel.h
   	SensorDisplayLib/ListViewSettings.cpp SensorDisplayLib/ListViewSettings.h
#   	SensorDisplayLib/modeltest.cpp
	)

ki18n_wrap_ui( libsensordisplays_SRCS
   SensorDisplayLib/ListViewSettingsWidget.ui
   SensorDisplayLib/LogFileSettings.ui
   SensorDisplayLib/MultiMeterSettingsWidget.ui
   SensorDisplayLib/SensorLoggerDlgWidget.ui
   SensorDisplayLib/SensorLoggerSettingsWidget.ui
)

set(ksysguard_SRCS ${libsensordisplays_SRCS}
   SensorBrowser.cpp SensorBrowser.h
   WorkSheet.cpp WorkSheet.h
   WorkSheetSettings.cpp WorkSheetSettings.h
   Workspace.cpp Workspace.h
   HostConnector.cpp HostConnector.h
   StyleEngine.cpp StyleEngine.h
   ksysguard.cpp ksysguard.h )

add_executable(ksysguard ${ksysguard_SRCS})
target_compile_definitions(ksysguard PRIVATE -DPROJECT_VERSION="${PROJECT_VERSION}")

target_link_libraries(ksysguard
    ProcessUi
    SysGuard
    SignalPlotter
    Qt6::Xml
    Qt6::Core5Compat
    KF6::XmlGui
    KF6::DBusAddons
    KF6::ItemViews
    KF6::KIOWidgets
    KF6::NewStuffCore
    KF6::NewStuffWidgets
    KF6::Notifications
    KF6::I18n
    KF6::IconThemes
    KF6::WindowSystem)

install(TARGETS ksysguard         ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})

########### install files ###############

install( PROGRAMS org.kde.ksysguard.desktop  DESTINATION  ${KDE_INSTALL_APPDIR} )
install( FILES org.kde.ksysguard.appdata.xml DESTINATION ${KDE_INSTALL_METAINFODIR} )
install( FILES ProcessTable.sgrd SystemLoad2.sgrd DESTINATION  ${KDE_INSTALL_DATADIR}/ksysguard )
install( FILES ksysguardui.rc  DESTINATION  ${KDE_INSTALL_KXMLGUIDIR}/ksysguard )
install( FILES ksysguard.knsrc DESTINATION ${KDE_INSTALL_KNSRCDIR})

