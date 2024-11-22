/*
    KSysGuard, the KDE System Guard

    SPDX-FileCopyrightText: 1999-2001 Chris Schlaeger <cs@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

*/

//#include <stdlib.h>

#include "ksgrd_debug.h"
#include <KLocalizedString>

#include "SensorClient.h"
#include "SensorManager.h"

#include "SensorAgent.h"

/**
  This can be used to debug communication problems with the daemon.
  Should be set to 0 in any production version.
*/
#define SA_TRACE 0

using namespace KSGRD;

SensorAgent::SensorAgent(SensorManager *sm)
    : QObject(sm)
    , mSensorManager(sm)
{
    mDaemonOnLine = false;
}

SensorAgent::~SensorAgent()
{
    for (int i = mInputFIFO.size() - 1; i >= 0; --i)
        delete mInputFIFO.takeAt(i);
    for (int i = mProcessingFIFO.size() - 1; i >= 0; --i)
        delete mProcessingFIFO.takeAt(i);
}

void SensorAgent::sendRequest(const QString &req, SensorClient *client, int id)
{
    SensorRequest nRequest{req, client, id};
    if (mUnderwayRequests.contains(nRequest))
        return;
    mUnderwayRequests.insert(nRequest);

    /* The request is registered with the FIFO so that the answer can be
     * routed back to the requesting client. */
    mInputFIFO.enqueue(new SensorRequest(req, client, id));

#if SA_TRACE
    qCDebug(LIBKSYSGUARD_KSGRD) << "-> " << req << "(" << mInputFIFO.count() << "/" << mProcessingFIFO.count() << ")" << endl;
#endif
    executeCommand();
}

void SensorAgent::processAnswer(const char *buf, int buflen)
{
    // It is possible for an answer/error message  to be split across multiple processAnswer calls.  This makes our life more difficult
    // We have to keep track of the state we are in.  Any characters that we have not parsed yet we put in
    // mLeftOverBuffer
    QByteArray buffer = QByteArray::fromRawData(buf, buflen);
    if (!mLeftOverBuffer.isEmpty()) {
        buffer = mLeftOverBuffer + buffer; // If we have data left over from a previous processAnswer, then we have to prepend this on
        mLeftOverBuffer.clear();
    }

#if SA_TRACE
    qCDebug(LIBKSYSGUARD_KSGRD) << "<- " << QString::fromUtf8(buffer, buffer.size());
#endif
    int startOfAnswer = 0; // This can become >= buffer.size(), so check before using!
    for (int i = 0; i < buffer.size(); ++i) {
        if (buffer.at(i) == '\033') { // 033 in octal is the escape character.  The signifies the start of an error
            int startOfError = i;
            bool found = false;
            while (++i < buffer.size()) {
                if (buffer.at(i) == '\033') {
                    QString error = QString::fromUtf8(buffer.constData() + startOfError + 1, i - startOfError - 1);
                    if (error.startsWith(QLatin1String("RECONFIGURE"))) {
                        Q_EMIT reconfigure(this);
                    } else {
                        /* We just received the end of an error message, so we
                         * can display it. */
                        SensorMgr->notify(i18nc("%1 is a host name", "Message from %1:\n%2", mHostName, error));
                    }
                    found = true;
                    break;
                }
            }
            if (found) {
                buffer.remove(startOfError, i - startOfError + 1);
                i = startOfAnswer - 1;
                continue;
            } else {
                // We have not found the end of the escape string.  Try checking in the next packet
                mLeftOverBuffer = QByteArray(buffer.constData() + startOfAnswer, buffer.size() - startOfAnswer);
                return;
            }
        }

        // The spec was supposed to be that it returned "\nksysguardd> " but some seem to forget the space, so we have to compensate.  Sigh
        if ((i == startOfAnswer && buffer.size() - i >= (signed)(sizeof("ksysguardd>")) - 1
             && qstrncmp(buffer.constData() + i, "ksysguardd>", sizeof("ksysguardd>") - 1) == 0)
            || (buffer.size() - i >= (signed)(sizeof("\nksysguardd>")) - 1
                && qstrncmp(buffer.constData() + i, "\nksysguardd>", sizeof("\nksysguardd>") - 1) == 0)) {
            QByteArray answer(buffer.constData() + startOfAnswer, i - startOfAnswer);
            if (!answer.isEmpty())
                mAnswerBuffer << answer;
#if SA_TRACE
            qCDebug(LIBKSYSGUARD_KSGRD) << "<= " << mAnswerBuffer << "(" << mInputFIFO.count() << "/" << mProcessingFIFO.count() << ")" << endl;
#endif
            if (buffer.at(i) == '\n')
                i++;
            i += sizeof("ksysguardd>")
                - 2; // Move i on to the next answer (if any). -2 because sizeof adds one for \0  and the for loop will increment by 1 also
            if (i + 1 < buffer.size() && buffer.at(i + 1) == ' ')
                i++;
            startOfAnswer = i + 1;

            // We have found the end of one reply
            if (!mDaemonOnLine) {
                /* First '\nksysguardd> ' signals that the daemon is
                 * ready to serve requests now. */
                mDaemonOnLine = true;
#if SA_TRACE
                qCDebug(LIBKSYSGUARD_KSGRD) << "Daemon now online!";
#endif
                mAnswerBuffer.clear();
                continue;
            }

            // Deal with the answer we have now read in

            // remove pending request from FIFO
            if (mProcessingFIFO.isEmpty()) {
                qCDebug(LIBKSYSGUARD_KSGRD) << "ERROR: Received answer but have no pending "
                                            << "request!";
                mAnswerBuffer.clear();
                continue;
            }

            SensorRequest *req = mProcessingFIFO.dequeue();
            mUnderwayRequests.remove(*req);
            // we are now responsible for the memory of req - we must delete it!
            if (!req->client()) {
                /* The client has disappeared before receiving the answer
                 * to his request. */
                delete req;
                mAnswerBuffer.clear();
                continue;
            }

            if (!mAnswerBuffer.isEmpty() && mAnswerBuffer[0] == "UNKNOWN COMMAND") {
                /* Notify client that the sensor seems to be no longer available. */
                qCDebug(LIBKSYSGUARD_KSGRD) << "Received UNKNOWN COMMAND for: " << req->request();
                req->client()->sensorLost(req->id());
            } else {
                // Notify client of newly arrived answer.
                req->client()->answerReceived(req->id(), mAnswerBuffer);
            }
            delete req;
            mAnswerBuffer.clear();
        } else if (buffer.at(i) == '\n') {
            mAnswerBuffer << QByteArray(buffer.constData() + startOfAnswer, i - startOfAnswer);
            startOfAnswer = i + 1;
        }
    }

    mLeftOverBuffer += QByteArray(buffer.constData() + startOfAnswer, buffer.size() - startOfAnswer);
    executeCommand();
}

