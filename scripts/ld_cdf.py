import sys
from collections import defaultdict
f = open(sys.argv[1])
counts = defaultdict(lambda: 0)
tot = 0
for l in f:
    p = l.split()
    diff = int(p[1])
    counts[diff] += 1
    tot += 1
prev = 0.0
for l in sorted(counts.keys()):
    prev += float(counts[l]) / float(tot)
    print "%d %f"%(l, prev)
