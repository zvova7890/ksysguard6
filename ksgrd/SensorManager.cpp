/*
    KSysGuard, the KDE System Guard

    SPDX-FileCopyrightText: 1999-2001 Chris Schlaeger <cs@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

*/

#include "ksgrd_debug.h"
#include <QCoreApplication>
#include <QDebug>
#include <kconfiggroup.h>
#include <klocalizedstring.h>

#include "SensorShellAgent.h"
#include "SensorSocketAgent.h"

#include "SensorManager.h"

using namespace KSGRD;

SensorManager::MessageEvent::MessageEvent(const QString &message)
    : QEvent(QEvent::User)
    , mMessage(message)
{
}

QString SensorManager::MessageEvent::message() const
{
    return mMessage;
}

SensorManager *KSGRD::SensorMgr;

SensorManager::SensorManager(QObject *parent)
    : QObject(parent)
{
    retranslate();
}
int SensorManager::count() const
{
    return mAgents.count();
}
void SensorManager::retranslate()
{
    // Fill the sensor description dictionary.
    mDict.clear();
    mDict.insert(QStringLiteral("Delta"), i18n("Change"));
    mDict.insert(QStringLiteral("Rate"), i18n("Rate"));

    mDict.insert(QStringLiteral("cpu"), i18n("CPU Load"));
    mDict.insert(QStringLiteral("idle"), i18n("Idling"));
    mDict.insert(QStringLiteral("nice"), i18n("Nice Load"));
    mDict.insert(QStringLiteral("user"), i18n("User Load"));
    mDict.insert(QStringLiteral("sys"), i18nc("@item sensor description", "System Load"));
    mDict.insert(QStringLiteral("wait"), i18n("Waiting"));
    mDict.insert(QStringLiteral("intr"), i18n("Interrupt Load"));
    mDict.insert(QStringLiteral("TotalLoad"), i18n("Total Load"));

    mDict.insert(QStringLiteral("mem"), i18n("Memory"));
    mDict.insert(QStringLiteral("physical"), i18n("Physical Memory"));
    mDict.insert(QStringLiteral("total"), i18n("Total Memory"));
    mDict.insert(QStringLiteral("swap"), i18n("Swap Memory"));
    mDict.insert(QStringLiteral("cached"), i18n("Cached Memory"));
    mDict.insert(QStringLiteral("buf"), i18n("Buffered Memory"));
    mDict.insert(QStringLiteral("used"), i18n("Used Memory"));
    mDict.insert(QStringLiteral("application"), i18n("Application Memory"));
    mDict.insert(QStringLiteral("allocated"), i18n("Allocated Memory"));
    mDict.insert(QStringLiteral("free"), i18n("Free Memory"));
    mDict.insert(QStringLiteral("available"), i18n("Available Memory"));
    mDict.insert(QStringLiteral("active"), i18n("Active Memory"));
    mDict.insert(QStringLiteral("inactive"), i18n("Inactive Memory"));
    mDict.insert(QStringLiteral("wired"), i18n("Wired Memory"));
    mDict.insert(QStringLiteral("execpages"), i18n("Exec Pages"));
    mDict.insert(QStringLiteral("filepages"), i18n("File Pages"));

    /* Processes */
    mDict.insert(QStringLiteral("processes"), i18n("Processes"));
    mDict.insert(QStringLiteral("ps"), i18n("Process Controller"));
    mDict.insert(QStringLiteral("lastpid"), i18n("Last Process ID"));
    mDict.insert(QStringLiteral("procspawn"), i18n("Process Spawn Count"));
    mDict.insert(QStringLiteral("pscount"), i18n("Process Count"));
    mDict.insert(QStringLiteral("psidle"), i18n("Idle Processes Count"));
    mDict.insert(QStringLiteral("psrun"), i18n("Running Processes Count"));
    mDict.insert(QStringLiteral("pssleep"), i18n("Sleeping Processes Count"));
    mDict.insert(QStringLiteral("psstop"), i18n("Stopped Processes Count"));
    mDict.insert(QStringLiteral("pszombie"), i18n("Zombie Processes Count"));
    mDict.insert(QStringLiteral("pswait"), i18n("Waiting Processes Count"));
    mDict.insert(QStringLiteral("pslock"), i18n("Locked Processes Count"));

    mDict.insert(QStringLiteral("disk"), i18n("Disk Throughput"));
    mDict.insert(QStringLiteral("load"), i18nc("CPU Load", "Load"));
    mDict.insert(QStringLiteral("totalio"), i18n("Total Accesses"));
    mDict.insert(QStringLiteral("rio"), i18n("Read Accesses"));
    mDict.insert(QStringLiteral("wio"), i18n("Write Accesses"));
    mDict.insert(QStringLiteral("rblk"), i18n("Read Data"));
    mDict.insert(QStringLiteral("wblk"), i18n("Written Data"));
    mDict.insert(QStringLiteral("rtim"), i18n("Milliseconds spent reading"));
    mDict.insert(QStringLiteral("wtim"), i18n("Milliseconds spent writing"));
    mDict.insert(QStringLiteral("ioqueue"), i18n("I/Os currently in progress"));
    mDict.insert(QStringLiteral("pageIn"), i18n("Pages In"));
    mDict.insert(QStringLiteral("pageOut"), i18n("Pages Out"));
    mDict.insert(QStringLiteral("context"), i18n("Context Switches"));
    mDict.insert(QStringLiteral("trap"), i18n("Traps"));
    mDict.insert(QStringLiteral("syscall"), i18n("System Calls"));
    mDict.insert(QStringLiteral("network"), i18n("Network"));
    mDict.insert(QStringLiteral("interfaces"), i18n("Interfaces"));
    mDict.insert(QStringLiteral("receiver"), i18n("Receiver"));
    mDict.insert(QStringLiteral("transmitter"), i18n("Transmitter"));

    mDict.insert(QStringLiteral("data"), i18n("Data Rate"));
    mDict.insert(QStringLiteral("compressed"), i18n("Compressed Packets Rate"));
    mDict.insert(QStringLiteral("drops"), i18n("Dropped Packets Rate"));
    mDict.insert(QStringLiteral("errors"), i18n("Error Rate"));
    mDict.insert(QStringLiteral("fifo"), i18n("FIFO Overruns Rate"));
    mDict.insert(QStringLiteral("frame"), i18n("Frame Error Rate"));
    mDict.insert(QStringLiteral("multicast"), i18n("Multicast Packet Rate"));
    mDict.insert(QStringLiteral("packets"), i18n("Packet Rate"));
    mDict.insert(QStringLiteral("carrier"), i18nc("@item sensor description ('carrier' is a type of network signal)", "Carrier Loss Rate"));
    mDict.insert(QStringLiteral("collisions"), i18n("Collisions"));

    mDict.insert(QStringLiteral("dataTotal"), i18n("Data"));
    mDict.insert(QStringLiteral("compressedTotal"), i18n("Compressed Packets"));
    mDict.insert(QStringLiteral("dropsTotal"), i18n("Dropped Packets"));
    mDict.insert(QStringLiteral("errorsTotal"), i18n("Errors"));
    mDict.insert(QStringLiteral("fifoTotal"), i18n("FIFO Overruns"));
    mDict.insert(QStringLiteral("frameTotal"), i18n("Frame Errors"));
    mDict.insert(QStringLiteral("multicastTotal"), i18n("Multicast Packets"));
    mDict.insert(QStringLiteral("packetsTotal"), i18n("Packets"));
    mDict.insert(QStringLiteral("carrierTotal"), i18nc("@item sensor description ('carrier' is a type of network signal)", "Carrier Losses"));
    mDict.insert(QStringLiteral("collisionsTotal"), i18n("Collisions"));

    /* Hardware monitors */
    mDict.insert(QStringLiteral("sockets"), i18n("Sockets"));
    mDict.insert(QStringLiteral("count"), i18n("Total Number"));
    mDict.insert(QStringLiteral("list"), i18n("Table"));
    mDict.insert(QStringLiteral("apm"), i18n("Advanced Power Management"));
    mDict.insert(QStringLiteral("acpi"), i18n("ACPI"));
    mDict.insert(QStringLiteral("Cooling_Device"), i18n("Cooling Device"));
    mDict.insert(QStringLiteral("Current_State"), i18n("Current State"));
    mDict.insert(QStringLiteral("thermal_zone"), i18n("Thermal Zone"));
    mDict.insert(QStringLiteral("Thermal_Zone"), i18n("Thermal Zone"));
    mDict.insert(QStringLiteral("temperature"), i18n("Temperature"));
    mDict.insert(QStringLiteral("Temperature"), i18n("Temperature"));
    mDict.insert(QStringLiteral("AverageTemperature"), i18n("Average CPU Temperature"));
    mDict.insert(QStringLiteral("fan"), i18n("Fan"));
    mDict.insert(QStringLiteral("state"), i18n("State"));
    mDict.insert(QStringLiteral("battery"), i18n("Battery"));
    mDict.insert(QStringLiteral("batterycapacity"), i18n("Battery Capacity"));
    mDict.insert(QStringLiteral("batterycharge"), i18n("Battery Charge"));
    mDict.insert(QStringLiteral("batteryusage"), i18n("Battery Usage"));
    mDict.insert(QStringLiteral("batteryvoltage"), i18n("Battery Voltage"));
    mDict.insert(QStringLiteral("batteryrate"), i18n("Battery Discharge Rate"));
    mDict.insert(QStringLiteral("remainingtime"), i18n("Remaining Time"));
    mDict.insert(QStringLiteral("interrupts"), i18n("Interrupts"));
    mDict.insert(QStringLiteral("loadavg1"), i18n("Load Average (1 min)"));
    mDict.insert(QStringLiteral("loadavg5"), i18n("Load Average (5 min)"));
    mDict.insert(QStringLiteral("loadavg15"), i18n("Load Average (15 min)"));
    mDict.insert(QStringLiteral("clock"), i18n("Clock Frequency"));
    mDict.insert(QStringLiteral("AverageClock"), i18n("Average Clock Frequency"));
    mDict.insert(QStringLiteral("lmsensors"), i18n("Hardware Sensors"));
    mDict.insert(QStringLiteral("partitions"), i18n("Partition Usage"));
    mDict.insert(QStringLiteral("usedspace"), i18n("Used Space"));
    mDict.insert(QStringLiteral("freespace"), i18n("Free Space"));
    mDict.insert(QStringLiteral("filllevel"), i18n("Fill Level"));
    mDict.insert(QStringLiteral("usedinode"), i18n("Used Inodes"));
    mDict.insert(QStringLiteral("freeinode"), i18n("Free Inodes"));
    mDict.insert(QStringLiteral("inodelevel"), i18n("Inode Level"));
    mDict.insert(QStringLiteral("system"), i18n("System"));
    mDict.insert(QStringLiteral("uptime"), i18n("Uptime"));
    mDict.insert(QStringLiteral("SoftRaid"), i18n("Linux Soft Raid (md)"));
    mDict.insert(QStringLiteral("processors"), i18n("Processors"));
    mDict.insert(QStringLiteral("cores"), i18n("Cores"));
    mDict.insert(QStringLiteral("NumBlocks"), i18n("Number of Blocks"));
    mDict.insert(QStringLiteral("TotalDevices"), i18n("Total Number of Devices"));
    mDict.insert(QStringLiteral("FailedDevices"), i18n("Failed Devices"));
    mDict.insert(QStringLiteral("SpareDevices"), i18n("Spare Devices"));
    mDict.insert(QStringLiteral("NumRaidDevices"), i18n("Number of Raid Devices"));
    mDict.insert(QStringLiteral("WorkingDevices"), i18n("Working Devices"));
    mDict.insert(QStringLiteral("ActiveDevices"), i18n("Active Devices"));
    mDict.insert(QStringLiteral("DeviceNumber"), i18n("Number of Devices"));
    mDict.insert(QStringLiteral("ResyncingPercent"), i18n("Resyncing Percent"));
    mDict.insert(QStringLiteral("DiskInfo"), i18n("Disk Information"));
    mDict.insert(QStringLiteral("CPUTIN"), i18n("CPU Temperature"));
    mDict.insert(QStringLiteral("SYSTIN"), i18n("Motherboard Temperature"));
    mDict.insert(QStringLiteral("AUXTIN"), i18n("Power Supply Temperature"));

    mDict.insert(QStringLiteral("__root__"), i18n("Filesystem Root"));

    for (int i = 0; i < 5; i++) {
        mDict.insert(QLatin1String("AUXTIN") + QString::number(i), i18n("Extra Temperature Sensor %1", i + 1));
    }

    for (int i = 0; i < 3; i++) {
        mDict.insert(QLatin1String("PECI Agent ") + QString::number(i), i18n("PECI Temperature Sensor %1", i + 1));
        mDict.insert(QLatin1String("PECI Agent %1 Calibration").arg(QString::number(i)), i18n("PECI Temperature Calibration %1", i + 1));
    }

    for (int i = 0; i < 32; i++) {
        mDict.insert(QLatin1String("cpu") + QString::number(i), i18n("CPU %1", i + 1));
        mDict.insert(QLatin1String("disk") + QString::number(i), i18n("Disk %1", i + 1));
    }

    for (int i = 1; i < 10; i++) {
        mDict.insert(QLatin1String("batt") + QString::number(i), i18n("Battery %1", i));
        mDict.insert(QLatin1String("fan") + QString::number(i), i18n("Fan %1", i));
        mDict.insert(QLatin1String("temp") + QString::number(i), i18n("Temperature %1", i));
    }

    mDict.insert(QStringLiteral("int00"), i18n("Total"));
    mDict.insert(QStringLiteral("softint"), i18n("Software Interrupts"));
    mDict.insert(QStringLiteral("hardint"), i18n("Hardware Interrupts"));

    QString num;
    for (int i = 1; i < 25; i++) {
        num = QString::asprintf("%.2d", i);
        mDict.insert(QLatin1String("int") + num, ki18n("Int %1").subs(i - 1, 3).toString());
        num = QString::asprintf("%.3d", i + 255);
        mDict.insert(QLatin1String("int") + num, ki18n("Int %1").subs(i + 255, 4).toString());
    }

    mDict.insert(QStringLiteral("quality"), i18n("Link Quality"));
    mDict.insert(QStringLiteral("signal"), i18n("Signal Level"));
    mDict.insert(QStringLiteral("noise"), i18n("Noise Level"));
    mDict.insert(QStringLiteral("nwid"), i18n("Rx Invalid Nwid Packets"));
    mDict.insert(QStringLiteral("nwidTotal"), i18n("Total Rx Invalid Nwid Packets"));
    mDict.insert(QStringLiteral("crypt"), i18n("Rx Invalid Crypt Packets"));
    mDict.insert(QStringLiteral("cryptTotal"), i18n("Total Rx Invalid Crypt Packets"));
    mDict.insert(QStringLiteral("frag"), i18n("Rx Invalid Frag Packets"));
    mDict.insert(QStringLiteral("fragTotal"), i18n("Total Rx Invalid Frag Packets"));
    mDict.insert(QStringLiteral("retry"), i18n("Tx Excessive Retries Packets"));
    mDict.insert(QStringLiteral("retryTotal"), i18n("Total Tx Excessive Retries Packets"));
    mDict.insert(QStringLiteral("misc"), i18n("Invalid Misc Packets"));
    mDict.insert(QStringLiteral("miscTotal"), i18n("Total Invalid Misc Packets"));
    mDict.insert(QStringLiteral("beacon"), i18n("Missed Beacons"));
    mDict.insert(QStringLiteral("beaconTotal"), i18n("Total Missed Beacons"));

    mDict.insert(QStringLiteral("logfiles"), i18n("Log Files"));

    // TODO: translated descriptions not yet implemented.
    mUnits.clear();
    mUnits.insert(QStringLiteral("1/s"), i18nc("the unit 1 per second", "1/s"));
    mUnits.insert(QStringLiteral("kBytes"), i18n("kBytes"));
    mUnits.insert(QStringLiteral("min"), i18nc("the unit minutes", "min"));
    mUnits.insert(QStringLiteral("MHz"), i18nc("the frequency unit", "MHz"));
    mUnits.insert(QStringLiteral("%"), i18nc("a percentage", "%"));
    mUnits.insert(QStringLiteral("mA"), i18nc("the unit milliamperes", "mA"));
    mUnits.insert(QStringLiteral("mAh"), i18nc("the unit milliampere hours", "mAh"));
    mUnits.insert(QStringLiteral("mW"), i18nc("the unit milliwatts", "mW"));
    mUnits.insert(QStringLiteral("mWh"), i18nc("the unit milliwatt hours", "mWh"));
    mUnits.insert(QStringLiteral("mV"), i18nc("the unit millivolts", "mV"));

    mTypes.clear();
    mTypes.insert(QStringLiteral("integer"), i18n("Integer Value"));
    mTypes.insert(QStringLiteral("float"), i18n("Floating Point Value"));
    mTypes.insert(QStringLiteral("table"), i18n("Process Controller"));
    mTypes.insert(QStringLiteral("listview"), i18n("Table"));
    mTypes.insert(QStringLiteral("logfile"), i18n("Log File"));

    mBroadcaster = nullptr;
}

