/*
    KSysGuard, the KDE System Guard

    Copyright (c) 2006 - 2008 John Tapsell <john.tapsell@kde.org>
    Copyright (c) 1999 - 2001 Chris Schlaeger <cs@kde.org>

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License as
 published by the Free Software Foundation; either version 2 of
 the License or (at your option) version 3 or any later version
 accepted by the membership of KDE e.V. (or its successor approved
 by the membership of KDE e.V.), which shall act as a proxy
 defined in Section 14 of version 3 of the license.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.

    KSysGuard has been written with some source code and ideas from
    ktop (<1.0). Early versions of ktop have been written by Bernd
    Johannes Wuebben <wuebben@math.cornell.edu> and Nicolas Leclercq
    <nicknet@planete.net>.

*/

#include "ksysguard.h"
#include "config-workspace.h"
#include "HostConnector.h"
#include "ProcessController.h"
#include "SensorBrowser.h"
#include "StyleEngine.h"
#include "Workspace.h"
#include "WorkSheet.h"

#include <assert.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <QApplication>
#include <QCommandLineParser>
#include <QDBusConnection>
#include <QIcon>
#include <QLabel>
#include <QMenuBar>
#include <QSplitter>
#include <QStatusBar>

#include <KAboutData>
#include <KActionCollection>
#include <KDBusService>
#include <KFormat>
#include <KLocalizedString>
#include <KMessageBox>
#include <KSharedConfig>
#include <KStandardAction>
#include <KToggleAction>
#include <KUserTimestamp>
#include <KWindowSystem>

#include "../ksgrd/SensorAgent.h"
#include "../ksgrd/SensorManager.h"
#include "../processui/ksysguardprocesslist.h"

//Comment out to stop ksysguard from forking.  Good for debugging
//#define FORK_KSYSGUARD

TopLevel* Toplevel;

