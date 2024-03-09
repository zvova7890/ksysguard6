/*
    KSysGuard, the KDE System Guard

    SPDX-FileCopyrightText: 1999 Chris Schlaeger <cs@kde.org>
    SPDX-FileCopyrightText: 2007 John Tapsell <tapsell@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later


*/
#include "ReniceDlg.h"

#include <klocalizedstring.h>

#include "../processcore/process.h"
#include "ui_ReniceDlgUi.h"
#include <QButtonGroup>
#include <QDialogButtonBox>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

ReniceDlg::ReniceDlg(QWidget *parent, const QStringList &processes, int currentCpuPrio, int currentCpuSched, int currentIoPrio, int currentIoSched)
    : QDialog(parent)
{
    setObjectName(QStringLiteral("Renice Dialog"));
    setModal(true);
    setWindowTitle(i18n("Set Priority"));
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    previous_cpuscheduler = 0;

    if (currentIoSched == KSysGuard::Process::None) {
        // CurrentIoSched == 0 means that the priority is set automatically.
        // Using the formula given by the linux kernel Documentation/block/ioprio
        currentIoPrio = (currentCpuPrio + 20) / 5;
    }
    if (currentIoSched == (int)KSysGuard::Process::BestEffort && currentIoPrio == (currentCpuPrio + 20) / 5) {
        // Unfortunately, in linux you can't ever set a process back to being None.  So we fake it :)
        currentIoSched = KSysGuard::Process::None;
    }
    ioniceSupported = (currentIoPrio != -2);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    QPushButton *okButton = buttonBox->button(QDialogButtonBox::Ok);
    okButton->setDefault(true);
    okButton->setShortcut(Qt::CTRL | Qt::Key_Return);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &ReniceDlg::slotOk);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    QWidget *widget = new QWidget(this);
    mainLayout->addWidget(widget);
    ui = new Ui_ReniceDlgUi();
    ui->setupUi(widget);
    ui->listWidget->insertItems(0, processes);

    cpuScheduler = new QButtonGroup(this);
    cpuScheduler->addButton(ui->radioNormal, (int)KSysGuard::Process::Other);
#ifndef Q_OS_SOLARIS
    cpuScheduler->addButton(ui->radioBatch, (int)KSysGuard::Process::Batch);
#else
    cpuScheduler->addButton(ui->radioBatch, (int)KSysGuard::Process::Interactive);
    ui->radioBatch->setText(i18nc("Scheduler", "Interactive"));
#endif
    cpuScheduler->addButton(ui->radioFIFO, (int)KSysGuard::Process::Fifo);
    cpuScheduler->addButton(ui->radioRR, (int)KSysGuard::Process::RoundRobin);
    if (currentCpuSched >= 0) { // negative means none of these
        QAbstractButton *sched = cpuScheduler->button(currentCpuSched);
        if (sched) {
            sched->setChecked(true); // Check the current scheduler
            previous_cpuscheduler = currentCpuSched;
        }
    }
    cpuScheduler->setExclusive(true);

    ioScheduler = new QButtonGroup(this);
    ioScheduler->addButton(ui->radioIONormal, (int)KSysGuard::Process::None);
    ioScheduler->addButton(ui->radioIdle, (int)KSysGuard::Process::Idle);
    ioScheduler->addButton(ui->radioRealTime, (int)KSysGuard::Process::RealTime);
    ioScheduler->addButton(ui->radioBestEffort, (int)KSysGuard::Process::BestEffort);
    if (currentIoSched >= 0) { // negative means none of these
        QAbstractButton *iosched = ioScheduler->button(currentIoSched);
        if (iosched)
            iosched->setChecked(true); // Check the current io scheduler
    }

    ioScheduler->setExclusive(true);

    setSliderRange(); // Update the slider ranges before trying to set their current values
    if (ioniceSupported)
        ui->sliderIO->setValue(currentIoPrio);
    ui->sliderCPU->setValue(currentCpuPrio);

    ui->imgCPU->setPixmap(QIcon::fromTheme(QStringLiteral("cpu")).pixmap(128, 128));
    ui->imgIO->setPixmap(QIcon::fromTheme(QStringLiteral("drive-harddisk")).pixmap(128, 128));

    newCPUPriority = 40;

    connect(cpuScheduler, &QButtonGroup::idClicked, this, &ReniceDlg::cpuSchedulerChanged);
    connect(ioScheduler, &QButtonGroup::idClicked, this, &ReniceDlg::updateUi);
    connect(ui->sliderCPU, &QAbstractSlider::valueChanged, this, &ReniceDlg::cpuSliderChanged);
    connect(ui->sliderIO, &QAbstractSlider::valueChanged, this, &ReniceDlg::ioSliderChanged);

    updateUi();

    mainLayout->addWidget(buttonBox);
}

