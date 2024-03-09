/*
 *  KSysGuard, the KDE System Guard
 *
 *  SPDX-FileCopyrightText: 2022 Eugene Popov <popov895@ukr.net>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef _MemoryMapsTab_h_
#define _MemoryMapsTab_h_

#include <QWidget>

class QLabel;
class QLineEdit;
class QSortFilterProxyModel;

class KMessageWidget;

class MemoryMapsModel;

class MemoryMapsTab : public QWidget
{
    Q_OBJECT

public:
    explicit MemoryMapsTab(QWidget *parent = nullptr);

    void setProcessId(long processId);

private Q_SLOTS:
    void refresh();

    void onProxyModelChanged();
    void onSearchEditEditingFinished();

private:
    long m_processId = 0;
    MemoryMapsModel *m_dataModel = nullptr;
    QSortFilterProxyModel *m_proxyModel = nullptr;

    KMessageWidget *m_errorWidget = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    QLabel *m_placeholderLabel = nullptr;
};

#endif // _MemoryMapsTab_h_
