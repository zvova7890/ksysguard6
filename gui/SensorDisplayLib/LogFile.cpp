/*
    KSysGuard, the KDE System Guard

	Copyright (c) 2001 Tobias Koenig <tokoe@kde.org>

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

*/

#include "LogFile.h"

#include <stdio.h>
#include <sys/types.h>

#include <QDebug>
#include <QDialog>
#include <QPushButton>
#include <QRegularExpression>
#include <QListWidget>
#include <QHBoxLayout>

#include <KLocalizedString>
#include <KNotification>
#include <KColorButton>
#include "StyleEngine.h"

#include "ui_LogFileSettings.h"



LogFile::LogFile(QWidget *parent, const QString& title, SharedSettings *workSheetSettings)
	: KSGRD::SensorDisplay(parent, title, workSheetSettings)
{
    qDebug() << "Making sensor logger";
	logFileID= 0;
    lfs = nullptr;
	QLayout *layout = new QHBoxLayout(this);
	monitor = new QListWidget(this);
	layout->addWidget(monitor);
	setLayout(layout);

	setMinimumSize(50, 25);
	monitor->setContextMenuPolicy( Qt::CustomContextMenu );
	connect(monitor, &QListWidget::customContextMenuRequested, this, &LogFile::showContextMenu);
	setPlotterWidget(monitor);
}

LogFile::~LogFile(void)
{
	sendRequest(sensors().at(0)->hostName(), QStringLiteral("logfile_unregister %1" ).arg(logFileID), 43);
}

bool
LogFile::addSensor(const QString& hostName, const QString& sensorName, const QString& sensorType, const QString& title)
{
	if (sensorType != QLatin1String("logfile"))
		return (false);

	registerSensor(new KSGRD::SensorProperties(hostName, sensorName, sensorType, title));

	QString sensorID = sensorName.right(sensorName.length() - (sensorName.lastIndexOf(QLatin1String("/")) + 1));

	sendRequest(sensors().at(0)->hostName(), QStringLiteral("logfile_register %1" ).arg(sensorID), 42);

	if (title.isEmpty())
		setTitle(sensors().at(0)->hostName() + QLatin1Char(':') + sensorID);
	else
		setTitle(title);

	return (true);
}


