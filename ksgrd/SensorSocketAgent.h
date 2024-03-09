/*
    KSysGuard, the KDE System Guard

    SPDX-FileCopyrightText: 1999, 2000 Chris Schlaeger <cs@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

*/

#ifndef KSG_SENSORSOCKETAGENT_H
#define KSG_SENSORSOCKETAGENT_H

#include <QTcpSocket>

#include "SensorAgent.h"

class QString;

namespace KSGRD
{
/**
  The SensorSocketAgent connects to a ksysguardd via a TCP
  connection. It keeps a list of pending requests that have not been
  answered yet by ksysguard. The current implementation only allows
  one pending requests. Incoming requests are queued in an input
  FIFO.
 */
class SensorSocketAgent : public SensorAgent
{
    Q_OBJECT

public:
    explicit SensorSocketAgent(SensorManager *sm);
    ~SensorSocketAgent() override;

    bool start(const QString &host, const QString &shell, const QString &command = QLatin1String(""), int port = -1) override;

    void hostInfo(QString &shell, QString &command, int &port) const override;

private Q_SLOTS:
    void connectionClosed();
    void msgSent();
    void msgRcvd();
    void error(QAbstractSocket::SocketError);

private:
    bool writeMsg(const char *msg, int len) override;

    QTcpSocket mSocket;
    int mPort;
};

}

#endif
