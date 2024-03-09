include(ECMAddQtDesignerPlugin)

ecm_qtdesignerplugin_widget(KSignalPlotter
    TOOLTIP "A graph plotter widget for plotting continuously incoming data"
    GROUP "KSysGuard"
)

ecm_add_qtdesignerplugin(ksignalplotterwidgets
    NAME KSignalPlotterWidgets
    OUTPUT_NAME ksignalplotter5widgets
    WIDGETS
        KSignalPlotter
    LINK_LIBRARIES
        ksignalplotter
    INSTALL_DESTINATION "${KDE_INSTALL_QTPLUGINDIR}/designer"
    COMPONENT Devel
)