void SensorAgent::executeCommand()
{
    /* This function is called whenever there is a chance that we have a
     * command to pass to the daemon. But the command may only be sent
     * if the daemon is online and there is no other command currently
     * being sent. */
    if (mDaemonOnLine && !mInputFIFO.isEmpty()) {
        SensorRequest *req = mInputFIFO.dequeue();

#if SA_TRACE
        qCDebug(LIBKSYSGUARD_KSGRD) << ">> " << req->request() << "(" << mInputFIFO.count() << "/" << mProcessingFIFO.count() << ")" << endl;
#endif
        // add request to processing FIFO.
        // Note that this means that mProcessingFIFO is now responsible for managing the memory for it.
        mProcessingFIFO.enqueue(req);

        // send request to daemon
        QString cmdWithNL = req->request() + '\n';
        if (!writeMsg(cmdWithNL.toLatin1().constData(), cmdWithNL.length()))
            qCDebug(LIBKSYSGUARD_KSGRD) << "SensorAgent::writeMsg() failed";
    }
}

void SensorAgent::disconnectClient(SensorClient *client)
{
    for (int i = 0, total = mInputFIFO.size(); i < total; ++i)
        if (mInputFIFO[i]->client() == client)
            mInputFIFO[i]->setClient(nullptr);
    for (int i = 0, total = mProcessingFIFO.size(); i < total; ++i)
        if (mProcessingFIFO[i]->client() == client)
            mProcessingFIFO[i]->setClient(nullptr);
}

SensorManager *SensorAgent::sensorManager()
{
    return mSensorManager;
}

void SensorAgent::setDaemonOnLine(bool value)
{
    mDaemonOnLine = value;
}

bool SensorAgent::daemonOnLine() const
{
    return mDaemonOnLine;
}

void SensorAgent::setHostName(const QString &hostName)
{
    mHostName = hostName;
}

QString SensorAgent::hostName() const
{
    return mHostName;
}

QString SensorAgent::reasonForOffline() const
{
    return mReasonForOffline;
}

void SensorAgent::setReasonForOffline(const QString &reasonForOffline)
{
    mReasonForOffline = reasonForOffline;
}

SensorRequest::SensorRequest(const QString &request, SensorClient *client, int id)
    : mRequest(request)
    , mClient(client)
    , mId(id)
{
}

SensorRequest::~SensorRequest()
{
}

void SensorRequest::setRequest(const QString &request)
{
    mRequest = request;
}

QString SensorRequest::request() const
{
    return mRequest;
}

void SensorRequest::setClient(SensorClient *client)
{
    mClient = client;
}

SensorClient *SensorRequest::client()
{
    return mClient;
}

void SensorRequest::setId(int id)
{
    mId = id;
}

int SensorRequest::id()
{
    return mId;
}
