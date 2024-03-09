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

#include <QDebug>
#include <QIcon>
#include <QLineEdit>
#include <QMimeData>
#include <QVBoxLayout>

#include <KLocalizedString>
#include <KMessageBox>
#include "../ksgrd/SensorManager.h"

#include "SensorBrowser.h"
//#define SENSOR_MODEL_DO_TEST
//uncomment the above to test the model
#ifdef SENSOR_MODEL_DO_TEST
#include "modeltest.h"
#endif

SensorBrowserModel::SensorBrowserModel()
{
#ifdef SENSOR_MODEL_DO_TEST
    new ModelTest(this);
#endif
    mIdCount=1;
}
SensorBrowserModel::~SensorBrowserModel()
{
    qDeleteAll( mHostInfoMap );
    mHostInfoMap.clear();
    qDeleteAll( mSensorInfoMap );
    mSensorInfoMap.clear();
}

int SensorBrowserModel::columnCount( const QModelIndex &) const { //virtual
    return 1;
}

QVariant SensorBrowserModel::data( const QModelIndex & index, int role) const { //virtual
    if(!index.isValid()) return QVariant();
    switch(role) {
        case Qt::DisplayRole: {
            if(index.column()==0) {
                uint id = index.internalId();
                if(mSensorInfoMap.contains(id)) {
                    Q_ASSERT(mSensorInfoMap.value(id));
                    SensorInfo *sensorInfo = mSensorInfoMap.value(id);
                    return QString(sensorInfo->description() + QStringLiteral(" (") + KSGRD::SensorMgr->translateSensorType(sensorInfo->type()) + QLatin1Char(')') );
                }
                if(mTreeNodeNames.contains(id)) return mTreeNodeNames.value(id);
                if(mHostInfoMap.contains(id)) {
                    Q_ASSERT(mHostInfoMap.value(id));
                    return mHostInfoMap.value(id)->hostName();
                }
            }
            return QString();
        }
        case Qt::DecorationRole: {
            if(index.column() == 0) {
                HostInfo *host = getHostInfo(index.internalId());
                KSGRD::SensorAgent *agent;
                if(host != nullptr && (agent = host->sensorAgent())) {
                    if(agent->daemonOnLine())
                        return QIcon::fromTheme(QStringLiteral("computer"));
                    else
                        return QIcon::fromTheme(QStringLiteral("dialog-warning"));
                } else
                    return QIcon();
            } else
                return QIcon();
            break;
        }
        case Qt::ToolTipRole: {
            if(index.column() == 0) {
                HostInfo *host = getHostInfo(index.internalId());
                KSGRD::SensorAgent *agent;
                if(host != nullptr && (agent = host->sensorAgent())) {
                    if(agent->daemonOnLine())
                        return agent->hostName();
                    else
                        return agent->reasonForOffline();
                }
            }
            break;
        }

    } //switch
    return QVariant();
}

QVariant SensorBrowserModel::headerData ( int section, Qt::Orientation , int role) const { //virtual
    if(role != Qt::DisplayRole) return QVariant();
    if(section==0) return i18n("Sensor Browser");
    return QVariant();
}

void  SensorBrowserModel::retranslate() {
    Q_EMIT headerDataChanged(Qt::Horizontal, 0,0);
}

QModelIndex SensorBrowserModel::index ( int row, int column, const QModelIndex & parent) const { //virtual
    if(column != 0) return QModelIndex();
    QList<int> ids;
    if(!parent.isValid()) {
        ids = mHostInfoMap.keys();
    }
    else {
        ids = mTreeMap.value(parent.internalId());
    }
    if( row >= ids.size() || row< 0) {
        return QModelIndex();
    }
    QModelIndex index = createIndex(row, column, ids[row]);
    Q_ASSERT(index.isValid());
    return index;
}

QStringList SensorBrowserModel::listHosts() const
{
    QStringList hostList;

    QMapIterator<int, HostInfo*> it( mHostInfoMap );
    while ( it.hasNext() ) {
        it.next();
        Q_ASSERT(it.value());
        hostList.append( it.value()->hostName() );
    }

    return hostList;
}

