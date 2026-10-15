# Battery tender (floating e-barge) for version 2 of the Albert Canal demo.
# Built from the parts of the hero cargo e-barge so the two vessels share one visual language.
# Blender frame: stern at Y=0, bow at +Y, X starboard, Z up. Exported to glTF (Y up).
# Animated parts (driven by the web page): Crane_slew (Z), Crane_boom (X), Crane_jib (X), Crane_tip (empty),
# Crane_ram1 / Crane_ram1_rod / Crane_ram1_anchor, Crane_ram2 / Crane_ram2_rod / Crane_ram2_anchor,
# Crane_spreader (root level, placed under the tip), Pack_proto (one battery pack, cloned into the Slot_* empties).
# Usage: python build_tender.py <out.glb>
import bpy, bmesh, sys, math, mathutils
V = mathutils.Vector
out = sys.argv[-1]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath='/home/claude/real/cma_cgm_ebarge_HERO.glb')
src = {o.name: o for o in bpy.context.scene.objects}
col = bpy.context.scene.collection

def mat(name, rgb, rough=0.5, metal=0.0, emit=None, es=4.0):
    m = bpy.data.materials.new(name); m.use_nodes = True
    b = m.node_tree.nodes['Principled BSDF']
    b.inputs['Base Color'].default_value = (*rgb, 1); b.inputs['Roughness'].default_value = rough; b.inputs['Metallic'].default_value = metal
    if emit:
        b.inputs['Emission Color'].default_value = (*emit, 1); b.inputs['Emission Strength'].default_value = es
    return m
def srgb(h):
    h = h.lstrip('#'); c = [int(h[i:i + 2], 16) / 255 for i in (0, 2, 4)]
    return tuple(x / 12.92 if x <= 0.04045 else ((x + 0.055) / 1.055) ** 2.4 for x in c)

HULL = mat('Tender_Hull', srgb('#28323b'), 0.42, 0.25)
STRIPE = mat('Tender_Stripe', srgb('#e9ecee'), 0.45)
DECK = mat('Tender_Deck', srgb('#7c858a'), 0.8, 0.2)
NONSLIP = mat('Tender_Walkway', srgb('#3d6b4f'), 0.9)
YEL = mat('Crane_Yellow', srgb('#f0b400'), 0.38, 0.15)
DARK = mat('Crane_Dark', srgb('#1c1f22'), 0.55, 0.4)
CHROME = mat('Crane_Chrome', srgb('#d8dde0'), 0.12, 1.0)
STEEL = mat('Steel_Galv2', srgb('#9aa2a8'), 0.45, 0.85)
RUBBER = mat('Fender_Rubber', srgb('#141414'), 0.9)
RED = mat('Paint_Red2', srgb('#b3261e'), 0.5)
GLASS = mat('Cab_Glass', srgb('#203040'), 0.05, 0.2)
WIRE = mat('Wire_Rope', srgb('#2a2a2a'), 0.6, 0.7)
ORANGE = mat('Plug_Orange2', srgb('#f07a1a'), 0.5)

def link(o):
    if o.name not in col.objects: col.objects.link(o)
    return o
def dup(name, newname, scale=(1, 1, 1), dz=0.0, material_map=None):
    s = src[name]; o = s.copy(); o.data = s.data.copy(); o.name = newname; link(o)
    o.parent = None; o.matrix_world = mathutils.Matrix.Identity(4)
    o.scale = scale; o.location = (0, 0, dz)
    if material_map:
        for i, m in enumerate(o.data.materials):
            if m and m.name in material_map: o.data.materials[i] = material_map[m.name]
    return o
def bevel(o, w=0.03, seg=2):
    m = o.modifiers.new('bev', 'BEVEL'); m.width = w; m.segments = seg; m.limit_method = 'ANGLE'
    o.modifiers.new('wn', 'WEIGHTED_NORMAL')
    return o
def box(name, c, s, m, parent=None, bev=0.03, rot=(0, 0, 0)):
    bpy.ops.mesh.primitive_cube_add(size=1, location=c, rotation=rot)
    o = bpy.context.object; o.name = name; o.scale = s; o.data.materials.append(m)
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    if bev: bevel(o, bev)
    if parent: setp(o, parent)
    return o
