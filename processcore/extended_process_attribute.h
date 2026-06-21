/*
    SPDX-FileCopyrightText: 2026 Juho Ovaska <ovaska.juho@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/
#pragma once

#include "process_attribute.h"

#include <QScopedPointer>

class QMenu;
class QAction;
class QDomElement;
class KConfigGroup;

namespace KSysGuard
{

class PROCESSCORE_EXPORT ExtendedProcessAttribute : public ProcessAttribute
{
public:
    class ProcessModelInterface
    {
    public:
        virtual ~ProcessModelInterface();
        virtual void saveSettings(KConfigGroup &cg);
        virtual void saveSettingsLegacy(QDomElement &cg);
        virtual void loadSettings(const KConfigGroup &cg);
        virtual void loadSettingsLegacy(QDomElement &cg);
        virtual void setupMenu(QMenu &menu);
        virtual void checkMenu(QAction *action);
        virtual QVariant getTooltip();
        virtual QVariant getWhatsThis();
    };

    ExtendedProcessAttribute(const QString &id, QObject *parent);
    ExtendedProcessAttribute(const QString &id, const QString &name, QObject *parent);

    ~ExtendedProcessAttribute() override;

    // sets up the interface between process attribute and the process model
    void setInterface(ProcessModelInterface *iface);

    // saves plugin settings to config group
    void saveSettings(KConfigGroup &cg);

    // saves plugin settings to XML element
    void saveSettingsLegacy(QDomElement &element);

    // loads plugin settings from config group
    void loadSettings(const KConfigGroup &cg);

    // loads plugin settings from XML element
    void loadSettingsLegacy(QDomElement &element);

    // sets up column context menu
    void setupMenu(QMenu &menu);

    // checks menu action
    void checkMenu(QAction *action);

    // returns the tooltip shown when column is hovered over
    QVariant getTooltip();

    // returns the Qt::WhatsThisRole decription
    QVariant getWhatsThis();

private:
    class Private;
    QScopedPointer<Private> d;
};

}
