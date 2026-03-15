# KSysGuard (KF6 port)

This is a community fork of the **deprecated KSysGuard app** (no longer shipped
with KDE 6). The goal of this repository is to **port KSysGuard to KDE
Frameworks 6 (KF6)** so it can still be built and used on modern KDE 6 setups.

## Community fixes and improvements

This fork also applies fixes and improvements from people who like KSysGuard
and want to continue using it.

KSysGuard monitors various elements of your system (or remote systems via the
KSysGuard daemon `ksysguardd`). The daemon is available on Linux, FreeBSD,
Irix, NetBSD, OpenBSD, Solaris, and Tru64 with varying degrees of completion.

## Packages

- **Arch Linux**: available on AUR: `https://aur.archlinux.org/packages/ksysguard6-git`

## Building (Debian/Ubuntu)

1) Install build dependencies:

```bash
sudo apt install build-essential debhelper-compat
sudo apt-get build-dep .
```

2) Build the package:

```bash
dpkg-buildpackage -b -us -uc
```

## KDE authors note

KSysGuard is originally developed by the KDE community. All original authors
and contributors retain their copyrights; this fork adapts the codebase for KF6
compatibility and includes community fixes and improvements.
