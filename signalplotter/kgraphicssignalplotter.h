/*
    SPDX-FileCopyrightText: 2006-2009 John Tapsell <tapsell@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#ifndef KGRAPHICSSIGNALPLOTTER_H
#define KGRAPHICSSIGNALPLOTTER_H

#include <QColor>
#include <QFont>
#include <QGraphicsWidget>
#include <QList>
#include <QString>
#include <klocalizedstring.h>

class QGraphicsSceneResizeEvent;
class KGraphicsSignalPlotterPrivate;

// Make sure only declare KLocalizedString once
#ifndef KSIGNALPLOTTER_H
Q_DECLARE_METATYPE(KLocalizedString)
#endif

/** \class KGraphicsSignalPlotter
 * \brief The KGraphicsSignalPlotter graphics widget draws a real time graph of data that updates continually.
 *
 *  Features include:
 *  \li Points are joined by a bezier curve.
 *  \li Lines are anti-aliased
 *  \li Background can be set as a specified SVG
 *  \li The lines can be reordered
 *  \li Uses as little memory and CPU as possible
 *  \li Graph can be smoothed using the formula (value * 2 + last_value)/3
 *  \li Can cope with positive/negative infinity and NaN values.
 *
 *  Example usage:
 *  \code
 *    KGraphicsSignalPlotter *s = KGraphicsSignalPlotter(parent);
 *    s->addBeam(Qt::blue);
 *    s->addBeam(Qt::green);
 *    QList<qreal> data;
 *    data << 4.0 << 5.0;
 *    s->addSample(data);
 *  \endcode
 *
 *  Note that the number of horizontal lines is calculated automatically based on the axis font size, even if the axis labels are not shown.
 *
 *  Smoothing looks very nice visually and is enabled by default.  It can be disabled with setSmoothGraph().
 *
 *  \image KSignalPlotter.png  Example KGraphicsSignalPlotter with two beams
 */
class Q_DECL_EXPORT KGraphicsSignalPlotter : public QGraphicsWidget
{
    Q_OBJECT
    Q_PROPERTY(qreal minimumValue READ minimumValue WRITE setMinimumValue)
    Q_PROPERTY(qreal maximumValue READ maximumValue WRITE setMaximumValue)
    Q_PROPERTY(bool useAutoRange READ useAutoRange WRITE setUseAutoRange)
    Q_PROPERTY(KLocalizedString unit READ unit WRITE setUnit)
    Q_PROPERTY(qreal scaleDownBy READ scaleDownBy WRITE setScaleDownBy)
    Q_PROPERTY(uint horizontalScale READ horizontalScale WRITE setHorizontalScale)
    Q_PROPERTY(bool showHorizontalLines READ showHorizontalLines WRITE setShowHorizontalLines)
    Q_PROPERTY(bool showVerticalLines READ showVerticalLines WRITE setShowVerticalLines)
    Q_PROPERTY(bool verticalLinesScroll READ verticalLinesScroll WRITE setVerticalLinesScroll)
    Q_PROPERTY(uint verticalLinesDistance READ verticalLinesDistance WRITE setVerticalLinesDistance)
    Q_PROPERTY(bool showAxis READ showAxis WRITE setShowAxis)
    Q_PROPERTY(QString svgBackground READ svgBackground WRITE setSvgBackground)
    Q_PROPERTY(bool thinFrame READ thinFrame WRITE setThinFrame)
    Q_PROPERTY(int maxAxisTextWidth READ maxAxisTextWidth WRITE setMaxAxisTextWidth)
    Q_PROPERTY(bool smoothGraph READ smoothGraph WRITE setSmoothGraph)
    Q_PROPERTY(bool stackGraph READ stackGraph WRITE setStackGraph)
    Q_PROPERTY(int fillOpacity READ fillOpacity WRITE setFillOpacity)

public:
    KGraphicsSignalPlotter(QGraphicsItem *parent = nullptr);
    ~KGraphicsSignalPlotter() override;

    /** \brief Add a new line to the graph plotter, with the specified color.
     *
     *  Note that the order you add the beams in must be the same order that
     *  the beam data is given in (Unless you reorder the beams).
     *
     *  \param color Color of beam - does not have to be unique.
     */
    void addBeam(const QColor &color);