SensorManager::~SensorManager()
{
}

bool SensorManager::engage(const QString &hostName, const QString &shell, const QString &command, int port)
{
    if (!mAgents.contains(hostName)) {
        SensorAgent *agent = nullptr;

        if (port == -1)
            agent = new SensorShellAgent(this);
        else
            agent = new SensorSocketAgent(this);

        if (!agent->start(hostName.toLatin1(), shell, command, port)) {
            delete agent;
            return false;
        }

        mAgents.insert(hostName, agent);
        connect(agent, &SensorAgent::reconfigure, this, &SensorManager::reconfigure);

        Q_EMIT hostAdded(agent, hostName);
        return true;
    }

    return false;
}

bool SensorManager::disengage(SensorAgent *agent)
{
    if (!agent)
        return false;
    const QString key = mAgents.key(const_cast<SensorAgent *>(agent));
    return disengage(key);
}

bool SensorManager::isConnected(const QString &hostName)
{
    return mAgents.contains(hostName);
}
bool SensorManager::disengage(const QString &hostName)
{
    if (mAgents.contains(hostName)) {
        mAgents.take(hostName)->deleteLater();

        Q_EMIT hostConnectionLost(hostName);
        return true;
    }

    return false;
}

bool SensorManager::resynchronize(const QString &hostName)
{
    const SensorAgent *agent = mAgents.value(hostName);

    if (!agent)
        return false;

    QString shell, command;
    int port;
    hostInfo(hostName, shell, command, port);

    mAgents.remove(hostName);

    qCDebug(LIBKSYSGUARD_KSGRD) << "Re-synchronizing connection to " << hostName;

    return engage(hostName, shell, command);
}

