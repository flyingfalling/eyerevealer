#!/bin/bash

wget http://veale.science/public/rteye2.tar.gz .
tar xfz rteye2.tar.gz
cd rteye2
mkdir build
cd build

INSTALLDIR="$PWD"

echo "INSTALLING IN $INSTALLDIR"

#REV: requires root
sh $INSTALLDIR/../scripts/install_ftdi.sh

cmake -DCMAKE_BUILD_TYPE=RELEASE ..
make -j4

echo "export DRI_RENDER_NODE="`sh $INSTALLDIR/../scripts/detect_dri_render_node.sh` | sudo tee /etc/profile.d/dri_render_node.sh

SHORTCUT="$HOME/Desktop/RTEYE2.SH"

echo "Creating desktop shortcut $SHORTCUT"

#write over (>)
echo "INSTALLDIR=$INSTALLDIR" > $SHORTCUT
cat $INSTALLDIR/../scripts/runrteye2viewer.sh >> $SHORTCUT

chmod +x $SHORTCUT
