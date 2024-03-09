/*
 *  KSysGuard, the KDE System Guard
 *
 *  SPDX-FileCopyrightText: 2022 Eugene Popov <popov895@ukr.net>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef _GeneralTab_h_
#define _GeneralTab_h_

#include <QWidget>

class QLabel;
class QTreeWidget;

class GeneralTab : public QWidget
{
    Q_OBJECT

public:
    explicit GeneralTab(QWidget *parent = nullptr);

    void setData(const QVariantMap &data);

private:
    QTreeWidget *m_dataTreeWidget = nullptr;
    QLabel *m_placeholderLabel = nullptr;
};

#endif // _GeneralTab_h_
