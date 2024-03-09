/*
    KSysGuard, the KDE System Guard

    SPDX-FileCopyrightText: 1999-2001 Chris Schlaeger <cs@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

*/

#include <QDebug>
#include <klocalizedstring.h>
#include <kprocess.h>
#include <kshell.h>

#include "SensorManager.h"
#include "ksgrd_debug.h"

#include "SensorShellAgent.h"

using namespace KSGRD;

SensorShellAgent::SensorShellAgent(SensorManager *sm)
    : SensorAgent(sm)
    , mDaemon(nullptr)
{
}

SensorShellAgent::~SensorShellAgent()
{
    if (mDaemon) {
        mDaemon->write("quit\n", sizeof("quit\n") - 1);
        mDaemon->disconnect();
        mDaemon->waitForFinished();
        delete mDaemon;
        mDaemon = nullptr;
    }
}

bool SensorShellAgent::start(const QString &host, const QString &shell, const QString &command, int)
{
    mDaemon = new KProcess();
    mDaemon->setOutputChannelMode(KProcess::SeparateChannels);
    mRetryCount = 3;
    setHostName(host);
    mShell = shell;
    mCommand = command;

    connect(mDaemon.data(), &QProcess::errorOccurred, this, &SensorShellAgent::daemonError);
    connect(mDaemon.data(), &QProcess::finished, this, &SensorShellAgent::daemonExited);
    connect(mDaemon.data(), &QProcess::readyReadStandardOutput, this, &SensorShellAgent::msgRcvd);
    connect(mDaemon.data(), &QProcess::readyReadStandardError, this, &SensorShellAgent::errMsgRcvd);

    if (!command.isEmpty()) {
        *mDaemon << KShell::splitArgs(command);
    } else
        *mDaemon << mShell << hostName() << QStringLiteral("ksysguardd");
    mDaemon->start();

    return true;
}

void SensorShellAgent::hostInfo(QString &shell, QString &command, int &port) const
{
    shell = mShell;
    command = mCommand;
    port = -1;
}

void SensorShellAgent::msgRcvd()
{
    QByteArray buffer = mDaemon->readAllStandardOutput();
    mRetryCount = 3; // we received an answer, so reset our retry count back to 3
    processAnswer(buffer.constData(), buffer.size());
}

void SensorShellAgent::errMsgRcvd()
{
    const QByteArray buffer = mDaemon->readAllStandardOutput();

    // Because we read the error buffer in chunks, we may not have a proper utf8 string.
    // We should never get input over stderr anyway, so no need to worry too much about it.
    // But if this is extended, we will need to handle this better
    const QString buf = QString::fromUtf8(buffer);

    qCDebug(LIBKSYSGUARD_KSGRD) << "SensorShellAgent: Warning, received text over stderr!"
                                << "\n"
                                << buf;
}

void SensorShellAgent::daemonExited(int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitCode);
    qCDebug(LIBKSYSGUARD_KSGRD) << "daemon exited, exit status " << exitStatus;
    if (mRetryCount-- <= 0 || (mDaemon->start(), !mDaemon->waitForStarted())) {
        setDaemonOnLine(false);
        if (sensorManager()) {
            sensorManager()->disengage(this); // delete ourselves
        }
    }
}

void SensorShellAgent::daemonError(QProcess::ProcessError errorStatus)
{
    QString error;
    switch (errorStatus) {
    case QProcess::FailedToStart:
        qCDebug(LIBKSYSGUARD_KSGRD) << "failed to run" << mDaemon->program().join(QLatin1Char(' '));
        error = i18n("Could not run daemon program '%1'.", mDaemon->program().join(" "));
        break;
    case QProcess::Crashed:
    case QProcess::Timedout:
    case QProcess::WriteError:
    case QProcess::ReadError:
    default:
        error = i18n("The daemon program '%1' failed.", mDaemon->program().join(" "));
    }
    setReasonForOffline(error);
    qCDebug(LIBKSYSGUARD_KSGRD) << "Error received " << error << "(" << errorStatus << ")";
    setDaemonOnLine(false);
    if (sensorManager())
        sensorManager()->disengage(this); // delete ourselves
}
bool SensorShellAgent::writeMsg(const char *msg, int len)
{
    // write returns -1 on error, in which case we should return false.  true otherwise.
    return mDaemon->write(msg, len) != -1;
}