QStringList SensorBrowserModel::listSensors( const QString &hostName ) const
{
    QMapIterator<int, HostInfo*> it( mHostInfoMap );
    while ( it.hasNext() ) {
        it.next();
        Q_ASSERT(it.value());
        if ( it.value()->hostName() == hostName ) {
            Q_ASSERT(mSensorInfoMap.contains(it.key()));
            return listSensors( it.key() );
        }
    }
    return QStringList();
}

QStringList SensorBrowserModel::listSensors( int parentId) const
{
    SensorInfo *sensor=mSensorInfoMap.value(parentId);
    if(sensor) return QStringList(sensor->name());

    QStringList childSensors;
    QList<int> children = mTreeMap.value(parentId);
    for(int i=0; i < children.size(); i++) {
        childSensors+= listSensors(children[i]);
    }
    return childSensors;
}
SensorInfo *SensorBrowserModel::getSensorInfo(QModelIndex index) const
{
    if(!index.isValid()) return nullptr;
    return mSensorInfoMap.value(index.internalId());
}
int SensorBrowserModel::makeSensor(HostInfo *hostInfo, int parentId, const QString &sensorName, const QString &name, const QString &sensorType) {
    //sensorName is the full version.  e.g.  mem/free
    //name is the short version. e.g. free
    //sensortype is e.g. Integer
    QList<int> children = mTreeMap.value(parentId);
    for(int i=0; i<children.size(); i++)
        if(mSensorInfoMap.contains(children[i])) {
            Q_ASSERT(mSensorInfoMap.value(children[i]));
            if(mSensorInfoMap.value(children[i])->name() == sensorName)
                return children[i];
        }

    QModelIndex parentModelIndex;
    if(hostInfo->id() == parentId) {
        parentModelIndex = createIndex(mHostInfoMap.keys().indexOf(parentId), 0 , parentId);
    } else {
        int parentsParentId = mParentsTreeMap.value(parentId);
        parentModelIndex = createIndex(mTreeMap.value(parentsParentId).indexOf(parentId), 0, parentId);
    }
    Q_ASSERT(parentModelIndex.isValid());
    QList<int> &parentTreemap = mTreeMap[parentId];
    SensorInfo *sensorInfo = new SensorInfo(hostInfo, sensorName, name, sensorType);
    beginInsertRows( parentModelIndex , parentTreemap.size(), parentTreemap.size() );
    parentTreemap << mIdCount;
    mParentsTreeMap.insert( mIdCount, parentId );
    mSensorInfoMap.insert(mIdCount, sensorInfo);
    mHostSensorsMap[hostInfo->id()].insert(sensorName, true);
    mIdCount++;
    endInsertRows();
    return mIdCount-1;  //NOTE mIdCount is next available number. Se we use it, then increment it, but return the number of the one that we use
}