void SensorManager::notify(const QString &msg) const
{
    /* This function relays text messages to the toplevel widget that
     * displays the message in a pop-up box. It must be used for objects
     * that might have been deleted before the pop-up box is closed. */
    if (mBroadcaster) {
        MessageEvent *event = new MessageEvent(msg);
        qApp->postEvent(mBroadcaster, event);
    }
}

void SensorManager::setBroadcaster(QObject *wdg)
{
    mBroadcaster = wdg;
}

void SensorManager::reconfigure(const SensorAgent *)
{
    Q_EMIT update();
}

bool SensorManager::sendRequest(const QString &hostName, const QString &req, SensorClient *client, int id)
{
    SensorAgent *agent = mAgents.value(hostName);
    if (!agent && hostName == QLatin1String("localhost")) {
        // we should always be able to reconnect to localhost
        engage(QStringLiteral("localhost"), QLatin1String(""), QStringLiteral("ksysguardd"), -1);
        agent = mAgents.value(hostName);
    }
    if (agent) {
        agent->sendRequest(req, client, id);
        return true;
    }

    return false;
}

const QString SensorManager::hostName(const SensorAgent *agent) const
{
    return mAgents.key(const_cast<SensorAgent *>(agent));
}

bool SensorManager::hostInfo(const QString &hostName, QString &shell, QString &command, int &port)
{
    const SensorAgent *agent = mAgents.value(hostName);
    if (agent) {
        agent->hostInfo(shell, command, port);
        return true;
    }

    return false;
}

