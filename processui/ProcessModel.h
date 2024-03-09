/*
    KSysGuard, the KDE System Guard

    SPDX-FileCopyrightText: 1999, 2000 Chris Schlaeger <cs@kde.org>
    SPDX-FileCopyrightText: 2006 John Tapsell <john.tapsell@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later

*/

#ifndef PROCESSMODEL_H_
#define PROCESSMODEL_H_

#include <QAbstractItemModel>

#include "../processcore/processes.h"

namespace KSysGuard
{
class Processes;
class Process;
class ProcessAttribute;
}

class ProcessModelPrivate;

#ifdef Q_OS_WIN
// this workaround is needed to make krunner link under msvc
// please keep it this way even if you port this library to have a _export.h header file
#define KSYSGUARD_EXPORT
#else
#define KSYSGUARD_EXPORT Q_DECL_EXPORT
#endif

class KSYSGUARD_EXPORT ProcessModel : public QAbstractItemModel
{
    Q_OBJECT
    Q_ENUMS(Units)

public:
    /** Storage for history values. PercentageHistoryRole returns a QVector of this. */
    struct PercentageHistoryEntry {
        unsigned long timestamp; // in ms, origin undefined as only the delta matters
        float value;
    };

    explicit ProcessModel(QObject *parent = nullptr, const QString &host = QString());
    ~ProcessModel() override;

    /* Functions for our Model for QAbstractItemModel*/
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &index) const override;

    bool hasChildren(const QModelIndex &parent) const override;
    /** Returns if (left < right), used by the sort-filter proxy model to sort the columns */
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const;

    /* Functions for drag and drop and copying to clipboard, inherited from QAbstractItemModel */
    QStringList mimeTypes() const override;
    QMimeData *mimeData(const QModelIndexList &indexes) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    /* Functions for setting the model */

    /** Setup the column headings by inserting the appropriate headings into the model.
     *  Can be called more than once to retranslate the headings if the system language changes.
     */
    void setupHeader();

    /** Update data.  You can pass in the time between updates to only update if there hasn't
     *  been an update within the last @p updateDurationMSecs milliseconds.  0 indicate to update
     *  regardless of when the last update was.
     *  The updateFlags indicates what to additional update, as well as the usual details. */
    void update(long updateDurationMSecs = 0, KSysGuard::Processes::UpdateFlags updateFlags = KSysGuard::Processes::IOStatistics);
    /** Return a string with the pid of the process and the name of the process.  E.g.  13343: ksysguard
     */
    QString getStringForProcess(KSysGuard::Process *process) const;
    KSysGuard::Process *getProcess(qlonglong pid);

    /** This is used from ProcessFilter to get the process at a given index when in flat mode */
    KSysGuard::Process *getProcessAtIndex(int index) const;

    /** Returns whether this user can log in or not.
     *  @see mUidCanLogin
     */
    bool canUserLogin(long uid) const;
    /** In simple mode, everything is flat, with no icons, few if any colors, no xres etc.
     *  This can be changed at any time.  It is a fairly quick operation.  Basically it resets the model
     */
    void setSimpleMode(bool simple);
    /** In simple mode, everything is flat, with no icons, few if any colors, no xres etc
     */
    bool isSimpleMode() const;

    /** Returns the total amount of physical memory in the machine. */
    qlonglong totalMemory() const;

    /** This returns a QModelIndex for the given process.  It has to look up the parent for this pid, find the offset this
     *  pid is from the parent, and return that.  It's not that slow, but does involve a couple of hash table lookups.
     */
    QModelIndex getQModelIndex(KSysGuard::Process *process, int column) const;

    /** Whether this is showing the processes for the current machine
     */
    bool isLocalhost() const;

    /** The host name that this widget is showing the processes of */
    QString hostName() const;

    /** Whether this process has a GUI window */
    bool hasGUIWindow(qlonglong pid) const;

    /** Returns for process controller pointer for this model */
    KSysGuard::Processes *processController() const; // The processes instance

    /** Returns the list of extra attributes provided by plugins */
    const QVector<KSysGuard::ProcessAttribute *> extraAttributes() const;

    /** Convenience function to get the number of processes.
     *
     *  Equivalent to processController->processCount() */
    int processCount() const
    {
        return processController()->processCount();
    }

    /** The headings in the model.  The order here is the order that they are shown
     *  in.  If you change this, make sure you also change the
     *  setup header function, and make sure you increase PROCESSHEADERVERSION.  This will ensure
     *  that old saved settings won't be used
     */
