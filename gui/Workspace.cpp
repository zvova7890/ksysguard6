/*
    KSysGuard, the KDE System Guard

    Copyright (c) 1999 - 2002 Chris Schlaeger <cs@kde.org>
    Copyright (c) 2006 John Tapsell <tapsell@kde.org>

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

*/

#include <QFileDialog>
#include <QStandardPaths>

#include <KLocalizedString>
#include <KMessageBox>
#include <KAcceleratorManager>
#include <KActionCollection>
// TODO
//#include <KNS3/QtQuickDialogWrapper>
#include <KNSWidgets/Dialog>
#include <KNSCore/enginebase.h>
#include <KConfigGroup>

#include "WorkSheet.h"
#include "WorkSheetSettings.h"

#include "Workspace.h"
#include "ksysguard.h"
#include "qtabbar.h"

Workspace::Workspace( QWidget* parent)
  : QTabWidget( parent )
{
  KAcceleratorManager::setNoAccel(this);
  setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
  setMovable(true);
  setDocumentMode(true);
  connect(&mDirWatch, SIGNAL(deleted(QString)), this, SLOT(removeWorkSheet(QString)));
  connect(tabBar(), SIGNAL(tabMoved(int,int)), this, SLOT(tabMoved(int,int)));
}

Workspace::~Workspace()
{
}

void Workspace::saveProperties( KConfigGroup& cfg )
{
  QStringList list;
  for(int i =0; i< mSheetList.size(); i++)
    if ( !mSheetList.at(i)->fileName().isEmpty() )
      list.append( mSheetList.at(i)->fileName() );

  cfg.writePathEntry( "SelectedSheets", list );
  cfg.writeEntry( "currentSheet", currentIndex() );
}

void Workspace::readProperties( const KConfigGroup& cfg )
{
  QStringList selectedSheets = cfg.readPathEntry( "SelectedSheets", QStringList() );

  if ( selectedSheets.isEmpty() ) {
   /* If SelectedSheets config entry is not there, then it's
    * probably the first time the user has started KSysGuard. We
    * then "restore" a special default configuration. */
    selectedSheets << QStringLiteral("ProcessTable.sgrd");
    selectedSheets << QStringLiteral("SystemLoad2.sgrd");
  } else if(selectedSheets[0] != QStringLiteral("ProcessTable.sgrd")) {
    //We need to make sure that this is really is the process table on the first tab. No GUI way of changing this, but should make sure anyway.
    //Plus this migrates users from the kde3 setup
    selectedSheets.removeAll(QStringLiteral("ProcessTable.sgrd"));
    selectedSheets.prepend( QStringLiteral("ProcessTable.sgrd"));
  }

  int oldSystemLoad = selectedSheets.indexOf(QStringLiteral("SystemLoad.sgrd"));
  if(oldSystemLoad != -1) {
    selectedSheets.replace(oldSystemLoad, QStringLiteral("SystemLoad2.sgrd"));
  }

  QString filename;
  for ( QStringList::Iterator it = selectedSheets.begin(); it != selectedSheets.end(); ++it ) {
    filename = QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("ksysguard/") + *it);
    if(!filename.isEmpty()) {
      restoreWorkSheet( filename, false);
    }
  }

  int idx = cfg.readEntry( "currentSheet", 0 );
  if (idx < 0 || idx > count() - 1) {
    idx = 0;
  }
  setCurrentIndex(idx);
}

QString Workspace::makeNameForNewSheet() const
{
  /* Find a name of the form "Sheet %d" that is not yet used by any
   * of the existing worksheets. */
  int i = 1;
  bool found = false;
  QString sheetName;
  do {
    sheetName = i18n( "Sheet %1" ,  i++ );
    //Check we don't have any existing files with this name
    found = !(QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("ksysguard/") + sheetName + QStringLiteral(".sgrd")).isEmpty());

    //Check if we have any sheets with the same tab name or file name
    for(int i = 0; !found && i < mSheetList.size(); i++)
      if ( tabText(indexOf(mSheetList.at(i))) == sheetName  || QString(sheetName + QStringLiteral(".sgrd")) == mSheetList.at(i)->fileName())
        found = true;

  } while ( found );

  return sheetName;
}