    /** \brief Add data to the graph, and advance the graph by one time period.
     *
     *  The data must be given as a list in the same order that the beams were
     *  added (or consequently reordered).  If samples.count() != numBeams(),
     *  a warning is printed and the data discarded.
     *
     *  To indicate that no data is available for a given beam, set its value to
     *  (non-signalling) NaN.
     *
     *  For example:
     *
     *  \code
     *    KSignalPlotter *s = KSignalPlotter(parent);
     *    s->addBeam(Qt::red);
     *    s->addBeam(Qt::green);
     *    s->addBeam(Qt::blue);
     *    signalPlotter->addSample( QList<qreal>() << std::numeric_limits<qreal>::quiet_NaN() << 1.0/0 << 10.0 );
     *  \endcode
     *
     *  This indicates that no data is available yet for red (so the beam will not be drawn for this section),
     *  that's it positive infinity for green, and 10 for blue.
     *
     *  Infinity is handled by drawing a straight line up to the top or bottom of the display, and does not affect the range.
     *  For the above example, the displayed range would now be 0 to 10.
     */
    void addSample(const QList<qreal> &samples);

    /** \brief Reorder the beams into the order given.
     *
     * For example:
     * \code
     *   KSignalPlotter *s = KSignalPlotter(parent);
     *   s->addBeam(Qt::blue);
     *   s->addBeam(Qt::green);
     *   s->addBeam(Qt::red);
     *   QList<int> neworder;
     *   neworder << 2 << 0 << 1;
     *   s->reorderBeams( newOrder);
     *   //Now the order is red, blue then green
     * \endcode
     *
     * The size of the \p newOrder list must be equal to the result of numBeams().
     * \param newOrder New order of beams.
     */
    void reorderBeams(const QList<int> &newOrder);

    /** \brief Removes the beam at the specified index.
     *
     * This causes the graph to be redrawn with the specified beam completely
     * removed.
     */
    void removeBeam(int index);

    /** \brief Get the color of the beam at the specified index.
     *
     * For example:
     * \code
     *   KSignalPlotter *s = KSignalPlotter(parent);
     *   s->addBeam(Qt::blue);
     *   s->addBeam(Qt::green);
     *   s->addBeam(Qt::red);
     *
     *   QColor color = s->beamColor(0);  //returns blue
     * \endcode
     *
     * \sa setBeamColor()
     */
    QColor beamColor(int index) const;

    /** \brief Set the color of the beam at the specified index.
     *
     * \sa beamColor()
     */
    void setBeamColor(int index, const QColor &color);

    /** \brief Returns the number of beams. */
    int numBeams() const;

    /** \brief Set the axis units with a localized string.
     *
     * The localized string must contain a placeholder "%1" which is substituted for the value.
     * The plural form (ki18np) can be used if the unit string changes depending on the number (for example
     * "1 second", "2 seconds").
     *
     * For example:
     *
     * \code
     *   KSignalPlotter plotter;
     *   plotter.setUnit( ki18ncp("Units", "%1 second", "%1 seconds") );
     *   QString formattedString = plotter.valueAsString(3.4); //returns "3.4 seconds"
     * \endcode
     *
     * Typically a new unit would be set when setScaleDownBy is called.
     * Note that even the singular should use "%1 second" instead of "1 second", so that a value of -1 works correctly.
     *
     * \see unit(), setScaleDownBy()
     */
    void setUnit(const KLocalizedString &unit);

    /** \brief The localizable units used on the vertical axis of the graph.
     *
     * The returns the localizable string set with setUnit().
     *
     * Default is the string "%1" - i.e. to just display the number.
     *
     * \see setUnit
     */
    KLocalizedString unit() const;

    /** \brief Scale all the values down by the given amount.
     *
     * This is useful when the data is given in, say, kilobytes, but you set
     * the units as megabytes.  Thus you would have to call this with @p value
     * set to 1024.  This affects all the data already entered.
     *
     * Typically this is followed by calling setUnit() to set the display axis
     * units.  Default value is 1.
     */
    void setScaleDownBy(qreal value);

    /** \brief Amount scaled down by.
     *
     * \sa setScaleDownBy */
    qreal scaleDownBy() const;

