#! /bin/bash

set -e
set -x

PKGNAME=$(grep "^pkgname" APKBUILD | awk -F= {' print $2 '})
PKGVER=$(grep "^pkgver" APKBUILD | awk -F= {' print $2 '})
TAREX="tar.bz2"
TMPDIR=$(mktemp -d)

# remove the manual build directory, if any
rm -rf build/

echo "$PKGNAME $PKGVER $TAREX $TMPDIR"
rm -f "$PKGNAME-$PKGVER.$TAREX"

cp -r . "$TMPDIR/$PKGNAME-$PKGVER"
cd $TMPDIR
rm -rf "$TMPDIR/$PKGNAME-$PKGVER/.git"
tar -cjf $PKGNAME-$PKGVER.$TAREX $PKGNAME-$PKGVER
cd - > /dev/null

mv "$TMPDIR/$PKGNAME-$PKGVER.$TAREX" .
rm -rf "$TMPDIR"