void SensorBrowserModel::removeSensor(HostInfo *hostInfo, int parentId, const QString &sensorName) {
    //sensorName is the full version.  e.g.  mem/free
    QList<int> children = mTreeMap.value(parentId);
    int idCount = -1;
    int index;
    for(index=0; index<children.size(); index++)
        if(mSensorInfoMap.contains(children[index])) {
            Q_ASSERT(mSensorInfoMap.value(children[index]));
            if(mSensorInfoMap.value(children[index])->name() == sensorName) {
                idCount = children[index];
                break;
            }
        }
    if(idCount == -1) {
        qDebug() << "removeSensor called for sensor that doesn't exist in the tree: " << sensorName ;
        return;
    }
    QModelIndex parentModelIndex;
    int parentsParentId = -1;
    if(hostInfo->id() == parentId) {
        parentModelIndex = createIndex(mHostInfoMap.keys().indexOf(parentId), 0 , parentId);
    } else {
        parentsParentId = mParentsTreeMap.value(parentId);
        parentModelIndex = createIndex(mTreeMap.value(parentsParentId).indexOf(parentId), 0, parentId);
    }
    Q_ASSERT(parentModelIndex.isValid());
    QList<int> &parentTreemap = mTreeMap[parentId];
    beginRemoveRows( parentModelIndex, index, index );
    parentTreemap.removeAll(idCount);
    mParentsTreeMap.remove(idCount);
    SensorInfo *sensorInfo = mSensorInfoMap.take(idCount);
    delete sensorInfo;
    mHostSensorsMap[hostInfo->id()].remove(sensorName);
    endRemoveRows();

    if(parentsParentId != -1)
        removeEmptyParentTreeBranches(hostInfo->id(), parentId, parentsParentId);
}
void SensorBrowserModel::removeEmptyParentTreeBranches(int hostId, int id, int parentId) {
    if(hostId == id)
        return;  //We don't want to remove hosts

    if(!mTreeMap.value(id).isEmpty()) return; // We should have no children

    QModelIndex parentModelIndex;
    int parentsParentId = -1;
    if(hostId == parentId) {
        parentModelIndex = createIndex(mHostInfoMap.keys().indexOf(parentId), 0 , parentId);
    } else {
        parentsParentId = mParentsTreeMap.value(parentId);
        parentModelIndex = createIndex(mTreeMap.value(parentsParentId).indexOf(parentId), 0, parentId);
    }

    int index = mTreeMap.value(parentId).indexOf(id);
    int idCount = mTreeMap.value(parentId).at(index);

    QList<int> &parentTreemap = mTreeMap[parentId];
    beginRemoveRows( parentModelIndex, index, index );
    parentTreemap.removeAll(idCount);
    mParentsTreeMap.remove(idCount);
    mTreeMap.remove(idCount);
    mTreeNodeNames.remove(idCount);
    endRemoveRows();

    if(parentsParentId != -1)
        removeEmptyParentTreeBranches(hostId, parentId, parentsParentId);
}
int SensorBrowserModel::makeTreeBranch(int parentId, const QString &name) {
    QList<int> children = mTreeMap.value(parentId);
    for(int i=0; i<children.size(); i++)
        if(mTreeNodeNames.value(children[i]) == name) return children[i];

    QModelIndex parentModelIndex;
    if(mHostInfoMap.contains(parentId)) {
        parentModelIndex = createIndex(mHostInfoMap.keys().indexOf(parentId), 0 , parentId);
    } else {
        int parentsParentId = mParentsTreeMap.value(parentId);
        parentModelIndex = createIndex(mTreeMap.value(parentsParentId).indexOf(parentId), 0, parentId);
    }
    Q_ASSERT(parentModelIndex.isValid());
    QList<int> &parentTreemap = mTreeMap[parentId];
    beginInsertRows( parentModelIndex , parentTreemap.size(), parentTreemap.size() );
    parentTreemap << mIdCount;
    mParentsTreeMap.insert( mIdCount, parentId );
    mTreeMap[mIdCount];  //create with empty qlist
    mTreeNodeNames.insert(mIdCount, name);
    mIdCount++;
    endInsertRows();

    return mIdCount-1;
}

