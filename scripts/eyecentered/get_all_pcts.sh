#!/bin/bash

DIR=$1
OUTDIR=$2
VID=$3

mkdir -p $OUTDIR

LIST=`find $DIR -maxdepth 1 -type d -name "raw-*"`

for f in $LIST;
do
    echo $f
    newdir=`basename $f`
    INFNAME=$f"/"$VID"_sal.db"
    OUTFNAME=$OUTDIR"/""$newdir""_""$VID"
    OUTSTD="$OUTFNAME"".OUT"
    echo $OUTSTD
    #python3 get_pcts.py $INFNAME $OUTFNAME 1> "$OUTSTD" 2>&1 &
    #python3 get_pcts.py $INFNAME $OUTFNAME 1>"$OUTSTD" 2>&1 &
    bash run_single_pct.sh $INFNAME $OUTFNAME &
    #1>$OUTSTD 2>&1 &
done
