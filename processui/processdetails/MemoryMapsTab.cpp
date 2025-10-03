/*
 *  KSysGuard, the KDE System Guard
 *
 *  SPDX-FileCopyrightText: 2022 Eugene Popov <popov895@ukr.net>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "MemoryMapsTab.h"

#include <QFile>
#include <QGraphicsOpacityEffect>
#include <QHeaderView>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QTextStream>
#include <QTimer>
#include <QTreeView>

#include <KLocalizedString>
#include <KMessageWidget>

class MemoryMapsModel : public QAbstractTableModel
{
public:
    using QAbstractTableModel::QAbstractTableModel;
    using QAbstractTableModel::setData;

    // we know for sure that the next columns are in these positions
    enum KnownColumn
    {
        Column_Filename,
        Column_Start,
        Column_End,
        Column_Permissions,
        Column_Offset,
        Column_Inode,
        KnownColumnCount
    };

    using DataItem = QVector<QVariant>;
    using Data = struct
    {
        QVector<QString> columns;
        QVector<DataItem> rows;
    };

    void setData(Data &&data)
    {
        beginResetModel();
        m_data = std::forward<Data>(data);
        endResetModel();
    }

    QVariant data(const QModelIndex &index, int role) const override
    {
        if (index.isValid() && (role == Qt::DisplayRole || role == Qt::EditRole)) {
            const int row = index.row();
            if (row >= 0 && row < m_data.rows.count()) {
                const DataItem &dataItem = m_data.rows.at(row);
                const int column = index.column();
                if (column >= 0 && column < dataItem.count()) {
                    const QVariant data = dataItem.at(column);
                    Q_ASSERT(data.isValid());
                    if (role == Qt::DisplayRole && data.typeId() != QMetaType::QString) {
                        if (column == Column_Start || column == Column_End || column == Column_Offset) {
                            return QString::number(data.toULongLong(), 16); // show in hex
                        }
                        if (column > Column_Inode) {
                            return i18nc("kilobytes", "%1 kB", data.toString());
                        }
                    }

                    return data;
                }
            }
        }

        return QVariant();
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role) const override
    {
        if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
            if (section >= 0 && section < m_data.columns.count()) {
                return m_data.columns.at(section);
            }
        }

        return QVariant();
    }

    int columnCount(const QModelIndex &parent) const override
    {
        Q_UNUSED(parent);

        return m_data.columns.count();
    }

    int rowCount(const QModelIndex &parent) const override
    {
        Q_UNUSED(parent);

        return m_data.rows.count();
    }

private:
    Data m_data;
};

MemoryMapsTab::MemoryMapsTab(QWidget *parent)
    : QWidget(parent)
{
    m_errorWidget = new KMessageWidget;
    m_errorWidget->setCloseButtonVisible(false);
    m_errorWidget->setMessageType(KMessageWidget::Error);
    m_errorWidget->setWordWrap(true);
    m_errorWidget->hide();

    QPushButton *refreshButton = new QPushButton(i18nc("@action:button", "Refresh"));

    m_searchEdit = new QLineEdit;
    m_searchEdit->setPlaceholderText(i18n("Quick search"));

    m_dataModel = new MemoryMapsModel(this);

    m_proxyModel = new QSortFilterProxyModel(this);
    m_proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxyModel->setFilterKeyColumn(-1);
    m_proxyModel->setSortRole(Qt::EditRole);
    m_proxyModel->setSourceModel(m_dataModel);

    QTreeView *dataTreeView = new QTreeView;
    dataTreeView->setAlternatingRowColors(true);
    dataTreeView->setRootIsDecorated(false);
    dataTreeView->setSortingEnabled(true);
    dataTreeView->sortByColumn(MemoryMapsModel::Column_Start, Qt::AscendingOrder);
    dataTreeView->setModel(m_proxyModel);
    dataTreeView->header()->setStretchLastSection(true);

    QGridLayout *rootLayout = new QGridLayout;
    rootLayout->addWidget(m_errorWidget, 0, 0, 1, 2);
    rootLayout->addWidget(refreshButton, 1, 0);
    rootLayout->addWidget(m_searchEdit, 1, 1);
    rootLayout->addWidget(dataTreeView, 2, 0, 1, 2);
    setLayout(rootLayout);

    m_placeholderLabel = new QLabel;
    m_placeholderLabel->setAlignment(Qt::AlignCenter);
    m_placeholderLabel->setMargin(20);
    m_placeholderLabel->setTextInteractionFlags(Qt::NoTextInteraction);
    m_placeholderLabel->setWordWrap(true);
    // To match the size of a level 2 Heading/KTitleWidget
    QFont placeholderFont = m_placeholderLabel->font();
    placeholderFont.setPointSize(qRound(placeholderFont.pointSize() * 1.3));
    m_placeholderLabel->setFont(placeholderFont);
    // Match opacity of QML placeholder label component
    QGraphicsOpacityEffect *opacityEffect = new QGraphicsOpacityEffect(m_placeholderLabel);
    opacityEffect->setOpacity(0.5);
    m_placeholderLabel->setGraphicsEffect(opacityEffect);

    QVBoxLayout *placeholderLayout = new QVBoxLayout;
    placeholderLayout->addWidget(m_placeholderLabel);
    dataTreeView->setLayout(placeholderLayout);

    // use some delay while searching as you type, because an immediate
    // search in large data can slow down the UI
    QTimer *applySearchTimer = new QTimer(this);
    applySearchTimer->setInterval(350);
    applySearchTimer->setSingleShot(true);

    connect(refreshButton, &QPushButton::clicked, this, &MemoryMapsTab::refresh);

    connect(m_searchEdit, &QLineEdit::textChanged, applySearchTimer, qOverload<>(&QTimer::start));
    connect(m_searchEdit, &QLineEdit::editingFinished, applySearchTimer, &QTimer::stop);
    connect(m_searchEdit, &QLineEdit::editingFinished, this, &MemoryMapsTab::onSearchEditEditingFinished);

    connect(m_proxyModel, &QSortFilterProxyModel::modelReset, this, &MemoryMapsTab::onProxyModelChanged);
    connect(m_proxyModel, &QSortFilterProxyModel::rowsInserted, this, &MemoryMapsTab::onProxyModelChanged);
    connect(m_proxyModel, &QSortFilterProxyModel::rowsRemoved, this, &MemoryMapsTab::onProxyModelChanged);

    connect(applySearchTimer, &QTimer::timeout, this, &MemoryMapsTab::onSearchEditEditingFinished);
}

void MemoryMapsTab::setProcessId(long processId)
{
    if (m_processId != processId) {
        m_processId = processId;
        refresh();
    }
}

void MemoryMapsTab::refresh()
{
    MemoryMapsModel::Data data;

    if (m_processId <= 0) {
        m_errorWidget->animatedHide();
    } else {
        const QString filePath = QStringLiteral("/proc/%1/smaps").arg(m_processId);
        QFile file(filePath);
        if (!file.open(QFile::ReadOnly)) {
            m_errorWidget->setText(QStringLiteral("%1: %2").arg(filePath).arg(file.errorString()));
            m_errorWidget->animatedShow();
        } else {
            m_errorWidget->animatedHide();

            MemoryMapsModel::DataItem dataItem;
            QTextStream smapsTextStream(file.readAll());
            QString line;
            QRegularExpressionMatch match;
            while (smapsTextStream.readLineInto(&line)) {
                // e.g. "Size: 80 kB"
                static QRegularExpression regexLineKb(QStringLiteral("^([^ ]+): +(\\d+) kB$"));
                match = regexLineKb.match(line);
                if (match.hasMatch()) {
                    if (data.rows.isEmpty()) {
                        // add the parsed column while we are parsing the first item
                        data.columns << match.captured(1);
                    }
                    dataItem << match.captured(2).toUInt();
                    continue;
                }

                // e.g. "VmFlags: rd ex mr mw me sd"
                static QRegularExpression regexLine(QStringLiteral("^([^ ]+): +(.+)$"));
                match = regexLine.match(line);
                if (match.hasMatch()) {
                    if (data.rows.isEmpty()) {
                        // add the parsed column while we are parsing the first item
                        data.columns << match.captured(1);
                    }
                    dataItem << match.captured(2);
                    continue;
                }

                // e.g. "7f935d6a5000-7f935d6a6000 rw-p 00040000 08:02 7457 /usr/lib64/libxcb.so.1.1.0"
                static QRegularExpression regexHeader(QStringLiteral("^([0-9A-Fa-f]+)-([0-9A-Fa-f]+) +([^ ]*) +([0-9A-Fa-f]+) "
                                                                     "+([0-9A-Fa-f]+:[0-9A-Fa-f]+) +(\\d+) +(.*)$"));
                match = regexHeader.match(line);
                if (match.hasMatch()) {
                    // we have reached the next header so we need to store the parsed smapsDataItem
                    if (!dataItem.isEmpty()) {
                        data.rows << std::move(dataItem);
                    }
                    if (data.rows.isEmpty()) {
                        // add known columns while we are parsing the first item
                        data.columns << i18nc("@title:column", "Filename");
                        data.columns << i18nc("@title:column Start of the address space", "Start");
                        data.columns << i18nc("@title:column End of the address space", "End");
                        data.columns << i18nc("@title:column", "Permissions");
                        data.columns << i18nc("@title:column Offset into the file", "Offset");
                        data.columns << i18nc("@title:column", "Inode");
                    }
                    dataItem << match.captured(7);                          // filename
                    dataItem << match.captured(1).toULongLong(nullptr, 16); // start
                    dataItem << match.captured(2).toULongLong(nullptr, 16); // end
                    dataItem << match.captured(3);                          // permissions
                    dataItem << match.captured(4).toULongLong(nullptr, 16); // offset
                    dataItem << match.captured(6).toUInt();                 // inode
                }
            }
            if (!dataItem.isEmpty()) {
                data.rows << std::move(dataItem);
            }
        }
    }

    m_dataModel->setData(std::move(data));
}

void MemoryMapsTab::onProxyModelChanged()
{
    if (m_proxyModel->rowCount() > 0) {
        m_placeholderLabel->hide();
    } else {
        if (m_proxyModel->sourceModel()->rowCount() == 0) {
            m_placeholderLabel->setText(i18nc("@info:status", "No data to display"));
        } else {
            m_placeholderLabel->setText(i18nc("@info:status", "No data matching the filter"));
        }
        m_placeholderLabel->show();
    }
}

void MemoryMapsTab::onSearchEditEditingFinished()
{
    m_proxyModel->setFilterFixedString(m_searchEdit->text());
}