ReniceDlg::~ReniceDlg()
{
    delete ui;
}

void ReniceDlg::ioSliderChanged(int value)
{
    ui->sliderIO->setToolTip(QString::number(value));
}

void ReniceDlg::cpuSchedulerChanged(int value)
{
    if (value != previous_cpuscheduler) {
        if ((value == (int)KSysGuard::Process::Other || value == KSysGuard::Process::Batch)
            && (previous_cpuscheduler == (int)KSysGuard::Process::Fifo || previous_cpuscheduler == (int)KSysGuard::Process::RoundRobin)) {
            int slider = -ui->sliderCPU->value() * 2 / 5 + 20;
            setSliderRange();
            ui->sliderCPU->setValue(slider);
        } else if ((previous_cpuscheduler == (int)KSysGuard::Process::Other || previous_cpuscheduler == KSysGuard::Process::Batch)
                   && (value == (int)KSysGuard::Process::Fifo || value == (int)KSysGuard::Process::RoundRobin)) {
            int slider = (-ui->sliderCPU->value() + 20) * 5 / 2;
            setSliderRange();
            ui->sliderCPU->setValue(slider);
        }
    }
    previous_cpuscheduler = value;
    updateUi();
}

void ReniceDlg::cpuSliderChanged(int value)
{
    if (ioniceSupported) {
        if (cpuScheduler->checkedId() == (int)KSysGuard::Process::Other || cpuScheduler->checkedId() == (int)KSysGuard::Process::Batch) {
            if (ioScheduler->checkedId() == -1 || ioScheduler->checkedId() == (int)KSysGuard::Process::None) {
                // ionice is 'Normal', thus automatically calculated based on cpunice
                ui->sliderIO->setValue((value + 20) / 5);
            }
        }
    }
    ui->sliderCPU->setToolTip(QString::number(value));
}

void ReniceDlg::updateUi()
{
    bool cpuPrioEnabled = (cpuScheduler->checkedId() != -1);
    bool ioPrioEnabled = (ioniceSupported && ioScheduler->checkedId() != -1 && ioScheduler->checkedId() != (int)KSysGuard::Process::Idle
                          && ioScheduler->checkedId() != (int)KSysGuard::Process::None);

    ui->sliderCPU->setEnabled(cpuPrioEnabled);
    ui->lblCpuLow->setEnabled(cpuPrioEnabled);
    ui->lblCpuHigh->setEnabled(cpuPrioEnabled);

    ui->sliderIO->setEnabled(ioPrioEnabled);
    ui->lblIOLow->setEnabled(ioPrioEnabled);
    ui->lblIOHigh->setEnabled(ioPrioEnabled);

    ui->radioIONormal->setEnabled(ioniceSupported);
    ui->radioIdle->setEnabled(ioniceSupported);
    ui->radioRealTime->setEnabled(ioniceSupported);
    ui->radioBestEffort->setEnabled(ioniceSupported);

    setSliderRange();
    cpuSliderChanged(ui->sliderCPU->value());
    ioSliderChanged(ui->sliderIO->value());
}

void ReniceDlg::setSliderRange()
{
    if (cpuScheduler->checkedId() == (int)KSysGuard::Process::Other || cpuScheduler->checkedId() == (int)KSysGuard::Process::Batch
        || cpuScheduler->checkedId() == (int)KSysGuard::Process::Interactive) {
        // The slider is setting the priority, so goes from 19 to -20.  We cannot actually do this with a slider, so instead we go from -19 to 20, and negate
        // later
        if (ui->sliderCPU->value() > 20)
            ui->sliderCPU->setValue(20);
        ui->sliderCPU->setInvertedAppearance(true);
        ui->sliderCPU->setMinimum(-20);
        ui->sliderCPU->setMaximum(19);
        ui->sliderCPU->setTickInterval(5);
    } else {
        if (ui->sliderCPU->value() < 1)
            ui->sliderCPU->setValue(1);
        ui->sliderCPU->setInvertedAppearance(false);
        ui->sliderCPU->setMinimum(1);
        ui->sliderCPU->setMaximum(99);
        ui->sliderCPU->setTickInterval(12);
    }
}

void ReniceDlg::slotOk()
{
    newCPUPriority = ui->sliderCPU->value();
    newIOPriority = ui->sliderIO->value();
    newCPUSched = cpuScheduler->checkedId();
    newIOSched = ioScheduler->checkedId();
    accept();
}
