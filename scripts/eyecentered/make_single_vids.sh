INCSV=$1 #csv including full path
INDIR=$2 #dir containing the blur etc. sal videos...
INVID=$3 #e.g. blur.mkv or etc. no directory path
SAVEDIR=$4 #full dir path to save to
CENT=$5 #centered or uncentered
DVAWID=$6  #164 for tobii2 82*2
DVAHEI=$7

mkdir -p $SAVEDIR

echo "MAKE_SINGLE_VID: " $SAVEDIR

SALDB="sal.db"
OUTVID="$INVID""_circles"

python3 makevids.py $INCSV [,] "$INDIR"/"$INVID" "$SAVEDIR"/"$OUTVID" $DVAWID $DVAHEI $CENT "$SAVEDIR"/"$SALDB" f "$INDIR"/"$INVID"_fin.mkv c "$INDIR"/"$INVID"_col.mkv l "$INDIR"/"$INVID"_lum.mkv m "$INDIR"/"$INVID"_mot.mkv o "$INDIR"/"$INVID"_ori.mkv s "$INDIR"/"$INVID"_smoothfin.mkv
