#! /usr/bin/env bash
$EXTRACTRC *.ui >> rc.cpp
$XGETTEXT `find . -name \*.cpp` -o $podir/processui.pot
rm -f rc.cpp
