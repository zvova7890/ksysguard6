/*
 *  KSysGuard, the KDE System Guard
 *
 *  SPDX-FileCopyrightText: 2022 Eugene Popov <popov895@ukr.net>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "ProcessDetailsDialog.h"

#include <QLayout>
#include <QTabWidget>

#include <KLocalizedString>
#include <KMessageWidget>

#include "../processcore/extended_process_list.h"

#include "GeneralTab.h"
#include "MemoryMapsTab.h"
#include "OpenFilesTab.h"

ProcessDetailsDialog::ProcessDetailsDialog(QWidget *parent)
    : QDialog(parent)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(i18nc("@title:window", "Process Details"));
    resize(900, 600);

    m_warningWidget = new KMessageWidget;
    m_warningWidget->setMessageType(KMessageWidget::Warning);
    m_warningWidget->setCloseButtonVisible(false);
    m_warningWidget->setWordWrap(true);
    m_warningWidget->hide();

    QTabWidget *tabWidget = new QTabWidget;

    m_generalTab = new GeneralTab;
    tabWidget->addTab(m_generalTab, i18nc("@title:tab", "General"));

    m_memoryMapsTab = new MemoryMapsTab;
    tabWidget->addTab(m_memoryMapsTab, i18nc("@title:tab", "Memory Maps"));

    m_openFilesTab = new OpenFilesTab;
    tabWidget->addTab(m_openFilesTab, i18nc("@title:tab", "Open Files"));

    QVBoxLayout *rootLayout = new QVBoxLayout;
    rootLayout->addWidget(m_warningWidget);
    rootLayout->addWidget(tabWidget);
    setLayout(rootLayout);

    const QSharedPointer<KSysGuard::ExtendedProcesses> processes = KSysGuard::ExtendedProcesses::instance();
    connect(processes.get(), &KSysGuard::ExtendedProcesses::beginRemoveProcess, this, &ProcessDetailsDialog::onBeginRemoveProcess);
    connect(processes.get(), &KSysGuard::ExtendedProcesses::processChanged, this, &ProcessDetailsDialog::onProcessChanged);
}

void ProcessDetailsDialog::setModelIndex(const QModelIndex &index)
{
    if (m_index != index) {
        m_index = index;

        if (m_index.isValid()) {
            m_warningWidget->animatedHide();
        }

        m_generalTab->setData(getProcessData());

        const long processId = getProcessId();
        m_memoryMapsTab->setProcessId(processId);
        m_openFilesTab->setProcessId(processId);
    }
}

void ProcessDetailsDialog::onBeginRemoveProcess(KSysGuard::Process *process)
{
    if (getProcessId() == process->pid()) {
        setModelIndex(QModelIndex());

        m_warningWidget->setText(i18nc("@info:status", "The \"%1\" (PID: %2) process has stopped executing", process->name(), process->pid()));
        m_warningWidget->animatedShow();
    }
}

void ProcessDetailsDialog::onProcessChanged(KSysGuard::Process *process)
{
    if (getProcessId() == process->pid()) {
        m_generalTab->setData(getProcessData());
    }
}

const KSysGuard::Process* ProcessDetailsDialog::getProcess() const
{
    if (m_index.isValid()) {
        return reinterpret_cast<const KSysGuard::Process*>(m_index.internalPointer());
    }

    return nullptr;
}

long ProcessDetailsDialog::getProcessId() const
{
    if (const KSysGuard::Process *process = getProcess()) {
        return process->pid();
    }

    return 0;
}

QVariantMap ProcessDetailsDialog::getProcessData() const
{
    QVariantMap processData;

    if (m_index.isValid()) {
        const QAbstractItemModel *model = m_index.model();
        const int row = m_index.row();
        const int column = m_index.column();
        for (int i = 0, n = model->columnCount(); i < n; ++i) {
            const QString columnTitle = model->headerData(i, Qt::Horizontal).toString();
            if (i == column) {
                processData.insert(columnTitle, m_index.data());
            } else {
                processData.insert(columnTitle, m_index.sibling(row, i).data());
            }
        }

        processData.insert(i18nc("@title:column Process I/O priority", "IO Priority"), getProcess()->ioPriorityClassAsString());
    }

    return processData;
}
