/*
    SPDX-FileCopyrightText: 2019 David Edmundson <davidedmundson@kde.org>
    SPDX-FileCopyrightText: 2026 Juho Ovaska <ovaska.juho@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "nvidia.h"

#include <QMenu>
#include <QDebug>
#include <QLibrary>
#include <QDomElement>
#include <QLoggingCategory>

#include <KConfigGroup>
#include <KLocalizedString>
#include <KPluginFactory>

#include <vector>
#include <algorithm>
#include <functional>
#include <unordered_set>
#include <unordered_map>
#include <set>

#include <cassert>

#include "nvidialogging.h"

#include "nvml.h"

using namespace KSysGuard;

class NvidiaPlugin::nvmlLib {
public:
    struct nvmlFuncs
    {
#define NV_FUNC(name) decltype(&::name) name;
NV_FUNC(nvmlInit)
NV_FUNC(nvmlShutdown)
NV_FUNC(nvmlDeviceGetCount)
NV_FUNC(nvmlDeviceGetHandleByIndex)
NV_FUNC(nvmlDeviceGetMemoryInfo)
NV_FUNC(nvmlDeviceGetProcessUtilization)
NV_FUNC(nvmlDeviceGetComputeRunningProcesses)
NV_FUNC(nvmlDeviceGetGraphicsRunningProcesses)
NV_FUNC(nvmlDeviceGetMPSComputeRunningProcesses)
#undef NV_FUNC
    };

    nvmlLib()
        : m_lib("nvidia-ml", 1), m_funcs{}
    {
    }

    ~nvmlLib()
    {
        unloadLib();
    }

    bool isLoaded() const
    {
        return m_lib.isLoaded();
    }

    bool tryLoad()
    {
        if (isLoaded())
        {
            return true;
        }

        if (!m_lib.load())
        {
            return false;
        }

        // try to dlsym() all NVML functions
        nvmlFuncs funcs{};
        bool failed = false;
#define NV_STR(name) #name
#define NV_FUNC(name) do {                                                                                       \
            auto *ptr = m_lib.resolve(NV_STR(name));                                                             \
            if (ptr == nullptr) {                                                                                \
                qCWarning(KSYSGUARD_PLUGIN_NVIDIA) << "ERROR: Failed to resolve NVML symbol '" NV_STR(name) "'"; \
                failed = true;                                                                                   \
            } else {                                                                                             \
                funcs.name = reinterpret_cast<decltype(funcs.name)>(ptr);                                        \
            }                                                                                                    \
        } while (0);
NV_FUNC(nvmlInit)
NV_FUNC(nvmlShutdown)
NV_FUNC(nvmlDeviceGetCount)
NV_FUNC(nvmlDeviceGetHandleByIndex)
NV_FUNC(nvmlDeviceGetMemoryInfo)
NV_FUNC(nvmlDeviceGetProcessUtilization)
NV_FUNC(nvmlDeviceGetComputeRunningProcesses)
NV_FUNC(nvmlDeviceGetGraphicsRunningProcesses)
NV_FUNC(nvmlDeviceGetMPSComputeRunningProcesses)
#undef NV_FUNC
#undef NV_STR

        // initialize NVML here
        if (!failed)
        {
            auto ret = funcs.nvmlInit();
            if (ret != NVML_SUCCESS)
            {
                qCWarning(KSYSGUARD_PLUGIN_NVIDIA) << "ERROR: Failed to initialize NVML, nvmlInit() returned " << ret;
                failed = true;
            }
        }

        if (failed) {
            m_lib.unload();
            return false;
        } else {
            m_funcs = funcs;
            return true;
        }
    }

    const nvmlFuncs &getFuncs() const
    {
        assert(isLoaded());
        return m_funcs;
    }

private:
    void unloadLib()
    {
        if (isLoaded())
        {
            assert(getFuncs().nvmlShutdown != nullptr);
            getFuncs().nvmlShutdown();
            m_funcs = nvmlFuncs{};
            m_lib.unload();
        }
    }

    QLibrary m_lib;
    nvmlFuncs m_funcs;
};

class NvidiaPlugin::nvmlDetail {
    struct processInfo {
        unsigned int gpuUtil;
        unsigned long long memUsage;
    };

public:
    explicit nvmlDetail(const NvidiaPlugin &plugin, const nvmlLib &lib)
        : m_plugin(plugin)
        , m_lib(lib)
        , m_pidListSelect(false)
        , m_bufferSize(0)
    {
    }

    bool loadGpus()
    {
        clearGpus();

        unsigned int num;
        auto ret = getFuncs().nvmlDeviceGetCount(&num);
        if (ret != NVML_SUCCESS)
        {
            return false;
        }

        m_gpus.reserve(num);
        m_timestamps.reserve(num);

        for (unsigned int i = 0; i < num; i++)
        {
            nvmlDevice_t gpu;
            ret = getFuncs().nvmlDeviceGetHandleByIndex(i, &gpu);
            if (ret != NVML_SUCCESS)
            {
                clearGpus();
                return false;
            }

            m_gpus.push_back(gpu);
            m_timestamps.push_back(0);
        }

        return true;
    }

    void clearGpus()
    {
        m_gpus.clear();
        m_timestamps.clear();
    }

    void update()
    {
        m_pidListSelect = !m_pidListSelect;
        m_infoList.clear();
        pidsNow().clear();

        for (unsigned int i = 0; i < m_gpus.size(); i++)
        {
            auto &gpu = m_gpus[i];
            auto &timestamp = m_timestamps[i];
            queryUtilization(gpu, timestamp);
            queryMemoryUsage(gpu);
        }

        m_pidListDelta.clear();
        std::set_difference(
            pidsOld().cbegin(), pidsOld().cend(),
            pidsNow().cbegin(), pidsNow().cend(),
            std::inserter(m_pidListDelta, m_pidListDelta.begin())
        );
    }

    unsigned int numGpus() const
    {
        return m_gpus.size();
    }

    void removedPids(std::function<void(unsigned int pid)> &&callback)
    {
        for (auto pid : m_pidListDelta)
        {
            callback(pid);
        }
    }

    void setUsage(std::function<void(unsigned int pid, unsigned int usage, unsigned long long memory)> &&callback)
    {
        for (auto &item : m_infoList)
        {
            callback(item.first, item.second.gpuUtil, item.second.memUsage);
        }
    }

private:

    const nvmlLib::nvmlFuncs &getFuncs()
    {
        return m_lib.getFuncs();
    }

    template <typename T>
    size_t bufferSize() const
    {
        return m_bufferSize / sizeof(T);
    }

    template <typename T>
    T *reserveBuffer(unsigned int count)
    {
        count *= sizeof(T);
        if (count > m_bufferSize)
        {
            m_buffer = std::unique_ptr<char[]>(new char[count]);
            m_bufferSize = count;
        }
        return reinterpret_cast<T*>(&m_buffer[0]);
    }

    std::set<unsigned int> &pidsNow()
    {
        return m_pidListSelect ? m_pidList1 : m_pidList2;
    }

    std::set<unsigned int> &pidsOld()
    {
        return m_pidListSelect ? m_pidList2 : m_pidList1;
    }

    processInfo &getInfo(unsigned int pid)
    {
        // process is using GPU at the moment
        pidsNow().insert(pid);

        // find or insert into list
        return m_infoList[pid];
    }

    void queryUtilization(nvmlDevice_t gpu, unsigned long long &timestamp)
    {
        unsigned int num;
        auto ret = getFuncs().nvmlDeviceGetProcessUtilization(gpu, nullptr, &num, timestamp);
        if (ret != NVML_ERROR_INSUFFICIENT_SIZE)
        {
            return;
        }

        auto *buf = reserveBuffer<nvmlProcessUtilizationSample_t>(num);
        num = bufferSize<nvmlProcessUtilizationSample_t>();

        ret = getFuncs().nvmlDeviceGetProcessUtilization(gpu, buf, &num, timestamp);
        if (ret != NVML_SUCCESS)
        {
            return;
        }

        for (unsigned int i = 0; i < num; i++)
        {
            auto &item = buf[i];
            auto &info = getInfo(item.pid);
            double usage = static_cast<double>(item.smUtil);
            if (m_plugin.isNormalizedGPUUsage())
            {
                usage /= static_cast<double>(m_gpus.size());
            }
            info.gpuUtil = usage + 0.5;
            if (timestamp < item.timeStamp)
            {
                timestamp = item.timeStamp;
            }
        }
    }

    void queryMemoryUsage(nvmlDevice_t gpu)
    {
        for (auto &func : { getFuncs().nvmlDeviceGetComputeRunningProcesses, getFuncs().nvmlDeviceGetGraphicsRunningProcesses, getFuncs().nvmlDeviceGetMPSComputeRunningProcesses })
        {
            unsigned int num = 0;
            auto ret = func(gpu, &num, nullptr);
            if (ret != NVML_ERROR_INSUFFICIENT_SIZE)
            {
                continue;
            }

            auto *buf = reserveBuffer<nvmlProcessInfo_t>(num);
            num = bufferSize<nvmlProcessInfo_t>();

            ret = func(gpu, &num, buf);
            if (ret != NVML_SUCCESS)
            {
                continue;
            }

            for (unsigned int i = 0; i < num; i++)
            {
                auto &item = buf[i];
                if (item.usedGpuMemory == NVML_VALUE_NOT_AVAILABLE)
                {
                    continue;
                }
                auto &info = getInfo(item.pid);
                info.memUsage += item.usedGpuMemory;
            }
        }
    }

    const NvidiaPlugin &m_plugin;
    const nvmlLib &m_lib;

    // used to select between m_pidList1/2 to avoid moves/reallocations
    bool m_pidListSelect;

    // buffer used for NVML operations
    unsigned int m_bufferSize;
    std::unique_ptr<char[]> m_buffer;

    std::vector<nvmlDevice_t> m_gpus;
    std::vector<unsigned long long> m_timestamps;

    // need to be sorted for std::set_difference()
    std::set<unsigned int> m_pidList1;
    std::set<unsigned int> m_pidList2;

    // doesn't need to be sorted
    std::unordered_set<unsigned int> m_pidListDelta;

    std::unordered_map<unsigned int, processInfo> m_infoList;
};

class NvidiaPlugin::usageInterface : public ExtendedProcessAttribute::ProcessModelInterface
{
public:
    explicit usageInterface(NvidiaPlugin &plugin)
        : m_plugin(plugin)
        , m_normalizeAction(nullptr)
    {}

    ~usageInterface() override
    {}

    void saveSettings(KConfigGroup &cg) override
    {
        cg.writeEntry("normalizeGPUUsage", m_plugin.isNormalizedGPUUsage());
    }

    void saveSettingsLegacy(QDomElement &element) override
    {
        element.setAttribute(QStringLiteral("normalizeGPUUsage"), m_plugin.isNormalizedGPUUsage());
    }

    void loadSettings(const KConfigGroup &cg) override
    {
        m_plugin.setNormalizedGPUUsage(cg.readEntry("normalizeGPUUsage", true));
    }

    void loadSettingsLegacy(QDomElement &element) override
    {
        int normalizeGPUUsage = element.attribute(QStringLiteral("normalizeGPUUsage"), "1").toUInt();
        m_plugin.setNormalizedGPUUsage(normalizeGPUUsage);
    }

    void setupMenu(QMenu &menu) override
    {
        m_normalizeAction = new QAction(&menu);
        m_normalizeAction->setText(i18n("Divide GPU usage by number of GPUs"));
        m_normalizeAction->setCheckable(true);
        m_normalizeAction->setChecked(m_plugin.isNormalizedGPUUsage());
        menu.addSeparator();
        menu.addAction(m_normalizeAction);
    }

    void checkMenu(QAction *action) override
    {
        if (action == m_normalizeAction)
        {
            m_plugin.setNormalizedGPUUsage(m_normalizeAction->isChecked());
        }
    }

    QVariant getTooltip() override
    {
        if (m_plugin.numGpus() == 1)
        {
            return i18n("The current GPU usage of the process.");
        }
        else if (m_plugin.isNormalizedGPUUsage())
        {
            return i18np("The current total GPU usage of the process, divided by the %1 GPU in the machine.",
                            "The current total GPU usage of the process, divided by the %1 GPUs in the machine.",
                            m_plugin.numGpus());
        }
        else
        {
            return i18n("The current total CPU usage of the process.");
        }
        
    }

    QVariant getWhatsThis() override
    {
        return i18n("<qt>The current total CPU usage of the process.");
    }
 
private:
    NvidiaPlugin &m_plugin;
    QAction *m_normalizeAction;
};

class NvidiaPlugin::memoryInterface : public ExtendedProcessAttribute::ProcessModelInterface
{
public:
    explicit memoryInterface(NvidiaPlugin &plugin)
        : m_plugin(plugin)
    {}

    QVariant getTooltip() override
    {
        return i18n("The current total VRAM allocations of the process. (graphics + compute)");
    }

    QVariant getWhatsThis() override
    {
        return i18n("<qt>The current VRAM allocations of the process.");
    }

private:
    NvidiaPlugin &m_plugin;
};

NvidiaPlugin::NvidiaPlugin(QObject *parent, const QVariantList &args)
    : ProcessDataProvider(parent, args)
    , m_normalizeUsage(true) // default to true
{
    m_nvmlLib = std::make_unique<nvmlLib>();
    if (!m_nvmlLib->tryLoad()) {
        m_nvmlLib.reset();
        return;
    }

    m_usage = new ExtendedProcessAttribute(QStringLiteral("nvidia_usage"), i18n("GPU Usage"), this);
    m_usage->setUnit(KSysGuard::UnitPercent);
    m_memory = new ExtendedProcessAttribute(QStringLiteral("nvidia_memory"), i18n("GPU Memory"), this);
    m_memory->setUnit(KSysGuard::UnitByte);

    m_usageInterface = std::make_unique<usageInterface>(*this);
    m_usage->setInterface(m_usageInterface.get());

    m_memoryInterface = std::make_unique<memoryInterface>(*this);
    m_memory->setInterface(m_memoryInterface.get());

    addProcessAttribute(m_usage);
    addProcessAttribute(m_memory);
}

NvidiaPlugin::~NvidiaPlugin()
{
}

void NvidiaPlugin::handleEnabledChanged(bool enabled)
{
    if (!m_nvmlLib)
        return;

    if (enabled) {
        m_nvmlDetail = std::make_unique<nvmlDetail>(*this, *m_nvmlLib.get());
        m_nvmlDetail->loadGpus();
    } else {
        m_nvmlDetail.reset();
    }
}

void NvidiaPlugin::update()
{
    if (!m_nvmlLib || !m_nvmlDetail)
        return;

    m_nvmlDetail->update();

    // clear usage data for processes that have stopped using GPU resources but are still alive
    m_nvmlDetail->removedPids([&](unsigned int pid){
        auto *proc = getProcess(pid);
        if (proc != nullptr)
        {
            m_usage->clearData(proc);
            m_memory->clearData(proc);
        }
    });

    // set usage data for processes with utilization / memory usage
    m_nvmlDetail->setUsage([&](unsigned int pid, unsigned int usage, unsigned long long memory){
        auto *proc = getProcess(pid);
        if (proc != nullptr)
        {
            m_usage->setData(proc, usage);
            m_memory->setData(proc, memory);
        }
    });
}

unsigned int NvidiaPlugin::numGpus()
{
    if (m_nvmlDetail)
    {
        return m_nvmlDetail->numGpus();
    }
    else
    {
        return 0;
    }
}

bool NvidiaPlugin::isNormalizedGPUUsage() const
{
    return m_normalizeUsage;
}

void NvidiaPlugin::setNormalizedGPUUsage(bool normalize)
{
    m_normalizeUsage = normalize;
}

KSysGuard::Unit NvidiaPlugin::getMemoryUnits()
{
    if (m_memory != nullptr)
    {
        return m_memory->unit();
    }
    else
    {
        return KSysGuard::UnitInvalid;
    }
}

void NvidiaPlugin::setMemoryUnits(KSysGuard::Unit unit)
{
    if (m_memory != nullptr)
    {
        return m_memory->setUnit(unit);
    }
}

K_PLUGIN_FACTORY_WITH_JSON(PluginFactory, "nvidia.json", registerPlugin<NvidiaPlugin>();)

#include "nvidia.moc"
