/*
    SPDX-FileCopyrightText: 2020 Arjen Hiemstra <ahiemstra@heimr.nl>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "FormatterPlugin.h"

#include "FormatterWrapper.h"
#include "Unit.h"

#include <QQmlEngine>

using namespace KSysGuard;

void FormatterPlugin::registerTypes(const char *uri)
{
    Q_ASSERT(QLatin1String(uri) == QLatin1String("org.kde.ksysguard.formatter"));

    qRegisterMetaType<KSysGuard::Unit>();
    qRegisterMetaType<KSysGuard::MetricPrefix>();
    qmlRegisterSingletonType<KSysGuard::FormatterWrapper>(uri, 1, 0, "Formatter", [](QQmlEngine *, QJSEngine *) -> QObject * {
        return new FormatterWrapper();
    });
    qmlRegisterUncreatableMetaObject(KSysGuard::staticMetaObject, uri, 1, 0, "Units", QStringLiteral("Contains unit enums"));
}
