#REV: just read and get PCT (and make histos because why not?)

#REV: redo raw-  23-15 (oops locked db

#REV: If I have the raw data, just get each condition, and summarize it...

import time
import sys
import pandas as pd
import sqlite3
import numpy as np

dbfname = sys.argv[1];
outfname = sys.argv[2];




#REV: read it all into memory or should I do something else like...only query where tpfpdelta == t Yea...
#REV: I do this with DISTINCT

#REV: this takes too long... I need to 1) create an index on timeidx 2) read chunks...
#REV: I do this, still too long... for iterating through timeidx even though sorted? FUck?

#REV: Best: create databases with foreign keys. Can JOIN each time without explicitly naming foreign key?
#REV: Ghtto foreign keys? I.e. just timeidxs, G type, S type, etc. Better to do all in SQL ugh.

#REV: all time data points have a foreign key! Basically I need to start from...something and build database up?
#REV: start from sample TPFP. At this point, we simply have centered or uncentered (again two things?) Why not just have its own indx?
#REV: faster to have foreign key for indexing?

import sqlite3;
tablename='data';
print("Will connect to DB {}".format(dbfname));
indbconn = sqlite3.connect( dbfname, timeout=60000000 );

print("Connected to db {}".format(dbfname));

#REV: try sorting it first???
dbtime = time.time();
cur = indbconn.cursor()
print("Got cursor...checking if IDX exists");
res = cur.execute("SELECT count(*) FROM  sqlite_master WHERE  type= 'index' and tbl_name = 'data' and name = 'timeidx_id'").fetchone();
indexexists=res[0];
print("Does index exist? # exists timeidx_id: {}".format(indexexists));
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
    
#REV: time indices, how many? I.e. 100*60*60 for an hour = 360000 doubles, i.e. 8*360k bytes. 720*4 1440*2 2880k bytes. 2.8mb?
atime =time.time();

'''
chromosome_set = set()
current_chromosome = ''
while True:
    db_record = cur.execute("SELECT chromosome FROM dbsnp WHERE chromosome > ? ORDER BY chromosome LIMIT 1", [current_chromosome,]).fetchall()
    if db_record:
        current_chromosome = db_record[0][0]
        chromosome_set.add(current_chromosome)
    else:
        break;
    pass;
'''

'''
timeidxdf = pd.read_sql("SELECT DISTINCT timeidx FROM data", indbconn);
print(timeidxdf);
maxtimeidx=timeidxdf.timeidx.max();
print("Read timeidxs, took {} msec. MIN {}  MAX {}   NPTS: {}".format(1e3*(time.time()-atime), timeidxdf.timeidx.min(), timeidxdf.timeidx.max(), len(timeidxdf.index)));
'''

#REV: do this with DB.

mylist = [];
togrp = ['blurdva', 'blur', 'saltype'];
threshs=list(range(0,256));

#timeidxs = cur.execute("SELECT MIN(timeidx), MAX(timeidx) FROM data").fetchone();
#print("Starting timeidx {}".format(timeidxs));
#print(timeidxs);
#mintimeidx = timeidxs[0];
#maxtimeidx = timeidxs[1];

#timeidx = mintimeidx;

mintimeidx = cur.execute("SELECT timeidx FROM data ORDER BY timeidx LIMIT 1").fetchone()[0];
maxtimeidx = cur.execute("SELECT timeidx FROM data ORDER BY timeidx DESC LIMIT 1").fetchone()[0];
print("Min: {}   Max: {}".format(mintimeidx, maxtimeidx));

timeidx = mintimeidx;

while( timeidx <= maxtimeidx ):
#REV: better to iterrows?
#for timeidx in timeidxdf.timeidx:
    print("Time idx {}/{}".format(timeidx,maxtimeidx));
    df = pd.read_sql( 'SELECT * FROM {} WHERE timeidx=={}'.format('data', timeidx), indbconn );
    if( len(df.index) < 1 ):
        print("Finished reading all time idxs! {}".format(timeidx));
        break;
    
    for val, vdf in df.groupby(togrp):
        myt = vdf[ vdf.tpfpdelta=='t' ].iloc[0]['sal'];
        fps = vdf[ vdf.tpfpdelta=='f' ];
        pctle = calc_pctl(myt, fps['sal']);
        #auroc = calc_auroc([myt], fps['sal'], threshs);
        val = list(val);
        #mylist.append( val + [pctle, auroc] );
        mylist.append( val + [timeidx, pctle] );
        pass;
    
    #timeidx = cur.execute("SELECT timeidx FROM data WHERE timeidx > ? ORDER BY timeidx LIMIT 1", [timeidx,]).fetchone();
    res = cur.execute("SELECT timeidx FROM data WHERE timeidx > ? ORDER BY timeidx LIMIT 1", [timeidx,]).fetchone();
    if( res is None ):
        print("Maybe last timeidx? Didn't get results...");
        break;
    else:
        timeidx=res[0];
        pass;
    
    pass;

#resdf = pd.DataFrame(columns=togrp+['pctle', 'auroc'], data=mylist);
resdf = pd.DataFrame(columns=togrp+['timeidx', 'pctle'], data=mylist);

#REV: not printing this out, this just summary.
result = resdf[['timeidx', 'blurdva', 'blur', 'saltype', 'pctle']].groupby(['blurdva', 'blur', 'saltype']).mean();
print(result);
print("Writing CSV to {}".format(outfname));
#result.to_csv(outfname);
resdf.to_csv(outfname+'.csv', index=False);

print("Finished {}".format(outfname));