#define PROCESSHEADERVERSION 10
    enum {
        HeadingName = 0,
        HeadingUser,
        HeadingPid,
        HeadingTty,
        HeadingNiceness,
        HeadingCPUUsage,
        HeadingCPUTime,
        HeadingIoRead,
        HeadingIoWrite,
        HeadingVmSize,
        HeadingMemory,
        HeadingSharedMemory,
        HeadingStartTime,
        HeadingNoNewPrivileges,
        HeadingCommand,
        HeadingXMemory,
        HeadingXTitle,
        HeadingCGroup,
        HeadingMACContext,
        HeadingVmPSS,
        // This entry should always match the actual last entry in this enum + 1.
        // It is used to determine where plugin-provided headings start.
        HeadingPluginStart = HeadingVmPSS + 1,
    };

    enum { UidRole = Qt::UserRole, SortingValueRole, WindowIdRole, PlainValueRole, PercentageRole, PercentageHistoryRole };

    bool showTotals() const;

    /** When displaying memory sizes, this is the units it should be displayed in */
    enum Units { UnitsAuto, UnitsKB, UnitsMB, UnitsGB, UnitsTB, UnitsPB, UnitsPercentage };
    /** Set the units memory sizes etc should be displayed in */
    void setUnits(Units units);
    /** The units memory sizes etc should be displayed in */
    Units units() const;
    /** Set the I/O units sizes etc should be displayed in */
    void setIoUnits(Units units);
    /** The units I/O sizes etc should be displayed in */
    Units ioUnits() const;

    enum IoInformation { Bytes, Syscalls, ActualBytes, BytesRate, SyscallsRate, ActualBytesRate };
    /** Set the information to show in the Io Read and Io Write columns */
    void setIoInformation(IoInformation ioInformation);
    /** The information to show in the Io Read and Io Write columns */
    IoInformation ioInformation() const;

    /** Take an amount in kb, and return a string in the units set by setUnits() */
    QString formatMemoryInfo(qlonglong amountInKB, Units units, bool returnEmptyIfValueIsZero = false) const;
    /** Whether to show the command line options in the process name column */
    bool isShowCommandLineOptions() const;
    /** Set whether to show the command line options in the process name column */
    void setShowCommandLineOptions(bool showCommandLineOptions);

    /** Whether to show tooltips when the mouse hovers over a process */
    bool isShowingTooltips() const;
    /** Set whether to show tooltips when the mouse hovers over a process */
    void setShowingTooltips(bool showTooltips);
    /** Whether to divide CPU usage by the number of CPUs */
    bool isNormalizedCPUUsage() const;
    /** Set whether to divide CPU usage by the number of CPUs */
    void setNormalizedCPUUsage(bool normalizeCPUUsage);

    /** Retranslate the GUI, for when the system language changes */
    void retranslateUi();

public Q_SLOTS:
    /** Whether to show the total cpu for the process plus all of its children */
    void setShowTotals(bool showTotals);

private:
    ProcessModelPrivate *const d;
    friend class ProcessModelPrivate;
};

Q_DECLARE_METATYPE(QVector<ProcessModel::PercentageHistoryEntry>);
Q_DECLARE_TYPEINFO(ProcessModel::PercentageHistoryEntry, Q_PRIMITIVE_TYPE);

#endif
