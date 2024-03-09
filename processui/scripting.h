/*
    KSysGuard, the KDE System Guard

    SPDX-FileCopyrightText: 2009 John Tapsell <john.tapsell@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later

*/

#ifndef KSYSGUARDSCRIPTING_H
#define KSYSGUARDSCRIPTING_H

#include "config-ksysguard.h"
#include "ProcessModel.h"
#include <QList>
#include <QString>
#include <QWidget>
#include "../processcore/processes.h"

class QAction;
class ScriptingHtmlDialog; // Defined in scripting.cpp file
class KSysGuardProcessList;
class ProcessObject;
class QWebChannel;

class Scripting : public QWidget
{
    Q_OBJECT
public:
    /** Create a scripting object */
    Scripting(KSysGuardProcessList *parent);
    /** Run the script in the given path */
    void runScript(const QString &path, const QString &name);
    /** Read all the script .desktop files and create an action for each one */
    void loadContextMenu();
    /** List of context menu actions that are created by loadContextMenu() */
    QList<QAction *> actions()
    {
        return mActions;
    }

public Q_SLOTS:
    /** Stop all scripts and delete the script engine */
    void stopAllScripts();
private Q_SLOTS:
    /** Run the script associated with the QAction that called this slot */
    void runScriptSlot();
#if WEBENGINE_SCRIPTING_ENABLED
    void setupJavascriptObjects();
    void refreshScript();
    void zoomIn();
    void zoomOut();
#endif
private:
    /** This is created on the fly as needed, and deleted when no longer used */
    ScriptingHtmlDialog *mScriptingHtmlDialog;
    /** Used to expose mProcessObject to the WebEnginePage */
    QWebChannel *mWebChannel;
    /** The parent process list to script for */
    KSysGuardProcessList *const mProcessList;
    /** List of context menu actions that are created by loadContextMenu() */
    QList<QAction *> mActions;
    QString mScriptPath;
    QString mScriptName;
    ProcessObject *mProcessObject;

    qlonglong mPid;
};

// QWebChannel only reloads properties on demand, so we need a signal.
#define P_PROPERTY(x) Q_PROPERTY(x NOTIFY anythingChanged)
#define PROPERTY(Type, Name)                                                                                                                                   \
    Type Name() const                                                                                                                                          \
    {                                                                                                                                                          \
        KSysGuard::Process *process = mModel->getProcess(mPid);                                                                                                \
        if (process)                                                                                                                                           \
            return process->Name();                                                                                                                            \
        else                                                                                                                                                   \
            return Type();                                                                                                                                     \
    }

