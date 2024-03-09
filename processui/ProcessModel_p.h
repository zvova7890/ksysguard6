/*
    KSysGuard, the KDE System Guard

    SPDX-FileCopyrightText: 2006-2007 John Tapsell <john.tapsell@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef PROCESSMODEL_P_H_
#define PROCESSMODEL_P_H_

#include "ProcessModel.h"
#include "../processcore/extended_process_list.h"

#include "config-ksysguard.h"
#include <QDebug>
#include <QHash>
#include <QList>
#include <QObject>
#include <QPixmap>
#include <QSet>
#include <QTime>
#include <QVariant>
#include <kuser.h>

#if HAVE_X11
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <private/qtx11extras_p.h>
#else
#include <QX11Info>
#endif
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <fixx11h.h>
#include <kwindowsystem.h>
#include <netwm.h>

struct WindowInfo {
    WindowInfo(WId _wid, qlonglong _pid)
         : wid(_wid)
    {
        pid = 0;
        pid = _pid;
    }
    qlonglong pid;
    QPixmap icon;
    WId wid;
    QString name;
};
#endif

namespace KSysGuard
{
class Processes;
}

class ProcessModelPrivate : public QObject
{
    Q_OBJECT
public:
    ProcessModelPrivate();
    ~ProcessModelPrivate() override;
public Q_SLOTS:

#if HAVE_X11
    /** When an X window is changed, this is called */
    void windowChanged(WId wid, NET::Properties properties, NET::Properties2 properties2);
    /** When an X window is created, this is called
     */
    void windowAdded(WId wid);
    /** When an X window is closed, this is called
     */
    void windowRemoved(WId wid);
#endif

    /** Change the data for a process.  This is called from KSysGuard::Processes
     *  if @p onlyCpuOrMem is set, only the total cpu usage is updated.
     *  process->changes  contains a bitfield of what has been changed
     */
    void processChanged(KSysGuard::Process *process, bool onlyCpuOrMem);
    /** Called from KSysGuard::Processes
     *  This indicates we are about to insert a process in the model.  Emit the appropriate signals
     */
    void beginInsertRow(KSysGuard::Process *parent);
    /** Called from KSysGuard::Processes
     *  We have finished inserting a process
     */
    void endInsertRow();
    /** Called from KSysGuard::Processes
     *  This indicates we are about to remove a process in the model.  Emit the appropriate signals
     */
    void beginRemoveRow(KSysGuard::Process *process);
    /** Called from KSysGuard::Processes
     *  We have finished removing a process
     */
    void endRemoveRow();
    /** Called from KSysGuard::Processes
     *  This indicates we are about to move a process in the model from one parent process to another.  Emit the appropriate signals
     */
    void beginMoveProcess(KSysGuard::Process *process, KSysGuard::Process *new_parent);
    /** Called from KSysGuard::Processes
     *  We have finished moving a process
     */
    void endMoveRow();

public:
    /** Connects to the host */
    void setupProcesses();
    /** A mapping of running,stopped,etc  to a friendly description like 'Stopped, either by a job control signal or because it is being traced.'*/
    QString getStatusDescription(KSysGuard::Process::ProcessStatus status) const;

    /** Return a qt markup tooltip string for a local user.  It will have their full name etc.
     *  This will be slow the first time, as it practically indirectly reads the whole of /etc/passwd
     *  But the second time will be as fast as hash lookup as we cache the result
     */
    inline QString getTooltipForUser(const KSysGuard::Process *process) const;

    /** Return a username for a local user if it can, otherwise just their uid.
     *  This may have been given from the result of "ps" (but not necessarily).
     *  If it's not found, then it needs to find out the username from the uid.
     *  This will be slow the first time, as it practically indirectly reads the whole of /etc/passwd
     *  But the second time will be as fast as hash lookup as we cache the result
     *
     *  If withuid is set, and the username is found, return: "username (Uid: uid)"
     */
    inline QString getUsernameForUser(long uid, bool withuid) const;

    /** Return the groupname for a given gid.  This is in the form of "gid" if not known, or
     *  "groupname (Uid: gid)" if known.
     */
    inline QString getGroupnameForGroup(long gid) const;
