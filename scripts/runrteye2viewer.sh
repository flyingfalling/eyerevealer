#!/bin/bash

#INSTALLDIR=$1

#timestamp cout/cerrs
LOGDIR="$HOME/RTEYE2_LOGS/"
TIMESTAMP=`date +%Y-%m-%d_%H-%M-%S`
mkdir -p $LOGDIR
OFILE=$LOGDIR"/OUT-"$TIMESTAMP
EFILE=$LOGDIR"/ERR-"$TIMESTAMP

echo "Outfile to ""$OFILE"
echo "Errors to ""$EFILE"

export DRI_RENDER_NODE=`bash $INSTALLDIR/../scripts/detect_dri_render_node.sh`

$INSTALLDIR/rteye2viewer.exe 1>$OFILE 2>$EFILE
