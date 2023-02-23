#REV: todo:
# Use smoothing using that soloky-nizy filter
# Also, do Larsson et al 2013 Detection of Saccades and Postsaccadic Oscillations in the Presence of Smooth Pursuit
# Also, check that other thing taht does eye tracking and quality etc. Also, use 3D values...to compute velocity that way.
# REV; use better velocity smoothing etc...

#REV: saccade detection from 2D data.

#REV: 3 arguments:
#1) File name of data (TSEC X Y)
#2) Column names of TIME TIMEU X XU Y YU
#     Where TIMEU, XU, YU are column units of TIME X Y in seconds and dva (indicates direction of X and Y as well, default of positive values is positive X to right, positive Y is up.
#4) X0 Y0 Coordinates of (X=0, Y=0) with respect to the coordinate of straight ahead being (0,0) (in their units)

#Tobii 2: 82 deg. horizontal / 52 deg. vertical
#Tobii 3: H: 95°, V: 63°

import sys
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import math

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

#xdir = math.copysign(1, xu);
#ydir = math.copysign(1, yu);

#REV: check edf mapper, to make sure I appropriately use Y up as positive (since that is assumed for angle calculation!)

#REV: example: xu = 1, yu = -1 (upside down), x0 = -0.5, y0 = -0.5 #REV: fuck requires knowledge of dva per pixel in X and Y directions...

#REV: I now know their (centered) coordinate, but in their units (i.e. -.5 -.5 is top-left if their xu=1 and yu=-1)

#REV: if reversed, need to subtract from "top"?
#REV: so, converting to my coordinates:
#   xp = x0 + xu*(x0 + x)     -1->0    -0.5->
#   yp = y0 + (y*yu);     #  -0.5->0  0->-0.5 0.5->-1
#   yp = y0 + yu*(y0 + y);    -1->1.5  -0.5->1.0  0->0.5   0.5->0    1.0->-0.5  (REV: this is not "centered")

#REV: what is MY 0,0 (i.e. center) in their units? It will be 0.5, 0.5...

# They have some units, containing some numbers, and some center.

# They tell me center in THEIR units, and THEIR units, and I will convert.
# step 1: center theirs to 0 at middle (will this be affected by up/down?) If they tell me their center is 0.5, 0.5, can I safely
# just subt it (their units, remember).
# Their 0,0 will become -0.5, -0.5.
# 0, 0 just represents 'straight ahead'. I still don't know their orientation, but I know that 0,0 is the origin. Now, I want them to
# Convert their stuff so that X goes right, Y goes up
# y*=yu
# x*=xu


df.x -= x0; #REV: now, left of image is -0.5, right is 0.5, middle is 0
df.y -= y0; #REV: ok...

#df.y = -df.y; #REV: flipping Y positive and negative
#REV: however, we know it is TOP-DOWN

#REV: for now, don't worry?
df.x *= xu;
df.y *= yu; #REV: will include the bottom-top flip!



df.t *= tu;

#REV: now, compute saccades...

df["Dt"] = np.diff( df.t, append=df.iloc[len(df.index)-1].t );
#REV: smooth it first? exponential smoothing in both x and y separately... (is that OK?)


df["dx"] = np.diff( df.x, append=df.iloc[len(df.index)-1].x );
df["dy"] = np.diff( df.y, append=df.iloc[len(df.index)-1].y );


df["vx"] = df.dx / df.Dt;
df["vy"] = df.dy / df.Dt;

#REV: in dva / sec
#df["v"] = np.sqrt( df.vx*df.vx + df.vy*df.vy );
df["v"] = np.sqrt( df["vx"] * df["vx"] + df["vy"] * df["vy"] ); #df.vx*df.vx + df.vy*df.vy );


def lpf( prev, curr, dt, tausec ):
    c = math.exp( -dt / tausec ); #REV: to mult by prev value...
    return (c*prev + (1-c)*curr);


#x = df.iloc[0].x;
#y = df.iloc[0].y;
#val=df.v;
tcsec = 0.010;
xfilt=[ df.iloc[0].x ];
yfilt=[ df.iloc[0].y ];

for i in range(1,len(df.index)):
    xfilt.append( lpf( xfilt[i-1], df.iloc[i].x, df.iloc[i].Dt, tcsec ) );
    yfilt.append( lpf( yfilt[i-1], df.iloc[i].y, df.iloc[i].Dt, tcsec ) );
    pass;

df["xfilt"] = xfilt;
df["yfilt"] = yfilt;

df["dxfilt"] = np.diff(df.xfilt, append=df.iloc[len(df.index)-1].xfilt );
df["dyfilt"] = np.diff(df.yfilt, append=df.iloc[len(df.index)-1].yfilt );
df["vxfilt"] = df.dxfilt/df.Dt;
df["vyfilt"] = df.dyfilt/df.Dt;

outdf = df.rename( columns={"t":"Tsec"} );
outdf["gaze2d_0"] = (df.xfilt / xu) + x0;
outdf["gaze2d_1"] = (df.yfilt / yu) + y0;

outdf.to_csv(fname+"_resampled.csv");
#print("REV: done");

#REV: wtf recursion error?
df["vfilt"] = np.sqrt( df["vxfilt"] * df["vxfilt"] + df["vyfilt"] * df["vyfilt"] );

print(df);


#REV: need to figure out how to detect saccades in direction (i.e. min/max curvature).
# start vel/end vel, and in between? Obviously, direction will be start-end. But, need to ensure that average vector of any time step never deviates
# more than a certain delta angle from its previous one...(small angle degree/second). Let's say maximum divergence of 90deg from the whole direction
# i.e. start to finish? Or should we say max from? But, there is the "hook" at the end, that we need to figure out.

