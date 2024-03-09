/*
    KSysGuard, the KDE System Guard

    SPDX-FileCopyrightText: 2006-2007 John Tapsell <tapsell@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later

*/

#ifndef _ReniceDlg_h_
#define _ReniceDlg_h_

#include <QDialog>

class Ui_ReniceDlgUi;
class QButtonGroup;

/**
 * This class creates and handles a simple dialog to change the scheduling
 * priority of a process.
 */
class ReniceDlg : public QDialog
{
    Q_OBJECT

public:
    /** Let the user specify the new priorities of the @p processes given, using the given current values.
     *  @p currentCpuSched The current Cpu Scheduler of the processes.  Set to -1 to they have different schedulers
     *  @p currentIoSched The current I/O Scheduler of the processes.  Set to -1 to they have different schedulers.  Leave as the default -2 if not supported
     */
    explicit ReniceDlg(QWidget *parent, const QStringList &processes, int currentCpuPrio, int currentCpuSched, int currentIoPrio = -2, int currentIoSched = -2);
    ~ReniceDlg() override;
    int newCPUPriority;
    int newIOPriority;
    int newCPUSched;
    int newIOSched;

    bool ioniceSupported;

public Q_SLOTS:
    void slotOk();
    void updateUi();
    void cpuSliderChanged(int value);
    void ioSliderChanged(int value);
    void cpuSchedulerChanged(int value);

private:
    void setSliderRange();
    Ui_ReniceDlgUi *ui;
    QButtonGroup *cpuScheduler;
    QButtonGroup *ioScheduler;
    int previous_cpuscheduler;
};

#endif
