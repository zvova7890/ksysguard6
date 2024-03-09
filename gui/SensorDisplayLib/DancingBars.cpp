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

#include <QCheckBox>
#include <QDebug>
#include <QDomElement>
#include <QPushButton>
#include <QHBoxLayout>

#include <KLocalizedString>
#include "../ksgrd/SensorManager.h"
#include "StyleEngine.h"

#include "BarGraph.h"
#include "DancingBarsSettings.h"

#include "DancingBars.h"

DancingBars::DancingBars( QWidget *parent, const QString &title, SharedSettings *workSheetSettings)
  : KSGRD::SensorDisplay( parent, title, workSheetSettings)
{
  mBars = 0;
  mFlags = QBitArray( 100 );
  mFlags.fill( false );

  QLayout *layout = new QHBoxLayout(this);
  mPlotter = new BarGraph( this );
  layout->addWidget(mPlotter);

  setMinimumSize( sizeHint() );

  /* All RMB clicks to the mPlotter widget will be handled by 
   * SensorDisplay::eventFilter. */
  mPlotter->installEventFilter( this );

  setPlotterWidget( mPlotter );

}

DancingBars::~DancingBars()
{
}

void DancingBars::configureSettings()
{
  DancingBarsSettings dlg( this );

  dlg.setTitle( title() );
  dlg.setMinValue( mPlotter->getMin() );
  dlg.setMaxValue( mPlotter->getMax() );

  double l, u;
  bool la, ua;
  mPlotter->getLimits( l, la, u, ua );

  dlg.setUseUpperLimit( ua );
  dlg.setUpperLimit( u );

  dlg.setUseLowerLimit( la );
  dlg.setLowerLimit( l );

  dlg.setForegroundColor( mPlotter->normalColor );
  dlg.setAlarmColor( mPlotter->alarmColor );
  dlg.setBackgroundColor( mPlotter->mBackgroundColor );
  dlg.setFontSize( mPlotter->fontSize );

  SensorModelEntry::List list;
  for(int i = 0; i < mBars; i++){
    SensorModelEntry entry;
    auto sensor = sensors().at( i );
    entry.setId( i );
    entry.setHostName( sensor->hostName() );
    entry.setSensorName( KSGRD::SensorMgr->translateSensor( sensor->name() ) );
    entry.setLabel( mPlotter->footers[ i ] );
    entry.setUnit( KSGRD::SensorMgr->translateUnit( sensor->unit() ) );
    entry.setStatus( sensor->isOk() ? i18n( "OK" ) : i18n( "Error" ) );

    list.append( entry );
  }
  dlg.setSensors( list );

  if ( !dlg.exec() )
    return;

  setTitle( dlg.title() );
  mPlotter->changeRange( dlg.minValue(), dlg.maxValue() );
  mPlotter->setLimits( dlg.useLowerLimit() ?
                       dlg.lowerLimit() : 0,
                       dlg.useLowerLimit(),
                       dlg.useUpperLimit() ?
                       dlg.upperLimit() : 0,
                       dlg.useUpperLimit() );

  mPlotter->normalColor = dlg.foregroundColor();
  mPlotter->alarmColor = dlg.alarmColor();
  mPlotter->mBackgroundColor = dlg.backgroundColor();
  mPlotter->fontSize = dlg.fontSize();

  // Each deleted Id is relative to the length of the list
  // at the time of deletion.
  QList<uint> deletedIds = dlg.getDeletedIds();
  for(int i = 0; i<deletedIds.count(); i++){
	  removeSensor(deletedIds[i]);
  }

  // If the range has reset to "auto-range" then we need to ask for
  // sensor info to re-calibrate. In answerReceived() there's a special-
  // case recalibrating on sensor 0 (with id 100), so ask for that one.
  if ( mPlotter->getMin() == 0.0 && mPlotter->getMax() == 0.0 && mBars > 0 ) {
      const auto& sensor = sensors().at( 0 );
      // The 100 is magic in answerReceived()
      sendRequest( sensor->hostName(), sensor->name() + QLatin1Char('?'), 100 );
  }

  // The remaining entries in the dialog are the ones that haven't been
  // deleted, so that should match the remaining sensors. The dialog does
  // not edit units or real sensor information, but can change the label.
  // Reset the footer labels as needed.
  const auto remainingSensors = dlg.sensors();
  if (remainingSensors.count() == mPlotter->footers.count())
  {
    for(int i = 0; i < remainingSensors.count(); ++i) {
      const auto newLabel = remainingSensors.at(i).label();
      if (newLabel != mPlotter->footers[ i ]) {
        mPlotter->footers[ i ] = newLabel;
      }
    }
  }

  repaint();
}

