#!/usr/bin/env python3
"""Compare replayed integer mouse counts at display-frame boundaries.

The residual is frame displacement minus a Gaussian local trend (sigma 25 ms).
It measures short-term variation, not sensor error: there is no ground-truth
finger trajectory in a capture. Both axes and empty frames are reported.
"""
import argparse
import bisect
import csv
import json
import math
from pathlib import Path


def measure(path, start, end, hz):
    with path.open(newline='') as f:
        rows = list(csv.DictReader(f))
    times, cumulative = [], []
    x = y = 0
    for row in rows:
        times.append(float(row['time']))
        x += int(row['ix'])
        y += int(row['iy'])
        cumulative.append((x, y))
    if not times or start < times[0] or end > times[-1] or end-start < .3:
        raise ValueError('Choose a >=300 ms interval within the recording')
    frames = []
    for i in range(math.ceil((end-start)*hz)):
        index = bisect.bisect_left(times, start+i/hz)-1
        frames.append(cumulative[index] if index >= 0 else (0, 0))
    deltas = [(b[0]-a[0], b[1]-a[1]) for a, b in zip(frames, frames[1:])]
    sigma = .025*hz
    radius = math.ceil(3*sigma)
    weights = [math.exp(-.5*(i/sigma)**2) for i in range(-radius, radius+1)]
    weights = [w/sum(weights) for w in weights]
    rms = []
    for axis in range(2):
        errors = []
        for i in range(radius, len(deltas)-radius):
            trend = sum(w*deltas[i+j-radius][axis] for j, w in enumerate(weights))
            errors.append((deltas[i][axis]-trend)**2)
        rms.append(math.sqrt(sum(errors)/len(errors)))
    return {
        'residual_rms_counts': {'x': rms[0], 'y': rms[1], 'vector': math.hypot(*rms)},
        'mean_counts_per_frame': [sum(d[a] for d in deltas)/len(deltas) for a in range(2)],
        'empty_frames_percent': 100*sum(d == (0, 0) for d in deltas)/len(deltas),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('before', type=Path)
    parser.add_argument('after', type=Path)
    parser.add_argument('--start', type=float, required=True)
    parser.add_argument('--end', type=float, required=True)
    parser.add_argument('--hz', type=float, default=240)
    args = parser.parse_args()
    if args.hz <= 0:
        parser.error('--hz must be positive')
    results = {name: measure(path, args.start, args.end, args.hz)
               for name, path in [('before', args.before), ('after', args.after)]}
    print(json.dumps(results, indent=2))


if __name__ == '__main__':
    main()
