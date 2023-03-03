#!/bin/bash

DIR=$1
OUTDIR=$2
VID=$3 #blur.mkv etc.
CENT=$4
DVAW=$5  #164 for tobii2...18X for tobii3
DVAH=$6  #104 for tobii2...?? for tobii3

mkdir -p $OUTDIR

LIST=`find $DIR -maxdepth 1 -type d -name "raw-*"`

for f in $LIST;
do
    echo $f
    newdir=`basename $f`
    MYOUTDIR="$OUTDIR""/""$newdir"
    echo "Output to: "$MYOUTDIR
    csvfile=$f"/tpfp.csv"

    mkdir -p $MYOUTDIR
    
    #REV: MYOUTDIR is created mkdir -p in 
    bash make_single_vids.sh $csvfile $f $VID $MYOUTDIR $CENT $DVAW $DVAH 1>"$MYOUTDIR""/MAKEVIDS"$VID".OUT" 2>&1 &
    #bash make_single_vids.sh $csvfile $f cent.mkv 1>"$MYOUTDIR""/MAKEVIDSCENT.OUT" 2>&1 &
    #bash make_single_vids.sh $csvfile $f uncent.mkv 1>"$MYOUTDIR""/MAKEVIDSUNCENT.OUT" 2>&1 &
done
