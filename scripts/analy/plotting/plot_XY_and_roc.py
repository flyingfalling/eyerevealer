

import sys
import cv2
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd



dir=sys.argv[1];


offsetfn = dir+"/offset"
with  open(offsetfn) as f:
    OFFSETSEC = float(f.readline());


#REV: note, timestamps will be in own time (not global tobii time)
#REV: add offset to rsgaze times to get tobii times. Same for sal.
tobiigaze = dir+"/tobii2_color.mkv.gaze";
rsgaze = dir+"/rs_color.mkv.gaze";


tobiiroc = dir+"/TOBII_fin.mkv.roc";
rsdeproc = dir+"/RSDEPTH_fin.mkv.roc";
rsnodeproc = dir+"/RSNODEPTH_fin.mkv.roc";


tobiigazedf = pd.read_csv(tobiigaze, sep=' ');
rsgazedf = pd.read_csv(rsgaze, sep=' ');

tts = tobiigazedf['TSEC'];
txs = tobiigazedf['X'];
tys = tobiigazedf['Y'];

OFFSETSEC=0.52
rts = rsgazedf['TSEC']+OFFSETSEC;
rxs = rsgazedf['X'];
rys = rsgazedf['Y'];




tobiirocdf = pd.read_csv(tobiiroc, sep=' ');
tobiirocts = tobiirocdf['TSEC'];
tobiirocvals = tobiirocdf['SALPCT'];


rsdeprocdf = pd.read_csv(rsdeproc, sep=' ');
rsdeprocts = rsdeprocdf['TSEC']+OFFSETSEC;
rsdeprocvals = rsdeprocdf['SALPCT'];

rsnodeprocdf = pd.read_csv(rsnodeproc, sep=' ');
rsnodeprocts = rsnodeprocdf['TSEC']+OFFSETSEC;
rsnodeprocvals = rsnodeprocdf['SALPCT'];



#https://matplotlib.org/devdocs/gallery/subplots_axes_and_figures/subfigures.html
fig = plt.figure(constrained_layout=True, figsize=(8, 5))
#fig.suptitle("X/Y and Saliency Pctile");

subfigs = fig.subplots(2, 1, sharex=True );
axt = subfigs[0];


axt.set_title("Gaze X/Y over Time (RS vs Tobii)");
axt.plot( tts, txs, linestyle='--', color='black' );
axt.plot( tts, tys, linestyle='-', color='black');

axt.plot( rts, rxs, linestyle='--', color='red' );
axt.plot( rts, rys, linestyle='-', color='red' );

axt.legend(['Tobii X', 'Tobii Y', 'RS X', 'RS Y']);

axt.set_ylabel("Norm. gaze X/Y");
axt.set_xlim([ 500, 510])
axt.set_ylim([ 0, 1 ] );


axb = subfigs[1];
axb.set_title("Saliency Pctile RS(+/-depth) vs Tobii");

axb.plot( tobiirocts, tobiirocvals, color='black');
axb.plot( rsdeprocts, rsdeprocvals, color='red');
axb.plot( rsnodeprocts, rsnodeprocvals, color='purple' );

axb.legend(['Tobii', 'RS+depth', 'RS-depth']);

axb.set_ylabel("Gaze Saliency Percentile");

axb.set_xlabel("Time (sec)");
axb.set_xlim([ 630, 640])
axt.set_ylim([ 0, 1 ] );

plt.show();
