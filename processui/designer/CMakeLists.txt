include(ECMAddQtDesignerPlugin)

ecm_qtdesignerplugin_widget(KSysGuardProcessList
    INCLUDE_FILE "ksysguardprocesslist.h"
    TOOLTIP "A list of processes (programs) running."
    WHATSTHIS "A widget for showing all the processes running along with their memory usage and other details."
    GROUP "KSysGuard"
)
ecm_qtdesignerplugin_widget(KTextEditVT
    INCLUDE_FILE "KTextEditVT.h"
    TOOLTIP "A text box suitable for displaying output from VT console-based programs."
    WHATSTHIS "A widget for displaying out from console based programs.  Some VT100 style commands are interpreted (For example to change the color) as well as some non-printable characters (backspace/delete etc will delete the last character.).  For example the output from 'ls --color' can be displayed."
    GROUP "KSysGuard"
)

ecm_add_qtdesignerplugin(ksysguardwidgets
    NAME KSysGuardWidgets
    OUTPUT_NAME ksysguardwidgets
    WIDGETS
        KSysGuardProcessList
        KTextEditVT
    LINK_LIBRARIES
        ProcessUi
    INSTALL_DESTINATION "${KDE_INSTALL_QTPLUGINDIR}/designer"
    COMPONENT Devel
)