def cyl(name, c, r, h, m, parent=None, rot=(0, 0, 0), v=32, bev=0.0):
    bpy.ops.mesh.primitive_cylinder_add(vertices=v, radius=r, depth=h, location=c, rotation=rot)
    o = bpy.context.object; o.name = name; o.data.materials.append(m)
    bpy.ops.object.shade_smooth()
    if bev: bevel(o, bev)
    if parent: setp(o, parent)
    return o
def empty(name, c, parent=None):
    o = bpy.data.objects.new(name, None); o.empty_display_size = 0.5; o.location = c; link(o)
    if parent: setp(o, parent)
    return o
def setp(o, p):
    bpy.context.view_layer.update()
    mw = o.matrix_world.copy(); o.parent = p; o.matrix_parent_inverse = p.matrix_world.inverted(); o.matrix_world = mw
def tapered(name, length, w0, h0, w1, h1, m, along=+1):
    """box section beam from the origin along +Y (along=+1) or -Y (along=-1), tapering"""
    bm = bmesh.new()
    vs = []
    for (y, w, h) in ((0, w0, h0), (length * along, w1, h1)):
        for (sx, sz) in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
            vs.append(bm.verts.new((sx * w / 2, y, sz * h / 2)))
    f = lambda *i: bm.faces.new([vs[k] for k in i])
    f(0, 1, 2, 3); f(7, 6, 5, 4); f(0, 4, 5, 1); f(1, 5, 6, 2); f(2, 6, 7, 3); f(3, 7, 4, 0)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
    o = bpy.data.objects.new(name, me); link(o); me.materials.append(m)
    return bevel(o, 0.04, 3)

# ------------------------------------------------------------------ hull, deck, superstructure
L, W = 56.0, 8.6
sx, sy = W / 11.44, L / 135.0
ZS = 0.78                                  # the tender is shallower than the cargo e-barge
hull = dup('Hull_shell', 'Tender_hull', (sx, sy, ZS), material_map={'Hull_CMA_Navy': HULL})
deck = dup('Deck_plating', 'Tender_deck', (sx, sy, ZS), material_map={'Deck_Grey': DECK})
stripe = dup('Sheer_stripe', 'Tender_sheer', (sx * 1.002, sy, ZS), material_map={'CMA_Red': STRIPE})
for nm in ('Rubbing_strake_lower', 'Rubbing_strake_upper'): dup(nm, 'Tender_' + nm, (sx * 1.0, sy, ZS))
DZ = 4.51 * ZS                              # deck top
for nm in ('Accommodation', 'Accommodation_roof_kit', 'Accommodation_windows', 'Wheelhouse_cabin', 'Wheelhouse_frames',
           'Wheelhouse_glass', 'Wheelhouse_interior', 'Wheelhouse_lift', 'Radar_mast', 'Stern_masts', 'Lifebuoy', 'Lifebuoy.001'):
    o = dup(nm, 'Tender_' + nm.replace('.', '_'), (0.86, 0.9, 0.9), dz=DZ - 3.8 * 0.9)
gr = dup('Guardrail', 'Tender_guardrail', (sx, sy, 1.0)); gr.location.z = DZ - 4.06
grs = dup('Guardrail_stanchions', 'Tender_stanchions', (sx, sy, 1.0)); grs.location.z = DZ - 3.6

# walkways along both sides in green non slip paint, and lashing rails for the battery racks
box('Tender_walkway_P', (-W / 2 + 0.45, 34, DZ + 0.006), (0.7, 30, 0.012), NONSLIP, bev=0)
box('Tender_walkway_S', (W / 2 - 0.45, 34, DZ + 0.006), (0.7, 30, 0.012), NONSLIP, bev=0)

# fenders: a row of black cylindrical fenders hung on chains along both sides
for side in (-1, 1):
    for i in range(6):
        y = 15 + i * 6.6
        f = cyl('Tender_fender', (side * (W / 2 + 0.38), y, 2.45), 0.38, 1.9, RUBBER, rot=(0, 0, 0), v=20)
        f.rotation_euler = (0, 0, 0)
        cyl('Tender_fender_chain', (side * (W / 2 + 0.22), y, 3.75), 0.025, 0.8, STEEL, v=6)