QString SensorManager::translateUnit(const QString &unit) const
{
    if (!unit.isEmpty() && mUnits.contains(unit))
        return mUnits[unit];
    else
        return unit;
}

QString SensorManager::translateSensorPath(const QString &path) const
{
    if (!path.isEmpty() && mDict.contains(path))
        return mDict[path];
    else
        return path;
}

QString SensorManager::translateSensorType(const QString &type) const
{
    if (!type.isEmpty() && mTypes.contains(type))
        return mTypes[type];
    else
        return type;
}

QString SensorManager::translateSensor(const QString &sensor) const
{
    QString out;
    int start = 0, end = 0;
    for (;;) {
        end = sensor.indexOf(QLatin1Char('/'), start);
        if (end > 0)
            out += translateSensorPath(sensor.mid(start, end - start)) + QLatin1Char('/');
        else {
            out += translateSensorPath(sensor.right(sensor.length() - start));
            break;
        }
        start = end + 1;
    }

    return out;
}

void SensorManager::readProperties(const KConfigGroup &cfg)
{
    mHostList = cfg.readEntry("HostList", QStringList());
    mCommandList = cfg.readEntry("CommandList", QStringList());
}

void SensorManager::saveProperties(KConfigGroup &cfg)
{
    cfg.writeEntry("HostList", mHostList);
    cfg.writeEntry("CommandList", mCommandList);
}

void SensorManager::disconnectClient(SensorClient *client)
{
    QHashIterator<QString, SensorAgent *> it(mAgents);

    while (it.hasNext())
        it.next().value()->disconnectClient(client);
}
