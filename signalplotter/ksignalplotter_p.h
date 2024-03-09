/*

    SPDX-FileCopyrightText: 2006-2009 John Tapsell <tapsell@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

//#define USE_QIMAGE

// SVG support causes it to crash at the moment :(
// (when re-enabling this remember to also link against plasma-framework)
//#define SVG_SUPPORT

// Use a separate child widget to draw the graph in
#ifndef KSYSGUARD_KSIGNALPLOTTER_P_H
#define KSYSGUARD_KSIGNALPLOTTER_P_H

#ifndef GRAPHICS_SIGNAL_PLOTTER
#define USE_SEPERATE_WIDGET
#include <QPaintEvent>
#include <QWidget>
#endif

#ifdef SVG_SUPPORT
namespace Plasma
{
class SVG;
}
#endif

#ifdef USE_SEPERATE_WIDGET
class GraphWidget;
#endif

class KSignalPlotter;

class KSignalPlotterPrivate
{
public:
    KSignalPlotterPrivate(KSignalPlotter *q_ptr);

    void drawWidget(QPainter *p, const QRect &boundingBox);
    void drawBackground(QPainter *p, const QRect &boundingBox) const;
    void drawThinFrame(QPainter *p, const QRect &boundingBox);
    void calculateNiceRange();
    void drawBeamToScrollableImage(QPainter *p, int index);
    void drawBeam(QPainter *p, const QRect &boundingBox, int horizontalScale, int index);
    void drawAxisText(QPainter *p, const QRect &boundingBox);
    void drawHorizontalLines(QPainter *p, const QRect &boundingBox) const;
    void drawVerticalLines(QPainter *p, const QRect &boundingBox, int correction = 0) const;
    void redrawScrollableImage();
    void reorderBeams(const QList<int> &newOrder);

    void recalculateMaxMinValueForSample(const QList<qreal> &sampleBuf, int time);
    void rescale();
    void updateDataBuffers();
    void setupStyle();
#ifdef GRAPHICS_SIGNAL_PLOTTER
    void themeChanged();
#endif

    /** Return the given value as a string, with the given precision */
    QString scaledValueAsString(qreal value, int precision) const;
    void addSample(const QList<qreal> &sampleBuf);
#ifdef SVG_SUPPORT
    void updateSvgBackground(const QRect &boundingBox);
    Plasma::SVG *mSvgRenderer;
#endif
    QString mSvgFilename;

    QPixmap mBackgroundImage; /// A cache of the background of the widget. Contains the SVG or just white background with lines
#ifdef USE_QIMAGE
    QImage mScrollableImage; /// The scrollable image for the widget.  Contains the SVG lines
#else
    QPixmap mScrollableImage; /// The scrollable image for the widget.  Contains the SVG lines
#endif
    int mScrollOffset; /// The scrollable image is, well, scrolled in a wrap-around window.  mScrollOffset determines where the left hand side of the
                       /// mScrollableImage should be drawn relative to the right hand side of view.  0 <= mScrollOffset < mScrollableImage.width()
    qreal mMinValue; /// The minimum value (unscaled) currently being displayed
    qreal mMaxValue; /// The maximum value (unscaled) currently being displayed

    qreal mUserMinValue; /// The minimum value (unscaled) set by changeRange().  This is the _maximum_ value that the range will start from.
    qreal mUserMaxValue; /// The maximum value (unscaled) set by changeRange().  This is the _minimum_ value that the range will reach to.
    unsigned int
        mRescaleTime; /// The number of data points passed since a value that is within 70% of the current maximum was found.  This is for scaling the graph

    qreal mNiceMinValue; /// The minimum value rounded down to a 'nice' value
    qreal mNiceMaxValue; /// The maximum value rounded up to a 'nice' value.  The idea is to round the value, say, 93 to 100.
    qreal mNiceRange; /// mNiceMaxValue - mNiceMinValue
    int mPrecision; /// The number of decimal place required to unambiguously label the axis

    qreal mScaleDownBy; /// @see setScaleDownBy
    bool mUseAutoRange; /// @see setUseAutoRange

    /**  Whether to show a white line on the left and bottom of the widget, for a 3D effect */
    bool mShowThinFrame;

    bool mShowVerticalLines;
    uint mVerticalLinesDistance;
    bool mVerticalLinesScroll;
    uint mVerticalLinesOffset;
    uint mHorizontalScale;
    int mHorizontalLinesCount;

    bool mShowHorizontalLines;

    bool mStackBeams; /// Set to add the beam values onto each other
    int mFillOpacity; /// Fill the area underneath the beams

    bool mShowAxis;

    QList<QList<qreal>> mBeamData; // Every item in the linked list contains a set of data points to plot.  The first item is the newest
    QList<QColor> mBeamColors; // These colors match up against the QList<qreal>  in mBeamData
    QList<QColor> mBeamColorsLight; // These colors match up against the QList<qreal> in mBeamData, and are lighter than mBeamColors.  Done for gradient effects

    unsigned int mMaxSamples; // This is what mBeamData.size() should equal when full.  When we start off and have no data then mSamples will be higher.  If we
                              // resize the widget so it's smaller, then for a short while this will be smaller
    int mNewestIndex; // The index to the newest item added.  newestIndex+1   is the second newest, and so on

    KLocalizedString mUnit;

    int mAxisTextWidth;
    int mActualAxisTextWidth; // Sometimes there just is not enough room for all the requested axisTextWidth
    QRect mPlottingArea; /// The area in which the beams are drawn.  Saved to make update() more efficient

    bool mSmoothGraph; /// Whether to smooth the graph by averaging using the formula (value*2 + last_value)/3.
    KSignalPlotter *q;
    bool mAxisTextOverlapsPlotter; // Whether we need to redraw the axis text on every update
#ifdef USE_SEPERATE_WIDGET
    GraphWidget *mGraphWidget; ///< This is the widget that draws the actual graph
#endif
};

#ifdef USE_SEPERATE_WIDGET
/* A class to draw the actual widget.  This is used for the QWidget version of KSignalPlotter in order to speed up redraws */
class GraphWidget : public QWidget
{
public:
    explicit GraphWidget(QWidget *parent);
    void paintEvent(QPaintEvent *event) override;

    KSignalPlotterPrivate *signalPlotterPrivate;
};
#endif

#endif // KSYSGUARD_KSIGNALPLOTTER_P_H