# bollards and mooring winches
for y in (3.5, 17.5, 36.5, 50.5):
    for side in (-1, 1):
        x = side * (W / 2 - 0.75)
        for dy in (-0.35, 0.35):
            cyl('Tender_bollard', (x, y + dy, DZ + 0.35), 0.17, 0.7, DARK, v=20)
            cyl('Tender_bollard_cap', (x, y + dy, DZ + 0.72), 0.22, 0.06, DARK, v=20)
        box('Tender_bollard_base', (x, y, DZ + 0.05), (0.6, 1.2, 0.1), DARK)
for y in (53.0,):
    box('Tender_winch_base', (0, y, DZ + 0.3), (2.6, 1.4, 0.6), DARK)
    cyl('Tender_winch_drum', (0, y, DZ + 0.95), 0.45, 1.6, RED, rot=(0, math.pi / 2, 0))
box('Tender_bow_mast', (0, 54.6, DZ + 2.6), (0.18, 0.18, 5.2), STEEL, bev=0.02)
cyl('Tender_bow_light', (0, 54.6, DZ + 5.3), 0.12, 0.25, mat('NavLight', (1, 1, 1), emit=(1, 0.95, 0.85), es=3))

# ------------------------------------------------------------------ battery racks: 3 abreast by 8 rows, row 4 holds the crane
ys = [20.0 + i * 6.6 for i in range(5)]
XS = (-2.66, 0.0, 2.66)
CR = 2                                   # crane row
for i, y in enumerate(ys):
    if i == CR: continue
    for dy in (-3.05, 3.05):
        box('Tender_rack_rail', (0, y + dy, DZ + 0.09), (8.4, 0.3, 0.18), DARK)
    for j, x in enumerate(XS):
        empty('Slot_r%d_c%d' % (i, j), (x, y, DZ + 0.18))
        empty('Slot_r%d_c%d_u' % (i, j), (x, y, DZ + 0.18 + 2.6))
        for cxy in ((-1.15, -3.0), (1.15, -3.0), (-1.15, 3.0), (1.15, 3.0)):
            box('Tender_twistlock', (x + cxy[0], y + cxy[1], DZ + 0.2), (0.2, 0.2, 0.06), STEEL, bev=0.01)
# power cabinets that connect each rack to the tender's busbar (orange plug sockets)
for i, y in enumerate(ys):
    if i == CR: continue
    for side in (-1, 1):
        box('Tender_rack_socket', (side * 4.05, y, DZ + 0.55), (0.35, 0.9, 1.0), STEEL, bev=0.02)
        box('Tender_rack_plug', (side * 3.86, y, DZ + 0.7), (0.06, 0.4, 0.25), ORANGE, bev=0.005)

# one battery pack (the same 20 ft unit the cargo e-barge carries), cloned into the slots by the page
b = src['Battery_A'].copy(); b.data = src['Battery_A'].data; b.name = 'Pack_proto'; link(b)
b.parent = None; b.matrix_world = mathutils.Matrix.Identity(4); b.location = (0, -60, 0)   # parked out of view in the file
l = src['Battery_A_LED'].copy(); l.data = src['Battery_A_LED'].data; l.name = 'Pack_proto_LED'; link(l)
l.parent = b; l.matrix_parent_inverse = mathutils.Matrix.Identity(4); l.location = (0, 0, 0)

# ------------------------------------------------------------------ knuckle boom crane on a pedestal
PY = ys[CR]
cyl('Crane_pedestal', (0, PY, DZ + 1.6), 1.05, 3.2, YEL, v=48, bev=0.02)
cyl('Crane_pedestal_flange', (0, PY, DZ + 0.12), 1.35, 0.24, YEL, v=48)
for k in range(12):
    a = k / 12 * math.tau
    box('Crane_pedestal_gusset', (math.cos(a) * 1.18, PY + math.sin(a) * 1.18, DZ + 0.6), (0.06, 0.32, 0.95), YEL, bev=0.005, rot=(0, 0, a + math.pi / 2))
