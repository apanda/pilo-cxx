import sys
import re
f = open(sys.argv[1])
prev_measures = {}
curr_measures = {}
start_pattern = re.compile(r"([0-9.]+) bandwidth measure")
end_pattern = re.compile(r"^--$")
measure_pattern = re.compile(r"\s+([A-Z_]+) bw total ([0-9.]+) link ([0-9.]+)")
total_measure_pattern = re.compile(r"([0-9.]+) bw  total ([0-9.]+) link ([0-9.]+)")
start = False
curr_time = 0.0
last_time = 0.0
for l in f:
    if start_pattern.match(l):
        match = start_pattern.match(l)
        print
        start = True
        last_time = curr_time
        prev_measures = curr_measures
        curr_measures = {}
        curr_time = float(match.group(1))
        print "%f "%curr_time,
    elif start and end_pattern.match(l):
        start = False
    elif start and total_measure_pattern.match(l):
        match = total_measure_pattern.match(l)
        assert(float(match.group(1)) == curr_time)
        curr_measures['total_global'] = curr_time * float(match.group(2))
        curr_measures['total_link'] = curr_time * float(match.group(3))
        diff_total = max(0., curr_measures['total_global'] - prev_measures.get('total_global', 0.))
        diff_link = max(0., curr_measures['total_link'] - prev_measures.get('total_link', 0.))
        print "%f "%(diff_link/1000000.),
    elif start and measure_pattern.match(l):
        match = measure_pattern.match(l)
        type = match.group(1)
        global_key = '%s_global'%type
        link_key = '%s_link'%type
        curr_measures[global_key] = curr_time * float(match.group(2))
        curr_measures[link_key] = curr_time * float(match.group(3))
        diff_total = max(0., curr_measures[global_key] - prev_measures.get(global_key, 0.))
        diff_link = max(0., curr_measures[link_key] - prev_measures.get(link_key, 0.))
        print "%f "%(diff_link/1000000.),
