/*
    KSysGuard, the KDE System Guard

    SPDX-FileCopyrightText: 2014 Gregor Mi <codestruct@posteo.org>

    SPDX-License-Identifier: LGPL-2.0-or-later

*/

#ifndef TIMEUTIL_H
#define TIMEUTIL_H

#include <cmath> // floor

#ifdef Q_OS_OSX
#include <mach/clock.h>
#include <mach/mach.h>
#else
#include <time.h>
#endif

#include <QDateTime>

#include <klocalizedstring.h> // KF5::I18n

class TimeUtil
{
public:
    /**
     * @Returns the amount of seconds passed since the system was booted
     */
    static long systemUptimeSeconds()
    {
#ifdef Q_OS_OSX
        clock_serv_t cclock;
        mach_timespec_t tp;

        host_get_clock_service(mach_host_self(), SYSTEM_CLOCK, &cclock);
        clock_get_time(cclock, &tp);
        mach_port_deallocate(mach_task_self(), cclock);
#else
        timespec tp;
#ifdef Q_OS_LINUX
        int isSuccess = clock_gettime(CLOCK_BOOTTIME, &tp);
        // _MONOTONIC doesn't increase while the system is suspended,
        // resulting in process start times in the future
#else
        int isSuccess =
            clock_gettime(CLOCK_MONOTONIC, &tp); // see https://stackoverflow.com/questions/8357073/get-uptime-in-seconds-or-miliseconds-on-unix-like-systems
#endif
        Q_ASSERT(isSuccess == 0);
#endif
        return tp.tv_sec;
    }

    /**
     * @Returns the point in time when the system was booted
     */
    static QDateTime systemUptimeAbsolute()
    {
        auto now = QDateTime::currentDateTime();
        return now.addSecs(-systemUptimeSeconds());
    }

    /**
     * Converts the given @param seconds into a human readable string.
     * It represents an elapsed time span, e.g. "3m 50s ago".
     */
    static QString secondsToHumanElapsedString(long seconds)
    {
        const int s_abs = seconds;
        const int m_abs = floor(seconds / 60.0);
        const int h_abs = floor(seconds / 60.0 / 60.0);
        const int d_abs = floor(seconds / 60.0 / 60.0 / 24.0);

        if (m_abs == 0) {
            return i18nc("contains a abbreviated time unit: (s)econds", "%1s ago", s_abs);
        } else if (h_abs == 0) {
            const int s = s_abs - m_abs * 60;
            return i18nc("contains abbreviated time units: (m)inutes and (s)econds", "%1m %2s ago", m_abs, s);
        } else if (d_abs == 0) {
            const int s = s_abs - m_abs * 60;
            const int m = m_abs - h_abs * 60;
            return i18nc("contains abbreviated time units: (h)ours, (m)inutes and (s)econds)", "%1h %2m %3s ago", h_abs, m, s);
        } else { // d_abs > 0
            const int m = m_abs - h_abs * 60;
            const int h = h_abs - d_abs * 24;
            return i18ncp("contains also abbreviated time units: (h)ours and (m)inutes", "%1 day %2h %3m ago", "%1 days %2h %3m ago", d_abs, h, m);
        }
    }
};

#endif
