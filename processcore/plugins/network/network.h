/*
    SPDX-FileCopyrightText: 2019 Arjen Hiemstra <ahiemstra@heimr.nl>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QScopedPointer>

#include "../processcore/process_data_provider.h"
#include "../processcore/extended_process_attribute.h"

class QProcess;

class NetworkPlugin : public KSysGuard::ProcessDataProvider
{
    Q_OBJECT
public:
    NetworkPlugin(QObject *parent, const QVariantList &args);
    ~NetworkPlugin() override;

    void handleEnabledChanged(bool enabled) override;

private:
    QProcess *m_process;
    KSysGuard::ExtendedProcessAttribute *m_inboundSensor;
    KSysGuard::ExtendedProcessAttribute *m_outboundSensor;
    
    class inboundInterface;
    class outboundInterface;

    QScopedPointer<inboundInterface> m_inboundSensorInterface;
    QScopedPointer<outboundInterface> m_outboundSensorInterface;
};
