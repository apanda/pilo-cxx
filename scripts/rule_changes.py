import sys
import re
f = open(sys.argv[1])
rule_change_re = re.compile(r"([0-9.]+) rule changes ([0-9]+)")
last_time = None
last_measure = 0
for l in f:
    if rule_change_re.match(l):
        match = rule_change_re.match(l)
        time = float(match.group(1))
        rules = int(match.group(2))
        if last_time:
            diff = float(rules - last_measure)
            rate = diff / (time - last_time)
            print "%f %f"%(time, diff)
        last_time = time
        last_measure = rules