void Workspace::refreshActiveWorksheet()
{
    WorkSheet* currentSheet = mSheetList.at(currentIndex());
    currentSheet->refreshSheet();
}
void Workspace::newWorkSheet()
{
  /* Find a name of the form "Sheet %d" that is not yet used by any
   * of the existing worksheets. */
  QString sheetName  = makeNameForNewSheet();

  WorkSheetSettings dlg( this, false /*not locked.  New custom sheets aren't locked*/ );
  dlg.setSheetTitle( sheetName );
  if ( dlg.exec() ) {
    WorkSheet* sheet = new WorkSheet( dlg.rows(), dlg.columns(), dlg.interval(), nullptr );
    sheet->setTitle( dlg.sheetTitle() );
    sheet->setFileName( sheetName + QStringLiteral(".sgrd") );
    insertTab(-1, sheet, dlg.sheetTitle().replace(QStringLiteral("&"), QStringLiteral("&&")) );
    mSheetList.append( sheet );
    setCurrentIndex(indexOf( sheet ));
    connect( sheet, &WorkSheet::titleChanged,
         this, &Workspace::updateSheetTitle);
  }
}
void Workspace::contextMenu (int index, const QPoint &point) {
//  KMenu pm;
  Q_UNUSED(index);
  Q_UNUSED(point);
//  QAction *new_worksheet = pm.addAction( Toplevel->actionCollection()->action("new_worksheet") );

 // QAction *action = pm.exec( point );


}
void Workspace::updateSheetTitle( QWidget* wdg )
{
  if ( wdg )
    setTabText( indexOf(wdg), static_cast<WorkSheet*>( wdg )->translatedTitle() );
}

bool Workspace::saveOnQuit()
{
  for(int i = 0; i < mSheetList.size(); i++) {
      if ( mSheetList.at(i)->fileName().isEmpty() ) {
        int res = KMessageBox::warningTwoActionsCancel( this,
                  i18n( "The tab '%1' contains unsaved data.\n"
                        "Do you want to save the tab?",
                    tabText(indexOf( mSheetList.at(i) )) ), QString(), KStandardGuiItem::save(), KStandardGuiItem::discard() );
        if ( res == KMessageBox::PrimaryAction )
          saveWorkSheet( mSheetList.at(i) );
        else if ( res == KMessageBox::Cancel )
          return false; // abort quit
      } else
        saveWorkSheet(mSheetList.at(i));
  }
  return true;
}

void Workspace::importWorkSheet()
{
  QUrl url = QFileDialog::getOpenFileUrl(this,
                                         i18n("Select Tab File to Import"),
                                         QUrl(),
                                         QStringLiteral("Sensor Files (*.sgrd)"));
  importWorkSheet( url );
}

void Workspace::importWorkSheet( const QUrl &url )
{
  if ( url.isEmpty() )
    return;

  // Import sheet from file.
  if (!restoreWorkSheet(url.toLocalFile())) {
    return;
  }

  mSheetList.last()->setFileName( makeNameForNewSheet() + QStringLiteral(".sgrd"));
}

bool Workspace::saveWorkSheet( WorkSheet *sheet )
{
  if ( !sheet ) {
    KMessageBox::error( this, i18n( "You do not have a tab that could be saved." ) );
    return false;
  }

  QString fileName = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QLatin1Char('/') + sheet->fileName();

  if ( !sheet->save( fileName ) ) {
    return false;
  }
  return true;
}

void Workspace::exportWorkSheet()
{
  exportWorkSheet( (WorkSheet*)currentWidget() );
}

void Workspace::exportWorkSheet( WorkSheet *sheet )
{
  if ( !sheet ) {
    KMessageBox::error( this, i18n( "You do not have a tab that could be saved." ) );
    return;
  }

  QString fileName;
  do {
    fileName = QFileDialog::getSaveFileName(this,
                                            i18n("Export Tab"),
                                            QString(tabText(indexOf(currentWidget())) + QStringLiteral(".sgrd")),
                                            QStringLiteral("Sensor Files (*.sgrd)"));
    if ( fileName.isEmpty() )
      return;

  } while ( !sheet->exportWorkSheet( fileName ) );

}

