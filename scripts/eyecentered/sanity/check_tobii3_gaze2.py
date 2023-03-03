#REV: tobii 3 seems to have gaze2d outside legal (0,1) range.
#REV: They match up with gaze3d though...so fine.

import pandas as pd
df = pd.read_csv('/home/riveale/mazdaexample/raw-2022-12-19-12-38-06/device-Tobii3_TG03B-080200014621___192.168.75.51_TG03B-080200014621___192.168.75.51_/resampled.csv')
dfx = df[(df.gaze2d_1<0) | (df.gaze2d_1>1) | (df.gaze2d_0>1) | (df.gaze2d_0<0)]
import math
import np
import numpy as np
dfx["angdowndeg"] = np.atan2(gaze3d_1, gaze3d_2);
dfx["angdowndeg"] = np.arctan2(gaze3d_1, gaze3d_2);
dfx["angdowndeg"] = np.arctan2(dfx.gaze3d_1, dfx.gaze3d_2);
dfx = dfx.copy()
dfx["angdowndeg"] = np.arctan2(dfx.gaze3d_1, dfx.gaze3d_2);
dfx
dfx["angdowndeg"] = np.degrees( np.arctan2(dfx.gaze3d_1, dfx.gaze3d_2));