void LogFile::configureSettings(void)
{
	QPalette cgroup = monitor->palette();

	lfs = new Ui_LogFileSettings;
	Q_CHECK_PTR(lfs);
    QDialog dlg;
    dlg.setWindowTitle( i18n("File logging settings") );
    QWidget *mainWidget = new QWidget( this );

    lfs->setupUi(mainWidget);

    QVBoxLayout *vlayout = new QVBoxLayout(this);
    vlayout->addWidget(mainWidget);
    dlg.setLayout(vlayout);

	lfs->fgColor->setColor(cgroup.color( QPalette::Text ));
	lfs->fgColor->setText(i18n("Foreground color:"));
	lfs->bgColor->setColor(cgroup.color( QPalette::Base ));
	lfs->bgColor->setText(i18n("Background color:"));
	lfs->fontRequester->setFont(monitor->font());
	lfs->ruleList->addItems(filterRules);
	lfs->title->setText(title());

    connect(lfs->buttonBox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(lfs->buttonBox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

	connect(lfs->addButton, &QPushButton::clicked, this, &LogFile::settingsAddRule);
	connect(lfs->deleteButton, &QPushButton::clicked, this, &LogFile::settingsDeleteRule);
	connect(lfs->changeButton, &QPushButton::clicked, this, &LogFile::settingsChangeRule);
	connect(lfs->ruleList, &QListWidget::currentRowChanged, this, &LogFile::settingsRuleListSelected);
    connect(lfs->ruleText, &QLineEdit::returnPressed, this, &LogFile::settingsAddRule);
    connect(lfs->ruleText, &QLineEdit::textChanged, this, &LogFile::settingsRuleTextChanged);

	settingsRuleListSelected(lfs->ruleList->currentRow());
	settingsRuleTextChanged();

	if (dlg.exec())
		applySettings();

	delete lfs;
    lfs = nullptr;
}

void LogFile::settingsRuleTextChanged()
{
	lfs->addButton->setEnabled(!lfs->ruleText->text().isEmpty());
	lfs->changeButton->setEnabled(!lfs->ruleText->text().isEmpty() && lfs->ruleList->currentRow() > -1);
}

void LogFile::settingsAddRule()
{
	if (!lfs->ruleText->text().isEmpty()) {
		lfs->ruleList->addItem(lfs->ruleText->text());
		lfs->ruleText->setText(QLatin1String(""));
	}
}

void LogFile::settingsDeleteRule()
{
	delete lfs->ruleList->takeItem(lfs->ruleList->currentRow());
	lfs->ruleText->setText(QLatin1String(""));
}

void LogFile::settingsChangeRule()
{
	if (lfs->ruleList->currentItem() && !lfs->ruleText->text().isEmpty())
		lfs->ruleList->currentItem()->setText(lfs->ruleText->text());
	lfs->ruleText->setText(QLatin1String(""));
}

void LogFile::settingsRuleListSelected(int index)
{
    bool anySelected = (index > -1);
    if (anySelected)
        lfs->ruleText->setText(lfs->ruleList->item(index)->text());

    lfs->changeButton->setEnabled(anySelected && !lfs->ruleText->text().isEmpty());
    lfs->deleteButton->setEnabled(anySelected);
}

void LogFile::applySettings(void)
{
	QPalette cgroup = monitor->palette();

	cgroup.setColor(QPalette::Text, lfs->fgColor->color());
	cgroup.setColor(QPalette::Base, lfs->bgColor->color());
	monitor->setPalette( cgroup );
	monitor->setFont(lfs->fontRequester->font());

	filterRules.clear();
	for (int i = 0; i < lfs->ruleList->count(); i++)
		filterRules.append(lfs->ruleList->item(i)->text());

	setTitle(lfs->title->text());
}

void
LogFile::applyStyle()
{
	QPalette cgroup = monitor->palette();

	cgroup.setColor(QPalette::Text, KSGRD::Style->firstForegroundColor());
	cgroup.setColor(QPalette::Base, KSGRD::Style->backgroundColor());
	monitor->setPalette( cgroup );
}

bool
LogFile::restoreSettings(QDomElement& element)
{
	QFont font;
	QPalette cgroup = monitor->palette();

	cgroup.setColor(QPalette::Active, QPalette::Text, restoreColor(element, QStringLiteral("textColor"), Qt::green));
	cgroup.setColor(QPalette::Active, QPalette::Base, restoreColor(element, QStringLiteral("backgroundColor"), Qt::black));
	cgroup.setColor(QPalette::Disabled, QPalette::Text, restoreColor(element, QStringLiteral("textColor"), Qt::green));
	cgroup.setColor(QPalette::Disabled, QPalette::Base, restoreColor(element, QStringLiteral("backgroundColor"), Qt::black));
	cgroup.setColor(QPalette::Inactive, QPalette::Text, restoreColor(element, QStringLiteral("textColor"), Qt::green));
	cgroup.setColor(QPalette::Inactive, QPalette::Base, restoreColor(element, QStringLiteral("backgroundColor"), Qt::black));
	monitor->setPalette(cgroup);

	addSensor(element.attribute(QStringLiteral("hostName")), element.attribute(QStringLiteral("sensorName")), (element.attribute(QStringLiteral("sensorType")).isEmpty() ? QStringLiteral("logfile") : element.attribute(QStringLiteral("sensorType"))), element.attribute(QStringLiteral("title")));

	font.fromString( element.attribute( QStringLiteral("font") ) );
	monitor->setFont(font);

	QDomNodeList dnList = element.elementsByTagName(QStringLiteral("filter"));
	for (int i = 0; i < dnList.count(); i++) {
		QDomElement element = dnList.item(i).toElement();
		filterRules.append(element.attribute(QStringLiteral("rule")));
	}

	SensorDisplay::restoreSettings(element);

	return true;
}

bool
LogFile::saveSettings(QDomDocument& doc, QDomElement& element)
{
	element.setAttribute(QStringLiteral("hostName"), sensors().at(0)->hostName());
	element.setAttribute(QStringLiteral("sensorName"), sensors().at(0)->name());
	element.setAttribute(QStringLiteral("sensorType"), sensors().at(0)->type());

	element.setAttribute(QStringLiteral("font"), monitor->font().toString());

	saveColor(element, QStringLiteral("textColor"), monitor->palette().color( QPalette::Text ) );
	saveColor(element, QStringLiteral("backgroundColor"), monitor->palette().color( QPalette::Base ) );

	for (QStringList::Iterator it = filterRules.begin();
		 it != filterRules.end(); ++it)
	{
		QDomElement filter = doc.createElement(QStringLiteral("filter"));
		filter.setAttribute(QStringLiteral("rule"), (*it));
		element.appendChild(filter);
	}

	SensorDisplay::saveSettings(doc, element);

	return true;
}

void
LogFile::updateMonitor()
{
	sendRequest(sensors().at(0)->hostName(),
				QStringLiteral("%1 %2" ).arg(sensors().at(0)->name()).arg(logFileID), 19);
}

void
LogFile::answerReceived(int id, const QList<QByteArray>& answer)
{
	/* We received something, so the sensor is probably ok. */
	sensorError(id, false);

	switch (id)
	{
		case 19: {
			QString s;
			for (int i = 0; i < answer.count(); i++) {
				s = QString::fromUtf8(answer[i]);
				if (monitor->count() == MAXLINES)
					monitor->takeItem(0);

				monitor->addItem(s);

				for (QStringList::Iterator it = filterRules.begin(); it != filterRules.end(); ++it) {
					QRegularExpression expr((*it));
					if (expr.matchView(s).hasMatch()) {
						KNotification::event(
							KNotification::Notification,
							QStringLiteral("pattern_match"),
							QStringLiteral("rule '%1' matched").arg(*it),
							QPixmap(),
							KNotification::CloseOnTimeout);
					}
				}
			}

			monitor->setCurrentRow( monitor->count() - 1 );

			break;
		}

		case 42: {
			if(answer.isEmpty())
				logFileID= 0;
			else
				logFileID = answer[0].toULong();
			break;
		}
	}
}

