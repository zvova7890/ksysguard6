/*
    KSysGuard, the KDE System Guard

    Copyright (c) 1999, 2000, 2001 Chris Schlaeger <cs@kde.org>

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License as
 published by the Free Software Foundation; either version 2 of
 the License or (at your option) version 3 or any later version
 accepted by the membership of KDE e.V. (or its successor approved
 by the membership of KDE e.V.), which shall act as a proxy 
 defined in Section 14 of version 3 of the license.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <KLocalizedString>
#include "../ksgrd/SensorManager.h"
#include <QMouseEvent>
#include <QLabel>
#include <QHBoxLayout>
#include "DummyDisplay.h"

DummyDisplay::DummyDisplay( QWidget* parent, SharedSettings *workSheetSettings )
  : KSGRD::SensorDisplay( parent, i18n( "Drop Sensor Here" ), workSheetSettings )
{
  setWhatsThis(i18n("This is an empty space in a worksheet. Drag a sensor from "
                    "the Sensor Browser and drop it here. A sensor display will "
                    "appear that allows you to monitor the values of the sensor "
                    "over time." ) );

  QLabel *label = new QLabel(this);
  label->setText( i18n( "Drop Sensor Here" ) );
  label->setAlignment( Qt::AlignCenter );

  QHBoxLayout *layout = new QHBoxLayout;
  layout->addWidget(label);

   this->setLayout(layout);
}

bool DummyDisplay::eventFilter( QObject* object, QEvent* event )
{
  if ( event->type() == QEvent::MouseButtonRelease &&
       ( (QMouseEvent*)event)->button() == Qt::LeftButton )
    setFocus();

  return QWidget::eventFilter( object, event );
}


