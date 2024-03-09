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

#include <math.h>
#include <stdlib.h>

#include <QDomElement>
#include <QLCDNumber>
#include <QHBoxLayout>


#include "../ksgrd/SensorManager.h"
#include "StyleEngine.h"

#include "MultiMeter.h"
#include "MultiMeterSettings.h"

MultiMeter::MultiMeter(QWidget* parent, const QString& title, SharedSettings *workSheetSettings)
  : KSGRD::SensorDisplay(parent, title, workSheetSettings)
{
  setShowUnit( true );
  mLowerLimit = mUpperLimit = 0.0;
  mLowerLimitActive = mUpperLimitActive = false;

  mIsFloat = false;

  mNormalDigitColor = KSGRD::Style->firstForegroundColor();
  mAlarmDigitColor = KSGRD::Style->alarmColor();
  QLayout *layout = new QHBoxLayout(this);
  mLcd = new QLCDNumber( this );
  layout->addWidget(mLcd);

  mLcd->setFrameStyle( QFrame::NoFrame );
  mLcd->setSegmentStyle( QLCDNumber::Filled );
  setDigitColor( KSGRD::Style->firstForegroundColor() );
  mLcd->setSizePolicy( QSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding ) );

  setBackgroundColor( KSGRD::Style->backgroundColor() );

  /* All RMB clicks to the mLcd widget will be handled by 
   * SensorDisplay::eventFilter. */
  mLcd->installEventFilter( this );

  setPlotterWidget( mLcd );

  setMinimumSize( 5, 5 );
}

bool MultiMeter::addSensor(const QString& hostName, const QString& sensorName,
          const QString& sensorType, const QString& title)
{
  if (sensorType != QLatin1String("integer") && sensorType != QLatin1String("float"))
    return false;

  if(!sensors().isEmpty())
    return false;

  mIsFloat = (sensorType == QLatin1String("float"));
  mLcd->setSmallDecimalPoint( mIsFloat );

  registerSensor(new KSGRD::SensorProperties(hostName, sensorName, sensorType, title));

  /* To differentiate between answers from value requests and info
   * requests we use 100 for info requests. */
  sendRequest(hostName, sensorName + QLatin1Char('?'), 100);

  mLcd->setToolTip( QStringLiteral("%1:%2").arg(hostName).arg(sensorName));

  return true;
}

void MultiMeter::answerReceived(int id, const QList<QByteArray>& answerlist)
{
  /* We received something, so the sensor is probably ok. */
  sensorError(id, false);
  QByteArray answer;
  if(!answerlist.isEmpty()) answer = answerlist[0];

  if (id == 100)
  {
    KSGRD::SensorIntegerInfo info(answer);
    setUnit(KSGRD::SensorMgr->translateUnit(info.unit()));
  }
  else
  {
    double val = answer.toDouble();

    int digits = 1;
    if (qAbs(val) >= 1) {
      digits = (int) log10(qAbs(val)) + 1;
    }
    if (mIsFloat) {
      //Show two digits after the decimal point
      digits += 3;
    }
    if (val < 0) {
      //Add a digit for the negative sign
      digits += 1;
    }

    mLcd->setDigitCount(qMin(15,digits));

    mLcd->display(val);
    if (mLowerLimitActive && val < mLowerLimit)
    {
      setDigitColor(mAlarmDigitColor);  
    }
    else if (mUpperLimitActive && val > mUpperLimit)
    {
      setDigitColor(mAlarmDigitColor);
    }
    else
      setDigitColor(mNormalDigitColor);
  }
}

