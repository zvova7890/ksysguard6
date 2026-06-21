/*
    SPDX-FileCopyrightText: 2026 Juho Ovaska <ovaska.juho@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "extended_process_attribute.h"
#include <qvariant.h>

using namespace KSysGuard;

class Q_DECL_HIDDEN KSysGuard::ExtendedProcessAttribute::Private
{
public:
    ExtendedProcessAttribute::ProcessModelInterface stub;
    ExtendedProcessAttribute::ProcessModelInterface *iface = &stub;
};

ExtendedProcessAttribute::ProcessModelInterface::~ProcessModelInterface()
{
}

void ExtendedProcessAttribute::ProcessModelInterface::saveSettings(KConfigGroup &cg)
{
    (void)cg;
}

void ExtendedProcessAttribute::ProcessModelInterface::saveSettingsLegacy(QDomElement &element)
{
    (void)element;
}

void ExtendedProcessAttribute::ProcessModelInterface::loadSettings(const KConfigGroup &cg)
{
    (void)cg;
}

void ExtendedProcessAttribute::ProcessModelInterface::loadSettingsLegacy(QDomElement &element)
{
    (void)element;
}

void ExtendedProcessAttribute::ProcessModelInterface::setupMenu(QMenu &menu)
{
    (void)menu;
}

void ExtendedProcessAttribute::ProcessModelInterface::checkMenu(QAction *action)
{
    (void)action;
}

QVariant ExtendedProcessAttribute::ProcessModelInterface::getTooltip()
{
    return QVariant();
}

QVariant ExtendedProcessAttribute::ProcessModelInterface::getWhatsThis()
{
    return QVariant();
}

ExtendedProcessAttribute::ExtendedProcessAttribute(const QString &id, QObject *parent)
    : ProcessAttribute(id, QString(), parent)
    , d(new Private)
{
}

ExtendedProcessAttribute::ExtendedProcessAttribute(const QString &id, const QString &name, QObject *parent)
    : ProcessAttribute(id, name, parent)
    , d(new Private)
{
}

ExtendedProcessAttribute::~ExtendedProcessAttribute()
{
}

void ExtendedProcessAttribute::setInterface(ProcessModelInterface *iface)
{
    if (iface != nullptr) {
        d->iface = iface;
    } else {
        d->iface = &d->stub;
    }
}

void ExtendedProcessAttribute::saveSettings(KConfigGroup &cg)
{
    d->iface->saveSettings(cg);
}

void ExtendedProcessAttribute::saveSettingsLegacy(QDomElement &element)
{
    d->iface->saveSettingsLegacy(element);
}

void ExtendedProcessAttribute::loadSettings(const KConfigGroup &cg)
{
    d->iface->loadSettings(cg);
}

void ExtendedProcessAttribute::loadSettingsLegacy(QDomElement &element)
{
    d->iface->loadSettingsLegacy(element);
}


void ExtendedProcessAttribute::setupMenu(QMenu &menu)
{
    d->iface->setupMenu(menu);
}

void ExtendedProcessAttribute::checkMenu(QAction *action)
{
    d->iface->checkMenu(action);
}

QVariant ExtendedProcessAttribute::getTooltip()
{
    return d->iface->getTooltip();
}

QVariant ExtendedProcessAttribute::getWhatsThis()
{
    return d->iface->getWhatsThis();
}