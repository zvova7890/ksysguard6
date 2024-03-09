/*
    KSysGuard, the KDE System Guard

    SPDX-FileCopyrightText: 1999, 2000 Chris Schlaeger <cs@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

*/

#ifndef KSG_SENSORMANAGER_H
#define KSG_SENSORMANAGER_H

#include <kconfig.h>

#include <QEvent>
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QStringList>

#include "SensorAgent.h"

namespace KSGRD
{
class SensorManagerIterator;

/**
  The SensorManager handles all interaction with the connected
  hosts. Connections to a specific hosts are handled by
  SensorAgents. Use engage() to establish a connection and
  disengage() to terminate the connection.
 */
class Q_DECL_EXPORT SensorManager : public QObject
{
    Q_OBJECT

    friend class SensorManagerIterator;

public:
    class Q_DECL_EXPORT MessageEvent : public QEvent
    {
    public:
        MessageEvent(const QString &message);

        QString message() const;

    private:
        QString mMessage;
    };

    explicit SensorManager(QObject *parent = nullptr);
    ~SensorManager() override;

    /*! Number of hosts connected to */
    int count() const;

    bool engage(const QString &hostName, const QString &shell = QStringLiteral("ssh"), const QString &command = QLatin1String(""), int port = -1);
    /* Returns true if we are connected or trying to connect to the host given
     */
    bool isConnected(const QString &hostName);
    bool disengage(SensorAgent *agent);
    bool disengage(const QString &hostName);
    bool resynchronize(const QString &hostName);
    void notify(const QString &msg) const;

    void setBroadcaster(QObject *wdg);

    bool sendRequest(const QString &hostName, const QString &request, SensorClient *client, int id = 0);

    const QString hostName(const SensorAgent *sensor) const;
    bool hostInfo(const QString &host, QString &shell, QString &command, int &port);

    QString translateUnit(const QString &unit) const;
    QString translateSensorPath(const QString &path) const;
    QString translateSensorType(const QString &type) const;
    QString translateSensor(const QString &u) const;

    void readProperties(const KConfigGroup &cfg);
    void saveProperties(KConfigGroup &cfg);

    void disconnectClient(SensorClient *client);
    /** Call to retranslate all the strings - for example if the language has changed */
    void retranslate();

public Q_SLOTS:
    void reconfigure(const SensorAgent *agent);

Q_SIGNALS:
    void update();
    void hostAdded(KSGRD::SensorAgent *sensorAgent, const QString &hostName);
    void hostConnectionLost(const QString &hostName);

protected:
    QHash<QString, SensorAgent *> mAgents;

private:
    /**
      These dictionary stores the localized versions of the sensor
      descriptions and units.
     */
    QHash<QString, QString> mDescriptions;
    QHash<QString, QString> mUnits;
    QHash<QString, QString> mDict;
    QHash<QString, QString> mTypes;

    /** Store the data from the config file to pass to the MostConnector dialog box*/
    QStringList mHostList;
    QStringList mCommandList;

    QPointer<QObject> mBroadcaster;
};

Q_DECL_EXPORT extern SensorManager *SensorMgr;

class Q_DECL_EXPORT SensorManagerIterator : public QHashIterator<QString, SensorAgent *>
{
public:
    explicit SensorManagerIterator(const SensorManager *sm)
        : QHashIterator<QString, SensorAgent *>(sm->mAgents)
    {
    }

    ~SensorManagerIterator()
    {
    }
};

}

#endif
