#REV: just read and get PCT (and make histos because why not?)

#REV: redo raw-  23-15 (oops locked db

#REV: If I have the raw data, just get each condition, and summarize it...

import sys
import pandas as pd
import sqlite3
import numpy as np

dbfname = sys.argv[1];
tablename = sys.argv[2];
outfname = sys.argv[3];


dbconn = sqlite3.connect( dbfname, timeout=60000 );

#REV: read it all into memory or should I do something else like...only query where tpfpdelta == t Yea...

dflist = [];
chunksizerows=100000;
iterated=0;
#for df in pd.read_sql('SELECT * FROM {} WHERE tpfpdelta="t"'.format(tablename), dbconn, chunksize=chunksizerows):
for df in pd.read_sql('SELECT * FROM {} WHERE timeidx>5'.format(tablename), dbconn, chunksize=chunksizerows):
#for df in pd.read_sql('SELECT * FROM {} WHERE timeidx>5 AND timeidx<8'.format(tablename), dbconn, chunksize=chunksizerows):
    iterated+=1;
    print("Reading {} rows ({} so far)".format(chunksizerows, chunksizerows*iterated));
    dflist.append(df);
    pass;

df = pd.concat( dflist ).reset_index(drop=True);


#REV: this will only work if PCT is already computed.

#REV: calculate true(er) pctile, not "worst case" percentile.
#REV: i.e. I should take percentile where X of ALL are less than me. So, 50 pctl means value where EXACTLY half are less than me.
def calc_pctl(posval, negvals):
    #pctle = ((negvals < posval).sum()) / float(len(negvals)+1);
    nlt = (negvals < posval).sum(); # / float(len(negvals)+1);
    neq = (negvals == posval).sum();
    pctle = (nlt + neq/2) / len(negvals); #REV: should I include myself? If zero neg vals we fucked anyways lol
    return pctle;

#REV: need to normalize "within" frames before sampling? fuck? Or just use raw pctiles lol
#REV: hope it is auto-normalized...
def calc_auroc(posvals, negvals, thresholds):
    tps=[1];  #REV: not really matter since it is >=, but just in case? I.e. if first threshold is higher than lowest value...
    fps=[1];
    posvals=np.array(posvals);
    negvals=np.array(negvals);
    
    for myt in thresholds:
        tpr = (posvals>=myt).sum() / len(posvals);
        fpr = (negvals>=myt).sum() / len(negvals);
        tps.append(tpr);
        fps.append(fpr);
        pass;

    tps.append(0);
    fps.append(0);
    fps = fps[::-1]; #REV: reverse
    tps = tps[::-1];

    #REV: take mean of every pair of guys...and use that? I.e. will be one less that way. For TP.
    mtp = np.sum(np.lib.stride_tricks.sliding_window_view(np.array(tps), window_shape = 2), axis = 1)/2;
    dfp = np.diff(fps);
    mult = np.array(mtp) * np.array(dfp);
    auroc = np.sum(mult);
    return auroc;
    #REV: now "area under" i.e. fpr is X, tpr is Y. FPR diff is "weight", and TPR is "value"? Need to CUMULATIVE though!? But,
    #REV: backwards...so wtf? Yea, I can't do diff. I need to use raw value.
    

mylist = [];
togrp = ['blurdva', 'blur', 'saltype', 'timeidx'];
threshs=list(range(0,256));
for val, vdf in df.groupby(togrp):
    myt = vdf[ vdf.tpfpdelta=='t' ].iloc[0]['sal'];
    fps = vdf[ vdf.tpfpdelta=='f' ];
    pctle = calc_pctl(myt, fps['sal']);
    roc = calc_auroc([myt], fps['sal'], threshs);
    #print(pctle);
    val = list(val);
    mylist.append( val + [pctle, roc] );
    pass;

resdf = pd.DataFrame(columns=togrp+['pct', 'roc'], data=mylist);

alllist=[];
togrp2=['blurdva', 'blur', 'saltype'];
for val, vdf in df.groupby(togrp2):
    val = list(val);
    allroc = calc_auroc( vdf[ vdf.tpfpdelta == 't' ][['sal']], vdf[ vdf.tpfpdelta == 'f' ][['sal']], threshs );
    alllist.append( val + [allroc] );
    pass;

alldf = pd.DataFrame(columns=togrp2+['allroc'], data=alllist);
print(alldf);
alldf.to_csv(outfname+'_all.csv', index=False);

result = resdf[['blurdva', 'blur', 'saltype', 'pct', 'roc']].groupby(['blurdva', 'blur', 'saltype']).mean();
print(result);
print("Writing CSV to {}".format(outfname));
#result.to_csv(outfname);
resdf.to_csv(outfname+'.csv', index=False);

print("Finished");
