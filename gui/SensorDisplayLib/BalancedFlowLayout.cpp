/*
    KSysGuard, the KDE System Guard
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2026 Volodymyr Zolotopupov <zvova7890@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QtWidgets>
#include "BalancedFlowLayout.h"

BalancedFlowLayout::BalancedFlowLayout(QWidget *parent, int margin, int hSpacing, int vSpacing)
    : QLayout(parent), m_hSpace(hSpacing), m_vSpace(vSpacing)
{
    setContentsMargins(margin, margin, margin, margin);
}

BalancedFlowLayout::BalancedFlowLayout(int margin, int hSpacing, int vSpacing)
    : m_hSpace(hSpacing), m_vSpace(vSpacing)
{
    setContentsMargins(margin, margin, margin, margin);
}

BalancedFlowLayout::~BalancedFlowLayout()
{
    QLayoutItem *item;
    while ((item = takeAt(0)))
        delete item;
}

void BalancedFlowLayout::addItem(QLayoutItem *item)
{
    itemList.append(item);
}

int BalancedFlowLayout::horizontalSpacing() const
{
    if (m_hSpace >= 0) {
        return m_hSpace;
    } else {
        return smartSpacing(QStyle::PM_LayoutHorizontalSpacing);
    }
}

int BalancedFlowLayout::verticalSpacing() const
{
    if (m_vSpace >= 0) {
        return m_vSpace;
    } else {
        return smartSpacing(QStyle::PM_LayoutVerticalSpacing);
    }
}

int BalancedFlowLayout::count() const
{
    return itemList.size();
}

QLayoutItem *BalancedFlowLayout::itemAt(int index) const
{
    return itemList.value(index);
}

QLayoutItem *BalancedFlowLayout::takeAt(int index)
{
    if (index >= 0 && index < itemList.size())
        return itemList.takeAt(index);
    return nullptr;
}

int BalancedFlowLayout::rowCount() const
{
    return m_rowCount;
}

int BalancedFlowLayout::columnWidth() const
{
    return m_columnWidth;
}

int BalancedFlowLayout::availableWidth() const
{
    return m_availableWidth;
}

Qt::Orientations BalancedFlowLayout::expandingDirections() const
{
    return Qt::Horizontal;
}

bool BalancedFlowLayout::hasHeightForWidth() const
{
    return true;
}

int BalancedFlowLayout::heightForWidth(int width) const
{
    int height = doLayout(QRect(0, 0, width, 0), true);
    return height;
}

void BalancedFlowLayout::setGeometry(const QRect &rect)
{
    QLayout::setGeometry(rect);
    int left, top, right, bottom;
    getContentsMargins(&left, &top, &right, &bottom);
    const QRect effectiveRect = rect.adjusted(+left, +top, -right, -bottom);
    const QList<Row> rows = rowsForWidth(effectiveRect.width());
    int columnCount = 0;
    for (const Row &row : rows) {
        columnCount = qMax(columnCount, row.count());
    }
    const int spaceX = qMax(horizontalSpacing(), 0);
    const int totalSpacing = spaceX * qMax(0, columnCount - 1);
    const int columnWidth = columnCount == 0 ? 0 : (effectiveRect.width() - totalSpacing) / columnCount;

    const int rowCount = rows.count();
    const int availableWidth = effectiveRect.width();
    if (m_rowCount != rowCount || m_columnWidth != columnWidth || m_availableWidth != availableWidth) {
        m_rowCount = rowCount;
        m_columnWidth = columnWidth;
        m_availableWidth = availableWidth;
        Q_EMIT layoutModeChanged();
    }
    doLayout(rect, false);
}

QSize BalancedFlowLayout::sizeHint() const
{
    // Width is dictated by the parent; height comes from heightForWidth().
    // Avoid advertising a large preferred width that would prevent wrapping
    return minimumSize();
}

QSize BalancedFlowLayout::minimumSize() const
{
    QSize size;
    for (const QLayoutItem *item : std::as_const(itemList))
        size = size.expandedTo(item->minimumSize());

    const QMargins margins = contentsMargins();
    size += QSize(margins.left() + margins.right(), margins.top() + margins.bottom());
    return size;
}

QList<BalancedFlowLayout::Row> BalancedFlowLayout::rowsForWidth(int width) const
{
    QList<Row> rows;
    Row items;
    const int spaceX = qMax(horizontalSpacing(), 0);

    for (QLayoutItem *item : std::as_const(itemList)) {
        const QSize compactSize = item->minimumSize().expandedTo(QSize(1, item->sizeHint().height()));
        const int itemWidth = qMin(compactSize.width(), width);
        items.append({item, itemWidth, compactSize.height()});
    }

    if (items.isEmpty()) {
        return rows;
    }

    int totalWidth = 0;
    for (const RowItem &item : items) {
        totalWidth += item.width;
    }
    totalWidth += spaceX * (items.count() - 1);

    // Pick the minimum row count, then fill a fixed number of columns per row.
    // This keeps wrapped rows on a grid; only the final row may be incomplete
    const int minimumRowCount = qMax(1, (totalWidth + width - 1) / width);
    for (int rowCount = minimumRowCount; rowCount <= items.count(); ++rowCount) {
        rows.clear();

        const int columnCount = (items.count() + rowCount - 1) / rowCount;
        int itemIndex = 0;

        for (int rowIndex = 0; rowIndex < rowCount; ++rowIndex) {
            const int count = qMin(columnCount, items.count() - itemIndex);
            Row row;
            row.reserve(count);

            for (int i = 0; i < count; ++i) {
                row.append(items.at(itemIndex++));
            }

            rows.append(row);
        }

        const int availableColumnWidth = columnCount == 0 ? 0 : (width - spaceX * (columnCount - 1)) / columnCount;
        const auto fits = [availableColumnWidth](const Row &row) {
            return std::all_of(row.cbegin(), row.cend(), [availableColumnWidth](const RowItem &item) {
                return item.width <= availableColumnWidth;
            });
        };
        if (availableColumnWidth > 0 && std::all_of(rows.cbegin(), rows.cend(), fits)) {
            return rows;
        }

    }

    return rows;
}

int BalancedFlowLayout::rowWidth(const Row &row) const
{
    if (row.isEmpty()) {
        return 0;
    }

    int width = 0;
    const int spaceX = qMax(horizontalSpacing(), 0);
    for (const RowItem &item : row) {
        width += item.width;
    }
    return width + spaceX * (row.count() - 1);
}

int BalancedFlowLayout::rowHeight(const Row &row) const
{
    int height = 0;
    for (const RowItem &item : row) {
        height = qMax(height, item.height);
    }
    return height;
}

int BalancedFlowLayout::doLayout(const QRect &rect, bool testOnly) const
{
    int left, top, right, bottom;
    getContentsMargins(&left, &top, &right, &bottom);
    QRect effectiveRect = rect.adjusted(+left, +top, -right, -bottom);
    int y = effectiveRect.y();

    const int spaceX = qMax(horizontalSpacing(), 0);
    const int spaceY = qMax(verticalSpacing(), 0);
    const QList<Row> rows = rowsForWidth(effectiveRect.width());
    int columnCount = 0;
    for (const Row &row : rows) {
        columnCount = qMax(columnCount, row.count());
    }
    const int totalSpacing = spaceX * qMax(0, columnCount - 1);
    const int columnWidth = columnCount == 0 ? 0 : (effectiveRect.width() - totalSpacing) / columnCount;
    const int extraWidthRemainder = columnCount == 0 ? 0 : (effectiveRect.width() - totalSpacing) % columnCount;

    if (rows.count() == 1) {
        // Single-row legends should keep natural item widths. Extra space is
        // reflected in stable virtual columns, matching an expanding QHBoxLayout
        const Row &row = rows.first();
        const int height = rowHeight(row);
        const int slotCount = row.count();
        const int totalSpacing = spaceX * qMax(0, slotCount - 1);
        const int slotWidth = slotCount == 0 ? 0 : (effectiveRect.width() - totalSpacing) / slotCount;
        const int slotWidthRemainder = slotCount == 0 ? 0 : (effectiveRect.width() - totalSpacing) % slotCount;
        int x = effectiveRect.x();

        for (int i = 0; i < row.count(); ++i) {
            const RowItem &rowItem = row.at(i);
            const int itemWidth = qMin(rowItem.item->sizeHint().width(), effectiveRect.right() - x + 1);
            if (!testOnly) {
                rowItem.item->setGeometry(QRect(QPoint(x, y), QSize(itemWidth, height)));
            }
            x += slotWidth + spaceX + (i < slotWidthRemainder ? 1 : 0);
        }

        return y + height - rect.y() + bottom;
    }

    for (const Row &row : rows) {
        const int height = rowHeight(row);
        int x = effectiveRect.x();

        for (int i = 0; i < row.count(); ++i) {
            const RowItem &rowItem = row.at(i);
            const int itemWidth = columnWidth + (i < extraWidthRemainder ? 1 : 0);
            if (!testOnly) {
                rowItem.item->setGeometry(QRect(QPoint(x, y), QSize(itemWidth, height)));
            }
            x += itemWidth + spaceX;
        }

        y += height + spaceY;
    }
    if (rows.isEmpty()) {
        return top + bottom;
    }
    return y - spaceY - rect.y() + bottom;
}

int BalancedFlowLayout::smartSpacing(QStyle::PixelMetric pm) const
{
    QObject *parent = this->parent();
    if (!parent) {
        return -1;
    } else if (parent->isWidgetType()) {
        QWidget *pw = static_cast<QWidget *>(parent);
        return pw->style()->pixelMetric(pm, nullptr, pw);
    } else {
        return static_cast<QLayout *>(parent)->spacing();
    }
}