#if HAVE_X11
    /** On X11 system, connects to the signals emitted when windows are created/destroyed */
    void setupWindows();
    void updateWindowInfo(WId wid, NET::Properties properties, NET::Properties2 properties2, bool newWindow);
    QMultiHash<long long, WindowInfo *> mPidToWindowInfo; ///< Map a process pid to X window info if available
    QHash<WId, WindowInfo *> mWIdToWindowInfo; ///< Map an X window id to window info
#if HAVE_XRES
    bool updateXResClientData();
    void queryForAndUpdateAllXWindows();
#endif
#endif
    void timerEvent(QTimerEvent *event) override; ///< Call dataChanged() for all the processes in mPidsToUpdate
    /** @see setIsLocalhost */
    bool mIsLocalhost;

    /** A caching hash for tooltips for a user.
     *  @see getTooltipForUser */
    mutable QHash<long long, QString> mUserTooltips;

    /** A caching hash for username for a user uid, or just their uid if it can't be found (as a long long)
     *  @see getUsernameForUser */
    mutable QHash<long long, QString> mUserUsername;

    /** A mapping of a user id to whether this user can log in.  We have to guess based on the shell.
     *  All are set to true to non localhost.
     *  It is set to:
     *    0 if the user cannot login
     *    1 is the user can login
     *  The reason for using an int and not a bool is so that we can do
     *  \code mUidCanLogin.value(uid,-1) \endcode  and thus we get a tristate for whether
     *  they are logged in, not logged in, or not known yet.
     *  */
    mutable QHash<long long, int> mUidCanLogin;

    /** A translated list of headings (column titles) in the order we want to display them. Used in headerData() */
    QStringList mHeadings;

    bool mShowChildTotals; ///< If set to true, a parent will return the CPU usage of all its children recursively

    bool mSimple; //< In simple mode, the model returns everything as flat, with no icons, etc.  This is set by changing cmbFilter

    QTime mLastUpdated; ///< Time that we last updated the processes.

    long long mMemTotal; ///< the total amount of physical memory in kb in the machine.  We can used this to determine the percentage of memory an app is using
    int mNumProcessorCores; ///< The number of (enabled) processor cores in the this machine

    QSharedPointer<KSysGuard::ExtendedProcesses> mProcesses; ///< The processes instance

    QPixmap mBlankPixmap; ///< Used to pad out process names which don't have an icon

    /** Show the process command line options in the process name column */
    bool mShowCommandLineOptions;

    bool mShowingTooltips;
    bool mNormalizeCPUUsage;
    /** When displaying memory sizes, this is the units it should be displayed in */
    ProcessModel::Units mUnits;
    ProcessModel::Units mIoUnits;

    ProcessModel::IoInformation mIoInformation;

    /** The hostname */
    QString mHostName;
    bool mHaveTimer;
    int mTimerId;
    QList<long> mPidsToUpdate; ///< A list of pids that we need to emit dataChanged() for regularly

    static const int MAX_HIST_ENTRIES = 100;
    static const int MIN_HIST_AGE = 200; ///< If the latest history entry is at least this ms old, a new one gets added
    /** Storage for the history entries. We need one per percentage column. */
    QHash<KSysGuard::Process *, QVector<ProcessModel::PercentageHistoryEntry>> mMapProcessCPUHistory;

    QVector<KSysGuard::ProcessAttribute *> mExtraAttributes;

#if HAVE_XRES
    bool mHaveXRes; ///< True if the XRes extension is available at run time
    QMap<qlonglong, XID> mXResClientResources;
#endif

    bool mMovingRow;
    bool mRemovingRow;
    bool mInsertingRow;

    bool mIsX11;

    ProcessModel *q;
};

#endif