void DancingBars::applyStyle()
{
  mPlotter->normalColor = KSGRD::Style->firstForegroundColor();
  mPlotter->alarmColor = KSGRD::Style->alarmColor();
  mPlotter->mBackgroundColor = KSGRD::Style->backgroundColor();
  mPlotter->fontSize = KSGRD::Style->fontSize();

  repaint();
}

bool DancingBars::addSensor( const QString &hostName, const QString &name,
                             const QString &type, const QString &title )
{
  if ( type != QLatin1String("integer") && type != QLatin1String("float") )
    return false;

  if ( mBars >= 32 )
    return false;

  if ( !mPlotter->addBar( title ) )
    return false;

  registerSensor( new KSGRD::SensorProperties( hostName, name, type, title ) );

  /* To differentiate between answers from value requests and info
   * requests we add 100 to the beam index for info requests. */
  sendRequest( hostName, name + QLatin1Char('?'), mBars + 100 );
  ++mBars;
  mSampleBuffer.resize( mBars );

  QString tooltip;
  for ( int i = 0; i < mBars; ++i ) {
    tooltip += QStringLiteral( "%1%2:%3" ).arg( i != 0 ? QStringLiteral("\n") : QString() )
                                   .arg( sensors().at( i )->hostName() )
                                   .arg( sensors().at( i )->name() );
  }
  mPlotter->setToolTip( tooltip );

  return true;
}

bool DancingBars::removeSensor( uint pos )
{
  if ( pos >= mBars ) {
    qDebug() << "DancingBars::removeSensor: idx out of range ("
                  << pos << ")";
    return false;
  }

  mPlotter->removeBar( pos );
  mBars--;
  KSGRD::SensorDisplay::removeSensor( pos );

  QString tooltip;
  for ( int i = 0; i < mBars; ++i ) {
    tooltip += QStringLiteral( "%1%2:%3" ).arg( i != 0 ? QStringLiteral("\n") : QString() )
                                   .arg( sensors().at( i )->hostName() )
                                   .arg( sensors().at( i )->name() );
  }
  mPlotter->setToolTip( tooltip );

  return true;
}

void DancingBars::updateSamples( const QVector<double> &samples )
{
  mPlotter->updateSamples( samples );
}

void DancingBars::answerReceived( int id, const QList<QByteArray> &answerlist )
{
  /* We received something, so the sensor is probably ok. */
  sensorError( id, false );
  QByteArray answer;
  if(!answerlist.isEmpty()) answer = answerlist[0];
  if ( id < 100 ) {
    if(id >= mSampleBuffer.count()) {
      qDebug() << "ERROR: DancingBars received invalid data";
      sensorError(id, true);
      return;
    }
    mSampleBuffer[ id ] = answer.toDouble();
    if ( mFlags.testBit( id ) == true ) {
      qDebug() << "ERROR: DancingBars lost sample (" << mFlags
                    << ", " << mBars << ")";
      sensorError( id, true );
      return;
    }
    mFlags.setBit( id, true );

    bool allBitsAvailable = true;
    for ( int i = 0; i < mBars; ++i )
      allBitsAvailable &= mFlags.testBit( i );

    if ( allBitsAvailable ) {
      mPlotter->updateSamples( mSampleBuffer );
      mFlags.fill( false );
    }
  } else if ( id >= 100 ) {
    KSGRD::SensorIntegerInfo info( answer );
    if ( id == 100 )
      if ( mPlotter->getMin() == 0.0 && mPlotter->getMax() == 0.0 ) {
        /* We only use this information from the sensor when the
         * display is still using the default values. If the
         * sensor has been restored we don't touch the already set
         * values. */
        mPlotter->changeRange( info.min(), info.max() );
      }

    sensors().at( id - 100 )->setUnit( info.unit() );
  }
}