TopLevel::TopLevel()
  : KXmlGuiWindow( nullptr, Qt::WindowFlags() | Qt::WindowContextHelpButtonHint)
{
  QDBusConnection::sessionBus().registerObject(QStringLiteral("/"), this, QDBusConnection::ExportScriptableSlots);
  mTimerId = -1;
  mLocalProcessController = nullptr;

  mSplitter = new QSplitter( this );
  mSplitter->setOrientation( Qt::Horizontal );
  setCentralWidget( mSplitter );

  mSensorBrowser = nullptr;

  mWorkSpace = new Workspace( mSplitter );
  connect( mWorkSpace, SIGNAL(setCaption(QString)),
           SLOT(setCaption(QString)) );
  connect( mWorkSpace, &QTabWidget::currentChanged,
           this, &TopLevel::currentTabChanged );

  /* Create the status bar. It displays some information about the
   * number of processes and the memory consumption of the local
   * host. */
  const int STATUSBAR_STRETCH=1;

  sbProcessCount = new QLabel();
  statusBar()->addWidget( sbProcessCount, STATUSBAR_STRETCH );

  sbCpuStat = new QLabel();
  statusBar()->addWidget( sbCpuStat, STATUSBAR_STRETCH );

  sbMemTotal = new QLabel();
  statusBar()->addWidget( sbMemTotal, STATUSBAR_STRETCH );

  sbSwapTotal = new QLabel();
  statusBar()->addWidget( sbSwapTotal, STATUSBAR_STRETCH );

  statusBar()->hide();

  // create actions for menu entries
  mRefreshTabAction = KStandardAction::redisplay(mWorkSpace,SLOT(refreshActiveWorksheet()),actionCollection());
  mNewWorksheetAction = actionCollection()->addAction(QStringLiteral("new_worksheet"));
  mNewWorksheetAction->setIcon(QIcon::fromTheme(QStringLiteral("tab-new")));
  connect(mNewWorksheetAction, &QAction::triggered, mWorkSpace, &Workspace::newWorkSheet);
  mInsertWorksheetAction = actionCollection()->addAction(QStringLiteral("import_worksheet"));
  mInsertWorksheetAction->setIcon(QIcon::fromTheme(QStringLiteral("document-open")) );
  connect(mInsertWorksheetAction, SIGNAL(triggered(bool)), mWorkSpace, SLOT(importWorkSheet()));
  mTabExportAction = actionCollection()->addAction( QStringLiteral("export_worksheet") );
  mTabExportAction->setIcon( QIcon::fromTheme(QStringLiteral("document-save-as")) );
  connect(mTabExportAction, SIGNAL(triggered(bool)), mWorkSpace, SLOT(exportWorkSheet()));
  mTabRemoveAction = actionCollection()->addAction( QStringLiteral("remove_worksheet") );
  mTabRemoveAction->setIcon( QIcon::fromTheme(QStringLiteral("tab-close")) );
  connect(mTabRemoveAction, SIGNAL(triggered(bool)), mWorkSpace, SLOT(removeWorkSheet()));
  mMonitorRemoteAction = actionCollection()->addAction( QStringLiteral("connect_host") );
  mMonitorRemoteAction->setIcon( QIcon::fromTheme(QStringLiteral("network-connect")) );
  connect(mMonitorRemoteAction, &QAction::triggered, this, &TopLevel::connectHost);
  //knewstuff2 action
  mHotNewWorksheetAction = actionCollection()->addAction( QStringLiteral("get_new_worksheet") );
  mHotNewWorksheetAction->setIcon( QIcon::fromTheme(QStringLiteral("network-server")) );
  connect(mHotNewWorksheetAction, &QAction::triggered, mWorkSpace, &Workspace::getHotNewWorksheet);
  mHotNewWorksheetUploadAction = actionCollection()->addAction( QStringLiteral("upload_worksheet") );
  mHotNewWorksheetUploadAction->setIcon( QIcon::fromTheme(QStringLiteral("network-server")) );
  connect(mHotNewWorksheetUploadAction, &QAction::triggered, mWorkSpace, &Workspace::uploadHotNewWorksheet);

  mQuitAction = nullptr;

  mConfigureSheetAction = actionCollection()->addAction( QStringLiteral("configure_sheet") );
  mConfigureSheetAction->setIcon( QIcon::fromTheme(QStringLiteral("configure")) );
  connect(mConfigureSheetAction, &QAction::triggered, this, &TopLevel::configureCurrentSheet);
  // set up 'Settings' menu
  mShowMenuBarAction = KStandardAction::showMenubar(this, &TopLevel::toggleShowMenuBar, actionCollection());

  retranslateUi();
}

void TopLevel::toggleShowMenuBar()
{
  if (mShowMenuBarAction->isChecked()) {
    menuBar()->show();
  } else {
    const QString accel = mShowMenuBarAction->shortcut().toString();
    KMessageBox::information(this, i18n("This will hide the menu bar completely."
                                        " You can show it again by typing %1.", accel),
                             i18n("Hide menu bar"),
                             QStringLiteral("HideMenuBarWarning"));
    menuBar()->hide();
  }
}

void TopLevel::setLocalProcessController(ProcessController * localProcessController)
{
  Q_ASSERT(!mLocalProcessController);
  mLocalProcessController = localProcessController;
  connect( mLocalProcessController, &ProcessController::processListChanged, this, &TopLevel::updateProcessCount);
  for(int i = 0; i < mLocalProcessController->actions().size(); i++) {
    actionCollection()->addAction(QStringLiteral("processAction") + QString::number(i), mLocalProcessController->actions().at(i));
  }
}

void TopLevel::retranslateUi()
{
  setPlainCaption( i18n( "System Monitor" ) );
  mRefreshTabAction->setText(i18n("&Refresh Tab"));
  mNewWorksheetAction->setText(i18n( "&New Tab..." ));
  mInsertWorksheetAction->setText(i18n( "Import Tab Fr&om File..." ));
  mTabExportAction->setText( i18n( "Save Tab &As..." ) );
  mTabRemoveAction->setText( i18n( "&Close Tab" ) );
  mMonitorRemoteAction->setText( i18n( "Monitor &Remote Machine..." ) );
  mHotNewWorksheetAction->setText( i18n( "&Download New Tabs..." ) );
  mHotNewWorksheetUploadAction->setText( i18n( "&Upload Current Tab..." ) );

  mConfigureSheetAction->setText( i18n( "Tab &Properties" ) );
  if(mQuitAction) {
    QAction *tmpQuitAction = KStandardAction::quit( nullptr, nullptr, nullptr );
    mQuitAction->setText(tmpQuitAction->text());
    mQuitAction->setWhatsThis(tmpQuitAction->whatsThis());
    mQuitAction->setToolTip(tmpQuitAction->toolTip());
    delete tmpQuitAction;
  } else
    mQuitAction = KStandardAction::quit( this, SLOT(close()), actionCollection() );
}

