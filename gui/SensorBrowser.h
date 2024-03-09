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

#ifndef KSG_SENSORBROWSER_H
#define KSG_SENSORBROWSER_H

#include <QMouseEvent>
#include <QTreeWidget>
#include <QMap>
#include <QHash>
#include "../ksgrd/SensorClient.h"
#include <QSortFilterProxyModel>


namespace KSGRD {
class SensorManager;
class SensorAgent;
}

class SensorInfo;
class HostInfo;

class SensorBrowserModel : public QAbstractItemModel, private KSGRD::SensorClient
{
  Q_OBJECT
  public:
    SensorBrowserModel();
    ~SensorBrowserModel() override;
    int columnCount( const QModelIndex &) const override;
    QVariant data( const QModelIndex & parent, int role) const override;
    QVariant headerData ( int section, Qt::Orientation orientation, int role) const override;
    QModelIndex index ( int row, int column, const QModelIndex & parent) const override;
    QModelIndex parent ( const QModelIndex & index ) const override;
    int rowCount ( const QModelIndex & parent = QModelIndex() ) const override;


    QStringList listSensors( const QString &hostName ) const;  ///Returns a list of sensors names.  E.g. (cpu/0, mem/free, mem/cache, etc)
    QStringList listSensors( int parentId ) const; ///Used recursively by listSensor(QString)
    QStringList listHosts( ) const; ///Returns a list of host names.  E.g. (localhost, 192.168.0.1,...)
    SensorInfo *getSensorInfo(QModelIndex index) const;
    HostInfo *getHostInfo(int hostId) const { return mHostInfoMap.value(hostId);}
    bool hasSensor(int hostId, const QString &sensor) const { return mHostSensorsMap.value(hostId).contains(sensor);} 
    int makeTreeBranch(int parentId, const QString &name);
    int makeSensor(HostInfo *hostInfo, int parentId, const QString &sensorName, const QString &name, const QString &sensorType);
    void removeSensor(HostInfo *hostInfo, int parentId, const QString &sensorName);
    void addHost(KSGRD::SensorAgent *sensorAgent, const QString &hostName);
    void clear();
    void disconnectHost(uint id);
    void disconnectHost(const HostInfo *hostInfo);
    void disconnectHost(const QString &hostname);
    Qt::ItemFlags flags ( const QModelIndex & index ) const override;
    QMimeData * mimeData ( const QModelIndexList & indexes ) const override;
    void retranslate();  /// Retranslate the model
  Q_SIGNALS:
    void sensorsAddedToHost(const QModelIndex &index );
  public Q_SLOTS:
    void update();
    void hostAdded(KSGRD::SensorAgent *sensorAgent, const QString &hostName);
    /**
     * Remove host from this model. The proper way this is called is with the signal in SensorManager
     * (from disengage), otherwise
     * if this is called directly the SensorManager container will be out of sync with ours.  Calling disconnectHost
     * from this object will also call disengage.
     */
    void hostRemoved(const QString &hostName);

  private:
    void answerReceived( int id, const QList<QByteArray>& ) override;
    void removeEmptyParentTreeBranches(int hostId, int id, int parentid);
    HostInfo* findHostInfoByHostName(const QString &hostName) const;
    void removeAllSensorUnderBranch(HostInfo* hostInfo, int parentId);

    int mIdCount; ///The lowest id that has not been used yet
    QMap<int, HostInfo*> mHostInfoMap; ///So each host has a number
    QHash<int, QList<int> > mTreeMap;   ///This describes the structure of the tree. It maps a parent branch number (which can be equal to the hostinfo number if it's a host branch) to a list of children.  The children themselves either have branches in the map, or else just relate to a sensor info
    QHash<int, int > mParentsTreeMap; ///
    QHash<int, QString> mTreeNodeNames; ///Maps the mTreeMap node id's to (translated) names
    QHash<int, QHash<QString,bool> > mHostSensorsMap; ///Maps a host id to a hash of sensor names.  Let's us quickly check if a sensor is registered for a given host. bool is just ignored
    QHash<int, SensorInfo *> mSensorInfoMap; ///Each sensor has a unique number as well.  This relates to the ID in mTreeMap
};

class SensorBrowserTreeWidget : public QTreeView
{
  Q_OBJECT

  public:
    SensorBrowserTreeWidget( QWidget* parent, KSGRD::SensorManager* sm );
    ~SensorBrowserTreeWidget() override;

    QStringList listHosts() const 
      { return mSensorBrowserModel.listHosts(); }
    QStringList listSensors( const QString &hostName ) const 
      { return mSensorBrowserModel.listSensors(hostName); }
    QSortFilterProxyModel & model()
      { return mSortFilterProxyModel; }

  public Q_SLOTS:
    void disconnect();
    void hostReconfigured( const QString &hostName );
  protected Q_SLOTS:
    void expandItem(const QModelIndex& model_index);
    void updateView();
  private:
    void retranslateUi();
    void changeEvent( QEvent * event ) override;

    KSGRD::SensorManager* mSensorManager;

    QString mDragText;
    SensorBrowserModel mSensorBrowserModel;
    QSortFilterProxyModel mSortFilterProxyModel;
};

/**
 * The SensorBrowserWidget is the graphical front-end of the SensorManager. It
 * displays the currently available hosts and their sensors.
 */
class SensorBrowserWidget : public QWidget
{
    Q_OBJECT
    public:
      explicit SensorBrowserWidget( QWidget* parent, KSGRD::SensorManager* sm );
      ~SensorBrowserWidget() override;
      QStringList listHosts() const 
      { return m_treeWidget->listHosts(); }
      QStringList listSensors( const QString &hostName ) const 
      { return m_treeWidget->listSensors(hostName); }

    private:
      SensorBrowserTreeWidget *m_treeWidget;

};

class SensorInfo
{
  public:
    SensorInfo( HostInfo *hostInfo, const QString &name,
                const QString &description, const QString &type );

    /**
      Returns the name of the sensor.  e.g. "cpu/free".  Not translated.
     */
    QString name() const;

    /**
      Returns the description of the sensor.  e.g. "free"
     */
    QString description() const;

    /**
      Returns the type of the sensor. e.g. "Integer"
     */
    QString type() const;

    /**
      Returns the host that this sensor is on. 
     */
    HostInfo *hostInfo() const;

  private:
    QString mName;
    QString mDesc;
    QString mType;
    HostInfo *mHostInfo;
};

class HostInfo
{
  public:
    HostInfo( int id, KSGRD::SensorAgent *agent, const QString &name) : mId(id), mSensorAgent(agent), mHostName(name) {}

    /**
      Returns the unique id of the host.
     */
    int id() const {return mId;}

    /**
      Returns a pointer to the sensor agent of the host.
     */
    KSGRD::SensorAgent* sensorAgent() const {return mSensorAgent;}

    /**
      Returns the name of the host.
     */
    QString hostName() const { return mHostName;}

  private:
    int mId;
    KSGRD::SensorAgent* mSensorAgent;
    const QString mHostName;
};

#endif
