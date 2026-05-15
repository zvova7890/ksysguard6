/*
    KSysGuard, the KDE System Guard
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2026 Volodymyr Zolotopupov <zvova7890@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef BALANCEDFLOWLAYOUT_H
#define BALANCEDFLOWLAYOUT_H

#include <QLayout>
#include <QRect>
#include <QStyle>

class BalancedFlowLayout : public QLayout
{
    Q_OBJECT

public:
    explicit BalancedFlowLayout(QWidget *parent, int margin = -1, int hSpacing = -1, int vSpacing = -1);
    explicit BalancedFlowLayout(int margin = -1, int hSpacing = -1, int vSpacing = -1);
    ~BalancedFlowLayout();

    void addItem(QLayoutItem *item) override;
    int horizontalSpacing() const;
    int verticalSpacing() const;
    Qt::Orientations expandingDirections() const override;
    bool hasHeightForWidth() const override;
    int heightForWidth(int) const override;
    int count() const override;
    QLayoutItem *itemAt(int index) const override;
    QSize minimumSize() const override;
    void setGeometry(const QRect &rect) override;
    QSize sizeHint() const override;
    QLayoutItem *takeAt(int index) override;
    int rowCount() const;
    int columnWidth() const;
    int availableWidth() const;

Q_SIGNALS:
    void layoutModeChanged();

private:
    struct RowItem {
        QLayoutItem *item;
        int width;
        int height;
    };

    using Row = QList<RowItem>;

    QList<Row> rowsForWidth(int width) const;
    int rowWidth(const Row &row) const;
    int rowHeight(const Row &row) const;
    int doLayout(const QRect &rect, bool testOnly) const;
    int smartSpacing(QStyle::PixelMetric pm) const;

    QList<QLayoutItem *> itemList;
    int m_hSpace;
    int m_vSpace;
    int m_rowCount = 0;
    int m_columnWidth = 0;
    int m_availableWidth = 0;
};

#endif // BALANCEDFLOWLAYOUT_H
