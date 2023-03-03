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



#REV: read it all into memory or should I do something else like...only query where tpfpdelta == t Yea...

#REV: this takes too long... I need to 1) create an index on timeidx 2) read chunks...

import sqlite3;
tablename='data';
indbconn = sqlite3.connect( dbfname, timeout=60000000 );

#REV: try sorting it first???
dbtime = time.time();
cur = indbconn.cursor()
res = cur.execute("SELECT count(*) FROM  sqlite_master WHERE  type= 'index' and tbl_name = 'data' and name = 'timeidx_id'").fetchone();
indexexists=res[0];
if( indexexists < 1 ):
    indbconn.execute('CREATE INDEX timeidx_id ON data(timeidx)');
    indbconn.commit();
    pass;
print("Sorted DB: {:4.1f} msec".format((time.time()-dbtime)*1e3));

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

#REV: this *exactly* equals pctl for one posval...lol. 
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
    

#REV: do this with DB.
    
mylist = [];
togrp = ['blurdva', 'blur', 'saltype'];
threshs=list(range(0,256));

timeidxs = cur.execute("SELECT MIN(timeidx), MAX(timeidx) FROM data").fetchone();
print("Starting timeidx {}".format(timeidx));
print(timeidxs);
mintimeidx = timeidxs[0];
maxtimeidx = timeidxs[1];
timeidx = mintimeidx;

while( timeidx <= maxtimeidx ):
    df = pd.read_sql( 'SELECT * FROM {} WHERE timeidx=={}'.format('data', timeidx));
    
    if( len(df.index) < 1 ):
        print("Finished reading all time idxs! {}".format(timeidx));
        break;
    
    for val, vdf in df.groupby(togrp):
        myt = vdf[ vdf.tpfpdelta=='t' ].iloc[0]['sal'];
        fps = vdf[ vdf.tpfpdelta=='f' ];
        pctle = calc_pctl(myt, fps['sal']);
        auroc = calc_auroc([myt], fps['sal'], threshs);
        val = list(val);
        mylist.append( val + [pctle, auroc] );
        pass;
    
    timeidx = cur.execute("SELECT MIN(timeidx) FROM data WHERE timeidx > {}".format(timeidx)).fetchone()[0];
    
    pass;

resdf = pd.DataFrame(columns=togrp+['pctle', 'auroc'], data=mylist);

#REV: not printing this out, this just summary.
result = resdf[['blurdva', 'blur', 'saltype', 'pctle', 'auroc']].groupby(['blurdva', 'blur', 'saltype']).mean();
print(result);
print("Writing CSV to {}".format(outfname));
#result.to_csv(outfname);
resdf.to_csv(outfname+'.csv', index=False);

print("Finished");