void Workspace::removeWorkSheet()
{
  WorkSheet *current = (WorkSheet*)currentWidget();

  if ( current ) {
    saveWorkSheet( current );

    removeTab(indexOf( current ));
    mSheetList.removeAll( current );
  } else {
    QString msg = i18n( "There are no tabs that could be deleted." );
    KMessageBox::error( this, msg );
  }
}

void Workspace::removeAllWorkSheets()
{
  WorkSheet *sheet;
  while ( ( sheet = (WorkSheet*)currentWidget() ) != nullptr ) {
    saveWorkSheet( sheet );
    removeTab(indexOf( sheet ));
    mSheetList.removeAll( sheet );
    delete sheet;
  }
}

void Workspace::removeWorkSheet( const QString &fileName )
{
  QString baseName = fileName.right( fileName.length() - fileName.lastIndexOf(QLatin1Char('/')) - 1 );
  for(int i = 0; i < mSheetList.size(); i++) {
    WorkSheet *sheet = mSheetList.at(i);
    if ( sheet->fileName() == baseName ) {
      removeTab(indexOf( sheet ));
      mSheetList.removeAt( i );
      delete sheet;
      return;
    }
  }
}

void Workspace::tabMoved(int from, int to)
{
    mSheetList.swapItemsAt(from, to);
}

WorkSheet *Workspace::currentWorkSheet()
{
    return (WorkSheet*)currentWidget();
}
void Workspace::uploadHotNewWorksheet()
{
    WorkSheet *currentWorksheet = currentWorkSheet();
    if(!currentWorksheet)
        return;

    KNSCore::EngineBase engine;
    engine.init(QStringLiteral("ksysguard.knsrc"));
    Q_ASSERT(engine.categories().size() == 1);
    KMessageBox::information(this,
                             xi18nc("@info",
                                 "<para>You can publish your custom tab on the <link url='%1'>KDE Store</link> in the <icode>%2</icode> category.</para>"
                                 "<para><filename>%3</filename></para>",
                                    QStringLiteral("https://store.kde.org"),
                                    engine.categories().at(0),
                                    currentWorksheet->fullFileName()),
                             i18n("Upload custom System Monitor tab"),
                             QString(),
                             KMessageBox::AllowLink);
}
void Workspace::getHotNewWorksheet()
{

    KNSWidgets::Dialog dialog(QStringLiteral("ksysguard.knsrc"));
    if( dialog.exec() == QDialog::Rejected )
        return;

    KNSCore::Entry::List entries = dialog.changedEntries();
    for(const auto &entry : std::as_const(entries)) {
        if(!entry.installedFiles().isEmpty()) {
            QString filename = entry.installedFiles().first();
            restoreWorkSheet(filename, true);
        }
    }
}

bool Workspace::restoreWorkSheet( const QString &fileName, bool switchToTab)
{
  // extract filename without path
  QString baseName = fileName.right( fileName.length() - fileName.lastIndexOf( QLatin1Char('/') ) - 1 );

  for( WorkSheet *sheet : mSheetList ) {
      if(sheet->fileName() == baseName)
          return false; //Don't add the same sheet twice
  }

  WorkSheet *sheet = new WorkSheet( nullptr );
  sheet->setFileName( baseName );
  if ( !sheet->load( fileName ) ) {
    delete sheet;
    return false;
  }
  mSheetList.append( sheet );

  connect( sheet, &WorkSheet::titleChanged,
    this, &Workspace::updateSheetTitle);

  insertTab(-1, sheet, sheet->translatedTitle().replace(QStringLiteral("&"), QStringLiteral("&&")) );
  if(switchToTab)
   setCurrentIndex(indexOf(sheet));

  //Watch the file incase it is deleted
  mDirWatch.addFile(fileName);

  return true;
}

void Workspace::cut()
{
  WorkSheet *current = currentWorkSheet();

  if ( current )
    current->cut();
}

void Workspace::copy()
{
  WorkSheet *current = currentWorkSheet();

  if ( current )
    current->copy();
}

void Workspace::paste()
{
  WorkSheet *current = currentWorkSheet();

  if ( current )
    current->paste();
}

void Workspace::configure()
{
  WorkSheet *current = currentWorkSheet();

  if ( current )
      current->settings();
}

void Workspace::applyStyle()
{
  WorkSheet *current = currentWorkSheet();
  if ( current )
    current->applyStyle();
}


