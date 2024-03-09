/*
    KSysGuard, the KDE System Guard

    SPDX-FileCopyrightText: 1999, 2000 Chris Schlaeger <cs@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

*/

#ifndef KSG_SENSORSHELLAGENT_H
#define KSG_SENSORSHELLAGENT_H

#include <QObject>
#include <QPointer>
#include <QProcess>

#include "SensorAgent.h"

class QString;

class KProcess;

namespace KSGRD
{
class SensorManager;

/**
  The SensorShellAgent starts a ksysguardd process and handles the
  asynchronous communication. It keeps a list of pending requests
  that have not been answered yet by ksysguard. The current
  implementation only allows one pending requests. Incoming requests
  are queued in an input FIFO.
 */
class SensorShellAgent : public SensorAgent
{
    Q_OBJECT

public:
    explicit SensorShellAgent(SensorManager *sm);
    ~SensorShellAgent() override;

    bool start(const QString &host, const QString &shell, const QString &command = QLatin1String(""), int port = -1) override;

    void hostInfo(QString &shell, QString &command, int &port) const override;

private Q_SLOTS:
    void msgRcvd();
    void errMsgRcvd();
    void daemonExited(int exitCode, QProcess::ExitStatus exitStatus);
    void daemonError(QProcess::ProcessError errorStatus);

private:
    bool writeMsg(const char *msg, int len) override;
    int mRetryCount;
    QPointer<KProcess> mDaemon;
    QString mShell;
    QString mCommand;
};

}

#endif
