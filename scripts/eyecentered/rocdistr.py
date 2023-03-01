import numpy as np
import pandas as pd
import sys
import matplotlib.pyplot as plt
from matplotlib.dates import DateFormatter, date2num
import datetime
import matplotlib

infile = sys.argv[1];

df = pd.read_csv(infile);

print(df.groupby(['blurdva', 'blur', 'saltype']).mean());
print(df.groupby(['blurdva', 'blur', 'saltype']).mean().max());
print(np.sum(df.pct - df.roc)); #REV: should be 0

ntypes=len(df.groupby(['blurdva', 'blur', 'saltype'])); #36, i.e. 3 * 2 * 6 i.e. 12*3 i.e. 36 ok.
print(ntypes);

fig, axs = plt.subplots(ntypes, 2, figsize=(5,40));
idx=0;
for grp, gdf in df.groupby(['blurdva', 'blur', 'saltype']):
    #print(grp);
    #print( gdf[['pct','roc']].mean() );
    h1 = axs[idx,0].hist( gdf.roc, bins=20 );
    h2 = axs[idx,1].hist( gdf.pct, bins=20 );
    axs[idx,0].set_title( 'ROC {} {:3.2f}'.format(grp, gdf.roc.mean()) );
    axs[idx,1].set_title( 'PCT {} {:3.2f}'.format(grp, gdf.pct.mean()) ); #REV: this isn't "all", which mixes up all together...
    #print(h1);
    #print(h2);
    idx+=1;
    pass;

fig.tight_layout();

fig.savefig('rochist.pdf');


#REV: do sliding window...
#1) convert to timedeltas...and sort.
print( df.timeidx.head() );
df.timeidx = df.timeidx.astype('timedelta64[s]'); #np.timedelta64( df.timeidx );

df.sort_values( by='timeidx', inplace=True );

ntypes = len(df.groupby(['blurdva', 'saltype'])); #36, i.e. 3 * 2 * 6 i.e. 12*3 i.e. 36 ok.

def timeTicks(x, pos):
    seconds = x / 1e9; #  seconds is seconds lol
    # create datetime object because its string representation is alright
    d = datetime.timedelta(seconds=seconds).total_seconds();
    return str(d);

formatter = matplotlib.ticker.FuncFormatter(timeTicks)

fig, axs = plt.subplots(ntypes, 2, figsize=(5,40));
idx=0;
for grp1, gdf1 in df.groupby(['blurdva', 'saltype']):
    #col=0;
    gdf = gdf1[gdf1.blur=='g'];
    #for grp, gdf in gdf1.groupby(['blur']):
        #REV: rolling window within each...
    rlist=[];
    WINSIZE=10;
    #REV: rolling just means it represents center of window...
    resdf = gdf.rolling(window='{}s'.format(WINSIZE), min_periods=WINSIZE*10, on='timeidx', center=True).mean();
    ax=axs[idx,0];
    #start=pd.Timestamp('20180606')
    #plt.plot(start+df.index, df)
    resdf.timeidx = pd.to_timedelta(resdf.timeidx, unit='s');
    
    
    xlab = [np.timedelta64(k, "ns") for k in resdf.timeidx];
    ax.plot( xlab, resdf.roc );
    
    ax.set_ylim([0,1]);
    ax.axhline(0.5, color='black', linewidth=1, linestyle='--');
    #start, end = ax.get_xlim();
    #ax.xaxis.set_ticks(np.arange(start, end, 30*1e9)); #REV: assume 15...seconsd?
    #ax.set_xticklabels(ax.get_xticks(), rotation = 90)
    
    ax.xaxis.set_major_locator(plt.MaxNLocator(10));
    ax.xaxis.set_tick_params(which='major', labelrotation=90)
    ax.xaxis.set_major_formatter(formatter);

    ax.set_title( '{}'.format(grp1));
    ax.grid(visible=True, which='major', axis='both');
    #ax.xaxis.set_major_formatter(myFmt);
    #print(resdf);
    
    #col+=1;
    ax=axs[idx,1];
    h1 = ax.hist( gdf.roc, bins=20 );
    ax.set_title( 'ROC {} {:3.2f}'.format(grp, gdf.roc.mean()) );

    idx+=1;
    pass;


fig.tight_layout();

fig.savefig('roctrajs.pdf');


'''
for slicedf in gdf.rolling(window='{}s'.format(WINSIZE), min_periods=WINSIZE*5, on='timeidx'):
#print(slicedf);
outrow=slicedf.tail(1);
mroc = slicedf.roc.mean();
outrow.roc = mroc;
rlist.append( outrow );
pass;
'''
