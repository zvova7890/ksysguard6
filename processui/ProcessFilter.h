/*
    KSysGuard, the KDE System Guard

    SPDX-FileCopyrightText: 1999, 2000 Chris Schlaeger <cs@kde.org>
    SPDX-FileCopyrightText: 2006 John Tapsell <john.tapsell@kdemail.net>

    SPDX-License-Identifier: LGPL-2.0-or-later

*/

#ifndef PROCESSFILTER_H_
#define PROCESSFILTER_H_

#include <QObject>
#include <QSortFilterProxyModel>

class QModelIndex;

#ifdef Q_OS_WIN
// this workaround is needed to make krunner link under msvc
// please keep it this way even if you port this library to have a _export.h header file
#define KSYSGUARD_EXPORT
#else
#define KSYSGUARD_EXPORT Q_DECL_EXPORT
#endif

class KSYSGUARD_EXPORT ProcessFilter : public QSortFilterProxyModel
{
    Q_OBJECT
    Q_ENUMS(State)

public:
    enum State { AllProcesses = 0, AllProcessesInTreeForm, SystemProcesses, UserProcesses, OwnProcesses, ProgramsOnly };
    explicit ProcessFilter(QObject *parent = nullptr)
        : QSortFilterProxyModel(parent)
    {
        mFilter = AllProcesses;
    }
    ~ProcessFilter() override
    {
    }
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;
    State filter() const
    {
        return mFilter;
    }

public Q_SLOTS:
    void setFilter(State index);

protected:
    bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;

    State mFilter;
};

#endif
