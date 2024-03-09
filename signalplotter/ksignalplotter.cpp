/*
    SPDX-FileCopyrightText: 2006-2009 John Tapsell <tapsell@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

*/

#ifdef GRAPHICS_SIGNAL_PLOTTER
#define KSignalPlotter KGraphicsSignalPlotter
#define KSignalPlotterPrivate KGraphicsSignalPlotterPrivate
#include "kgraphicssignalplotter.h"
#else
#undef KSignalPlotter
#undef KSignalPlotterPrivate
#include "ksignalplotter.h"
#endif

#include "ksignalplotter_debug.h"
#include "ksignalplotter_p.h"

#include <QDebug>
#include <QEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

#ifdef GRAPHICS_SIGNAL_PLOTTER
#include <QGraphicsSceneResizeEvent>
#include <QStyleOptionGraphicsItem>
#endif

#include <cmath>
#include <kiconloader.h>
#include <limits>

#ifdef SVG_SUPPORT
#include <Plasma/Svg>
#include <Plasma/Theme>
#endif

#define VERTICAL_LINE_OFFSET 1
// Never store less 1000 samples if not visible.  This is kinda arbitrary
#define NUM_SAMPLES_WHEN_INVISIBLE ((uint)1000)

#ifdef GRAPHICS_SIGNAL_PLOTTER
KGraphicsSignalPlotter::KGraphicsSignalPlotter(QGraphicsItem *parent)
    : QGraphicsWidget(parent)
    , d(new KGraphicsSignalPlotterPrivate(this))
#else
KSignalPlotter::KSignalPlotter(QWidget *parent)
    : QWidget(parent)
    , d(new KSignalPlotterPrivate(this))
#endif
{
    qRegisterMetaType<KLocalizedString>("KLocalizedString");
    // Anything smaller than this does not make sense.
    setMinimumSize(KIconLoader::SizeSmall, KIconLoader::SizeSmall);
    QSizePolicy sizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    sizePolicy.setHeightForWidth(false);
    setSizePolicy(sizePolicy);

#ifdef GRAPHICS_SIGNAL_PLOTTER
    setFlag(QGraphicsItem::ItemClipsToShape);
#endif
}

KSignalPlotter::~KSignalPlotter()
{
    delete d;
}

KLocalizedString KSignalPlotter::unit() const
{
    return d->mUnit;
}
void KSignalPlotter::setUnit(const KLocalizedString &unit)
{
    d->mUnit = unit;
}

void KSignalPlotter::addBeam(const QColor &color)
{
    // When we add a new beam, go back and set the data for this beam to NaN for all the other times, to pad it out.
    // This is because it makes it easier for moveSensors
    for (QList<QList<qreal>>::Iterator it = d->mBeamData.begin(), total = d->mBeamData.end(); it != total; ++it) {
        (*it).append(std::numeric_limits<qreal>::quiet_NaN());
    }
    d->mBeamColors.append(color);
    d->mBeamColorsLight.append(color.lighter());
}

QColor KSignalPlotter::beamColor(int index) const
{
    return d->mBeamColors[index];
}

void KSignalPlotter::setBeamColor(int index, const QColor &color)
{
    if (!color.isValid()) {
        qCDebug(LIBKSYSGUARD_KSIGNALPLOTTER) << "Invalid color";
        return;
    }
    if (index >= d->mBeamColors.count()) {
        qCDebug(LIBKSYSGUARD_KSIGNALPLOTTER) << "Invalid index" << index;
        return;
    }
    Q_ASSERT(d->mBeamColors.count() == d->mBeamColorsLight.count());

    d->mBeamColors[index] = color;
    d->mBeamColorsLight[index] = color.lighter();
}

int KSignalPlotter::numBeams() const
{
    return d->mBeamColors.count();
}

void KSignalPlotter::addSample(const QList<qreal> &sampleBuf)
{
    d->addSample(sampleBuf);
#ifdef USE_SEPERATE_WIDGET
    d->mGraphWidget->update();
#else
    update(d->mPlottingArea);
#endif
}
void KSignalPlotter::reorderBeams(const QList<int> &newOrder)
{
    d->reorderBeams(newOrder);
}

void KSignalPlotter::changeRange(qreal min, qreal max)
{
    if (min == d->mUserMinValue && max == d->mUserMaxValue)
        return;
    d->mUserMinValue = min;
    d->mUserMaxValue = max;
    d->calculateNiceRange();
}

void KSignalPlotter::removeBeam(int index)
{
    if (index >= d->mBeamColors.size())
        return;
    if (index >= d->mBeamColorsLight.size())
        return;
    d->mBeamColors.removeAt(index);
    d->mBeamColorsLight.removeAt(index);

    QList<QList<qreal>>::Iterator i;
    for (i = d->mBeamData.begin(); i != d->mBeamData.end(); ++i) {
        if ((*i).size() >= index)
            (*i).removeAt(index);
    }
    if (d->mUseAutoRange)
        d->rescale();
}

void KSignalPlotter::setScaleDownBy(qreal value)
{
    if (d->mScaleDownBy == value)
        return;
    d->mScaleDownBy = value;
    d->mBackgroundImage = QPixmap(); // we changed a paint setting, so reset the cache
    d->calculateNiceRange();
    update();
}
qreal KSignalPlotter::scaleDownBy() const
{
    return d->mScaleDownBy;
}

void KSignalPlotter::setUseAutoRange(bool value)
{
    d->mUseAutoRange = value;
    d->calculateNiceRange();
    // this change will be detected in paint and the image cache regenerated
}