bool MultiMeter::restoreSettings(QDomElement& element)
{
  mLowerLimitActive = element.attribute(QStringLiteral("lowerLimitActive")).toInt();
  mLowerLimit = element.attribute(QStringLiteral("lowerLimit")).toDouble();
  mUpperLimitActive = element.attribute(QStringLiteral("upperLimitActive")).toInt();
  mUpperLimit = element.attribute(QStringLiteral("upperLimit")).toDouble();

  mNormalDigitColor = restoreColor(element, QStringLiteral("normalDigitColor"),
            KSGRD::Style->firstForegroundColor());
  mAlarmDigitColor = restoreColor(element, QStringLiteral("mAlarmDigitColor"),
            KSGRD::Style->alarmColor());
  setBackgroundColor(restoreColor(element, QStringLiteral("backgroundColor"),
            KSGRD::Style->backgroundColor()));

  addSensor(element.attribute(QStringLiteral("hostName")), element.attribute(QStringLiteral("sensorName")), (element.attribute(QStringLiteral("sensorType")).isEmpty() ? QStringLiteral("integer") : element.attribute(QStringLiteral("sensorType"))), QLatin1String(""));

  SensorDisplay::restoreSettings(element);

  return true;
}

bool MultiMeter::saveSettings(QDomDocument& doc, QDomElement& element)
{
  if(!sensors().isEmpty()) {
    element.setAttribute(QStringLiteral("hostName"), sensors().at(0)->hostName());
    element.setAttribute(QStringLiteral("sensorName"), sensors().at(0)->name());
    element.setAttribute(QStringLiteral("sensorType"), sensors().at(0)->type());
  }
  element.setAttribute(QStringLiteral("showUnit"), showUnit());
  element.setAttribute(QStringLiteral("lowerLimitActive"), (int) mLowerLimitActive);
  element.setAttribute(QStringLiteral("lowerLimit"), mLowerLimit);
  element.setAttribute(QStringLiteral("upperLimitActive"), (int) mUpperLimitActive);
  element.setAttribute(QStringLiteral("upperLimit"), mUpperLimit);

  saveColor(element, QStringLiteral("normalDigitColor"), mNormalDigitColor);
  saveColor(element, QStringLiteral("mAlarmDigitColor"), mAlarmDigitColor);
  saveColor(element, QStringLiteral("backgroundColor"), mBackgroundColor);

  SensorDisplay::saveSettings(doc, element);

  return true;
}

void MultiMeter::configureSettings()
{
  MultiMeterSettings dlg( this );

  dlg.setTitle(title());
  dlg.setShowUnit(showUnit());
  dlg.setLowerLimitActive(mLowerLimitActive);
  dlg.setLowerLimit(mLowerLimit);
  dlg.setUpperLimitActive(mUpperLimitActive);
  dlg.setUpperLimit(mUpperLimit);
  dlg.setNormalDigitColor(mNormalDigitColor);
  dlg.setAlarmDigitColor(mAlarmDigitColor);
  dlg.setMeterBackgroundColor(mBackgroundColor);

  if ( dlg.exec() ) {
    setShowUnit( dlg.showUnit() );
    setTitle( dlg.title() );
    mLowerLimitActive = dlg.lowerLimitActive();
    mLowerLimit = dlg.lowerLimit();
    mUpperLimitActive = dlg.upperLimitActive();
    mUpperLimit = dlg.upperLimit();

    mNormalDigitColor = dlg.normalDigitColor();
    mAlarmDigitColor = dlg.alarmDigitColor();
    setBackgroundColor( dlg.meterBackgroundColor() );

    repaint();
  }
}

void MultiMeter::applyStyle()
{
  mNormalDigitColor = KSGRD::Style->firstForegroundColor();
  setBackgroundColor( KSGRD::Style->backgroundColor() );

  repaint();
}

void MultiMeter::setDigitColor( const QColor& color )
{
  QPalette palette = mLcd->palette();
  palette.setColor( QPalette::WindowText, color );
  mLcd->setPalette( palette );
}

void MultiMeter::setBackgroundColor( const QColor& color )
{
  mBackgroundColor = color;

  QPalette pal = mLcd->palette();
  pal.setColor( mLcd->backgroundRole(), mBackgroundColor );
  mLcd->setPalette( pal );
}