# Assume proper units detected, i.e. angles and time. Filter position better, or time?

#REV: segment based on "inter time" I.e. combine saccades if they are not separated by like 50 msec? Note, "drift" etc.
#REV: I can calculate VOR...from gyroscope? Note it may enter a smooth pursuit afterwards...or OKR etc. So I really should separate those out faster...

#REV: v cutoff is e.g. 20 deg/sec? Or, use accelerations?

#REV: particle filter? Eye tends to stay at same location, or accelerate with some velocity, in same direction, etc.
#estimate hidden from noisy observable...

#REV: first, just do it the "dumb" way lol

#REV: note, I should handle what happens if I jump out of blinks or something? I.e. there should be NAN in between. So, should resample first...

#REV: use polynomial filter, local taps, etc. etc., lots of work on this shit. People doing matlab, VOR detection, error, RMS error.
#REV: undistort the camera? Fuck.

saccs=[]
stt=None;
stx=None;
sty=None;
ent=None;
enx=None;
eny=None;

class saccade():
    def __init__(self, startidx, endidx, startsec, endsec, startx, endx, starty, endy):
        self.startidx=startidx;
        self.endidx=endidx;
        self.startsec=startsec;
        self.endsec=endsec;
        self.startx=startx;
        self.endx=endx;
        self.starty=starty;
        self.endy=endy;
        self.magnitude=self.calc_mag();
        return;

    def calc_mag(self):
        return math.sqrt( (self.endx-self.startx)**2 + (self.endy-self.starty)**2 );

    pass;



cutoff=50.0;
dcutoff=50.0; #REV: fuck can't account for counter-rotation ;(
insacc=False;
TOUSE="v";
for idx in range(0,len(df.index)):
    if( df.iloc[idx][TOUSE] >= cutoff and False == insacc):
        insacc=True;
        stt=idx-1;
        #stx=df.x[idx];
        #sty=df.y[idx];
        pass;
    elif( df.iloc[idx][TOUSE] < dcutoff and True == insacc ):
        insacc=False;
        ent=idx; #REV: end is one after it goes below
        #enx=df.x[idx];
        #eny=df.y[idx];
        saccs.append( [stt, ent] );
        pass;
    pass;


#REV: filter out (a) too high accelerations (b) too high change in direction.
#REV: i.e. apply a particle filter?

#print(saccs);

sdf = pd.DataFrame( columns=["STIDX", "ENIDX", "STSEC", "ENSEC", "STX", "ENX", "STY", "ENY" ] );

for sacc in saccs:
    sti = sacc[0];
    eni = sacc[1];
    strow = df.iloc[sti];
    enrow = df.iloc[eni];
    #print(strow);
    newrow = [ int(sti), int(eni), strow.t, enrow.t, strow.xfilt, enrow.xfilt, strow.yfilt, enrow.yfilt ];
    sdf.loc[ len(sdf.index) ] = newrow;
    pass;

sdf["magnitude"] = np.sqrt( (sdf.ENX-sdf.STX)**2 + (sdf.ENY-sdf.STY)**2 );
sdf["duration"] = sdf.ENSEC-sdf.STSEC;


print(sdf);
elt = df.t.max() - df.t.min();
print("Fname: {}".format(fname));
print("Time: {} (Nsac: {}   sacc/sec: {})".format( elt, len(sdf.index), len(sdf.index)/elt) );

#REV: unscale, uncenter it...

#outdf = df.rename( columns={"xfilt":"gaze2d_0", "yfilt":"gaze2d_1"} );
sdf.to_csv(fname+"_resampled_sacc.csv");



plt.hist( sdf.magnitude, bins=100 );
plt.show();

plotdf=pd.DataFrame( columns=["tsec", "vel", "sidx", "magn"] );
sidx=0;

#REV: make a quick sketch of saccades by 1 deg things..
for mag, grpdf in sdf.groupby(pd.cut(sdf["magnitude"], np.arange(5.5, 20.5, 1.0))):
    print("Mag: {}".format(mag));
    #print(grpdf);
    print(grpdf.duration.median());
    for idx, row in grpdf.iterrows():
        #tmpdf = df.iloc[ int(row.STIDX) : int(row.ENIDX) ][["t", TOUSE]];
        tmpdf = df[ (df.t >= row.STSEC) & (df.t <= row.ENSEC) ][["t", TOUSE]];
        tmpdf.t -= tmpdf.t.min();
        tmpdf.sort_values(by="t", inplace=True);
        tmpdf["sidx"] = sidx;
        tmpdf["magn"] = mag;
        plotdf = pd.concat( [plotdf, tmpdf] );
        sidx += 1;
        pass;
    pass;

import seaborn as sns
sns.set_theme(style="ticks")

# Define the palette as a list to specify exact values
palette = sns.color_palette("rocket_r")

# Plot the lines on two facets
sns.relplot(
    data=plotdf,
    x="t", y=TOUSE,
    hue="sidx", row="magn",
    kind="line", palette=palette,
    height=3, aspect=3.0, facet_kws=dict(sharex=True),
    )

plt.xlim(0,0.5);
plt.show();




#REV: detect smooth purusits (note, need to counter VOR etc?)
#REV: just detect going above some drift, then falling down...in same direction?)

#REV: name it: gaze2d_0, gaze2d_1 and Tsec