class ProcessObject : public QObject
{
    Q_OBJECT
public:
    // clang-format off
       P_PROPERTY(qlonglong pid READ pid WRITE setPid)                 /* Add functionality to 'set' the pid to change which process to read from */
       P_PROPERTY(qlonglong ppid READ parentPid)                       /* Map 'ppid' to 'parentPid' to give it a nicer scripting name */
       P_PROPERTY(QString name READ name)                              /* Defined below to return the first word of the name */
       P_PROPERTY(QString fullname READ fullname)                      /* Defined below to return 'name' */
       P_PROPERTY(qlonglong rss READ vmRSS)                            /* Map 'rss' to 'vmRSS' just to give it a nicer scripting name */
       P_PROPERTY(qlonglong urss READ vmURSS)                          /* Map 'urss' to 'vmURSS' just to give it a nicer scripting name */
       P_PROPERTY(int numThreads READ numThreads)                      PROPERTY(int, numThreads)
       P_PROPERTY(qlonglong fsgid READ fsgid)                          PROPERTY(qlonglong, fsgid)
       P_PROPERTY(qlonglong parentPid READ parentPid)                  PROPERTY(qlonglong, parentPid)
       P_PROPERTY(QString login READ login)                            PROPERTY(QString, login)
       P_PROPERTY(qlonglong uid READ uid)                              PROPERTY(qlonglong, uid)
       P_PROPERTY(qlonglong euid READ euid)                            PROPERTY(qlonglong, euid)
       P_PROPERTY(qlonglong suid READ suid)                            PROPERTY(qlonglong, suid)
       P_PROPERTY(qlonglong fsuid READ fsuid)                          PROPERTY(qlonglong, fsuid)
       P_PROPERTY(qlonglong gid READ gid)                              PROPERTY(qlonglong, gid)
       P_PROPERTY(qlonglong egid READ egid)                            PROPERTY(qlonglong, egid)
       P_PROPERTY(qlonglong sgid READ sgid)                            PROPERTY(qlonglong, sgid)
       P_PROPERTY(qlonglong tracerpid READ tracerpid)                  PROPERTY(qlonglong, tracerpid)
       P_PROPERTY(QByteArray tty READ tty)                             PROPERTY(QByteArray, tty)
       P_PROPERTY(qlonglong userTime READ userTime)                    PROPERTY(qlonglong, userTime)
       P_PROPERTY(qlonglong sysTime READ sysTime)                      PROPERTY(qlonglong, sysTime)
       P_PROPERTY(int userUsage READ userUsage)                        PROPERTY(int, userUsage)
       P_PROPERTY(int sysUsage READ sysUsage)                          PROPERTY(int, sysUsage)
       P_PROPERTY(int totalUserUsage READ totalUserUsage)              PROPERTY(int, totalUserUsage)
       P_PROPERTY(int totalSysUsage READ totalSysUsage)                PROPERTY(int, totalSysUsage)
       P_PROPERTY(int numChildren READ numChildren)                    PROPERTY(int, numChildren)
       P_PROPERTY(int niceLevel READ niceLevel)                        PROPERTY(int, niceLevel)
       P_PROPERTY(int scheduler READ scheduler)                        PROPERTY(int, scheduler)
       P_PROPERTY(int ioPriorityClass READ ioPriorityClass)            PROPERTY(int, ioPriorityClass)
       P_PROPERTY(int ioniceLevel READ ioniceLevel)                    PROPERTY(int, ioniceLevel)
       P_PROPERTY(qlonglong vmSize READ vmSize)                        PROPERTY(qlonglong, vmSize)
       P_PROPERTY(qlonglong vmRSS READ vmRSS)                          PROPERTY(qlonglong, vmRSS)
       P_PROPERTY(qlonglong vmURSS READ vmURSS)                        PROPERTY(qlonglong, vmURSS)
       P_PROPERTY(qlonglong pixmapBytes READ pixmapBytes)              PROPERTY(qlonglong, pixmapBytes)
       P_PROPERTY(bool hasManagedGuiWindow READ hasManagedGuiWindow)   PROPERTY(bool, hasManagedGuiWindow)
       P_PROPERTY(QString command READ command)                        PROPERTY(QString, command)
       P_PROPERTY(qlonglong status READ status)                        PROPERTY(qlonglong, status)
       P_PROPERTY(qlonglong ioCharactersRead READ ioCharactersRead)    PROPERTY(qlonglong, ioCharactersRead)
       P_PROPERTY(qlonglong ioCharactersWritten READ ioCharactersWritten)                  PROPERTY(qlonglong, ioCharactersWritten)
       P_PROPERTY(qlonglong ioReadSyscalls READ ioReadSyscalls)                            PROPERTY(qlonglong, ioReadSyscalls)
       P_PROPERTY(qlonglong ioWriteSyscalls READ ioWriteSyscalls)                          PROPERTY(qlonglong, ioWriteSyscalls)
       P_PROPERTY(qlonglong ioCharactersActuallyRead READ ioCharactersActuallyRead)        PROPERTY(qlonglong, ioCharactersActuallyRead)
       P_PROPERTY(qlonglong ioCharactersActuallyWritten READ ioCharactersActuallyWritten)  PROPERTY(qlonglong, ioCharactersActuallyWritten)
       P_PROPERTY(qlonglong ioCharactersReadRate READ ioCharactersReadRate)                PROPERTY(qlonglong, ioCharactersReadRate)
       P_PROPERTY(qlonglong ioCharactersWrittenRate READ ioCharactersWrittenRate)          PROPERTY(qlonglong, ioCharactersWrittenRate)
       P_PROPERTY(qlonglong ioReadSyscallsRate READ ioReadSyscallsRate)                    PROPERTY(qlonglong, ioReadSyscallsRate)
       P_PROPERTY(qlonglong ioWriteSyscallsRate READ ioWriteSyscallsRate)                  PROPERTY(qlonglong, ioWriteSyscallsRate)
       P_PROPERTY(qlonglong ioCharactersActuallyReadRate READ ioCharactersActuallyReadRate)        PROPERTY(qlonglong, ioCharactersActuallyReadRate)
       P_PROPERTY(qlonglong ioCharactersActuallyWrittenRate READ ioCharactersActuallyWrittenRate)  PROPERTY(qlonglong, ioCharactersActuallyWrittenRate)
        // clang-format off

        ProcessObject(ProcessModel * processModel, int pid);
        void update(KSysGuard::Process *process);

        int pid() const { return mPid; }
        void setPid(int pid) { mPid = pid; }
        QString name() const { KSysGuard::Process *process = mModel->getProcess(mPid); if(process) return process->name().section(QLatin1Char(' '), 0,0); else return QString(); }
        QString fullname() const { KSysGuard::Process *process = mModel->getProcess(mPid); if(process) return process->name(); else return QString(); }

        public Q_SLOTS:
        bool fileExists(const QString &filename);
        QString readFile(const QString &filename);

    Q_SIGNALS:
        void anythingChanged();

    private:
        int mPid;
        ProcessModel *mModel;
};

#endif
