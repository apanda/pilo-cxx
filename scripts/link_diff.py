import sys
import re
f = open(sys.argv[1])
cd_re = re.compile(r"([0-9.]+) CTRL_LINK_DIFF")
ce_re = re.compile(r"([0-9.]+) CTRL_LINK_EXTRA")
curr_time = None
curr_diff = 0
for l in f:
    match = None
    if cd_re.match(l):
        match = cd_re.match(l)
    elif ce_re.match(l):
        match = ce_re.match(l)
    if match:
        time = float(match.group(1))
        if time != curr_time:
            if curr_time:
                print "%f %d"%(curr_time, curr_diff)
            curr_time = time
            curr_diff = 0
        p = l.strip().split()
        d = int(p[-1])
        curr_diff += d

if curr_time:
    print "%f %d"%(curr_time, curr_diff)
curr_time = time
curr_diff = 0