bool KSignalPlotter::useAutoRange() const
{
    return d->mUseAutoRange;
}

void KSignalPlotter::setMinimumValue(qreal min)
{
    if (min == d->mUserMinValue)
        return;
    d->mUserMinValue = min;
    d->calculateNiceRange();
    update();
    // this change will be detected in paint and the image cache regenerated
}

qreal KSignalPlotter::minimumValue() const
{
    return d->mUserMinValue;
}

void KSignalPlotter::setMaximumValue(qreal max)
{
    if (max == d->mUserMaxValue)
        return;
    d->mUserMaxValue = max;
    d->calculateNiceRange();
    update();
    // this change will be detected in paint and the image cache regenerated
}

qreal KSignalPlotter::maximumValue() const
{
    return d->mUserMaxValue;
}

qreal KSignalPlotter::currentMaximumRangeValue() const
{
    return d->mNiceMaxValue;
}

qreal KSignalPlotter::currentMinimumRangeValue() const
{
    return d->mNiceMinValue;
}

void KSignalPlotter::setHorizontalScale(uint scale)
{
    if (scale == d->mHorizontalScale || scale == 0)
        return;

    d->mHorizontalScale = scale;
    d->updateDataBuffers();
#ifdef USE_QIMAGE
    d->mScrollableImage = QImage();
#else
    d->mScrollableImage = QPixmap();
#endif

    update();
}

int KSignalPlotter::horizontalScale() const
{
    return d->mHorizontalScale;
}

void KSignalPlotter::setShowVerticalLines(bool value)
{
    if (d->mShowVerticalLines == value)
        return;
    d->mShowVerticalLines = value;
    d->mBackgroundImage = QPixmap(); // we changed a paint setting, so reset the cache
#ifdef USE_QIMAGE
    d->mScrollableImage = QImage();
#else
    d->mScrollableImage = QPixmap();
#endif
    update();
}

bool KSignalPlotter::showVerticalLines() const
{
    return d->mShowVerticalLines;
}

void KSignalPlotter::setVerticalLinesDistance(uint distance)
{
    if (distance == d->mVerticalLinesDistance)
        return;
    d->mVerticalLinesDistance = distance;
    d->mBackgroundImage = QPixmap(); // we changed a paint setting, so reset the cache
#ifdef USE_QIMAGE
    d->mScrollableImage = QImage();
#else
    d->mScrollableImage = QPixmap();
#endif
    update();
}

uint KSignalPlotter::verticalLinesDistance() const
{
    return d->mVerticalLinesDistance;
}

void KSignalPlotter::setVerticalLinesScroll(bool value)
{
    if (value == d->mVerticalLinesScroll)
        return;
    d->mVerticalLinesScroll = value;
#ifdef USE_QIMAGE
    d->mScrollableImage = QImage();
#else
    d->mScrollableImage = QPixmap();
#endif
    update();
}

bool KSignalPlotter::verticalLinesScroll() const
{
    return d->mVerticalLinesScroll;
}

void KSignalPlotter::setShowHorizontalLines(bool value)
{
    if (value == d->mShowHorizontalLines)
        return;
    d->mShowHorizontalLines = value;
#ifdef USE_QIMAGE
    d->mScrollableImage = QImage();
#else
    d->mScrollableImage = QPixmap();
#endif

    update();
}

bool KSignalPlotter::showHorizontalLines() const
{
    return d->mShowHorizontalLines;
}

void KSignalPlotter::setShowAxis(bool value)
{
    if (value == d->mShowAxis)
        return;
    d->mShowAxis = value;
    d->mBackgroundImage = QPixmap(); // we changed a paint setting, so reset the cache
#ifdef USE_QIMAGE
    d->mScrollableImage = QImage();
#else
    d->mScrollableImage = QPixmap();
#endif
    update();
}

bool KSignalPlotter::showAxis() const
{
    return d->mShowAxis;
}

QString KSignalPlotter::svgBackground() const
{
    return d->mSvgFilename;
}
void KSignalPlotter::setSvgBackground(const QString &filename)
{
    if (d->mSvgFilename == filename)
        return;
    d->mSvgFilename = filename;
#ifdef SVG_SUPPORT
    if (filename.isEmpty()) {
        delete d->mSvgRenderer;
        d->mSvgRenderer = NULL;
    } else {
        if (!d->mSvgRenderer)
            d->mSvgRenderer = new Plasma::Svg(this);
        d->mSvgRenderer->setImagePath(d->mSvgFilename);
    }
#endif
    d->mBackgroundImage = QPixmap();
    update();
}

void KSignalPlotter::setMaxAxisTextWidth(int axisTextWidth)
{
    if (d->mAxisTextWidth == axisTextWidth)
        return;
    d->mAxisTextWidth = axisTextWidth;
    d->mBackgroundImage = QPixmap(); // we changed a paint setting, so reset the cache
    update();
}

