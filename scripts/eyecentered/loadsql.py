
import sqlite3;
import pandas as pd;

import sys;

dbfname = sys.argv[1];
tablename = sys.argv[2];

dbconn = sqlite3.connect(dbfname);

#df = pd.read_sql('SELECT * FROM {}'.format(tablename), dbconn );
dflist = [];
chunksizerows=100000;
iterated=0;
for df in pd.read_sql('SELECT * FROM {}'.format(tablename), dbconn, chunksize=chunksizerows):
    iterated+=1;
    print("Reading {} rows ({} so far)".format(chunksizerows, chunksizerows*iterated));
    dflist.append(df);
    pass;

df = pd.concat( dflist ).reset_index(drop=True);

print(df);

