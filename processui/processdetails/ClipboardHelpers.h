/*
 *  KSysGuard, the KDE System Guard
 *
 *  SPDX-FileCopyrightText: 2026 Volodymyr Zolotopupov <zvova7890@gmail.com>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef CLIPBOARDHELPERS_H
#define CLIPBOARDHELPERS_H

#include <QApplication>
#include <QClipboard>
#include <QModelIndex>
#include <QString>

namespace ProcessDetails
{
inline QString indexesToText(const QModelIndexList &indexes)
{
    QString text;
    int previousRow = -1;
    bool firstColumnInRow = true;

    for (const QModelIndex &index : indexes) {
        if (!index.isValid()) {
            continue;
        }

        if (index.row() != previousRow) {
            if (previousRow != -1) {
                text += QLatin1Char('\n');
            }
            previousRow = index.row();
            firstColumnInRow = true;
        } else if (!firstColumnInRow) {
            text += QLatin1Char('\t');
        }

        text += index.data(Qt::DisplayRole).toString();
        firstColumnInRow = false;
    }

    return text;
}

inline void copyIndexes(const QModelIndexList &indexes)
{
    const QString text = indexesToText(indexes);
    if (!text.isEmpty()) {
        QApplication::clipboard()->setText(text);
    }
}
}

#endif // CLIPBOARDHELPERS_H