void SensorBrowserModel::answerReceived( int hostId,  const QList<QByteArray>&answer )
{
    /* An answer has the following example format:

       cpu/system/idle integer
       cpu/system/sys  integer
       cpu/system/nice integer
       cpu/system/user integer
       ps       table
       */
    HostInfo *hostInfo = getHostInfo(hostId);
    if(!hostInfo) {
        qDebug() << "Invalid hostId " << hostId ;
        return;
    }
    /* We keep a copy of the previous sensor names so that we can detect what sensors have been removed */
    QHash<QString,bool> oldSensorNames = mHostSensorsMap.value(hostId);
    for ( int i = 0; i < answer.count(); ++i ) {
        if ( answer[ i ].isEmpty() )
            continue;

        QList<QByteArray> words = answer[ i ].split('\t');
        if(words.size() != 2) {
            qDebug() << "Invalid data " << answer[i];
            continue;  /* Something wrong with this line of data */
        }
        QString sensorName = QString::fromUtf8(words[ 0 ]);
        QString sensorType = QString::fromUtf8(words[ 1 ]);
        oldSensorNames.remove(sensorName);  /* This sensor has not been removed */
        if ( hasSensor(hostId, sensorName)) {
            continue;
        }
        if(sensorName.isEmpty()) continue;

        if(sensorType == QLatin1String("string")) continue;

        /* The sensor browser can display sensors in a hierarchical order.
         * Sensors can be grouped into nodes by separating the hierarchical
         * nodes through slashes in the sensor name. E. g. cpu/system/user is
         * the sensor user in the cpu node. There is no limit for the
         * depth of nodes. */
        int currentNodeId = hostId;  //Start from the host branch and work our way down the tree
        QStringList absolutePath = sensorName.split( QLatin1Char('/') );
        for ( int j = 0; j < absolutePath.count()-1; ++j ) {
            // Localize the sensor name part by part.
            QString name = KSGRD::SensorMgr->translateSensorPath( absolutePath[ j ] );
            currentNodeId = makeTreeBranch(currentNodeId, name);
        }
        QString name = KSGRD::SensorMgr->translateSensorPath( absolutePath[ absolutePath.size()-1] );
        makeSensor(hostInfo, currentNodeId, sensorName, name, sensorType);
    }
    /* Now we have to remove sensors that were not found */
    QHashIterator<QString, bool> it( oldSensorNames );
    while ( it.hasNext() ) {
        it.next();

        int currentNodeId = hostId;  //Start from the host branch and work our way down the tree
        QStringList absolutePath = it.key().split( QLatin1Char('/') );
        for ( int j = 0; j < absolutePath.count()-1; ++j ) {
            // Localize the sensor name part by part.
            QString name = KSGRD::SensorMgr->translateSensorPath( absolutePath[ j ] );
            currentNodeId = makeTreeBranch(currentNodeId, name);
        }
        removeSensor(hostInfo, currentNodeId, it.key());
    }
    Q_EMIT sensorsAddedToHost( createIndex( mHostInfoMap.keys().indexOf(hostId), 0, hostId ) );
}

//virtual
QModelIndex SensorBrowserModel::parent ( const QModelIndex & index ) const {
    if(!index.isValid() || index.column() != 0)
        return QModelIndex();
    if(mHostInfoMap.contains(index.internalId())) return QModelIndex();
    if(!mParentsTreeMap.contains(index.internalId())) {
        qDebug() << "Something is wrong with the model.  Doesn't contain " << index.internalId();
        return QModelIndex();
    }
    int parentId = mParentsTreeMap.value(index.internalId());

    QModelIndex parentModelIndex;
    if(mHostInfoMap.contains(parentId)) {
        parentModelIndex = createIndex(mHostInfoMap.keys().indexOf(parentId), 0 , parentId);
    } else {
        int parentsParentId = mParentsTreeMap.value(parentId);
        parentModelIndex = createIndex(mTreeMap.value(parentsParentId).indexOf(parentId), 0, parentId);
    }
    Q_ASSERT(parentModelIndex.isValid());
    return parentModelIndex;
}
//virtual
int SensorBrowserModel::rowCount ( const QModelIndex & parent ) const {
    if(!parent.isValid()) return mHostInfoMap.size();
    if(parent.column() != 0) return 0;
    return mTreeMap.value(parent.internalId()).size();
}
//virtual
Qt::ItemFlags SensorBrowserModel::flags ( const QModelIndex & index ) const {
    if(!index.isValid()) return Qt::NoItemFlags;
    if(mSensorInfoMap.contains(index.internalId())) return Qt::ItemIsDragEnabled | Qt::ItemIsSelectable | Qt::ItemIsEnabled;
    else return Qt::ItemIsEnabled;
}

