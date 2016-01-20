import sys
import re
f = open(sys.argv[1])
idistance_re = re.compile(r"([0-9.]+) IDISTANCE ([0-9.]+) ([0-9]+)")
checked_re = re.compile(r"\s+([0-9.]+) Checked ([0-9]+)\s+passed\s+([0-9]+)")
last_time = None
last_measure = 0
current_time = None
current_cdf = {}
for l in f:
    if idistance_re.match(l):
        match = idistance_re.match(l)
        # print match.group(1)
        time = float(match.group(1))
        if current_time != time:
            current_time = time
            current_cdf = {}
        current_cdf[float(match.group(2))] = int(match.group(3))
    elif checked_re.match(l):
        match = checked_re.match(l)
        time = float(match.group(1))
        assert(current_time == time)
        checked = int(match.group(2))
        passed = int(match.group(3))
        failed = checked - passed
        print "%f"%time,
        for k in sorted(current_cdf.keys()):
            print " %f %f"%(k, float(current_cdf[k])/float(checked)),
        print "d %f"%(float(failed)/float(checked))