    /** \brief Set whether to scale the graph automatically beyond the given range.
     *
     * If true, the range on vertical axis is automatically expanded from the
     * data available, expanding beyond the range set by changeRange() if data
     * values are outside of this range.
     *
     * Regardless whether this is set of not, the range of the vertical axis
     * will never be less than the range given by maximumValue() and minimumvalue().
     *
     * \param value Whether to scale beyond the given range. Default is true.
     *
     * \sa useAutoRange
     */
    void setUseAutoRange(bool value);

    /** \brief Whether the vertical axis range is set automatically.
     */
    bool useAutoRange() const;

    /** \brief Change the minimum and maximum values drawn on the graph.
     *
     *  Note that these values are sanitised.  For example, if you
     *  set the minimum as 3, and the maximum as 97, then the graph
     *  would be drawn between 0 and 100.  The algorithm to determine
     *  this "nice range" attempts to minimize the number of non-zero
     *  digits.
     *
     *  If autoRange() is true, then this range is taking as a 'hint'.
     *  The range will never be smaller than the given range, but can grow
     *  if there are values larger than the given range.
     *
     *  This is equivalent to calling
     *  \code
     *    setMinimumValue(min);
     *    setMaximumValue(max);
     *  \endcode
     *
     *  \sa setMinimumValue(), setMaximumValue(), minimumValue(), maximumValue()
     */
    void changeRange(qreal min, qreal max);

    /** \brief Set the min value hint for the vertical axis.
     *
     * \sa changeRange(), minimumValue(), setMaximumValue(), maximumValue() */
    void setMinimumValue(qreal min);

    /** \brief Get the min value hint for the vertical axis.
     *
     * \sa changeRange(), minimumValue(), setMaximumValue(), maximumValue() */
    qreal minimumValue() const;

    /** \brief Set the max value hint for the vertical axis. *
     *
     * \sa changeRange(), minimumValue(), setMaximumValue(), maximumValue() */
    void setMaximumValue(qreal max);

    /** \brief Get the maximum value hint for the vertical axis.
     *
     * \sa changeRange(), minimumValue(), setMaximumValue(), maximumValue() */
    qreal maximumValue() const;

    /** \brief Get the current maximum value on the y-axis.
     *
     *  This will never be lower than maximumValue(), and if autoRange() is true,
     *  it will be equal or larger (due to rounding up to make it a nice number)
     *  than the highest value being shown.
     */
    qreal currentMaximumRangeValue() const;
    /** \brief Get the current minimum value on the y-axis.
     *
     *  This will never be lower than minimumValue(), and if autoRange() is true,
     *  it will be equal or larger (due to rounding up to make it a nice number)
     *  than the highest value being shown.
     */
    qreal currentMinimumRangeValue() const;

    /** \brief Set the number of pixels horizontally between data points.
     *  Default is 6. */
    void setHorizontalScale(uint scale);
    /** \brief The number of pixels horizontally between data points.
     *  Default is 6. */
    int horizontalScale() const;

    /** \brief Set whether to draw the vertical grid lines.
     *  Default is false. */
    void setShowVerticalLines(bool value);
    /** \brief Whether to draw the vertical grid lines.
     *  Default is false. */
    bool showVerticalLines() const;

    /** \brief Set the horizontal distance, in pixels, between the vertical grid lines.
     *  Must be a distance of 1 or more.
     *  Default is 30 pixels. */
    void setVerticalLinesDistance(uint distance);
    /** \brief The horizontal distance, in pixels, between the vertical grid lines.
     *  Default is 30 pixels. */
    uint verticalLinesDistance() const;

    /** \brief Set whether the vertical lines move with the data.
     *  Default is true. This has no effect is showVerticalLines is false. */
    void setVerticalLinesScroll(bool value);
    /** \brief Whether the vertical lines move with the data.
     *  Default is true. This has no effect is showVerticalLines is false. */
    bool verticalLinesScroll() const;

    /** \brief Set whether to draw the horizontal grid lines.
     *  Default is true. */
    void setShowHorizontalLines(bool value);
    /** \brief Whether to draw the horizontal grid lines.
     *  Default is true. */
    bool showHorizontalLines() const;

    /** \brief The number of decimal places used for the axis labels
     *
     *  This is calculated based on the current range */
    int currentAxisPrecision() const;