SensorBrowserWidget::SensorBrowserWidget( QWidget* parent, KSGRD::SensorManager* sm ) : QWidget( parent )
{
    QVBoxLayout *layout = new QVBoxLayout;
    m_treeWidget = new SensorBrowserTreeWidget(this, sm);
    QLineEdit * search_line = new QLineEdit(this);
    connect(search_line, &QLineEdit::textChanged, this, [this](const QString &text) {
        m_treeWidget->model().setFilterFixedString(text);
    });
    layout->addWidget(search_line);
    layout->addWidget(m_treeWidget);
    setLayout(layout);
}
SensorBrowserWidget::~SensorBrowserWidget()
{
}
SensorBrowserTreeWidget::SensorBrowserTreeWidget( QWidget* parent, KSGRD::SensorManager* sm ) : QTreeView( parent ), mSensorManager( sm )
{
    mSortFilterProxyModel.setSourceModel(&mSensorBrowserModel);
    mSortFilterProxyModel.setRecursiveFilteringEnabled(true);
    mSortFilterProxyModel.setFilterCaseSensitivity(Qt::CaseInsensitive);
    setModel(&mSortFilterProxyModel);
    connect(mSensorManager, &KSGRD::SensorManager::update, &mSensorBrowserModel, &SensorBrowserModel::update);
    connect(mSensorManager, &KSGRD::SensorManager::hostAdded, &mSensorBrowserModel, &SensorBrowserModel::hostAdded);
    connect(mSensorManager, &KSGRD::SensorManager::hostConnectionLost, &mSensorBrowserModel, &SensorBrowserModel::hostRemoved);
//  connect( mSensorManager, SIGNAL(hostAdded(KSGRD::SensorAgent*,QString)), SLOT(updateView()) );
//  connect( mSensorManager, SIGNAL(hostConnectionLost(QString)), SLOT(updateView()) );
    connect(&mSortFilterProxyModel, &QSortFilterProxyModel::rowsInserted, this, &SensorBrowserTreeWidget::updateView);

    setDragDropMode(QAbstractItemView::DragOnly);
    setUniformRowHeights(true);

    //setMinimumWidth( 1 );
    retranslateUi();
    connect(&mSensorBrowserModel, &SensorBrowserModel::sensorsAddedToHost, this, &SensorBrowserTreeWidget::expandItem);

    KSGRD::SensorManagerIterator it( mSensorManager );
    while ( it.hasNext() ) {
        KSGRD::SensorAgent* sensorAgent = it.next().value();
        QString hostName = mSensorManager->hostName( sensorAgent );
        mSensorBrowserModel.addHost(sensorAgent, hostName);
    }
    updateView();
}

SensorBrowserTreeWidget::~SensorBrowserTreeWidget()
{
}

void SensorBrowserTreeWidget::updateView()
{
    if(mSensorManager->count() == 1) {
        setRootIsDecorated( false );
        //expand the top level
        for(int i = 0; i < mSortFilterProxyModel.rowCount(); i++)
            expand(mSortFilterProxyModel.index(i,0));
    } else
        setRootIsDecorated( true );
}
void SensorBrowserTreeWidget::expandItem(const QModelIndex &model_index)
{
    expand(mSortFilterProxyModel.mapFromSource(model_index));
}
void SensorBrowserTreeWidget::retranslateUi() {

    this->setToolTip( i18n( "Drag sensors to empty cells of a worksheet "));
    this->setWhatsThis( i18n( "The sensor browser lists the connected hosts and the sensors "
                "that they provide. Click and drag sensors into drop zones "
                "of a worksheet. A display will appear "
                "that visualizes the "
                "values provided by the sensor. Some sensor displays can "
                "display values of multiple sensors. Simply drag other "
                "sensors on to the display to add more sensors." ) );
}

void SensorBrowserTreeWidget::changeEvent( QEvent * event )
{
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
        mSensorBrowserModel.retranslate();
        mSensorBrowserModel.update();
    }
    QWidget::changeEvent(event);
}

void SensorBrowserTreeWidget::disconnect()
{
    QModelIndexList indexlist = selectionModel()->selectedRows();
    for(int i=0; i < indexlist.size(); i++)
    {
        mSensorBrowserModel.disconnectHost(indexlist.value(i).internalId());
    }
}

void SensorBrowserTreeWidget::hostReconfigured( const QString& )
{
    // TODO: not yet implemented.
}

void SensorBrowserModel::clear() {
    qDeleteAll(mHostInfoMap);
    mHostInfoMap.clear();

}

void SensorBrowserModel::disconnectHost(uint id)
{
    disconnectHost(mHostInfoMap.value(id));
}
void SensorBrowserModel::disconnectHost(const HostInfo *hostInfo)
{
    KSGRD::SensorMgr->disengage( hostInfo->sensorAgent() );
}
void SensorBrowserModel::disconnectHost(const QString &hostname)
{
    HostInfo* toDelete = findHostInfoByHostName(hostname);
    if (toDelete != nullptr)
        disconnectHost(toDelete);
}
HostInfo* SensorBrowserModel::findHostInfoByHostName(const QString &hostName) const {
    HostInfo* toReturn = nullptr;
    QMapIterator<int, HostInfo*> it( mHostInfoMap );
    while (it.hasNext() && toReturn == nullptr) {
        it.next();
        if (it.value()->hostName() == hostName) {
            toReturn = it.value();
        }
    }
    return toReturn;
}
void SensorBrowserModel::hostAdded(KSGRD::SensorAgent *sensorAgent, const QString &hostName)  {
    addHost(sensorAgent,hostName);
    update();
}