cyl('Crane_slew_ring', (0, PY, DZ + 3.32), 1.2, 0.24, DARK, v=48)
SZ = DZ + 3.44
slew = empty('Crane_slew', (0, PY, SZ))
cyl('Crane_column', (0, PY, SZ + 1.3), 0.85, 2.6, YEL, parent=slew, v=40, bev=0.02)
box('Crane_column_head', (0, PY + 0.2, SZ + 2.75), (1.5, 1.9, 0.9), YEL, parent=slew, bev=0.08)
box('Crane_hyd_tank', (0.0, PY - 1.05, SZ + 0.75), (1.3, 0.7, 1.3), YEL, parent=slew, bev=0.06)
box('Crane_valve_bank', (-1.05, PY - 0.2, SZ + 0.9), (0.4, 1.2, 0.8), DARK, parent=slew, bev=0.03)
# operator cab on the starboard side, facing outboard and forward
box('Crane_cab', (1.55, PY + 0.35, SZ + 1.45), (1.15, 1.45, 2.0), YEL, parent=slew, bev=0.1)
box('Crane_cab_glass_f', (1.55, PY + 1.08, SZ + 1.75), (0.95, 0.03, 1.1), GLASS, parent=slew, bev=0)
box('Crane_cab_glass_s', (2.13, PY + 0.35, SZ + 1.75), (0.03, 1.2, 1.1), GLASS, parent=slew, bev=0)
box('Crane_cab_roofrail', (1.55, PY + 0.35, SZ + 2.5), (1.2, 1.5, 0.06), DARK, parent=slew, bev=0)
box('Crane_ladder', (0.0, PY - 0.95, SZ - 1.5), (0.5, 0.06, 3.0), STEEL, bev=0)
cyl('Crane_beacon', (-0.55, PY - 0.6, SZ + 3.3), 0.09, 0.22, mat('Beacon', (1, 0.5, 0), emit=(1, 0.45, 0), es=6), parent=slew, v=12)

HZ = SZ + 3.0                 # boom heel pin height
boom = empty('Crane_boom', (0, PY + 0.55, HZ), parent=slew)
LB, LJ = 10.0, 9.0
o = tapered('Crane_boom_beam', LB, 0.85, 1.05, 0.65, 0.75, YEL); o.location = (0, PY + 0.55, HZ); setp(o, boom)
cyl('Crane_boom_pin', (0, PY + 0.55, HZ), 0.32, 1.2, CHROME, parent=boom, rot=(0, math.pi / 2, 0), v=24)
for sxp in (-0.46, 0.46):
    box('Crane_boom_cheek', (sxp, PY + 0.55 + LB - 0.2, HZ), (0.08, 1.2, 1.2), YEL, parent=boom, bev=0.04)
box('Crane_boom_hose', (0.2, PY + 0.55 + LB / 2, HZ + 0.42), (0.12, LB - 1.0, 0.08), DARK, parent=boom, bev=0.02)
jib = empty('Crane_jib', (0, PY + 0.55 + LB, HZ), parent=boom)
cyl('Crane_knuckle_pin', (0, PY + 0.55 + LB, HZ), 0.28, 1.05, CHROME, parent=jib, rot=(0, math.pi / 2, 0), v=24)
o = tapered('Crane_jib_beam', LJ * 0.62, 0.62, 0.72, 0.5, 0.58, YEL); o.location = (0, PY + 0.55 + LB, HZ); setp(o, jib)
o = tapered('Crane_jib_ext', LJ * 0.42, 0.44, 0.5, 0.38, 0.42, YEL); o.location = (0, PY + 0.55 + LB + LJ * 0.58, HZ); setp(o, jib)
box('Crane_jib_head', (0, PY + 0.55 + LB + LJ - 0.15, HZ - 0.1), (0.5, 0.5, 0.65), DARK, parent=jib, bev=0.05)
cyl('Crane_sheave', (0, PY + 0.55 + LB + LJ - 0.05, HZ - 0.35), 0.28, 0.18, CHROME, parent=jib, rot=(0, math.pi / 2, 0), v=24)
box('Crane_winch', (0, PY + 0.55 + LB + 1.2, HZ + 0.55), (0.55, 1.1, 0.45), DARK, parent=jib, bev=0.04)
tip = empty('Crane_tip', (0, PY + 0.55 + LB + LJ - 0.05, HZ - 0.65), parent=jib)
# hydraulic rams: barrel and rod modelled along -Y from their base pin (three.js lookAt aims +Z, which is -Y here)
def ram(nm, base, anchor_parent, anchor, r_barrel, r_rod, L_barrel, parent):
    e = empty(nm, base, parent=parent)
    o = cyl(nm + '_barrel', (base[0], base[1] - L_barrel / 2, base[2]), r_barrel, L_barrel, YEL, rot=(math.pi / 2, 0, 0), v=24)
    setp(o, e)
    rod = empty(nm + '_rod', base, parent=e)
    o2 = cyl(nm + '_rodmesh', (base[0], base[1] - 0.5, base[2]), r_rod, 1.0, CHROME, rot=(math.pi / 2, 0, 0), v=20)
    setp(o2, rod)
    empty(nm + '_anchor', anchor, parent=anchor_parent)