void TopLevel::configureCurrentSheet() {
  mWorkSpace->configure();
  mRefreshTabAction->setVisible( mWorkSpace->currentWorkSheet()->updateInterval() == 0 );
}
void TopLevel::currentTabChanged(int index)
{
  QWidget *wdg = mWorkSpace->widget(index);
  WorkSheet *sheet = (WorkSheet *)(wdg);
  Q_ASSERT(sheet);
  bool locked = !sheet || sheet->isLocked();
  mTabRemoveAction->setVisible(!locked);
  mTabExportAction->setVisible(!locked);
  mHotNewWorksheetUploadAction->setVisible(!locked);
  mMonitorRemoteAction->setVisible(!locked);

  //only show refresh option is update interval is 0 (manual)
  mRefreshTabAction->setVisible( sheet->updateInterval() == 0 );

  if(!locked && !mSensorBrowser) {
    startSensorBrowserWidget();
  }
  if(mSensorBrowser) {
    if(mSensorBrowser->isVisible() && locked) //going from visible to not visible to save the state
      mSplitterSize = mSplitter->sizes();
    mSensorBrowser->setVisible(!locked);

  }
}
void TopLevel::startSensorBrowserWidget()
{
  if(mSensorBrowser) return;
  mSensorBrowser = new SensorBrowserWidget( nullptr, KSGRD::SensorMgr );
  mSplitter->insertWidget(2,mSensorBrowser);
  mSplitter->setSizes( mSplitterSize );
}

/*
 * DBUS Interface functions
 */

void TopLevel::showOnCurrentDesktop()
{
  //KWindowSystem::setOnDesktop( winId(), KWindowSystem::currentDesktop() );
  KUserTimestamp::updateUserTimestamp();
  KWindowSystem::activateWindow( windowHandle() );
  //KWindowSystem::forceActiveWindow( winId() );
}

void TopLevel::importWorkSheet( const QString &fileName )
{
  mWorkSpace->importWorkSheet(QUrl::fromLocalFile(fileName));
}

void TopLevel::removeWorkSheet( const QString &fileName )
{
  mWorkSpace->removeWorkSheet( fileName );
}

void TopLevel::getHotNewWorksheet()
{
  mWorkSpace->getHotNewWorksheet( );
}

QStringList TopLevel::listSensors( const QString &hostName )
{
  if(!mSensorBrowser) {
    setUpdatesEnabled(false);
    startSensorBrowserWidget();
    mSensorBrowser->setVisible(false);
    setUpdatesEnabled(true);
  }
  return mSensorBrowser->listSensors( hostName );
}

QStringList TopLevel::listHosts()
{
  if(!mSensorBrowser) {
    setUpdatesEnabled(false);
    startSensorBrowserWidget();
    mSensorBrowser->setVisible(false);
    setUpdatesEnabled(true);
  }
  return mSensorBrowser->listHosts();
}

void TopLevel::initStatusBar()
{
  KSGRD::SensorMgr->engage( QStringLiteral("localhost"), QLatin1String(""), QStringLiteral("ksysguardd") );
  /* Request info about the swap space size and the units it is
   * measured in.  The requested info will be received by
   * answerReceived(). */
  KSGRD::SensorMgr->sendRequest( QStringLiteral("localhost"), QStringLiteral("mem/swap/used?"),
                                 (KSGRD::SensorClient*)this, 7 );

  KToggleAction *sb = dynamic_cast<KToggleAction*>(action(QStringLiteral("options_show_statusbar")));
  if (sb)
     connect(sb, &QAction::toggled, this, &TopLevel::updateStatusBar);
  setupGUI(QSize(800,600), ToolBar | Keys | StatusBar | Save | Create);

  updateStatusBar();
}

