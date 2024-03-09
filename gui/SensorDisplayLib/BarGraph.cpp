/*
    KSysGuard, the KDE System Guard

    Copyright (c) 1999, 2000 Chris Schlaeger <cs@kde.org>

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

#include <assert.h>
#include <string.h>

#include <QDebug>
#include <QPainter>

#include <KIconLoader>
#include "../formatter/Formatter.h"  // from libksysguard

#include "StyleEngine.h"

#include "BarGraph.h"

BarGraph::BarGraph( QWidget *parent )
  : QWidget( parent )
{
  bars = 0;
  minValue = 0.0;
  maxValue = 100.0;
  lowerLimit = upperLimit = 0.0;
  lowerLimitActive = upperLimitActive = false;

  normalColor = KSGRD::Style->firstForegroundColor();
  alarmColor = KSGRD::Style->alarmColor();
  mBackgroundColor = KSGRD::Style->backgroundColor();
  fontSize = KSGRD::Style->fontSize();

  // Anything smaller than this does not make sense.
  setMinimumSize( 16, 16 );
  setSizePolicy( QSizePolicy( QSizePolicy::Expanding,
                 QSizePolicy::Expanding ) );
}

BarGraph::~BarGraph()
{
}

bool BarGraph::addBar( const QString &footer )
{
  samples.resize( bars + 1 );
  samples[ bars++ ] = 0.0;
  footers.append( footer );

  return true;
}

bool BarGraph::removeBar( uint idx )
{
  if ( idx >= bars ) {
    qDebug() << "BarGraph::removeBar: idx " << idx << " out of range "
                  << bars;
    return false;
  }

  samples.resize( --bars );
  footers.removeAt(idx);
  update();

  return true;
}

void BarGraph::updateSamples( const QVector<double> &newSamples )
{
  samples = newSamples;
  update();
}

void BarGraph::changeRange( double min, double max )
{
  minValue = min;
  maxValue = max;
}

void BarGraph::paintEvent( QPaintEvent* )
{
  int w = width();
  int h = height();

  QPainter p( this );

  p.fillRect(0,0,w, h, mBackgroundColor);

  p.setBrush( palette().color( QPalette::Light) );
  p.setFont( QFont( p.font().family(), fontSize ) );
  QFontMetrics fm( p.font() );

  /* Draw white line along the bottom and the right side of the
   * widget to create a 3D like look. */
  p.drawLine( 0, h - 1, w - 1, h - 1 );
  p.drawLine( w - 1, 0, w - 1, h - 1 );

  p.setClipRect( 1, 1, w - 2, h - 2 );

  if ( bars > 0 ) {
    int barWidth = ( w - 2 ) / bars;
    uint b;
    /* Labels are only printed underneath the bars if the labels
     * for all bars are smaller than the bar width. If a single
     * label does not fit no label is shown. */
    bool showLabels = true;
    for ( b = 0; b < bars; b++ )
      if ( fm.boundingRect( footers[ b ] ).width() > barWidth )
        showLabels = false;

    int barHeight;
    if ( showLabels )
      barHeight = h - 2 - ( 2 * fm.lineSpacing() ) - 2;
    else
      barHeight = h - 2;

    for ( uint b = 0; b < bars; b++ ) {
      int topVal = int( (maxValue == 0.0) ? 0.0 :
                   ( (float)barHeight / maxValue * ( samples[ b ] - minValue ) ) );
      /* TODO: This widget does not handle negative values properly. */
      if ( topVal < 0 )
        topVal = 0;

      for ( int i = 0; i < barHeight && i < topVal; i += 2 ) {
        if ( ( upperLimitActive && samples[ b ] > upperLimit ) ||
             ( lowerLimitActive && samples[ b ] < lowerLimit ) )
          p.setPen( alarmColor.lighter( static_cast<int>( 30 + ( 70.0 /
                                      ( barHeight + 1 ) * i ) ) ) );
        else
          p.setPen( normalColor.lighter( static_cast<int>( 30 + ( 70.0 /
                                      ( barHeight + 1 ) * i ) ) ) );
        p.drawLine( b * barWidth + 3, barHeight - i, ( b + 1 ) * barWidth - 3,
                    barHeight - i );
      }

      if ( ( upperLimitActive && samples[ b ] > upperLimit ) ||
           ( lowerLimitActive && samples[ b ] < lowerLimit ) )
        p.setPen( alarmColor );
      else
        p.setPen( normalColor );

      if ( showLabels ) {
        p.drawText( b * barWidth + 3, h - ( 2 * fm.lineSpacing() ) - 2,
                    barWidth - 2 * 3, fm.lineSpacing(), Qt::AlignCenter,
                    footers[ b ] );
        p.drawText( b * barWidth + 3, h - fm.lineSpacing() - 2,
                    barWidth - 2 * 3, fm.lineSpacing(), Qt::AlignCenter,
                    KSysGuard::Formatter::formatValue( samples[b], KSysGuard::UnitNone ) );
      }
    }
  }

  p.end();
}


