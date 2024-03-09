/*
    KSysGuard, the KDE System Guard
   
    Copyright (c) 1999 - 2001 Chris Schlaeger <cs@kde.org>
    
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

#ifndef KSG_SENSORDISPLAY_H
#define KSG_SENSORDISPLAY_H

#include <QEvent>
#include <QPointer>
#include <QWidget>

#include "../ksgrd/SensorClient.h"
#include "SharedSettings.h"

class QDomDocument;
class QDomElement;

namespace KSGRD {

class SensorProperties;

/**
  This class is the base class for all displays for sensors. A
  display is any kind of widget that can display the value of one or
  more sensors in any form. It must be inherited by all displays that
  should be inserted into the work sheet.
 */
class SensorDisplay : public QWidget, public SensorClient
{
  Q_OBJECT

  public:
    class DeleteEvent : public QEvent
    {
      public:
        DeleteEvent( SensorDisplay *display );

        SensorDisplay* display() const;

      private:
        SensorDisplay *mDisplay;
    };

    /**
      Constructor.
     */
    SensorDisplay( QWidget *parent, const QString &title, SharedSettings *workSheetSettings );

    /**
      Destructor.
     */
    ~SensorDisplay() override;

    /**
      Sets the title of the display.  If you override, please call this
     */
    virtual void setTitle( const QString &title );

    /**
      Returns the title of the display.
     */
    QString title() const;

    /**
      Returns the translated title of the display.
     */
    QString translatedTitle() const;

    /**
      Sets the unit of the display.
     */
    void setUnit( const QString &unit );

    /**
      Returns the unit of the display.
     */
    QString unit() const;

    /**
      Sets whether the unit string should be displayed at the top
      of the display frame.
     */
    void setShowUnit( bool value );

    /**
      Returns whether the unit string should be displayed at the top
      of the display frame. @see setShowUnit()
     */
    bool showUnit() const;

    /**
      This method appends all hosts of the display to @ref list.
     */
    void hosts( QStringList& list );

    /**
      Sets the widget on which the error icon can be drawn.
     */
    void setPlotterWidget( QWidget *plotter );

    /**
      Add a sensor to the display.

      @param hostName The name of the host, the sensor belongs to.
      @param name The sensor name.
      @param type The type of the sensor.
      @param description A short description of the sensor.
     */
    virtual bool addSensor( const QString &hostName, const QString &name,
                            const QString &type, const QString &description );

    /**
      Removes the sensor from the display, that is at the position
      @ref pos of the intern sensor list.
     */
    virtual bool removeSensor( uint pos );

    /**
      This function is a wrapper function to SensorManager::sendRequest.
      It should be used by all SensorDisplay functions that need to send
      a request to a sensor since it performs an appropriate error
      handling by removing the display of necessary.
     */
    void sendRequest( const QString &hostName, const QString &cmd, int id );

    /**
      Returns whether the display provides a settings dialog.
      This method should be reimplemented in the derived class.
     */
    virtual bool hasSettingsDialog() const;

    /**
      This method is called to raise the settings dialog of the
      display. It should be reimplemented in the derived class.
     */
    virtual void configureSettings();

    /**
      Reimplement this method to setup the display from config data.
     */
    virtual bool restoreSettings( QDomElement& );

    /**
      Reimplement this method to save the displays config data.
     */
    virtual bool saveSettings( QDomDocument&, QDomElement&);

    /**
      Reimplement this method to catch error messages from the SensorManager.

      @param sensorId The unique id of the sensor.
      @param mode The mode: true = error, false = everything ok
     */
    virtual void sensorError( int sensorId, bool mode );

    /**
      Normally you shouldn't reimplement this method
     */
    void sensorLost( int reqId ) override;

    /**
     * Sets the object where the delete events will be sent to.
     */
    void setDeleteNotifier( QObject *object );

  public Q_SLOTS:

    /**
      Calling this method emits the @ref showPopupMenu() with this
      display as argument.
     */
    void rmbPressed();

    /**
      This method can be used to apply the new settings. Just connect
      the applyClicked() signal of your configuration dialog with this
      slot and reimplement it.
     */
    virtual void applySettings();

    /**
      This methid is called whenever the global style is changed.
      Reimplement it to apply the new style settings to the display.
     */
    virtual void applyStyle();

    /**
     * This is called when the display should request more information
     * and update itself
     */
    virtual void timerTick();

    void showContextMenu(const QPoint &);

  Q_SIGNALS:
    void showPopupMenu( KSGRD::SensorDisplay *display );
    void titleChanged(const QString&);
    void translatedTitleChanged(const QString&);

  protected:
    bool eventFilter( QObject*, QEvent* ) override;
    void changeEvent( QEvent * event ) override;

    void registerSensor( SensorProperties *sp );
    void unregisterSensor( uint pos );

    QColor restoreColor( QDomElement &element, const QString &attr,
                         const QColor& fallback );
    void saveColor( QDomElement &element, const QString &attr,
                    const QColor &color );
    void saveColorAppend( QDomElement &element, const QString &attr,
                    const QColor &color );

    virtual QString additionalWhatsThis();

    void setSensorOk( bool ok );


    QList<SensorProperties *> &sensors();

    SharedSettings *mSharedSettings;

  private:
    void updateWhatsThis();

    bool mShowUnit;
    bool mUseGlobalUpdateInterval;

    int mTimerId;
    int mUpdateInterval;

    QList<SensorProperties *> mSensors;

    QString mTitle;
    QString mTranslatedTitle;
    QString mUnit;

    QWidget* mErrorIndicator;
    QWidget* mPlotterWdg;

    QPointer<QObject> mDeleteNotifier;
};

class SensorProperties
{
  public:
    SensorProperties();
    SensorProperties( const QString &hostName, const QString &name,
                      const QString &type, const QString &description );
    virtual ~SensorProperties();

    void setHostName( const QString &hostName );
    QString hostName() const;

    bool isLocalhost() const;

    void setName( const QString &name );
    QString name() const;

    void setType( const QString &type );
    QString type() const;

    void setDescription( const QString &description );
    QString description() const;

    void setUnit( const QString &unit );
    QString unit() const;

    void setIsOk( bool value );
    bool isOk() const;

    void setRegExpName( const QString &name );
    QString regExpName() const;

  private:
    bool mIsLocalhost;
    QString mHostName;
    QString mName;
    QString mType;
    QString mDescription;
    QString mUnit;
    QString mRegExpName;

    /* This flag indicates whether the communication to the sensor is
     * ok or not. */
    bool mOk;
};

}

#endif
