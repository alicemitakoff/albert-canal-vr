# Ground textures in Google Maps colour coding with the real photo drawn through them.
# Land use from OpenStreetMap (map_0..7.osm) is painted in Google's palette, then the photo's light and shade
# (field furrows, tree crowns, roof edges) is laid on top so it still reads as a real place. Roads and rail are
# drawn in Google's road colours. Input: the existing photo chunks; output: chunks_g (and a Quest set).
import json, glob, math, os, sys
import numpy as np
import xml.etree.ElementTree as ET
from PIL import Image, ImageDraw, ImageFilter
from scipy.ndimage import gaussian_filter
sys.path.insert(0, '/home/claude/real')
from geo import ll_to_xz

SRC = '/home/claude/albert-canal-vr/v2/assets/scene/'
OUT = SRC + 'chunks_g/'; OUTQ = SRC + 'chunks_gq/'
os.makedirs(OUT, exist_ok=True); os.makedirs(OUTQ, exist_ok=True)
meta = json.load(open(SRC + 'chunks.json'))

# ---------- OSM ----------
nodes = {}; ways = {}; rels = []
for f in sorted(glob.glob('/home/claude/real/osm/map_*.osm')):
    r = ET.parse(f).getroot()
    for n in r.iter('node'): nodes[n.get('id')] = (float(n.get('lon')), float(n.get('lat')))
    for w in r.iter('way'):
        ways[w.get('id')] = ([nd.get('ref') for nd in w.findall('nd')], {t.get('k'): t.get('v') for t in w.findall('tag')})
    for rl in r.iter('relation'):
        rels.append(([(m.get('type'), m.get('ref'), m.get('role')) for m in rl.findall('member')], {t.get('k'): t.get('v') for t in rl.findall('tag')}))

def xy(ids):
    pts = [nodes[i] for i in ids if i in nodes]
    if len(pts) < 2: return None
    a = np.array(pts); X, Z = ll_to_xz(a[:, 0], a[:, 1]); return np.stack([X, Z], 1)

# class ids, painted in this order (later wins)
CL = ['land', 'farmland', 'meadow', 'scrub', 'forest', 'residential', 'industrial', 'paved', 'water', 'building']
PAL = {  # Google Maps palette, deepened about 10% so it still reads green under sunlight in 3D
    'land': (226, 228, 212), 'farmland': (212, 228, 186), 'meadow': (186, 222, 162), 'scrub': (174, 212, 154),
    'forest': (146, 198, 134), 'residential': (226, 224, 220), 'industrial': (220, 222, 226), 'paved': (210, 212, 216),
    'water': (140, 182, 240), 'building': (208, 206, 203)}
# how much of the photo's own shade (detail) and colour (photo) shows through, per class
MIX = {'land': (0.95, 0.50), 'farmland': (0.9, 0.36), 'meadow': (0.9, 0.34), 'scrub': (1.0, 0.36), 'forest': (1.2, 0.38),
       'residential': (0.70, 0.30), 'industrial': (0.60, 0.25), 'paved': (0.55, 0.20), 'water': (0.25, 0.10), 'building': (0.5, 0.25)}

def classify(t):
    lu = t.get('landuse'); na = t.get('natural'); le = t.get('leisure'); am = t.get('amenity')
    if 'building' in t: return 'building'
    if na == 'water' or t.get('waterway') in ('riverbank', 'dock') or lu in ('basin', 'reservoir'): return 'water'
    if lu in ('forest',) or na in ('wood',): return 'forest'
    if na in ('scrub', 'heath'): return 'scrub'
    if lu in ('meadow', 'grass', 'greenfield', 'recreation_ground', 'village_green') or na == 'grassland' or le in ('park', 'garden', 'pitch', 'playground'): return 'meadow'
    if lu in ('farmland', 'orchard', 'allotments'): return 'farmland'
    if lu in ('residential',): return 'residential'
    if lu in ('industrial', 'commercial', 'retail', 'farmyard', 'railway', 'port', 'brownfield', 'construction'): return 'industrial'
    if am in ('parking',) or t.get('area:highway') or (t.get('surface') in ('asphalt', 'concrete', 'paving_stones', 'paved', 'concrete:plates') and t.get('area') == 'yes'): return 'paved'
    return None

polys = []   # (class, outer ring xz)
for wid, (ids, t) in ways.items():
    c = classify(t)
    if c and ids and ids[0] == ids[-1]:
        p = xy(ids)
        if p is not None: polys.append((c, p))
for members, t in rels:
    if t.get('type') != 'multipolygon': continue
    c = classify(t)
    if not c: continue
    # join outer ways into rings
    segs = [ways[ref][0] for (ty, ref, role) in members if ty == 'way' and role == 'outer' and ref in ways]
    rings = []
    while segs:
        ring = list(segs.pop(0)); changed = True
        while changed and ring[0] != ring[-1]:
            changed = False
            for k, s in enumerate(segs):
                if s[0] == ring[-1]: ring += s[1:]; segs.pop(k); changed = True; break
                if s[-1] == ring[-1]: ring += s[::-1][1:]; segs.pop(k); changed = True; break
        rings.append(ring)
    for ring in rings:
        p = xy(ring)
        if p is not None: polys.append((c, p))
order = {c: i for i, c in enumerate(CL)}
polys.sort(key=lambda a: order[a[0]])

