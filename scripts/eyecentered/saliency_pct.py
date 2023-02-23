#REV: this will calculate saliency percent from some set of of sal maps.
#REV: will take (1) eye positions (and times), (2) saliency maps, (3) raw computed inputs (for visualization only).
#REV: need to compute both (a) ROC and (b) percentiles? Are they different?

#REV: for centered, we need to sample at the offset value of position (negatively). I.e. where was the CURRENT eye position
#     100 msec ago (and sample saliency there).
#REV: for uncentered, we need to simply sample the specified location

# Can average timempoints around there, or use the smoothfin. Negative will be the (offset) baseline prior (at that time point).
import sys
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import math
import cv2

fname = sys.argv[1];


tcol = sys.argv[2];
tu = float(sys.argv[3]);

xcol = sys.argv[4];
xu = float(sys.argv[5]);
ycol = sys.argv[6];
yu = float(sys.argv[7]);

x0 = float(sys.argv[8]);
y0 = float(sys.argv[9]);

if( len(sys.argv) > 10 ):
    sep = sys.argv[10];
else:
    sep = ' ';
    pass;

print("Using sep: [{}] (Xcol: [{}], Ycol: [{}])".format(sep, xcol, ycol));
df2 = pd.read_csv( fname, sep=sep );


df = df2.rename(columns={tcol:"t", xcol:"x", ycol:"y"}); #inplace=True                                                     

print(df);

df.x -= x0;
df.y -= y0;

df.x *= xu;
df.y *= yu; #REV: will include the bottom-top flip!        
df.t *= tu;

#REV: need something to access "target" in sal video...

#REV: null model is all eye positions... (relative to current position, not as possible saccades...right?)
#REV: better: conditional probability from current position...oh well
