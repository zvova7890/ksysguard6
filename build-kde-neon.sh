#!/bin/bash -x

# Original source: https://discuss.kde.org/t/app-system-activity-gone-in-kde6/14743/6

# Modified to build on KDE Neon.

# clone the code, note the kf6 branch, you can also download the tarball
# or zip file
# git clone --branch=kf6 https://github.com/zvova7890/ksysguard6.git

# the git clone command will create a directory named the same as the repo
# let's move into this directory
# cd ksysguard6

# on debian/ubuntu based systems this would be
# the same as installing the "build-essentials" package
# it provides basic compilation packages like gcc, git, etc.
sudo apt install build-essential

# cmake and KDE's extra cmake modules
sudo apt install \
    cmake \
    kf6-extra-cmake-modules

# required QT6 dependencies
sudo apt install \
    qt6-base-dev \
    qt6-tools-dev \
    qt6-webview-dev \
    qt6-doc-dev

# required KDE dependencies
sudo apt install \
    kf6-kwidgetsaddons-dev \
    libksysguard-dev \
    kf6-kconfig-dev \
    kf6-kcoreaddons-dev \
    kf6-kdbusaddons-dev \
    kf6-kdoctools-dev \
    kf6-ki18n-dev \
    kf6-kiconthemes-dev \
    kf6-kitemviews-dev \
    kf6-kio-dev \
    kf6-knewstuff-dev \
    kf6-kauth-dev \
    kf6-knotifications-dev \
    kf6-kwindowsystem-dev \
    kf6-kconfigwidgets-dev \
    kf6-kglobalaccel-dev \
    kf6-kxmlgui-dev

# optional dependencies
sudo apt install \
    libqt6webenginewidgets6 \
    qt6-webchannel-dev \
    libnl-3-dev \
    libnl-route-3-dev \
    zlib1g-dev \
    libpcap-dev \
    libcap-dev \
    libsensors4-dev

# configure
build_dir=.build
cmake -B $build_dir -DCMAKE_BUILD_TYPE=Release

# compile
j=$(($(nproc) / 2))
make -C $build_dir -j$j

# install
sudo make -C $build_dir install