void SensorBrowserModel::hostRemoved(const QString &hostName)  {
    HostInfo* toRemove = findHostInfoByHostName(hostName);
    if (toRemove != nullptr)  {
        beginResetModel();
        int hostId = toRemove->id();
        removeAllSensorUnderBranch(toRemove,hostId);
        removeEmptyParentTreeBranches(hostId,hostId,hostId);

        delete mHostInfoMap.take(hostId);
        mTreeMap.take(hostId);
        mHostSensorsMap.take(hostId);
        endResetModel();
    }
    update();
}

void SensorBrowserModel::removeAllSensorUnderBranch(HostInfo* hostInfo, int parentId)  {

    QList<int> children = mTreeMap.value(parentId);

    for (int i = 0; i < children.size(); i++) {

        if (mTreeMap.contains(children[i]))  {
            //well our children is not a sensor so remove what is under him
            removeAllSensorUnderBranch(hostInfo,children[i]);

        } else  {
            //well this should be a sensor so remove it
            if (mSensorInfoMap.contains(children[i])) {
                SensorInfo* sensorToRemove = mSensorInfoMap.value(children[i]);
                Q_ASSERT(sensorToRemove);
                removeSensor(hostInfo, parentId, sensorToRemove->name());
            }
        }
    }


}
void SensorBrowserModel::addHost(KSGRD::SensorAgent *sensorAgent, const QString &hostName)
{
    beginInsertRows( QModelIndex() , mHostInfoMap.size(), mHostInfoMap.size() );
    HostInfo* hostInfo = new HostInfo( mIdCount, sensorAgent, hostName);
    mHostInfoMap.insert(mIdCount, hostInfo);
    mTreeMap.insert(mIdCount, QList<int>());
    mHostSensorsMap.insert(mIdCount, QHash<QString, bool>());
    mIdCount++;
    endInsertRows();
    hostInfo->sensorAgent()->sendRequest( QStringLiteral("monitors"), this, mIdCount-1 );
}

void SensorBrowserModel::update()
{
    QMapIterator<int, HostInfo*> it( mHostInfoMap );
    while ( it.hasNext() ) {
        it.next();
        KSGRD::SensorAgent* sensorAgent = it.value()->sensorAgent();
        int id = it.key();
        sensorAgent->sendRequest( QStringLiteral("monitors"), this, id );
    }
}
QMimeData * SensorBrowserModel::mimeData ( const QModelIndexList & indexes ) const { //virtual
    QMimeData *mimeData = new QMimeData();
    if(indexes.size() != 1) return mimeData;
    SensorInfo *sensor = getSensorInfo(indexes[0]);
    if(!sensor) return mimeData;
    // Create text drag object as
    // "<hostname> <sensorname> <sensortype> <sensordescription>".
    // Only the description may contain blanks.
    Q_ASSERT(sensor);
    Q_ASSERT(sensor->hostInfo());
    QString mDragText = sensor->hostInfo()->hostName() + QLatin1Char(' ') +
        sensor->name() + QLatin1Char(' ') +
        sensor->type()+ QLatin1Char(' ') +
        sensor->description();


    mimeData->setData( QStringLiteral("application/x-ksysguard"), mDragText.toUtf8() );
    return mimeData;
}

SensorInfo::SensorInfo( HostInfo *hostInfo, const QString &name,
        const QString &desc, const QString &type )
  : mName( name ), mDesc( desc ), mType( type ), mHostInfo( hostInfo )
{
    Q_ASSERT(mHostInfo);
}

QString SensorInfo::name() const
{
    return mName;
}

QString SensorInfo::type() const
{
    return mType;
}

QString SensorInfo::description() const
{
    return mDesc;
}

HostInfo *SensorInfo::hostInfo() const
{
    return mHostInfo;
}