# roads and rail: (width m, fill, casing)
ROAD = {'motorway': (14, (253, 226, 147), (249, 171, 0)), 'motorway_link': (8, (253, 226, 147), (249, 171, 0)), 'trunk': (12, (253, 226, 147), (249, 171, 0)),
        'primary': (10, (255, 255, 255), (218, 220, 224)), 'secondary': (9, (255, 255, 255), (218, 220, 224)), 'tertiary': (8, (255, 255, 255), (218, 220, 224)),
        'unclassified': (6, (255, 255, 255), (218, 220, 224)), 'residential': (6, (255, 255, 255), (218, 220, 224)), 'service': (4, (255, 255, 255), (225, 227, 230)),
        'track': (3, (232, 228, 214), None), 'cycleway': (2, (236, 240, 246), None), 'footway': (1.6, (236, 236, 236), None), 'path': (1.6, (236, 236, 236), None)}
lines = []
for wid, (ids, t) in ways.items():
    hw = t.get('highway'); rw = t.get('railway')
    if hw in ROAD or rw == 'rail':
        p = xy(ids)
        if p is not None: lines.append(('rail' if rw == 'rail' else hw, p))
rank = {k: i for i, k in enumerate(['path', 'footway', 'cycleway', 'track', 'service', 'residential', 'unclassified', 'tertiary', 'secondary', 'primary', 'trunk', 'motorway_link', 'motorway', 'rail'])}
lines.sort(key=lambda a: rank.get(a[0], 0))
print('polys', len(polys), 'lines', len(lines))

def bake(c):
    img = Image.open(SRC + 'chunks/' + c['f']).convert('RGB'); S = img.size[0]
    SS = 2 if S <= 1024 else 1                    # supersample the small tiers for clean edges
    W = S * SS
    sx = W / c['w']; sz = W / c['h']
    tf = lambda p: [((x - c['x']) * sx, (z - c['z']) * sz) for x, z in p]
    x0, z0, x1, z1 = c['x'] - 50, c['z'] - 50, c['x'] + c['w'] + 50, c['z'] + c['h'] + 50
    cls = Image.new('L', (W, W), 0); d = ImageDraw.Draw(cls)
    for k, p in polys:
        if p[:, 0].max() < x0 or p[:, 0].min() > x1 or p[:, 1].max() < z0 or p[:, 1].min() > z1: continue
        d.polygon(tf(p), fill=order[k])
    cls = np.asarray(cls.resize((S, S), Image.NEAREST)) if SS > 1 else np.asarray(cls)
    P = np.asarray(img).astype(np.float32) / 255
    L = P @ np.array([0.299, 0.587, 0.114])
    sig = max(1.5, 7.0 * S / c['w'])               # about 7 m: keeps furrows and tree crowns, drops the colour cast
    Lb = gaussian_filter(L, sig)
    det = np.clip((L - Lb) * 2.2, -0.45, 0.45)
    base = np.zeros_like(P); kd = np.zeros(L.shape, np.float32); kp = np.zeros(L.shape, np.float32)
    for k in CL:
        m = cls == order[k]
        if not m.any(): continue
        base[m] = np.array(PAL[k]) / 255; kd[m] = MIX[k][0]; kp[m] = MIX[k][1]
    # soft class edges
    for ch in range(3): base[..., ch] = gaussian_filter(base[..., ch], 0.8 * S / 1024 + 0.4)
    kd = gaussian_filter(kd, 1.0); kp = gaussian_filter(kp, 1.0)
    # the photo, brightened to sit on Google's light palette
    Pb = np.clip(P * 1.12 + 0.05, 0, 1)
    out = base * (1 + det[..., None] * kd[..., None])
    out = out * (1 - kp[..., None]) + Pb * kp[..., None]
    o8 = Image.fromarray(np.clip(out * 255, 0, 255).astype(np.uint8))
    # roads and rail on top, in Google colours
    if SS > 1: o8 = o8.resize((W, W), Image.BILINEAR)
    dr = ImageDraw.Draw(o8)
    for k, p in lines:
        if p[:, 0].max() < x0 or p[:, 0].min() > x1 or p[:, 1].max() < z0 or p[:, 1].min() > z1: continue
        pts = tf(p)
        if k == 'rail':
            dr.line(pts, fill=(160, 164, 170), width=max(1, int(3 * sx))); continue
        w, fill, case = ROAD[k]
        if case: dr.line(pts, fill=case, width=max(1, int((w + 1.6) * sx)), joint='curve')
    for k, p in lines:
        if k == 'rail' or p[:, 0].max() < x0 or p[:, 0].min() > x1 or p[:, 1].max() < z0 or p[:, 1].min() > z1: continue
        w, fill, case = ROAD[k]; dr.line(tf(p), fill=fill, width=max(1, int(w * sx)), joint='curve')
    if SS > 1: o8 = o8.resize((S, S), Image.LANCZOS)
    o8.save(OUT + c['f'], quality=86)
    q = o8.resize((max(256, S // 2), max(256, S // 2)), Image.LANCZOS) if S > 512 else o8
    q.save(OUTQ + c['f'], quality=82)

only = sys.argv[1:]
for c in meta['chunks']:
    if only and c['f'] not in only: continue
    bake(c)
print('done')