void TopLevel::updateStatusBar()
{
  if ( mTimerId == -1 )
    mTimerId = startTimer( 2000 );

  // call timerEvent to fill the status bar with real values
  timerEvent( nullptr );
}

void TopLevel::connectHost()
{
  HostConnector hostConnector( this );

//  hostConnector.setHostNames( mHostList );
//  hostConnector.setCommands( mCommandList );

//  hostConnector.setCurrentHostName( "" );

  if ( !hostConnector.exec() )
    return;

//  mHostList = hostConnector.hostNames();
//  mCommandList = hostConnector.commands();

  QString shell = QLatin1String("");
  QString command = QLatin1String("");
  int port = -1;

  /* Check which radio button is selected and set parameters
   * appropriately. */
  if ( hostConnector.useSsh() )
    shell = QStringLiteral("ssh");
  else if ( hostConnector.useRsh() )
    shell = QStringLiteral("rsh");
  else if ( hostConnector.useDaemon() )
    port = hostConnector.port();
  else
    command = hostConnector.currentCommand();

  KSGRD::SensorMgr->engage( hostConnector.currentHostName(), shell, command, port );
}

void TopLevel::disconnectHost()
{
  if(mSensorBrowser)
    mSensorBrowser->disconnect();
}

bool TopLevel::event( QEvent *e )
{
  if ( e->type() == QEvent::User ) {
    /* Due to the asynchronous communication between ksysguard and its
     * back-ends, we sometimes need to show message boxes that were
     * triggered by objects that have died already. */
    KMessageBox::error( this, static_cast<KSGRD::SensorManager::MessageEvent*>(e)->message() );

    return true;
  }

  return KXmlGuiWindow::event( e );
}

void TopLevel::timerEvent( QTimerEvent* )
{
  if ( statusBar()->isVisibleTo( this ) ) {
    /* Request some info about the memory status. The requested
     * information will be received by answerReceived(). */
    const auto localhostString = QStringLiteral("localhost");
    KSGRD::SensorMgr->sendRequest( localhostString, QStringLiteral("cpu/idle"),
                                   (KSGRD::SensorClient*)this, 1 );
    KSGRD::SensorMgr->sendRequest( localhostString, QStringLiteral("mem/physical/free"),
                                   (KSGRD::SensorClient*)this, 2 );
    KSGRD::SensorMgr->sendRequest( localhostString, QStringLiteral("mem/physical/used"),
                                   (KSGRD::SensorClient*)this, 3 );
    KSGRD::SensorMgr->sendRequest( localhostString, QStringLiteral("mem/physical/application"),
                                   (KSGRD::SensorClient*)this, 4 );
    KSGRD::SensorMgr->sendRequest( localhostString, QStringLiteral("mem/swap/free"),
                                   (KSGRD::SensorClient*)this, 5 );
    KSGRD::SensorMgr->sendRequest( localhostString, QStringLiteral("mem/swap/used"),
                                   (KSGRD::SensorClient*)this, 6 );
  }
}

void TopLevel::updateProcessCount()  {
    const int count = mLocalProcessController->processList()->visibleProcessesCount();
    const QString s = i18np( "1 process", "%1 processes", count );
    sbProcessCount->setText( s );
}
void TopLevel::changeEvent( QEvent * event )
{
  if (event->type() == QEvent::LanguageChange) {
    KSGRD::SensorMgr->retranslate();
    setUpdatesEnabled(false);
    setupGUI(ToolBar | Keys | StatusBar | Create);
    retranslateUi();
    setUpdatesEnabled(true);
  }
  KXmlGuiWindow::changeEvent(event);
}

bool TopLevel::queryClose()
{
  if ( !mWorkSpace->saveOnQuit() )
    return false;

  KConfigGroup cg( KSharedConfig::openConfig(), QStringLiteral("MainWindow") );
  saveProperties( cg );
  KSharedConfig::openConfig()->sync();

  return true;
}

