import sys
import re
from collections import defaultdict
f = open(sys.argv[1])
converge_re = re.compile(r"CONVERGE [s1-9\-]+ ([0-9.]+)")
cdf = defaultdict(lambda: 0)
tot = 0
for l in f:
    if converge_re.match(l):
        match = converge_re.match(l)
        time = float(match.group(1))
        tot += 1
        cdf[time] += 1
    # if idistance_re.match(l):
        # match = idistance_re.match(l)
        # # print match.group(1)
        # time = float(match.group(1))
        # if current_time != time:
            # current_time = time
        # cdf[float(match.group(2))] += int(match.group(3))
        # tot += int(match.group(3))
cum = 0
for k in sorted(cdf.keys()):
    cum += cdf[k]
    print k, float(cum)/float(tot)