bool DancingBars::restoreSettings( QDomElement &element )
{
  SensorDisplay::restoreSettings( element );

  mPlotter->changeRange( element.attribute( QStringLiteral("min"), QStringLiteral("0") ).toDouble(),
                         element.attribute( QStringLiteral("max"), QStringLiteral("0") ).toDouble() );

  mPlotter->setLimits( element.attribute( QStringLiteral("lowlimit"), QStringLiteral("0") ).toDouble(),
                       element.attribute( QStringLiteral("lowlimitactive"), QStringLiteral("0") ).toInt(),
                       element.attribute( QStringLiteral("uplimit"), QStringLiteral("0") ).toDouble(),
                       element.attribute( QStringLiteral("uplimitactive"), QStringLiteral("0") ).toInt() );

  mPlotter->normalColor = restoreColor( element, QStringLiteral("normalColor"),
                                        KSGRD::Style->firstForegroundColor() );
  mPlotter->alarmColor = restoreColor( element, QStringLiteral("alarmColor"),
                                       KSGRD::Style->alarmColor() );
  mPlotter->mBackgroundColor = restoreColor( element, QStringLiteral("backgroundColor"),
                                            KSGRD::Style->backgroundColor() );
  mPlotter->fontSize = element.attribute( QStringLiteral("fontSize"), QStringLiteral( "%1" ).arg(
                                          KSGRD::Style->fontSize() ) ).toInt();

  QDomNodeList dnList = element.elementsByTagName( QStringLiteral("beam") );
  for ( int i = 0; i < dnList.count(); ++i ) {
    QDomElement el = dnList.item( i ).toElement();
    addSensor( el.attribute( QStringLiteral("hostName") ), el.attribute( QStringLiteral("sensorName") ),
               ( el.attribute( QStringLiteral("sensorType") ).isEmpty() ? QStringLiteral("integer") :
               el.attribute( QStringLiteral("sensorType") ) ), el.attribute( QStringLiteral("sensorDescr") ) );
  }


  return true;
}

bool DancingBars::saveSettings( QDomDocument &doc, QDomElement &element)
{
  element.setAttribute( QStringLiteral("min"), mPlotter->getMin() );
  element.setAttribute( QStringLiteral("max"), mPlotter->getMax() );
  double l, u;
  bool la, ua;
  mPlotter->getLimits( l, la, u, ua );
  element.setAttribute( QStringLiteral("lowlimit"), l );
  element.setAttribute( QStringLiteral("lowlimitactive"), la );
  element.setAttribute( QStringLiteral("uplimit"), u );
  element.setAttribute( QStringLiteral("uplimitactive"), ua );

  saveColor( element, QStringLiteral("normalColor"), mPlotter->normalColor );
  saveColor( element, QStringLiteral("alarmColor"), mPlotter->alarmColor );
	saveColor( element, QStringLiteral("backgroundColor"), mPlotter->mBackgroundColor );
  element.setAttribute( QStringLiteral("fontSize"), mPlotter->fontSize );

  for ( int i = 0; i < mBars; ++i ) {
    QDomElement beam = doc.createElement( QStringLiteral("beam") );
    element.appendChild( beam );
    beam.setAttribute( QStringLiteral("hostName"), sensors().at( i )->hostName() );
    beam.setAttribute( QStringLiteral("sensorName"), sensors().at( i )->name() );
    beam.setAttribute( QStringLiteral("sensorType"), sensors().at( i )->type() );
    beam.setAttribute( QStringLiteral("sensorDescr"), mPlotter->footers[ i ] );
  }

  SensorDisplay::saveSettings( doc, element );

  return true;
}

bool DancingBars::hasSettingsDialog() const
{
  return true;
}


