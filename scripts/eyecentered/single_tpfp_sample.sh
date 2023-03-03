INCSV=$1 #~/mazda_results_allcalced/raw-2022-02-23-15-28-45/tobii2_color.mkv.gaze_resampled.csv
OUTDIR=$2
OUTCSV=$OUTDIR"/tpfp.db"
echo "OUT CSV (DB) TO: $OUTCSV"
#REV: both has cent and uncent auto inside...

#REV: not correct since further away pixels are "larger" (more of them) for same angle (or, in other words, pixels are smaller)
#REV: thus, I have too "few" pixels in WIDTH from HEIGHT's point of view... note DVA per pixel (in middle) is more accurate for
#REV: Y lol. Closer to linear regime (at 26) only 5pct off. If I followed vert, I should have like 95 deg width for same
#REV: number of pixels? Tan(x) goes ABOVE y=x, quickly outpaces it. I.e. it is counting TOO MUCH DVA for further out values..
#REV: I.e. it is counting pixels "bigger" than they actually are. Cuz if i count 90 out, that's basically infinite pixels for tan(x),
#REV: but only 90 for x. Counts way more pixels with each step. Multiple pixels. I.e. pixels are small. Other way around, for
#REV: very high pixperdva on edges. In middle theyre same. Tan is never lower than X linear.

xu=1
yu=1

#REV: this is still in DVA!!!! 1 and 1. No need to fuck with it. Problem is in the pixel conversion which I do later ;)
#0.88702
# Inverse is: 1.127371273712737

python3 tpfp_sample.py $INCSV Tsec 1 xfilt $xu yfilt $yu 0 0 [,] $OUTCSV

#xu=82
#yu=52
#python3 tpfp_sample.py $INCSV Tsec 1 gaze2d_0 $xu gaze2d_1 $yu 0.5 0.5 [,] $OUTCSV
