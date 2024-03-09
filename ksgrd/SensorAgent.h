/*
    KSysGuard, the KDE System Guard

    SPDX-FileCopyrightText: 1999, 2000 Chris Schlaeger <cs@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

*/

#ifndef KSG_SENSORAGENT_H
#define KSG_SENSORAGENT_H

#include <QObject>
#include <QPointer>
#include <QQueue>
#include <QSet>

class QString;

namespace KSGRD
{
class SensorClient;
class SensorManager;
class SensorRequest;

/**
  The SensorAgent depending on the type of requested connection
  starts a ksysguardd process or connects through a tcp connection to
  a running ksysguardd and handles the asynchronous communication. It
  keeps a list of pending requests that have not been answered yet by
  ksysguardd. The current implementation only allows one pending
  requests. Incoming requests are queued in an input FIFO.
*/
class Q_DECL_EXPORT SensorAgent : public QObject
{
    Q_OBJECT

public:
    explicit SensorAgent(SensorManager *sm);
    ~SensorAgent() override;

    virtual bool start(const QString &host, const QString &shell, const QString &command = QLatin1String(""), int port = -1) = 0;

    /**
      This function should only be used by the SensorManager and
      never by the SensorClients directly since the pointer returned by
      engaged is not guaranteed to be valid. Only the SensorManager knows
      whether a SensorAgent pointer is still valid or not.

      This function sends out a command to the sensor and notifies the
      agent to return the answer to 'client'. The 'id' can be used by the
      client to identify the answer. It is only passed through and never
      used by the SensorAgent. So it can be any value the client suits to
      use.
     */
    void sendRequest(const QString &req, SensorClient *client, int id = 0);

    virtual void hostInfo(QString &sh, QString &cmd, int &port) const = 0;

    void disconnectClient(SensorClient *client);

    QString hostName() const;

    bool daemonOnLine() const;
    QString reasonForOffline() const;

Q_SIGNALS:
    void reconfigure(const SensorAgent *);

protected:
    void processAnswer(const char *buf, int buflen);
    void executeCommand();

    SensorManager *sensorManager();

    void setDaemonOnLine(bool value);

    void setHostName(const QString &hostName);
    void setReasonForOffline(const QString &reasonForOffline);

private:
    virtual bool writeMsg(const char *msg, int len) = 0;
    QString mReasonForOffline;

    QQueue<SensorRequest *> mInputFIFO;
    QQueue<SensorRequest *> mProcessingFIFO;
    QList<QByteArray> mAnswerBuffer; /// A single reply can be on multiple lines.
    QString mErrorBuffer;
    QByteArray mLeftOverBuffer; /// Any data read in but not terminated is copied into here, awaiting the next load of data

    QPointer<SensorManager> mSensorManager;

    bool mDaemonOnLine;
    QString mHostName;
    QSet<SensorRequest> mUnderwayRequests;
};

/**
  This auxiliary class is used to store requests during their processing.
*/
class SensorRequest
{
public:
    SensorRequest(const QString &request, SensorClient *client, int id);
    ~SensorRequest();

    void setRequest(const QString &);
    QString request() const;

    void setClient(SensorClient *);
    SensorClient *client();

    void setId(int);
    int id();

    friend uint qHash(const SensorRequest &sr, uint seed = 0)
    {
        return qHash(qMakePair(sr.mRequest, qMakePair(sr.mClient, sr.mId)), seed);
    }
    friend bool operator==(const SensorRequest &a, const SensorRequest &b)
    {
        return a.mRequest == b.mRequest && a.mClient == b.mClient && a.mId == b.mId;
    }

private:
    QString mRequest;
    SensorClient *mClient = nullptr;
    int mId;
};

}

#endif
