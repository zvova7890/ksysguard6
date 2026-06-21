/*
    SPDX-FileCopyrightText: 2019 David Edmundson <davidedmundson@kde.org>
    SPDX-FileCopyrightText: 2026 Juho Ovaska <ovaska.juho@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#pragma once

#include "../processcore/process_data_provider.h"
#include "../processcore/extended_process_attribute.h"

#include <memory>

class NvidiaPlugin : public KSysGuard::ProcessDataProvider
{
    Q_OBJECT
public:
    NvidiaPlugin(QObject *parent, const QVariantList &args);
    ~NvidiaPlugin() override;

    void handleEnabledChanged(bool enabled) override;

private:
    void update() override;

    unsigned int numGpus();

    bool isNormalizedGPUUsage() const;
    void setNormalizedGPUUsage(bool normalize);

    KSysGuard::Unit getMemoryUnits();
    void setMemoryUnits(KSysGuard::Unit unit);

    bool m_normalizeUsage;

    KSysGuard::ExtendedProcessAttribute *m_usage = nullptr;
    KSysGuard::ExtendedProcessAttribute *m_memory = nullptr;

    class nvmlLib;
    class nvmlDetail;

    std::unique_ptr<nvmlLib> m_nvmlLib;
    std::unique_ptr<nvmlDetail> m_nvmlDetail;

    class usageInterface;
    class memoryInterface;
 
    std::unique_ptr<usageInterface> m_usageInterface;
    std::unique_ptr<memoryInterface> m_memoryInterface;
};
