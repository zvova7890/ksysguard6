/*
    SPDX-FileCopyrightText: 2020 Arjen Hiemstra <ahiemstra@heimr.nl>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "FormatterWrapper.h"

#include "Formatter.h"

namespace KSysGuard
{
QString FormatterWrapper::formatValue(const QVariant &value, KSysGuard::Unit unit, KSysGuard::MetricPrefix targetPrefix)
{
    return Formatter::formatValue(value, unit, targetPrefix);
}

QString FormatterWrapper::formatValueShowNull(const QVariant &value, KSysGuard::Unit unit, KSysGuard::MetricPrefix targetPrefix)
{
    return Formatter::formatValue(value, unit, targetPrefix, FormatOptionShowNull);
}

qreal KSysGuard::FormatterWrapper::maximumLength(KSysGuard::Unit unit, const QFont &font)
{
    return Formatter::maximumLength(unit, font);
}

}