void TopLevel::readProperties( const KConfigGroup& cfg )
{

  /* we can ignore 'isMaximized' because we can't set the window
     maximized, so we save the coordinates instead */
//  if ( cfg.readEntry( "isMinimized" , false) == true )
//    showMinimized();

  mSplitterSize = cfg.readEntry( "SplitterSizeList",QList<int>() );
  if ( mSplitterSize.isEmpty() ) {
    // start with a 30/70 ratio
    mSplitterSize.append( 10 );
    mSplitterSize.append( 90 );
  }

  KSGRD::SensorMgr->readProperties( cfg );
  KSGRD::Style->readProperties( cfg );

  mWorkSpace->readProperties( cfg );
}

void TopLevel::saveProperties( KConfigGroup& cfg )
{
  cfg.writeEntry( "isMinimized", isMinimized() );

  if(mSensorBrowser && mSensorBrowser->isVisible())
    cfg.writeEntry( "SplitterSizeList",  mSplitter->sizes());
  else if(mSplitterSize.size() == 2 && mSplitterSize.value(0) != 0 && mSplitterSize.value(1) != 0)
    cfg.writeEntry( "SplitterSizeList", mSplitterSize );

  KSGRD::Style->saveProperties( cfg );
  KSGRD::SensorMgr->saveProperties( cfg );

  saveMainWindowSettings( cfg );
  mWorkSpace->saveProperties( cfg );
}

void TopLevel::answerReceived( int id, const QList<QByteArray> &answerList )
{
  // we have received an answer from the daemon.
  QByteArray answer;
  if(!answerList.isEmpty()) answer = answerList[0];
  QString s;
  static QString unit;
  static qlonglong mFree = 0;
  static qlonglong mUsedApplication = 0;
  static qlonglong mUsedTotal = 0;
  static qlonglong sUsed = 0;
  static qlonglong sFree = 0;

  switch ( id ) {
    case 1:
    {
      const auto percent = (int) (100 - answer.toFloat());
      s = i18n( "CPU: %1%", percent );
      sbCpuStat->setText( s );
      break;
    }

    case 2:
      mFree = answer.toLongLong();
      break;

    case 3:
      mUsedTotal = answer.toLongLong();
      break;

    case 4:
    {
      mUsedApplication = answer.toLongLong();
      const auto used = KFormat().formatByteSize( mUsedApplication*1024);
      const auto total = KFormat().formatByteSize( (mFree+mUsedTotal)*1024 );
      //Use a multi-length string
      s = i18nc( "Arguments are formatted byte sizes (used/total)", "Memory: %1 / %2", used, total );
      sbMemTotal->setText( s );
      break;
    }

    case 5:
      sFree = answer.toLongLong();
      break;

    case 6:
      sUsed = answer.toLongLong();
      setSwapInfo( sUsed, sFree, unit );
      break;

    case 7: {
      KSGRD::SensorIntegerInfo info( answer );
      unit = KSGRD::SensorMgr->translateUnit( info.unit() );
      break;
    }
  }
}

void TopLevel::setSwapInfo( qlonglong used, qlonglong free, const QString & )
{
  QString msg;
  if ( used == 0 && free == 0 ) // no swap available
    msg = i18n( " No swap space available " );
  else {
    const auto formattedUsed = KFormat().formatByteSize( used*1024 );
    const auto formattedTotal = KFormat().formatByteSize( (free+used)*1024);
    msg = i18nc( "Arguments are formatted byte sizes (used/total)", "Swap: %1 / %2", formattedUsed, formattedTotal );
  }

  sbSwapTotal->setText( msg );
}

/*
 * Once upon a time...
 */