ram('Crane_ram1', (0, PY + 0.7, SZ + 0.9), boom, (0, PY + 0.55 + 5.2, HZ - 0.6), 0.22, 0.13, 3.0, slew)
ram('Crane_ram2', (0, PY + 0.55 + LB - 4.6, HZ + 0.62), jib, (0, PY + 0.55 + LB + 1.0, HZ + 0.45), 0.17, 0.1, 2.6, boom)

# spreader for 20 ft containers: frame, corner twistlocks, guide flippers and four slings to a master link
sp = empty('Crane_spreader', (0, 0, 0))
SPZ = 0.0   # origin at the master link; frame hangs 2.2 m below
fz = -2.25
box('Spreader_frame_long_P', (-1.1, 0, fz), (0.22, 6.0, 0.32), YEL, parent=sp, bev=0.03)
box('Spreader_frame_long_S', (1.1, 0, fz), (0.22, 6.0, 0.32), YEL, parent=sp, bev=0.03)
for y in (-2.9, 0.0, 2.9): box('Spreader_frame_cross', (0, y, fz), (2.4, 0.22, 0.28), YEL, parent=sp, bev=0.03)
for (cx, cy) in ((-1.1, -2.95), (1.1, -2.95), (-1.1, 2.95), (1.1, 2.95)):
    box('Spreader_corner', (cx, cy, fz - 0.12), (0.34, 0.34, 0.3), DARK, parent=sp, bev=0.03)
    box('Spreader_flipper', (cx * 1.08, cy * 1.02, fz - 0.45), (0.06, 0.4, 0.5), YEL, parent=sp, bev=0.01)
    a = V((cx, cy * 0.98, fz + 0.15)); bpt = V((0, 0, -0.25)); d = bpt - a
    o = cyl('Spreader_sling', tuple((a + bpt) / 2), 0.025, d.length, WIRE, v=8)
    o.rotation_euler = d.to_track_quat('Z', 'Y').to_euler(); setp(o, sp)
bpy.ops.mesh.primitive_torus_add(major_radius=0.18, minor_radius=0.045, location=(0, 0, -0.15), rotation=(math.pi / 2, 0, 0))
o = bpy.context.object; o.name = 'Spreader_master_link'; o.data.materials.append(STEEL); setp(o, sp)
box('Spreader_hook', (0, 0, 0.12), (0.18, 0.12, 0.42), DARK, parent=sp, bev=0.03)
sp.location = (-9.0, PY, DZ + 8.0)    # rest position for the file preview; the page places it under the tip

# rest support for the folded boom, on the aft rack row
box('Crane_boom_rest', (0, ys[1] - 0.2, DZ + 3.6), (0.3, 0.3, 7.2), YEL, bev=0.02)
box('Crane_boom_rest_saddle', (0, ys[1] - 0.2, DZ + 7.25), (1.1, 0.6, 0.15), DARK, bev=0.02)

# ------------------------------------------------------------------ clean up and export
keep = set(o.name for o in bpy.context.scene.objects if not o.name in src or o.name.startswith('Tender_'))
for o in list(bpy.context.scene.objects):
    if o.name in src and o.name not in ('Pack_proto',) and not o.name.startswith('Tender_'):
        bpy.data.objects.remove(o, do_unlink=True)
root = empty('Tender', (0, 0, 0))
for o in list(bpy.context.scene.objects):
    if o.parent is None and o is not root: setp(o, root)
bpy.ops.object.select_all(action='DESELECT')
bpy.ops.export_scene.gltf(filepath=out, export_format='GLB', export_apply=True, export_yup=True, export_extras=False)
print('exported', out, len(bpy.context.scene.objects))
