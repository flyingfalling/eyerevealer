#!/bin/bash

DIR=$1
OUTDIR=$2
VID=$3 #blur.mkv etc.
CENT=$4
DVAW=$5  #164 for tobii2...190 for tobii3
DVAH=$6  #104 for tobii2...126 for tobii3

mkdir -p $OUTDIR

LIST=`find $DIR -maxdepth 1 -type d -name "raw-*"`

for f in $LIST;
do
    echo $f
    newdir=`basename $f`
    MYOUTDIR="$OUTDIR""/""$newdir"
    echo "Output to: "$MYOUTDIR
    
    mkdir -p $MYOUTDIR

    DEVDIR=`find $f -maxdepth 1 -type d -name "device-Tobii3*"`
    for df in $DEVDIR;
    do
	csvfile=$df"/tpfp.db"
	#REV: df is the location of the true stuff (i.e. f)
	bash make_single_vids.sh $csvfile $df $VID $MYOUTDIR $CENT $DVAW $DVAH 1>"$MYOUTDIR""/MAKEVIDS"$VID".OUT" 2>&1 &
    done
done
