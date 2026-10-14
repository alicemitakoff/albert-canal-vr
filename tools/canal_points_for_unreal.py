"""Turn a canal or river centreline into the CanalPoints list of the Swap_Experience actor in Unreal.

The barge follows CanalPoints when it sails outside the hub and the lock. Each point is in Unreal centimetres
(X east, Y south) measured from the scene origin, one point every 100 m along the water.

Run it from the project folder, for example:

    python scripts/tools/canal_points_for_unreal.py data/albert_canal_centreline.geojson ^
        --origin 51.10111 5.08895 --start 51.0950 5.0560 --end 51.1130 5.1500

  data/....geojson   a GeoJSON file with the centreline as a LineString (OpenStreetMap waterways work)
  --origin LAT LON   the scene origin, the same numbers as Origin Latitude and Longitude in CesiumGeoreference
  --start  LAT LON   a point on the water where the barge story starts (downstream)
  --end    LAT LON   a point on the water past the end of the story (upstream)
  --step   metres    distance between points (default 100, which is what the code expects)

It writes canal_points.txt next to the GeoJSON file. In Unreal: click Swap_Experience, find Canal Points under
Scene in the Details panel, right click the words Canal Points, choose Paste, and the whole list goes in at once.
Needs only Python 3, no extra packages.
"""
import argparse, json, math, os, sys

A, E2 = 6378137.0, 0.00669437999014   # WGS84 ellipsoid, the same constants as the web page and geo.py


def frame(lat0, lon0):
    s = math.sin(math.radians(lat0))
    nr = A / math.sqrt(1 - E2 * s * s)
    mr = A * (1 - E2) / (1 - E2 * s * s) ** 1.5
    mlon = math.radians(1) * nr * math.cos(math.radians(lat0))   # metres per degree of longitude
    mlat = math.radians(1) * mr                                    # metres per degree of latitude
    return lambda lon, lat: ((lon - lon0) * mlon, -(lat - lat0) * mlat)   # metres east, metres south


def lines(gj):
    feats = gj['features'] if gj.get('type') == 'FeatureCollection' else [gj]
    for f in feats:
        g = f.get('geometry') or {}
        if g.get('type') == 'LineString':
            yield g['coordinates']
        elif g.get('type') == 'MultiLineString':
            yield from g['coordinates']


def nearest(pts, p):
    best = (1e30, 0, 0.0)
    for i in range(len(pts) - 1):
        (ax, ay), (bx, by) = pts[i], pts[i + 1]
        dx, dy = bx - ax, by - ay
        L2 = dx * dx + dy * dy or 1e-9
        u = max(0.0, min(1.0, ((p[0] - ax) * dx + (p[1] - ay) * dy) / L2))
        qx, qy = ax + dx * u, ay + dy * u
        d = (qx - p[0]) ** 2 + (qy - p[1]) ** 2
        if d < best[0]:
            best = (d, i, u)
    return best


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('geojson')
    ap.add_argument('--origin', nargs=2, type=float, required=True, metavar=('LAT', 'LON'))
    ap.add_argument('--start', nargs=2, type=float, required=True, metavar=('LAT', 'LON'))
    ap.add_argument('--end', nargs=2, type=float, required=True, metavar=('LAT', 'LON'))
    ap.add_argument('--step', type=float, default=100.0)
    a = ap.parse_args()
    to_m = frame(*a.origin)
    S, E = to_m(a.start[1], a.start[0]), to_m(a.end[1], a.end[0])
    # pick the line that passes closest to the start point
    best = None
    for coords in lines(json.load(open(a.geojson, encoding='utf-8'))):
        pts = [to_m(c[0], c[1]) for c in coords]
        if len(pts) < 2:
            continue
        d = nearest(pts, S)[0] + nearest(pts, E)[0]
        if best is None or d < best[0]:
            best = (d, pts)
    if best is None:
        sys.exit('No LineString found in ' + a.geojson)
    pts = best[1]
    _, i0, u0 = nearest(pts, S)
    _, i1, u1 = nearest(pts, E)
    if (i1 + u1) < (i0 + u0):           # the line runs the other way: turn it round
        pts = pts[::-1]
        _, i0, u0 = nearest(pts, S)
        _, i1, u1 = nearest(pts, E)
    # cut the line between start and end, then resample every step metres
    lerp = lambda p, q, u: (p[0] + (q[0] - p[0]) * u, p[1] + (q[1] - p[1]) * u)
    path = [lerp(pts[i0], pts[i0 + 1], u0)] + pts[i0 + 1:i1 + 1] + [lerp(pts[i1], pts[i1 + 1], u1)]
    out, carry = [path[0]], 0.0
    for p, q in zip(path[:-1], path[1:]):
        L = math.dist(p, q)
        t = a.step - carry
        while t <= L:
            out.append(lerp(p, q, t / L))
            t += a.step
        carry = L - (t - a.step)
    text = '(' + ','.join('(X=%.1f,Y=%.1f,Z=0.0)' % (x * 100, y * 100) for x, y in out) + ')'
    dst = os.path.join(os.path.dirname(os.path.abspath(a.geojson)), 'canal_points.txt')
    open(dst, 'w').write(text)
    print('%d points, %.1f km of water, written to %s' % (len(out), (len(out) - 1) * a.step / 1000, dst))
    print('First point (cm): X=%.0f Y=%.0f   last point: X=%.0f Y=%.0f' % (out[0][0] * 100, out[0][1] * 100, out[-1][0] * 100, out[-1][1] * 100))
    print('Set Km Start in Swap_Experience to the canal kilometre of your start point.')


if __name__ == '__main__':
    main()