int main( int argc, char** argv )
{
  // initpipe is used to keep the parent process around till the child
  // has registered with dbus
#ifdef FORK_KSYSGUARD
  int initpipe[ 2 ];
  pipe( initpipe );
  /* This forking will put ksysguard in it's own session not having a
   * controlling terminal attached to it. This prevents ssh from
   * using this terminal for password requests. Thus, you
   * need a ssh with ssh-askpass support to popup an X dialog to
   * enter the password. */
  pid_t pid;
  if ( ( pid = fork() ) < 0 )
    return -1;
  else
    if ( pid != 0 ) {
      close( initpipe[ 1 ] );

      // wait till init is complete
      char c;
      while( read( initpipe[ 0 ], &c, 1 ) < 0 );

      // then exit
      close( initpipe[ 0 ] );
      exit( 0 );
    }

  close( initpipe[ 0 ] );
  setsid();
#endif

  QApplication app(argc, argv);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  app.setAttribute(Qt::AA_UseHighDpiPixmaps, true);
#endif
  KLocalizedString::setApplicationDomain("ksysguard");

  KAboutData aboutData( QStringLiteral("ksysguard"), i18n( "System Monitor" ),
                        PROJECT_VERSION, i18n("KDE System Monitor"), KAboutLicense::GPL,
                        i18n( "(c) 1996-2016 The KDE System Monitor Developers" ) );
  aboutData.setOrganizationDomain(QByteArray("kde.org"));
  aboutData.addAuthor( i18n("John Tapsell"), i18n("Current Maintainer"), QStringLiteral("john.tapsell@kde.org") );
  aboutData.addAuthor( i18n("Chris Schlaeger"), i18n("Previous Maintainer"), QStringLiteral("cs@kde.org") );
  aboutData.addAuthor( i18n("Greg Martyn"), QString(), QStringLiteral("greg.martyn@gmail.com") );
  aboutData.addAuthor( i18n("Tobias Koenig"), QString(), QStringLiteral("tokoe@kde.org") );
  aboutData.addAuthor( i18n("Nicolas Leclercq"), QString(), QStringLiteral("nicknet@planete.net") );
  aboutData.addAuthor( i18n("Alex Sanda"), QString(), QStringLiteral("alex@darkstart.ping.at") );
  aboutData.addAuthor( i18n("Bernd Johannes Wuebben"), QString(), QStringLiteral("wuebben@math.cornell.edu") );
  aboutData.addAuthor( i18n("Ralf Mueller"), QString(), QStringLiteral("rlaf@bj-ig.de") );
  aboutData.addAuthor( i18n("Hamish Rodda"), QString(), QStringLiteral("rodda@kde.org") );
  aboutData.addAuthor( i18n("Torsten Kasch"), i18n( "Solaris Support\n"
                       "Parts derived (by permission) from the sunos5\n"
                       "module of William LeFebvre's \"top\" utility." ),
                       QStringLiteral("tk@Genetik.Uni-Bielefeld.DE") );
    KAboutData::setApplicationData(aboutData);
    app.setWindowIcon(QIcon::fromTheme(QStringLiteral("utilities-system-monitor")));

    QCommandLineParser parser;
    aboutData.setupCommandLine(&parser);
    parser.addPositionalArgument(QStringLiteral("[worksheet]"), i18n( "Optional worksheet files to load" ));

    parser.process(app);
    aboutData.processCommandLine(&parser);

  KSGRD::SensorMgr = new KSGRD::SensorManager();
  KSGRD::Style = new KSGRD::StyleEngine();

#ifdef FORK_KSYSGUARD
  char c = 0;
  write( initpipe[ 1 ], &c, 1 );
  close( initpipe[ 1 ] );
#endif
  Toplevel = new TopLevel();


  // create top-level widget
  Toplevel->readProperties( KConfigGroup( KSharedConfig::openConfig(), QStringLiteral("MainWindow") ) );
  // setup the statusbar, toolbar etc.
  // Note that this comes after creating the top-level widgets whcih also
  // sets up the various QActions that the user may have added to the toolbar
  Toplevel->initStatusBar();

  //There seems to be some serious bugs with the session restore code.  Disabling
//  if ( app->isSessionRestored() )
//    Toplevel->restore( 1 );

  Toplevel->show();
  KSGRD::SensorMgr->setBroadcaster( Toplevel );  // SensorMgr uses a QPointer for toplevel, so it is okay if Toplevel is deleted first

  // register to DBus
  const KDBusService dbusService(KDBusService::Multiple);

  // run the application
  int result = app.exec();

  delete KSGRD::SensorMgr;
  delete KSGRD::Style;

  return result;
}


