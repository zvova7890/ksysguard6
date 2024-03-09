/*
 *  KSysGuard, the KDE System Guard
 *
 *  SPDX-FileCopyrightText: 2022 Eugene Popov <popov895@ukr.net>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "GeneralTab.h"

#include <QGraphicsOpacityEffect>
#include <QHeaderView>
#include <QLabel>
#include <QLayout>
#include <QTreeWidget>

#include <KLocalizedString>

GeneralTab::GeneralTab(QWidget *parent)
    : QWidget(parent)
{
    m_dataTreeWidget = new QTreeWidget;
    m_dataTreeWidget->setAlternatingRowColors(true);
    m_dataTreeWidget->setColumnCount(2);
    m_dataTreeWidget->setHeaderHidden(true);
    m_dataTreeWidget->setRootIsDecorated(false);
    m_dataTreeWidget->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_dataTreeWidget->header()->setStretchLastSection(true);

    QVBoxLayout *rootLayout = new QVBoxLayout;
    rootLayout->addWidget(m_dataTreeWidget);
    setLayout(rootLayout);

    m_placeholderLabel = new QLabel;
    m_placeholderLabel->setAlignment(Qt::AlignCenter);
    m_placeholderLabel->setMargin(20);
    m_placeholderLabel->setTextInteractionFlags(Qt::NoTextInteraction);
    m_placeholderLabel->setWordWrap(true);
    m_placeholderLabel->setText(i18nc("@info:status", "No data to display"));
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
    m_dataTreeWidget->setLayout(placeholderLayout);
}

void GeneralTab::setData(const QVariantMap &data)
{
    m_dataTreeWidget->clear();

    QList<QTreeWidgetItem*> items;
    for (QVariantMap::const_iterator i = data.cbegin(); i != data.cend(); ++i) {
        items << new QTreeWidgetItem({ i.key(), i.value().toString() });
    }

    if (items.isEmpty()) {
        m_placeholderLabel->show();
    } else {
        m_placeholderLabel->hide();
        m_dataTreeWidget->addTopLevelItems(items);
    }
}
