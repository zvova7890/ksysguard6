/*
    KSysGuard, the KDE System Guard

    SPDX-FileCopyrightText: 2007 Trent Waddington <trent.waddington@gmail.com>
    SPDX-FileCopyrightText: 2008 John Tapsell <tapsell@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later


*/

#include "KTextEditVT.h"

#include <QFontDatabase>

KTextEditVT::KTextEditVT(QWidget *parent)
    : QTextEdit(parent)
    , escape_code(QChar(0))
{
    mParseAnsi = true;
    escape_sequence = false;
    escape_CSI = false;
    escape_OSC = false;
    escape_number1 = -1;
    escape_number_separator = false;
    escape_number2 = -1;
    setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
}

void KTextEditVT::insertVTChar(const QChar &c)
{
    if (escape_sequence) {
        if (escape_CSI || escape_OSC) {
            if (c.isDigit()) {
                if (!escape_number_separator) {
                    if (escape_number1 == -1)
                        escape_number1 = c.digitValue();
                    else
                        escape_number1 = escape_number1 * 10 + c.digitValue();
                } else {
                    if (escape_number2 == -1)
                        escape_number2 = c.digitValue();
                    else
                        escape_number2 = escape_number2 * 10 + c.digitValue();
                }
            } else if (c == QLatin1Char(';')) {
                escape_number_separator = true;
            } else if (escape_OSC && c == QChar(7)) { // Throw away any letters that are not OSC
                escape_code = c;
            } else if (escape_CSI)
                escape_code = c;
        } else if (c == QLatin1Char('[')) {
            escape_CSI = true;
        } else if (c == QLatin1Char(']')) {
            escape_OSC = true;
        } else if (c == QLatin1Char('(') || c == QLatin1Char(')')) {
        } else
            escape_code = c;
        if (!escape_code.isNull()) {
            // We've read in the whole escape sequence.  Now parse it
            if (escape_code == QLatin1Char('m')) { // change color
                switch (escape_number2) {
                case 0: // all off
                    setFontWeight(QFont::Normal);
                    setTextColor(Qt::black);
                    break;
                case 1: // bold
                    setFontWeight(QFont::Bold);
                    break;
                case 31: // red
                    setTextColor(Qt::red);
                    break;
                case 32: // green
                    setTextColor(Qt::green);
                    break;
                case 33: // yellow
                    setTextColor(Qt::yellow);
                    break;
                case 34: // blue
                    setTextColor(Qt::blue);
                    break;
                case 35: // magenta
                    setTextColor(Qt::magenta);
                    break;
                case 36: // cyan
                    setTextColor(Qt::cyan);
                    break;
                case -1:
                case 30: // black
                case 39: // reset
                case 37: // white
                    setTextColor(Qt::black);
                    break;
                }
            }
            escape_code = QChar(0);
            escape_number1 = -1;
            escape_number2 = -1;
            escape_CSI = false;
            escape_OSC = false;
            escape_sequence = false;
            escape_number_separator = false;
        }
    } else if (c == QChar(0x0d)) {
        insertPlainText(QStringLiteral("\n"));
    } else if (c.isPrint() || c == QLatin1Char('\n')) {
        insertPlainText(c);
    } else if (mParseAnsi) {
        if (c == QChar(127) || c == QChar(8)) { // delete or backspace, respectively
            textCursor().deletePreviousChar();
        } else if (c == QChar(27)) { // escape key
            escape_sequence = true;
        } else if (c == QChar(0x9b)) { // CSI - equivalent to esc [
            escape_sequence = true;
            escape_CSI = true;
        } else if (c == QChar(0x9d)) { // OSC - equivalent to esc ]
            escape_sequence = true;
            escape_OSC = true;
        }

    } else if (!c.isNull()) {
        insertPlainText(QStringLiteral("["));
        QString num;
        num = c;
        insertPlainText(num);
        insertPlainText(QStringLiteral("]"));
    }
}

void KTextEditVT::insertVTText(const QByteArray &string)
{
    const int size = string.size();
    for (int i = 0; i < size; i++)
        insertVTChar(QLatin1Char(string.at(i)));
}

void KTextEditVT::insertVTText(const QString &string)
{
    int size = string.size();
    for (int i = 0; i < size; i++)
        insertVTChar(string.at(i));
}

void KTextEditVT::setParseAnsiEscapeCodes(bool parseAnsi)
{
    mParseAnsi = parseAnsi;
}

bool KTextEditVT::parseAnsiEscapeCodes() const
{
    return mParseAnsi;
}