    /** \brief Set whether to show the vertical axis labels.
     *
     * Default is true.
     * \sa showAxis(), setAxisFont(), setAxisFontColor(), setMaxAxisTextWidth() */
    void setShowAxis(bool show);
    /** \brief Whether to show the vertical axis labels.
     *
     * Default is true.
     * \sa setShowAxis(), axisFont(), axisFontColor(), maxAxisTextWidth() */
    bool showAxis() const;

    /** \brief Set the filename of the SVG background.
     *
     * Set to empty (default) to disable again. */
    void setSvgBackground(const QString &filename);

    /** \brief The filename of the SVG background. */
    QString svgBackground() const;

    /** \brief Return the last value that we have for the given beam index.
     *
     * \return last value, or 0 if not known. */
    qreal lastValue(int index) const;

    /** \brief Return a translated string for the last value at the given index.
     *
     * Returns, for example,  "34 %" or "100 KB" for the given beam index,
     * using the last value set for the beam, using the given precision.
     *
     * If precision is -1 (the default) then if @p value is greater than 99.5, no decimal figures are shown,
     * otherwise if @p value is greater than 0.995, 1 decimal figure is used, otherwise 2.
     */
    QString lastValueAsString(int index, int precision = -1) const;

    /** \brief Return a translated string for the given value.
     *
     * Returns, for example, "34 %" or "100 KB" for the given value in unscaled units.
     *
     * If precision is -1 (the default) then if @p value is greater than 99.5, no decimal figures are shown,
     * otherwise if @p value is greater than 0.995, 1 decimal figure is used, otherwise 2.
     *
     * For example:
     * \code
     *   KSignalPlotter plotter;
     *   plotter.setUnit( ki18ncp("Units", "1 hour", "%1 hours") );
     *   plotter.scaleDownBy( 60 ); //The input will be in seconds, and there's 60 seconds in an hour
     *   QString formattedString = plotter.valueAsString(150); //returns "2.5 hours"
     * \endcode
     *
     */
    QString valueAsString(qreal value, int precision = -1) const;

    /** \brief Set the distance between the left of the widget and the left of the plotting region.
     *
     *  For example:
     *  \code
     *      int axisTextWidth = fontMetrics().width(i18nc("Largest axis title", "99999 XXXX"));
     *      plotter->setMaxAxisTextWidth(axisTextWidth);
     *  \endcode
     *
     *  If this is 0, the default, then the text will be shown inside the plotting area.
     */
    void setMaxAxisTextWidth(int maxAxisTextWidth);
    /** \brief Get the distance between the left of the widget and the left of the plotting region. */
    int maxAxisTextWidth() const;

    /** \brief Set whether to smooth the graph by averaging the points.
     *
     * This uses the formula:  (value*2 + last_value)/3.
     * Default is true. */
    void setSmoothGraph(bool smooth);
    /** \brief Whether to smooth the graph by averaging the points.
     *
     * This uses the formula:  (value*2 + last_value)/3.
     * Default is true. */
    bool smoothGraph() const;

    /** \brief Set whether to stack the beams on top of each other.
     *
     * Default is false */
    void setStackGraph(bool stack);
    /** \brief Whether to stack the beams on top of each other.
     *
     * Default is false */
    bool stackGraph() const;

    /** \brief Alpha value for filling the area underneath the graph lines.
     *
     * Set to 0 to disable filling the graph, and 255 for a solid fill. Default is 20*/
    void setFillOpacity(int fill);
    /** \brief Alpha value for filling the area underneath the graph lines. */
    int fillOpacity() const;

    /* Whether to show a thin line on the left and bottom of the widget, for a slight 3D effect */
    bool thinFrame() const;
    /* Set whether to show a thin line on the left and bottom of the widget, for a slight 3D effect */
    void setThinFrame(bool thinFrame);

Q_SIGNALS:
    /** When the axis has changed this signal is emitted. */
    void axisScaleChanged();

protected:
    /* Reimplemented */
    void resizeEvent(QGraphicsSceneResizeEvent *event) override;
    void paint(QPainter *p, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) override;
    QSizeF sizeHint(Qt::SizeHint which, const QSizeF &constraint = QSizeF()) const override;
    void changeEvent(QEvent *event) override;
    QPainterPath opaqueArea() const override;

private:
    KGraphicsSignalPlotterPrivate *const d;
    friend class KGraphicsSignalPlotterPrivate;
};

#endif
