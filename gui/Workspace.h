/*
    KSysGuard, the KDE System Guard

    Copyright (c) 1999, 2000 Chris Schlaeger <cs@kde.org>

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

#ifndef KSG_WORKSPACE_H
#define KSG_WORKSPACE_H

#include <KDirWatch>

#include <QTabWidget>

class KConfig;
class QString;
class WorkSheet;

class Workspace : public QTabWidget
{
  Q_OBJECT

  public:
    explicit Workspace( QWidget* parent);
    ~Workspace() override;

    void saveProperties( KConfigGroup& cfg );
    void readProperties( const KConfigGroup& cfg );

    bool saveOnQuit();

    bool restoreWorkSheet( const QString &fileName, bool switchToTab = true);
    QList<WorkSheet *> getWorkSheets() const { return mSheetList; }
    WorkSheet *currentWorkSheet();

  public Q_SLOTS:
    void newWorkSheet();
    void importWorkSheet();
    void importWorkSheet( const QUrl& );
    bool saveWorkSheet( WorkSheet *sheet );
    void exportWorkSheet();
    void exportWorkSheet( WorkSheet *sheet );
    void removeWorkSheet();
    void removeWorkSheet( const QString &fileName );
    void removeAllWorkSheets();
    void tabMoved(int from, int to);
    void getHotNewWorksheet();
    void uploadHotNewWorksheet();
    void cut();
    void copy();
    void paste();
    void configure();
    void updateSheetTitle( QWidget* );
    void applyStyle();
    void refreshActiveWorksheet();

  private Q_SLOTS:
    virtual void contextMenu (int, const QPoint &);

  Q_SIGNALS:
    void setCaption( const QString &text);

  private:
    QList<WorkSheet *> mSheetList;

    QString makeNameForNewSheet() const;
    KDirWatch mDirWatch;
};

#endif
