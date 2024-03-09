/*
 *  KSysGuard, the KDE System Guard
 *
 *  SPDX-FileCopyrightText: 2022 Eugene Popov <popov895@ukr.net>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "OpenFilesTab.h"

#include <QDir>
#include <QGraphicsOpacityEffect>
#include <QHeaderView>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QTimer>
#include <QTreeView>

#include <KLocalizedString>
#include <KMessageWidget>

#include <unistd.h>
#include <sys/stat.h>

QString fileTypeFromPath(const QString &path)
{
    struct stat statbuf;
    if (stat(qPrintable(path), &statbuf) == 0) {
        if (S_ISREG(statbuf.st_mode) || S_ISLNK(statbuf.st_mode)) {
            return i18nc("Device type", "File");
        }
        if (S_ISCHR(statbuf.st_mode)) {
            return i18nc("Device type", "Character device");
        }
        if (S_ISBLK(statbuf.st_mode)) {
            return i18nc("Device type", "Block device");
        }
        if (S_ISFIFO(statbuf.st_mode)) {
            return i18nc("Device type", "Pipe");
        }
        if (S_ISSOCK(statbuf.st_mode)) {
            return i18nc("Device type", "Socket");
        }
    }

    return QString();
}

QString symLinkTargetFromPath(const QString &path)
{
    struct stat statbuf;
    if (lstat(qPrintable(path), &statbuf) == 0) {
        QVarLengthArray<char, 256> symLinkTarget(statbuf.st_size + 1);
        ssize_t count;
        if ((count = readlink(qPrintable(path), symLinkTarget.data(), symLinkTarget.size() - 1)) > 0) {
            symLinkTarget[count] = '\0';

            return QString::fromUtf8(symLinkTarget.constData());
        }
    }

    return QString();
}

class OpenFilesModel : public QAbstractTableModel
{
public:
    using QAbstractTableModel::QAbstractTableModel;
    using QAbstractTableModel::setData;

    enum Column
    {
        Column_Id,
        Column_Type,
        Column_Filename,
        ColumnCount
    };

    using DataItem = struct
    {
        uint id;
        QString type;
        QString filename;
    };
    using Data = QVector<DataItem>;

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
            if (row >= 0 && row < m_data.count()) {
                const DataItem &dataItem = m_data.at(row);
                switch (index.column()) {
                    case Column_Id:
                        return dataItem.id;
                    case Column_Type:
                        return dataItem.type;
                    case Column_Filename:
                        return dataItem.filename;
                    default:
                        Q_UNREACHABLE();
                }
            }
        }

        return QVariant();
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role) const override
    {
        if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
            switch (section) {
                case Column_Id:
                    return i18nc("@title:column File ID", "Id");
                case Column_Type:
                    return i18nc("@title:column File type", "Type");
                case Column_Filename:
                    return i18nc("@title:column", "Filename");
                default:
                    Q_UNREACHABLE();
            }
        }

        return QVariant();
    }

    int columnCount(const QModelIndex &parent) const override
    {
        Q_UNUSED(parent);

        return ColumnCount;
    }

    int rowCount(const QModelIndex &parent) const override
    {
        Q_UNUSED(parent);

        return m_data.count();
    }

private:
    Data m_data;
};

OpenFilesTab::OpenFilesTab(QWidget *parent)
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

    m_dataModel = new OpenFilesModel(this);

    m_proxyModel = new QSortFilterProxyModel(this);
    m_proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxyModel->setFilterKeyColumn(-1);
    m_proxyModel->setSortRole(Qt::EditRole);
    m_proxyModel->setSourceModel(m_dataModel);

    QTreeView *dataTreeView = new QTreeView;
    dataTreeView->setAlternatingRowColors(true);
    dataTreeView->setRootIsDecorated(false);
    dataTreeView->setSortingEnabled(true);
    dataTreeView->sortByColumn(OpenFilesModel::Column_Id, Qt::AscendingOrder);
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

    connect(refreshButton, &QPushButton::clicked, this, &OpenFilesTab::refresh);

    connect(m_searchEdit, &QLineEdit::textChanged, applySearchTimer, qOverload<>(&QTimer::start));
    connect(m_searchEdit, &QLineEdit::editingFinished, applySearchTimer, &QTimer::stop);
    connect(m_searchEdit, &QLineEdit::editingFinished, this, &OpenFilesTab::onSearchEditEditingFinished);

    connect(m_proxyModel, &QSortFilterProxyModel::modelReset, this, &OpenFilesTab::onProxyModelChanged);
    connect(m_proxyModel, &QSortFilterProxyModel::rowsInserted, this, &OpenFilesTab::onProxyModelChanged);
    connect(m_proxyModel, &QSortFilterProxyModel::rowsRemoved, this, &OpenFilesTab::onProxyModelChanged);

    connect(applySearchTimer, &QTimer::timeout, this, &OpenFilesTab::onSearchEditEditingFinished);
}

void OpenFilesTab::setProcessId(long processId)
{
    if (m_processId != processId) {
        m_processId = processId;
        refresh();
    }
}

void OpenFilesTab::refresh()
{
    OpenFilesModel::Data data;

    if (m_processId <= 0) {
        m_errorWidget->animatedHide();
    } else {
        const QString dirPath = QStringLiteral("/proc/%1/fd").arg(m_processId);
        const QFileInfo dirFileInfo(dirPath);
        if (!dirFileInfo.exists() || !dirFileInfo.isReadable() || !dirFileInfo.isDir()) {
            m_errorWidget->setText(i18nc("@info:status", "%1: Failed to list directory contents", dirPath));
            m_errorWidget->animatedShow();
        } else {
            m_errorWidget->animatedHide();

            const QFileInfoList filesInfo = QDir(dirPath).entryInfoList(QDir::Files);
            for (const QFileInfo &fileInfo : filesInfo) {
                OpenFilesModel::DataItem dataItem;
                dataItem.id = fileInfo.fileName().toUInt();
                dataItem.type = fileTypeFromPath(fileInfo.absoluteFilePath());
                if (fileInfo.isFile()) {
                    dataItem.filename = fileInfo.symLinkTarget();
                } else {
                    dataItem.filename = symLinkTargetFromPath(fileInfo.absoluteFilePath());
                }
                data << std::move(dataItem);
            }
        }
    }

    m_dataModel->setData(std::move(data));
}

void OpenFilesTab::onProxyModelChanged()
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

void OpenFilesTab::onSearchEditEditingFinished()
{
    m_proxyModel->setFilterFixedString(m_searchEdit->text());
}
