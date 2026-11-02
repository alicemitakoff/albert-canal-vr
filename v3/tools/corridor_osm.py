"""Collect the bridges, locks, moorings, terminals and the water polygon along the Albert Canal corridor
(Antwerp to km 72, past BCTN Meerhout) from the OpenStreetMap API, which only answers small boxes.
Writes v3/data/corridor.json next to the canal centreline from v2. Run from the repository root:
    python3 v3/tools/corridor_osm.py
OpenStreetMap data, ODbL. Credited on screen."""
import json, math, os, sys, time, urllib.request, xml.etree.ElementTree as ET

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
ROUTE = json.load(open(os.path.join(ROOT, 'v2/assets/map/route.json')))
OUT = os.path.join(ROOT, 'v3/data/corridor.json')
KM_END = 72.0
BOX = 0.028          # degrees, about 2 km by 3 km
NEAR = 220.0         # metres from the centreline to count as "on the canal"

canal = ROUTE['canal']; km = ROUTE['km']

def dist_m(lon1, lat1, lon2, lat2):
    x = (lon2 - lon1) * 111320 * math.cos(math.radians((lat1 + lat2) / 2)); y = (lat2 - lat1) * 110540
    return math.hypot(x, y)

def seg_dist(lon, lat, a, b):
    ax, ay = a; bx, by = b
    cx = math.cos(math.radians(lat)) * 111320
    px, py = (lon - ax) * cx, (lat - ay) * 110540; vx, vy = (bx - ax) * cx, (by - ay) * 110540
    L = vx * vx + vy * vy
    t = 0 if L == 0 else max(0, min(1, (px * vx + py * vy) / L))
    return math.hypot(px - t * vx, py - t * vy), t

def project(lon, lat):
    """distance to the canal in m and km along it from Antwerp"""
    best = (1e9, 0)
    for i in range(len(canal) - 1):
        d, t = seg_dist(lon, lat, canal[i], canal[i + 1])
        if d < best[0]:
            best = (d, km[i] + t * (km[i + 1] - km[i]))
    return best

def boxes():
    out = []; last = None
    for i, p in enumerate(canal):
        if km[i] > KM_END: break
        if last is None or dist_m(*last, *p) > 1500:
            out.append((p[0] - BOX / 2, p[1] - BOX / 2, p[0] + BOX / 2, p[1] + BOX / 2)); last = p
    return out

def fetch(bbox):
    url = 'https://api.openstreetmap.org/api/0.6/map?bbox=%.4f,%.4f,%.4f,%.4f' % bbox
    req = urllib.request.Request(url, headers={'User-Agent': 'AImovation Albert Canal corridor map (alice.mitakoff@aimovation.com)'})
    for attempt in range(3):
        try:
            with urllib.request.urlopen(req, timeout=120) as r:
                return ET.fromstring(r.read())
        except Exception as e:
            print('retry', bbox, e); time.sleep(5)
    return None

nodes, ways = {}, {}
for n, bb in enumerate(boxes()):
    root = fetch(bb)
    if root is None: continue
    for el in root:
        if el.tag == 'node':
            nodes[el.get('id')] = (float(el.get('lon')), float(el.get('lat')), {t.get('k'): t.get('v') for t in el.findall('tag')})
        elif el.tag == 'way':
            ways[el.get('id')] = ([nd.get('ref') for nd in el.findall('nd')], {t.get('k'): t.get('v') for t in el.findall('tag')})
    print('box', n + 1, 'nodes', len(nodes), 'ways', len(ways)); time.sleep(1)

def centre(refs):
    pts = [nodes[r][:2] for r in refs if r in nodes]
    if not pts: return None
    return sum(p[0] for p in pts) / len(pts), sum(p[1] for p in pts) / len(pts)

bridges, locks, moorings, terminals, water = [], [], [], [], []
seen_bridge = []
for wid, (refs, tags) in ways.items():
    c = centre(refs)
    if not c: continue
    d, at = project(*c)
    if tags.get('bridge') == 'yes' and d < NEAR and (tags.get('highway') or tags.get('railway')):
        if any(abs(at - b['km']) < 0.12 for b in seen_bridge): continue
        name = tags.get('name') or tags.get('ref') or ('Railway bridge' if tags.get('railway') else 'Road bridge')
        kind = 'rail' if tags.get('railway') else ('motorway' if tags.get('highway') in ('motorway', 'motorway_link', 'trunk') else 'road')
        b = {'name': name, 'kind': kind, 'km': round(at, 2), 'll': [round(c[0], 6), round(c[1], 6)]}
        if tags.get('maxheight'): b['clearance'] = tags['maxheight']
        bridges.append(b); seen_bridge.append(b)
    elif tags.get('waterway') in ('lock_gate',) or tags.get('lock') == 'yes':
        if d < 400: locks.append({'name': tags.get('name', 'Lock'), 'km': round(at, 2), 'll': [round(c[0], 6), round(c[1], 6)]})
    elif tags.get('mooring') in ('yes', 'commercial', 'waiting') or tags.get('man_made') in ('pier', 'quay') and d < NEAR:
        moorings.append({'name': tags.get('name', ''), 'type': tags.get('mooring', tags.get('man_made')), 'km': round(at, 2), 'll': [round(c[0], 6), round(c[1], 6)]})
    elif (tags.get('industrial') == 'port' or tags.get('landuse') == 'port' or tags.get('harbour') == 'yes' or tags.get('man_made') == 'port') and d < 600:
        terminals.append({'name': tags.get('name', ''), 'km': round(at, 2), 'll': [round(c[0], 6), round(c[1], 6)]})
    elif (tags.get('natural') == 'water' or tags.get('waterway') == 'riverbank' or tags.get('water') in ('canal', 'river', 'harbour')) and d < 120:
        pts = [[round(nodes[r][0], 6), round(nodes[r][1], 6)] for r in refs if r in nodes]
        if len(pts) > 3: water.append(pts)
for nid, (lon, lat, tags) in nodes.items():
    if tags.get('waterway') == 'lock_gate' or tags.get('lock') == 'yes':
        d, at = project(lon, lat)
        if d < 300: locks.append({'name': tags.get('name', 'Lock gate'), 'km': round(at, 2), 'll': [round(lon, 6), round(lat, 6)]})
    if tags.get('mooring') or tags.get('seamark:type') == 'mooring':
        d, at = project(lon, lat)
        if d < NEAR: moorings.append({'name': tags.get('name', ''), 'type': tags.get('mooring', 'mooring'), 'km': round(at, 2), 'll': [round(lon, 6), round(lat, 6)]})

bridges.sort(key=lambda b: b['km'])
data = {'source': 'OpenStreetMap contributors (ODbL), canal centreline from the AImovation survey',
        'canal': [p for i, p in enumerate(canal) if km[i] <= KM_END + 2], 'km': [k for k in km if k <= KM_END + 2],
        'locks': [l for l in ROUTE['locks'] if l['km'] <= KM_END], 'osmLocks': locks, 'bridges': bridges, 'moorings': moorings,
        'osmTerminals': terminals, 'terminals': ROUTE['terminals'], 'hubs': ROUTE['hubs'], 'towns': ROUTE['towns'],
        'portLocks': ROUTE['portLocks'], 'port': ROUTE['port'], 'water': water}
os.makedirs(os.path.dirname(OUT), exist_ok=True)
json.dump(data, open(OUT, 'w'), separators=(',', ':'))
print('bridges', len(bridges), 'locks', len(locks), 'moorings', len(moorings), 'terminals', len(terminals), 'water', len(water), 'bytes', os.path.getsize(OUT))
