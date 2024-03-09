/*
 *  KSysGuard, the KDE System Guard
 *
 *  SPDX-FileCopyrightText: 2022 Eugene Popov <popov895@ukr.net>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef _ProcessDetailsDialog_h_
#define _ProcessDetailsDialog_h_

#include <QDialog>
#include <QPersistentModelIndex>

class KMessageWidget;

namespace KSysGuard
{
class Process;
}

class GeneralTab;
class MemoryMapsTab;
class OpenFilesTab;

class ProcessDetailsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ProcessDetailsDialog(QWidget *parent = nullptr);

    void setModelIndex(const QModelIndex &index);

private Q_SLOTS:
    void onBeginRemoveProcess(KSysGuard::Process *process);
    void onProcessChanged(KSysGuard::Process *process);

private:
    const KSysGuard::Process* getProcess() const;
    long getProcessId() const;
    QVariantMap getProcessData() const;

    QPersistentModelIndex m_index;

    KMessageWidget *m_warningWidget = nullptr;
    GeneralTab *m_generalTab = nullptr;
    MemoryMapsTab *m_memoryMapsTab = nullptr;
    OpenFilesTab *m_openFilesTab = nullptr;
};

#endif // _ProcessDetailsDialog_h_