int KSignalPlotter::maxAxisTextWidth() const
{
    return d->mAxisTextWidth;
}
void KSignalPlotter::changeEvent(QEvent *event)
{
    switch (event->type()) {
    case QEvent::ApplicationPaletteChange:
    case QEvent::PaletteChange:
    case QEvent::FontChange:
    case QEvent::LanguageChange:
    case QEvent::LocaleChange:
    case QEvent::LayoutDirectionChange:
        d->mBackgroundImage = QPixmap(); // we changed a paint setting, so reset the cache
        update();
        break;
    default: // Do nothing
        break;
    }
}
#ifdef GRAPHICS_SIGNAL_PLOTTER
void KGraphicsSignalPlotter::resizeEvent(QGraphicsSceneResizeEvent *event)
{
    QRect boundingBox(QPoint(0, 0), event->newSize().toSize());
    int fontHeight = QFontMetrics(font()).height();
#else
void KSignalPlotter::resizeEvent(QResizeEvent *event)
{
    QRect boundingBox(QPoint(0, 0), event->size());
    int fontHeight = fontMetrics().height();
#endif
    if (d->mShowAxis && d->mAxisTextWidth != 0 && boundingBox.width() > (d->mAxisTextWidth * 1.10 + 2)
        && boundingBox.height() > fontHeight) { // if there's room to draw the labels, then draw them!
        // We want to adjust the size of plotter bit inside so that the axis text aligns nicely at the top and bottom
        // but we don't want to sacrifice too much of the available room, so don't use it if it will take more than 20% of the available space
        qreal offset = (fontHeight + 1) / 2;
        if (offset < boundingBox.height() * 0.1)
            boundingBox.adjust(0, offset, 0, -offset);

        int padding = 1;
        if (boundingBox.width() > d->mAxisTextWidth + 50)
            padding = 10; // If there's plenty of room, at 10 pixels for padding the axis text, so that it looks nice

        if (layoutDirection() == Qt::RightToLeft)
            boundingBox.setRight(boundingBox.right() - d->mAxisTextWidth - padding);
        else
            boundingBox.setLeft(d->mAxisTextWidth + padding);
        d->mActualAxisTextWidth = d->mAxisTextWidth;
    } else {
        d->mActualAxisTextWidth = 0;
    }
    if (d->mShowThinFrame)
        boundingBox.adjust(0, 0, -1, -1);

    // Remember bounding box to pass to update, so that we only update the plotting area the next time, if that's the only that thing that has changed
    d->mPlottingArea = boundingBox;

    // Calculate the new number of horizontal lines
    int newHorizontalLinesCount = qBound(0, (int)(boundingBox.height() / fontHeight) - 2, 4);
    if (newHorizontalLinesCount != d->mHorizontalLinesCount) {
        d->mHorizontalLinesCount = newHorizontalLinesCount;
        d->calculateNiceRange();
    }
#ifdef USE_QIMAGE
    d->mScrollableImage = QImage();
#else
    d->mScrollableImage = QPixmap();
#endif

#ifdef USE_SEPERATE_WIDGET
    d->mGraphWidget->setVisible(true);
    d->mGraphWidget->setGeometry(boundingBox);
#endif

    d->updateDataBuffers();
}

KSignalPlotterPrivate::KSignalPlotterPrivate(KSignalPlotter *q_ptr_)
    : q(q_ptr_)
{
    mPrecision = 0;
    mMaxSamples = NUM_SAMPLES_WHEN_INVISIBLE;
    mMinValue = mMaxValue = std::numeric_limits<qreal>::quiet_NaN();
    mUserMinValue = mUserMaxValue = 0.0;
    mNiceMinValue = mNiceMaxValue = 0.0;
    mNiceRange = 0;
    mUseAutoRange = true;
    mScaleDownBy = 1;
    mShowThinFrame = true;
    mSmoothGraph = true;
    mShowVerticalLines = false;
    mVerticalLinesDistance = 30;
    mVerticalLinesScroll = true;
    mVerticalLinesOffset = 0;
    mHorizontalScale = 6;
    mShowHorizontalLines = true;
    mHorizontalLinesCount = 4;
    mShowAxis = true;
    mAxisTextWidth = 0;
    mScrollOffset = 0;
    mStackBeams = false;
    mFillOpacity = 20;
    mRescaleTime = 0;
    mUnit = ki18n("%1");
    mAxisTextOverlapsPlotter = false;
    mActualAxisTextWidth = 0;
#ifdef USE_SEPERATE_WIDGET
    mGraphWidget = new GraphWidget(q); ///< This is the widget that draws the actual graph
    mGraphWidget->signalPlotterPrivate = this;
    mGraphWidget->setVisible(false);
#endif
}

void KSignalPlotterPrivate::recalculateMaxMinValueForSample(const QList<qreal> &sampleBuf, int time)
{
    if (mStackBeams) {
        qreal value = 0;
        for (int i = sampleBuf.count() - 1; i >= 0; i--) {
            qreal newValue = sampleBuf[i];
            if (!std::isinf(newValue) && !std::isnan(newValue))
                value += newValue;
        }
        if (std::isnan(mMinValue) || mMinValue > value)
            mMinValue = value;
        if (std::isnan(mMaxValue) || mMaxValue < value)
            mMaxValue = value;
        if (value > 0.7 * mMaxValue)
            mRescaleTime = time;
    } else {
        qreal value;
        for (int i = sampleBuf.count() - 1; i >= 0; i--) {
            value = sampleBuf[i];
            if (!std::isinf(value) && !std::isnan(value)) {
                if (std::isnan(mMinValue) || mMinValue > value)
                    mMinValue = value;
                if (std::isnan(mMaxValue) || mMaxValue < value)
                    mMaxValue = value;
                if (value > 0.7 * mMaxValue)
                    mRescaleTime = time;
            }
        }
    }
}

void KSignalPlotterPrivate::rescale()
{
    mMaxValue = mMinValue = std::numeric_limits<qreal>::quiet_NaN();
    for (int i = mBeamData.count() - 1; i >= 0; i--) {
        recalculateMaxMinValueForSample(mBeamData[i], i);
    }
    calculateNiceRange();
}

void KSignalPlotterPrivate::addSample(const QList<qreal> &sampleBuf)
{
    if (sampleBuf.count() != mBeamColors.count()) {
        qCDebug(LIBKSYSGUARD_KSIGNALPLOTTER) << "Sample data discarded - contains wrong number of beams";
        return;
    }
    mBeamData.prepend(sampleBuf);
    if ((unsigned int)mBeamData.size() > mMaxSamples) {
        mBeamData.removeLast(); // we have too many.  Remove the last item
        if ((unsigned int)mBeamData.size() > mMaxSamples)
            mBeamData
                .removeLast(); // If we still have too many, then we have resized the widget.  Remove one more.  That way we will slowly resize to the new size
    }

    if (mUseAutoRange) {
        recalculateMaxMinValueForSample(sampleBuf, 0);

        if (mRescaleTime++ > mMaxSamples)
            rescale();
        else if (mMinValue < mNiceMinValue || mMaxValue > mNiceMaxValue
                 || (mMaxValue > mUserMaxValue && mNiceRange != 1 && mMaxValue < (mNiceRange * 0.75 + mNiceMinValue)) || mNiceRange == 0)
            calculateNiceRange();
    } else {
        if (mMinValue < mNiceMinValue || mMaxValue > mNiceMaxValue
            || (mMaxValue > mUserMaxValue && mNiceRange != 1 && mMaxValue < (mNiceRange * 0.75 + mNiceMinValue)) || mNiceRange == 0)
            calculateNiceRange();
    }

    if (mScrollableImage.isNull())
        return;
    QPainter pCache(&mScrollableImage);
    drawBeamToScrollableImage(&pCache, 0);
}

void KSignalPlotterPrivate::reorderBeams(const QList<int> &newOrder)
{
    if (newOrder.count() != mBeamColors.count()) {
        return;
    }
    QList<QList<qreal>>::Iterator it;
    for (it = mBeamData.begin(); it != mBeamData.end(); ++it) {
        if (newOrder.count() != (*it).count()) {
            qCWarning(LIBKSYSGUARD_KSIGNALPLOTTER) << "Serious problem in move sample.  beamdata[i] has " << (*it).count() << " and neworder has "
                                                   << newOrder.count();
        } else {
            QList<qreal> newBeam;
            for (int i = 0; i < newOrder.count(); i++) {
                int newIndex = newOrder[i];
                newBeam.append((*it).at(newIndex));
            }
            (*it) = newBeam;
        }
    }
    QList<QColor> newBeamColors;
    QList<QColor> newBeamColorsDark;
    for (int i = 0; i < newOrder.count(); i++) {
        int newIndex = newOrder[i];
        newBeamColors.append(mBeamColors.at(newIndex));
        newBeamColorsDark.append(mBeamColorsLight.at(newIndex));
    }
    mBeamColors = newBeamColors;
    mBeamColorsLight = newBeamColorsDark;
}

void KSignalPlotterPrivate::updateDataBuffers()
{
    /*  This is called when the widget has resized
     *
     *  Determine new number of samples first.
     *  +0.5 to ensure rounding up
     *  +4 for extra data points so there is
     *     1) no wasted space and
     *     2) no loss of precision when drawing the first data point.
     */
    if (q->isVisible())
        mMaxSamples = uint(q->size().width() / mHorizontalScale + 4);
    else // If it's not visible, we can't rely on sensible values for width.  Store some minimum number of data points
        mMaxSamples = qMin((uint)(q->size().width() / mHorizontalScale + 4), NUM_SAMPLES_WHEN_INVISIBLE);
}

#ifdef GRAPHICS_SIGNAL_PLOTTER
void KGraphicsSignalPlotter::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(widget);
#else
void KSignalPlotter::paintEvent(QPaintEvent *event)
{
    if (testAttribute(Qt::WA_PendingResizeEvent))
        return; // lets not do this more than necessary, shall we?
    QPainter p(this);
    QPainter *painter = &p;
#endif

    uint w = size().width();
    uint h = size().height();
    /* Do not do repaints when the widget is not yet setup properly. */
    if (w <= 2 || h <= 2)
        return;

#ifdef GRAPHICS_SIGNAL_PLOTTER
    bool onlyDrawPlotter = option && d->mPlottingArea.contains(option->exposedRect.toRect());
#else
    bool onlyDrawPlotter = event && d->mPlottingArea.contains(event->rect());
#endif

#ifdef USE_SEPERATE_WIDGET
    if (onlyDrawPlotter)
        return; // The painting will be handled entirely by GraphWidget::paintEvent
#endif

    if (!onlyDrawPlotter && d->mShowThinFrame)
        d->drawThinFrame(painter, d->mPlottingArea.adjusted(0, 0, 1, 1)); // We have a 'frame' in the bottom and right - so subtract them from the view

#ifndef USE_SEPERATE_WIDGET
    d->drawWidget(painter, d->mPlottingArea); // Draw the widget only if we don't have a GraphWidget to draw it for us
#endif

    if (d->mShowAxis) {
#ifdef GRAPHICS_SIGNAL_PLOTTER
        int fontheight = QFontMetrics(font()).height();
#else
        int fontheight = fontMetrics().height();
#endif
        if (d->mPlottingArea.height() > fontheight) { // if there's room to draw the labels, then draw them!
            d->drawAxisText(painter, QRect(0, 0, w, h));
        }
    }
}
#ifdef USE_SEPERATE_WIDGET
void GraphWidget::paintEvent(QPaintEvent *)
{
    if (testAttribute(Qt::WA_PendingResizeEvent))
        return; // lets not do this more than necessary, shall we?

    uint w = width();
    uint h = height();
    /* Do not do repaints when the widget is not yet setup properly. */
    if (w <= 2 || h <= 2)
        return;
    QPainter p(this);

    signalPlotterPrivate->drawWidget(&p, QRect(0, 0, w, h));

    if (signalPlotterPrivate->mAxisTextOverlapsPlotter && signalPlotterPrivate->mShowAxis) {
        uint fontheight = signalPlotterPrivate->q->fontMetrics().height();
        if (h > fontheight) { // if there's room to draw the labels, then draw them!
            QSize originalSize = signalPlotterPrivate->q->size();
            signalPlotterPrivate->drawAxisText(&p, QRect(-signalPlotterPrivate->mPlottingArea.topLeft(), originalSize));
        }
    }
}
#endif
void KSignalPlotterPrivate::drawWidget(QPainter *p, const QRect &boundingBox)
{
#ifdef SVG_SUPPORT
    if (!mSvgFilename.isEmpty()) {
        if (mBackgroundImage.isNull() || mBackgroundImage.height() != boundingBox.height()
            || mBackgroundImage.width() != boundingBox.width()) { // recreate on resize etc
            updateSvgBackground(boundingBox);
        }
        p->drawPixmap(boundingBox, mBackgroundImage);
    }
#endif

    if (mScrollableImage.isNull())
        redrawScrollableImage();

    // We draw the pixmap in two halves, wrapping around the window
    if (mScrollOffset > 1) {
#ifdef USE_QIMAGE
        p->drawImage(boundingBox.right() - mScrollOffset + 2, boundingBox.top(), mScrollableImage, 0, 0, mScrollOffset - 1, boundingBox.height());
#else
        p->drawPixmap(boundingBox.right() - mScrollOffset + 2, boundingBox.top(), mScrollableImage, 0, 0, mScrollOffset - 1, boundingBox.height());
#endif
    }
    int widthOfSecondHalf = boundingBox.width() - mScrollOffset + 1;
    if (widthOfSecondHalf > 0) {
#ifdef USE_QIMAGE
        p->drawImage(boundingBox.left(),
                     boundingBox.top(),
                     mScrollableImage,
                     mScrollableImage.width() - widthOfSecondHalf - 1,
                     0,
                     widthOfSecondHalf,
                     boundingBox.height());
#else
        p->drawPixmap(boundingBox.left(),
                      boundingBox.top(),
                      mScrollableImage,
                      mScrollableImage.width() - widthOfSecondHalf - 1,
                      0,
                      widthOfSecondHalf,
                      boundingBox.height());
#endif
    }

    /* Draw scope-like grid vertical lines if it doesn't move.  If it does move, draw it in the dynamic part of the code*/
    if (mShowVerticalLines && !mVerticalLinesScroll)
        drawVerticalLines(p, boundingBox);
}
void KSignalPlotterPrivate::drawBackground(QPainter *p, const QRect &boundingBox) const
{
    p->setRenderHint(QPainter::Antialiasing, false);
#ifdef SVG_SUPPORT
    if (!mSvgFilename.isEmpty()) // our background is an svg, so don't paint over the top of it
        p->fillRect(boundingBox, Qt::transparent);
    else
#endif
        p->fillRect(boundingBox, q->palette().brush(QPalette::Base));

    if (mShowHorizontalLines)
        drawHorizontalLines(p, boundingBox.adjusted(0, 0, 1, 0));

    if (mShowVerticalLines && mVerticalLinesScroll)
        drawVerticalLines(p, boundingBox, mVerticalLinesOffset);

    p->setRenderHint(QPainter::Antialiasing, true);
}

bool KSignalPlotter::thinFrame() const
{
    return d->mShowThinFrame;
}
void KSignalPlotter::setThinFrame(bool thinFrame)
{
    if (thinFrame == d->mShowThinFrame)
        return;

    d->mShowThinFrame = thinFrame;
    update(); // Trigger a repaint
}

#ifdef SVG_SUPPORT
void KSignalPlotterPrivate::updateSvgBackground(const QRect &boundingBox)
{
    Q_ASSERT(!mSvgFilename.isEmpty());
    Q_ASSERT(boundingBox.isNull());
    mBackgroundImage = QPixmap(boundingBox.width(), boundingBox.height());
    Q_ASSERT(!mBackgroundImage.isNull());
    QPainter pCache(&mBackgroundImage);
    pCache.fill(q->palette().color(QPalette::Base));

    svgRenderer->resize(boundingBox.size());
    svgRenderer->paint(&pCache, 0, 0);
}
#endif
void KSignalPlotterPrivate::redrawScrollableImage()
{
    // Align width of bounding box to the size of the horizontal scale
    int alignedWidth = ((mPlottingArea.width() + 1) / mHorizontalScale + 1) * mHorizontalScale;
    // Redraw the whole thing
#ifdef USE_QIMAGE
    mScrollableImage = QImage(alignedWidth, mPlottingArea.height(), QImage::Format_ARGB32_Premultiplied);
#else
    mScrollableImage = QPixmap(alignedWidth, mPlottingArea.height());
#endif
    Q_ASSERT(!mScrollableImage.isNull());

    mScrollOffset = 0;
    mVerticalLinesOffset = mVerticalLinesDistance - mHorizontalScale + 1; // mVerticalLinesDistance - alignedWidth % mVerticalLinesDistance;
    // We need to draw the background for areas without a beam
    int withoutBeamWidth = qMax(mBeamData.size() - 1, 0) * mHorizontalScale;
    QPainter pCache(&mScrollableImage);
    if (withoutBeamWidth < mScrollableImage.width())
        drawBackground(&pCache, QRect(withoutBeamWidth, 0, alignedWidth - withoutBeamWidth, mScrollableImage.height()));

    /* Draw scope-like grid vertical lines */
    mVerticalLinesOffset = 0;
    if (mBeamData.size() > 2) {
        for (int i = mBeamData.size() - 2; i >= 0; i--)
            drawBeamToScrollableImage(&pCache, i);
    }
}

void KSignalPlotterPrivate::drawThinFrame(QPainter *p, const QRect &boundingBox)
{
    /* Draw a line along the bottom and the right side of the
     * widget to create a 3D like look. */
    p->setRenderHint(QPainter::Antialiasing, false);
    p->setPen(QPen(q->palette().color(QPalette::Light), 0));
    p->drawLine(boundingBox.bottomLeft(), boundingBox.bottomRight());
    p->drawLine(boundingBox.bottomRight(), boundingBox.topRight());
    p->setRenderHint(QPainter::Antialiasing, true);
}

void KSignalPlotterPrivate::calculateNiceRange()
{
    qreal max = mUserMaxValue;
    qreal min = mUserMinValue;
    if (mUseAutoRange) {
        if (!std::isnan(mMaxValue) && mMaxValue * 0.99 > max) // Allow max value to go very slightly over the given max, for rounding reasons
            max = mMaxValue;
        if (!std::isnan(mMinValue) && mMinValue * 0.99 < min) {
            min = mMinValue;
        }
    }

    /* If the range is too small we will force it to 1.0 since it
     * looks a lot nicer. */
    if (max - min < 0.000001)
        max = min + 1;

    qreal range = max - min;

    // Massage the range so that the grid shows some nice values.
    qreal step;
    int number_lines_above_zero = 0;
    int number_lines_below_zero = 0;
    // If y=0 is visible and have at least 1 horizontal lines make sure that we have a line crossing through 0
    bool alignToXAxis = min < 0 && max > 0 && mHorizontalLinesCount >= 1;
    if (alignToXAxis) {
        number_lines_above_zero = int(mHorizontalLinesCount * max / range);
        number_lines_below_zero = mHorizontalLinesCount - number_lines_above_zero - 1; // subtract 1 line for the actual 0 line
        step = qMax(max / (mScaleDownBy * (number_lines_above_zero + 1)), -min / (mScaleDownBy * (number_lines_below_zero + 1)));
    } else
        step = range / (mScaleDownBy * (mHorizontalLinesCount + 1));

    const int sigFigs = 2; // Number of significant figures of the step to use.  Update the 0.05 below if this changes
    int logdim = (int)floor(log10(step)) - (sigFigs - 1); // find the order of the number, reduced by 1.  E.g. if step=1234 then logdim is 3-1=2
    qreal dim = pow((qreal)10.0, logdim); // e.g.  if step=1234, logdim=2, so dim = 100
    int a = (int)ceil(step / dim - 0.000005); // so a = ceil(1234/100) = ceil(12.34) = 13    (we subtract an epsilon)

    if (logdim >= 0)
        mPrecision = 0; // Work out the number of decimal places.  e.g. If step=1.5, then logdim = -1 so precision is 1
    else if (a % 10 == 0) // We just happened to round to a nice number, requiring 1 less precision
        mPrecision = -logdim - 1;
    else
        mPrecision = -logdim;
    step = dim * a; // e.g. if step=1234, dim=100, a=13  so step= 1300.  So 1234 was rounded up to 1300

    range = mScaleDownBy * step * (mHorizontalLinesCount + 1);

    if (alignToXAxis) {
        max = mScaleDownBy * step * (number_lines_above_zero + 1);
        min = -mScaleDownBy * step * (number_lines_below_zero + 1);
    } else
        max = min + range;

    if (mNiceMinValue == min && mNiceRange == range)
        return; // nothing changed

    mNiceMaxValue = max;
    mNiceMinValue = min;
    mNiceRange = range;

#ifdef USE_QIMAGE
    mScrollableImage = QImage();
#else
    mScrollableImage = QPixmap();
#endif
    Q_EMIT q->axisScaleChanged();
    q->update();
}

void KSignalPlotterPrivate::drawVerticalLines(QPainter *p, const QRect &boundingBox, int offset) const
{
    QColor color = q->palette().color(QPalette::Window);
    if (!mVerticalLinesScroll)
        color.setAlpha(127);
    p->setPen(QPen(color, 0));

    p->setRenderHint(QPainter::Antialiasing, false);
    for (int x = boundingBox.right() - (offset % mVerticalLinesDistance); x >= boundingBox.left(); x -= mVerticalLinesDistance)
        p->drawLine(x, boundingBox.top(), x, boundingBox.bottom());
    p->setRenderHint(QPainter::Antialiasing, true);
}

void KSignalPlotterPrivate::drawBeamToScrollableImage(QPainter *p, int index)
{
    QRect cacheBoundingBox = QRect(mScrollOffset, 0, mHorizontalScale, mScrollableImage.height());

    drawBackground(p, cacheBoundingBox);
    drawBeam(p, cacheBoundingBox, mHorizontalScale, index);

    mScrollOffset += mHorizontalScale;
    mVerticalLinesOffset = (mVerticalLinesOffset + mHorizontalScale) % mVerticalLinesDistance;
    if (mScrollOffset >= mScrollableImage.width() - 1) {
        mScrollOffset = 0;
        mVerticalLinesOffset--; // We skip over the last pixel drawn
    }
}

void KSignalPlotterPrivate::drawBeam(QPainter *p, const QRect &boundingBox, int horizontalScale, int index)
{
    if (mNiceRange == 0)
        return;
    QPen pen;
    if (mHorizontalScale == 1) // Don't use a pen width of 2 if there's only 1 pixel between points
        pen.setWidth(1);
    else
        pen.setWidth(2);

    pen.setCapStyle(Qt::FlatCap);

    qreal scaleFac = (boundingBox.height() - 2) / mNiceRange;
    if (mBeamData.size() - 1 <= index)
        return; // Something went wrong?

    const QList<qreal> &datapoints = mBeamData[index];
    const QList<qreal> &prev_datapoints = mBeamData[index + 1];
    bool hasPrevPrevDatapoints = (index + 2 < mBeamData.size()); // used for bezier curve gradient calculation
    const QList<qreal> &prev_prev_datapoints = hasPrevPrevDatapoints ? mBeamData[index + 2] : prev_datapoints;

    qreal x0 = boundingBox.right();
    qreal x1 = qMax(boundingBox.right() - horizontalScale, 0);

    qreal xaxis = boundingBox.bottom();
    if (mNiceMinValue < 0)
        xaxis = qMax(qreal(xaxis + mNiceMinValue * scaleFac), qreal(boundingBox.top()));

    const int count = qMin(datapoints.size(), mBeamColors.size());
    QVector<QPainterPath> paths(count);
    QPointF previous_c0;
    QPointF previous_c1;
    QPointF previous_c2;
    QPointF previous_c3;
    qreal previous_point0 = 0;
    qreal previous_point1 = 0;
    qreal previous_point2 = 0;
    bool firstLine = true;
    for (int j = 0; j < count; ++j) {
        qreal point0 = datapoints[j];
        if (std::isnan(point0))
            continue; // Just do not draw points with nans. skip them

        qreal point1 = prev_datapoints[j];
        qreal point2 = prev_prev_datapoints[j];

        if (std::isnan(point1))
            point1 = point0;
        else if (mSmoothGraph && !std::isinf(point1)) {
            // Apply a weighted average just to smooth the graph out a bit
            // Do not try to smooth infinities or nans
            point0 = (2 * point0 + point1) / 3;
            if (!std::isnan(point2) && !std::isinf(point2))
                point1 = (2 * point1 + point2) / 3;
            // We don't bother to average out y2.  This will introduce slight inaccuracies in the gradients, but they aren't really noticeable.
        }
        if (std::isnan(point2))
            point2 = point1;

        if (mStackBeams) {
            point0 = previous_point0 = previous_point0 + point0;
            point1 = previous_point1 = previous_point1 + point1;
            point2 = previous_point2 = previous_point2 + point2;
        }

        qreal y0 = qBound((qreal)boundingBox.top(), boundingBox.bottom() - (point0 - mNiceMinValue) * scaleFac, (qreal)boundingBox.bottom());
        qreal y1 = qBound((qreal)boundingBox.top(), boundingBox.bottom() - (point1 - mNiceMinValue) * scaleFac, (qreal)boundingBox.bottom());
        qreal y2 = qBound((qreal)boundingBox.top(), boundingBox.bottom() - (point2 - mNiceMinValue) * scaleFac, (qreal)boundingBox.bottom());

        QPainterPath &path = paths[j];
        path.moveTo(x1, y1);
        QPointF c1(x1 + horizontalScale / 3.0, (4 * y1 - y2) / 3.0); // Control point 1 - same gradient as prev_prev_datapoint to prev_datapoint
        QPointF c2(x1 + 2 * horizontalScale / 3.0, (2 * y0 + y1) / 3.0); // Control point 2 - same gradient as prev_datapoint to datapoint
        QPointF c3(x0, y0);
        path.cubicTo(c1, c2, c3);
        if (mFillOpacity) {
            QPainterPath fillPath = path; // Make a copy to do our fill
            if (!mStackBeams || firstLine) {
                fillPath.lineTo(x0, xaxis);
                fillPath.lineTo(x1, xaxis);
                fillPath.lineTo(x1, y1);
            } else {
                fillPath.lineTo(x0, previous_c3.y());
                fillPath.cubicTo(previous_c2, previous_c1, previous_c0);
                fillPath.lineTo(x1, y1);
            }
            if (mStackBeams) {
                previous_c0 = QPointF(x1, y1);
                previous_c1 = c1;
                previous_c2 = c2;
                previous_c3 = c3;
            }
            QColor fillColor = mBeamColors[j];
            fillColor.setAlpha(mFillOpacity);
            p->fillPath(fillPath, fillColor);
        }
        firstLine = false;
    }
    for (int j = count - 1; j >= 0; --j) {
        if (mFillOpacity)
            pen.setColor(mBeamColorsLight.at(j));
        else
            pen.setColor(mBeamColors.at(j));
        p->strokePath(paths.at(j), pen);
    }
}
void KSignalPlotterPrivate::drawAxisText(QPainter *p, const QRect &boundingBox)
{
    if (mHorizontalLinesCount < 0)
        return;
    p->setFont(q->font());
    qreal stepsize = mNiceRange / (mScaleDownBy * (mHorizontalLinesCount + 1));
    if (mActualAxisTextWidth == 0) // If we are drawing completely inside the plotter area, using the Text color
        p->setPen(QPen(q->palette().brush(QPalette::Text), 0));
    else
        p->setPen(QPen(q->palette().brush(QPalette::WindowText), 0));
    int axisTitleIndex = 1;
    QString val;
    int numItems = mHorizontalLinesCount + 2;
    int fontHeight = p->fontMetrics().height();
    if (numItems == 2 && boundingBox.height() < fontHeight * 2)
        numItems = 1;
    mAxisTextOverlapsPlotter = false;
    for (int y = 0; y < numItems; y++, axisTitleIndex++) {
        int y_coord =
            boundingBox.top() + (y * (boundingBox.height() - fontHeight)) / (mHorizontalLinesCount + 1); // Make sure it's y*h first to avoid rounding bugs
        qreal value;
        if (y == mHorizontalLinesCount + 1)
            value = mNiceMinValue / mScaleDownBy; // sometimes using the formulas gives us a value very slightly off
        else
            value = mNiceMaxValue / mScaleDownBy - y * stepsize;

        val = scaledValueAsString(value, mPrecision);
        QRect textBoundingRect = p->fontMetrics().boundingRect(val);

        if (textBoundingRect.width() > mActualAxisTextWidth)
            mAxisTextOverlapsPlotter = true;
        int offset = qMax(mActualAxisTextWidth - textBoundingRect.right(), -textBoundingRect.left());
        if (q->layoutDirection() == Qt::RightToLeft)
            p->drawText(boundingBox.left(), y_coord, boundingBox.width() - offset, fontHeight + 1, Qt::AlignLeft | Qt::AlignTop, val);
        else
            p->drawText(boundingBox.left() + offset, y_coord, boundingBox.width() - offset, fontHeight + 1, Qt::AlignLeft | Qt::AlignTop, val);
    }
}

void KSignalPlotterPrivate::drawHorizontalLines(QPainter *p, const QRect &boundingBox) const
{
    if (mHorizontalLinesCount <= 0)
        return;
    p->setPen(QPen(q->palette().color(QPalette::Window)));
    for (int y = 0; y <= mHorizontalLinesCount + 1; y++) {
        // note that the y_coord starts from 0.  so we draw from pixel number 0 to h-1.  Thus the -1 in the y_coord
        int y_coord = boundingBox.top() + (y * (boundingBox.height() - 1)) / (mHorizontalLinesCount + 1); // Make sure it's y*h first to avoid rounding bugs
        p->drawLine(boundingBox.left(), y_coord, boundingBox.right() - 1, y_coord);
    }
}

int KSignalPlotter::currentAxisPrecision() const
{
    return d->mPrecision;
}

qreal KSignalPlotter::lastValue(int i) const
{
    if (d->mBeamData.isEmpty() || d->mBeamData.first().size() <= i)
        return std::numeric_limits<qreal>::quiet_NaN();
    return d->mBeamData.first().at(i);
}
QString KSignalPlotter::lastValueAsString(int i, int precision) const
{
    if (d->mBeamData.isEmpty() || d->mBeamData.first().size() <= i || std::isnan(d->mBeamData.first().at(i)))
        return QString();
    return valueAsString(d->mBeamData.first().at(i), precision); // retrieve the newest value for this beam
}
QString KSignalPlotter::valueAsString(qreal value, int precision) const
{
    if (std::isnan(value))
        return QString();
    value = value / d->mScaleDownBy; // scale the value.  E.g. from Bytes to KiB
    return d->scaledValueAsString(value, precision);
}
QString KSignalPlotterPrivate::scaledValueAsString(qreal value, int precision) const
{
    qreal absvalue = qAbs(value);
    if (precision == -1) {
        if (absvalue >= 99.5)
            precision = 0;
        else if (absvalue >= 0.995 || (mScaleDownBy == 1 && mNiceMaxValue > 20))
            precision = 1;
        else
            precision = 2;
    }

    if (absvalue < 1E6) {
        if (precision == 0)
            return mUnit.subs((long)value).toString();
        else
            return mUnit.subs(value, 0, 'f', precision).toString();
    } else
        return mUnit.subs(value, 0, 'g', precision).toString();
}

bool KSignalPlotter::smoothGraph() const
{
    return d->mSmoothGraph;
}

void KSignalPlotter::setSmoothGraph(bool smooth)
{
    d->mSmoothGraph = smooth;
#ifdef USE_QIMAGE
    d->mScrollableImage = QImage();
#else
    d->mScrollableImage = QPixmap();
#endif
}

bool KSignalPlotter::stackGraph() const
{
    return d->mStackBeams;
}
void KSignalPlotter::setStackGraph(bool stack)
{
    d->mStackBeams = stack;
#ifdef USE_QIMAGE
    d->mScrollableImage = QImage();
#else
    d->mScrollableImage = QPixmap();
#endif
}

int KSignalPlotter::fillOpacity() const
{
    return d->mFillOpacity;
}
void KSignalPlotter::setFillOpacity(int fill)
{
    d->mFillOpacity = fill;
#ifdef USE_QIMAGE
    d->mScrollableImage = QImage();
#else
    d->mScrollableImage = QPixmap();
#endif
}

#ifdef GRAPHICS_SIGNAL_PLOTTER
QPainterPath KGraphicsSignalPlotter::opaqueArea() const
{
    return shape(); // The whole thing is opaque
}

QSizeF KGraphicsSignalPlotter::sizeHint(Qt::SizeHint which, const QSizeF &constraint) const
{
    Q_UNUSED(constraint);
    if (which == Qt::PreferredSize)
        return QSizeF(200, 200);
    return QGraphicsWidget::sizeHint(which, constraint);
}
#else
QSize KSignalPlotter::sizeHint() const
{
    return QSize(200, 200); // Just a random size which would usually look okay
}
#endif

#ifdef USE_SEPERATE_WIDGET
GraphWidget::GraphWidget(QWidget *parent)
    : QWidget(parent)
{
}
#endif

#undef KSignalPlotter
#undef KSignalPlotterPrivate
