"""Opus 5.5 Minecraft held-item hero props (runs inside Blender, UnrealMinecraft.blend).

Sent by Tools/BlenderMCP/run_items.py through the blender_unreal MCP server (port 9878,
BLENDER_MCP_SAFE_MODE=1), so it only uses bpy / bmesh / math and no lambdas or classes.

Every prop is authored in Unreal item space, the same frame the runtime uses for the
extruded item sprites (UMCItemVisualComponent): X right, Z up, Y = thickness, centred on
the origin, 1 unit = 1 block, laid along the sprite diagonal (handle bottom-left, business
end top-right).  That way the held / dropped / item-frame / mob-held transforms apply to
sprites and authored props alike.  Blender (x, y, z) arrives in Unreal as (100x, -100y, 100z).

Material slots: "MI_Metal" (tool heads, fittings: metallic at runtime) and "MI_Grip"
(wood, leather, string).  Vertex colour rgb = linear albedo; alpha 1 = the tier tint
applies (wooden..netherite heads share one mesh), alpha 0 = fixed colour.

Output: //Saved/Opus55Fbx/Items/SM_<Prop>.fbx  ->  /Game/Opus55Minecraft/Items/SM_<Prop>
"""
import bpy
import bmesh
import math

EXPORT_DIR = "//Saved/Opus55Fbx/Items"
SHEET_Y = -84.0          # model-sheet row for the props (below the rig sheet)
R2 = 0.70710678
AXIS_D = (R2, 0.0, R2)   # along the item (handle -> head)
AXIS_P = (-R2, 0.0, R2)  # across the item, in the sprite plane
AXIS_W = (0.0, 1.0, 0.0) # thickness

HEAD = (0.86, 0.86, 0.88)      # tinted per tier at runtime (alpha 1)
WOOD = 0x8A6440
WOOD_DARK = 0x5E4128
LEATHER = 0x4A3223
LEATHER_LIGHT = 0x6B4A33
IRON = 0x8E8E96
IRON_DARK = 0x55555C


# ---------------------------------------------------------------- small vector / colour helpers

def hexc(h):
    return (((h >> 16) & 255) / 255.0, ((h >> 8) & 255) / 255.0, (h & 255) / 255.0)


def lin(c):
    out = []
    for v in c:
        if v <= 0.04045:
            out.append(v / 12.92)
        else:
            out.append(((v + 0.055) / 1.055) ** 2.4)
    return (out[0], out[1], out[2])


def shade(c, f):
    return (min(1.0, c[0] * f), min(1.0, c[1] * f), min(1.0, c[2] * f))


def at(s, t, w):
    """Point in item space from (along, across, thickness) coordinates."""
    return (AXIS_D[0] * s + AXIS_P[0] * t + AXIS_W[0] * w,
            AXIS_D[1] * s + AXIS_P[1] * t + AXIS_W[1] * w,
            AXIS_D[2] * s + AXIS_P[2] * t + AXIS_W[2] * w)


def xyz(x, y, z):
    return (x, y, z)


def ue_to_bl(p):
    return (p[0], -p[1], p[2])


def grain(i, seed):
    h = (i * 73856093 + seed * 19349663) & 0xFFFF
    return 0.92 + (h % 17) / 100.0


# ---------------------------------------------------------------- mesh builder

def new_builder():
    bm = bmesh.new()
    layer = bm.loops.layers.float_color.new("Color")
    return [bm, layer]


def loft(b, rings, colors, alpha, mat, smooth, edge=None):
    """Closed tube through the rings (all the same vertex count, or 1 = a point / apex).
    colors: one sRGB colour per ring.  edge: optional per-ring-vertex brightness factors."""
    bm = b[0]
    layer = b[1]
    vrings = []
    for ring in rings:
        vr = []
        for p in ring:
            vr.append(bm.verts.new(ue_to_bl(p)))
        vrings.append(vr)
    faces = []
    loop_info = []
    n = 0
    for vr in vrings:
        n = max(n, len(vr))
    for i in range(len(vrings) - 1):
        a = vrings[i]
        c = vrings[i + 1]
        for k in range(n):
            k2 = (k + 1) % n
            if len(a) == 1:
                quad = [a[0], c[k2], c[k]]
                info = [(i, 0), (i + 1, k2), (i + 1, k)]
            elif len(c) == 1:
                quad = [a[k], a[k2], c[0]]
                info = [(i, k), (i, k2), (i + 1, 0)]
            else:
                quad = [a[k], a[k2], c[k2], c[k]]
                info = [(i, k), (i, k2), (i + 1, k2), (i + 1, k)]
            f = bm.faces.new(quad)
            faces.append(f)
            loop_info.append(info)
    if len(vrings[0]) > 2:
        f = bm.faces.new(vrings[0])
        faces.append(f)
        info = []
        for k in range(len(vrings[0])):
            info.append((0, k))
        loop_info.append(info)
    last = len(vrings) - 1
    if len(vrings[last]) > 2:
        f = bm.faces.new(list(reversed(vrings[last])))
        faces.append(f)
        info = []
        for k in reversed(range(len(vrings[last]))):
            info.append((last, k))
        loop_info.append(info)
    bmesh.ops.recalc_face_normals(bm, faces=faces)
    for fi in range(len(faces)):
        f = faces[fi]
        f.material_index = mat
        f.smooth = smooth
        verts_of_face = [lv.vert for lv in f.loops]
        info = loop_info[fi]
        for lp in f.loops:
            ring_i = 0
            vert_k = 0
            for j in range(len(info)):
                ri, rk = info[j]
                if vrings[ri][rk] == lp.vert:
                    ring_i = ri
                    vert_k = rk
            col = colors[min(ring_i, len(colors) - 1)]
            if edge is not None and len(vrings[ring_i]) > 1:
                col = shade(col, edge[vert_k % len(edge)])
            lc = lin(col)
            lp[layer] = (lc[0], lc[1], lc[2], alpha)
        verts_of_face = None
    return faces


def ring_circle(center_s, center_t, center_w, radius, segs):
    out = []
    for k in range(segs):
        a = 2.0 * math.pi * k / segs
        out.append(at(center_s, center_t + math.cos(a) * radius, center_w + math.sin(a) * radius))
    return out


def ring_rect_s(s, t0, t1, w):
    """Rectangle in the (P, W) plane at station s (tube running along D)."""
    return [at(s, t1, w), at(s, t0, w), at(s, t0, -w), at(s, t1, -w)]


def ring_rect_t(t, s0, s1, w):
    """Rectangle in the (D, W) plane at station t (tube running along P)."""
    return [at(s1, t, w), at(s0, t, w), at(s0, t, -w), at(s1, t, -w)]


def ring_diamond_s(s, t_center, half_width, half_thick):
    return [at(s, t_center + half_width, 0.0), at(s, t_center, half_thick), at(s, t_center - half_width, 0.0), at(s, t_center, -half_thick)]


def box(b, s0, s1, t0, t1, w, col, alpha, mat, smooth=False):
    return loft(b, [ring_rect_s(s0, t0, t1, w), ring_rect_s(s1, t0, t1, w)], [col, shade(col, 1.06)], alpha, mat, smooth)


def handle(b, s0, s1, radius, seed, wrap_from=None, wrap_to=None, color=None):
    """Round wooden handle along D with grain variation and an optional leather wrap."""
    base = hexc(WOOD) if color is None else hexc(color)
    stations = []
    s = s0
    step = 0.06
    while s < s1 - 1e-4:
        stations.append(s)
        s += step
    stations.append(s1)
    rings = []
    cols = []
    for i in range(len(stations)):
        si = stations[i]
        r = radius
        col = shade(base, grain(i, seed))
        if wrap_from is not None and wrap_from <= si <= wrap_to:
            band = int((si - wrap_from) / 0.03) % 2
            r = radius * (1.12 if band == 0 else 1.04)
            col = hexc(LEATHER) if band == 0 else hexc(LEATHER_LIGHT)
        rings.append(ring_circle(si, 0.0, 0.0, r, 10))
        cols.append(col)
    # rounded butt end
    rings.insert(0, [at(s0 - radius * 0.35, 0.0, 0.0)])
    cols.insert(0, shade(base, 0.8))
    return loft(b, rings, cols, 0.0, 1, True)


def finish(b, name):
    bm = b[0]
    mesh = bpy.data.meshes.new(name)
    bm.to_mesh(mesh)
    bm.free()
    attr = mesh.color_attributes.get("Color")
    if attr is not None:
        mesh.color_attributes.active_color = attr
        mesh.color_attributes.render_color_index = mesh.color_attributes.active_color_index
    mesh.materials.append(material("MI_Metal", (0.75, 0.75, 0.78)))
    mesh.materials.append(material("MI_Grip", (0.45, 0.32, 0.2)))
    return mesh


def material(name, rgb):
    m = bpy.data.materials.get(name)
    if m is None:
        m = bpy.data.materials.new(name)
    m.diffuse_color = (rgb[0], rgb[1], rgb[2], 1.0)
    return m


# ---------------------------------------------------------------- props

def make_sword():
    b = new_builder()
    head = HEAD
    # pommel (tier metal) and grip
    loft(b, [[at(-0.64, 0.0, 0.0)], ring_circle(-0.62, 0.0, 0.0, 0.035, 10), ring_circle(-0.585, 0.0, 0.0, 0.046, 10),
             ring_circle(-0.55, 0.0, 0.0, 0.036, 10), ring_circle(-0.54, 0.0, 0.0, 0.024, 10)],
         [head, head, shade(head, 1.08), head, shade(head, 0.9)], 1.0, 0, True)
    handle(b, -0.545, -0.305, 0.026, 3, -0.545, -0.305)
    # crossguard: thicker in the middle, flared quillons
    loft(b, [ring_rect_t(-0.17, -0.305, -0.265, 0.018), ring_rect_t(-0.15, -0.31, -0.262, 0.024), ring_rect_t(-0.05, -0.305, -0.268, 0.03),
             ring_rect_t(0.05, -0.305, -0.268, 0.03), ring_rect_t(0.15, -0.31, -0.262, 0.024), ring_rect_t(0.17, -0.305, -0.265, 0.018)],
         [shade(head, 0.9), head, shade(head, 1.05), shade(head, 1.05), head, shade(head, 0.9)], 1.0, 0, False)
    # blade: diamond section, bright honed edges, tapering to the point
    stations = [(-0.265, 0.07, 0.019), (-0.2, 0.066, 0.018), (0.05, 0.061, 0.016), (0.3, 0.054, 0.013), (0.47, 0.043, 0.01), (0.57, 0.026, 0.007)]
    rings = []
    cols = []
    for st in stations:
        rings.append(ring_diamond_s(st[0], 0.0, st[1], st[2]))
        cols.append(head)
    rings.append([at(0.645, 0.0, 0.0)])
    cols.append(shade(head, 1.1))
    loft(b, rings, cols, 1.0, 0, False, [1.18, 0.9, 1.18, 0.9])
    # fuller: a darker inlaid groove on both faces
    for side in (1.0, -1.0):
        loft(b, [[at(-0.25, 0.012, 0.0165 * side), at(-0.25, -0.012, 0.0165 * side), at(-0.25, -0.012, 0.0185 * side), at(-0.25, 0.012, 0.0185 * side)],
                 [at(0.28, 0.008, 0.0125 * side), at(0.28, -0.008, 0.0125 * side), at(0.28, -0.008, 0.0145 * side), at(0.28, 0.008, 0.0145 * side)]],
             [shade(head, 0.72), shade(head, 0.8)], 1.0, 0, False)
    return finish(b, "SM_Sword")


def make_pickaxe():
    b = new_builder()
    head = HEAD
    handle(b, -0.62, 0.43, 0.03, 5, -0.6, -0.42)
    # socket collar around the top of the handle
    box(b, 0.335, 0.47, -0.058, 0.058, 0.045, shade(head, 0.92), 1.0, 0)
    # arched double pick: square section tapering to points
    stations = [-0.46, -0.36, -0.24, -0.12, 0.0, 0.12, 0.24, 0.36, 0.46]
    rings = [[at(0.40 - 0.2, -0.53, 0.0)]]
    cols = [shade(head, 1.12)]
    for t in stations:
        u = abs(t) / 0.53
        sc = 0.405 - 0.2 * u * u
        half = 0.052 * (1.0 - 0.72 * u)
        thick = 0.042 * (1.0 - 0.62 * u)
        rings.append(ring_rect_t(t, sc - half, sc + half, thick))
        cols.append(shade(head, 1.0 + 0.1 * u))
    rings.append([at(0.40 - 0.2, 0.53, 0.0)])
    cols.append(shade(head, 1.12))
    loft(b, rings, cols, 1.0, 0, False, [1.05, 0.95, 0.9, 1.0])
    return finish(b, "SM_Pickaxe")


def make_axe():
    b = new_builder()
    head = HEAD
    handle(b, -0.62, 0.5, 0.03, 7, -0.6, -0.44)
    # poll (back of the head) and the eye around the handle
    box(b, 0.3, 0.47, -0.1, 0.02, 0.046, shade(head, 0.9), 1.0, 0)
    # bearded blade: wedge along +P, flaring along D and thinning to the edge
    profile = [(0.02, 0.30, 0.47, 0.044), (0.08, 0.28, 0.48, 0.034), (0.15, 0.22, 0.52, 0.022), (0.22, 0.16, 0.56, 0.012), (0.27, 0.13, 0.585, 0.006), (0.3, 0.12, 0.6, 0.003)]
    rings = []
    cols = []
    for pr in profile:
        rings.append(ring_rect_t(pr[0], pr[1], pr[2], pr[3]))
        cols.append(shade(head, 1.0 + (pr[0] / 0.3) * 0.16))
    loft(b, rings, cols, 1.0, 0, False)
    return finish(b, "SM_Axe")


def make_shovel():
    b = new_builder()
    head = HEAD
    handle(b, -0.62, 0.2, 0.029, 9, -0.6, -0.44)
    # neck
    loft(b, [ring_circle(0.16, 0.0, 0.0, 0.034, 10), ring_circle(0.27, 0.0, 0.0, 0.028, 10)], [shade(head, 0.9), head], 1.0, 0, True)
    # dished spade: a curved plate lofted along D
    rows = [(0.25, 0.05), (0.3, 0.115), (0.4, 0.138), (0.5, 0.13), (0.57, 0.1), (0.62, 0.055), (0.645, 0.012)]
    rings = []
    cols = []
    for r in rows:
        s = r[0]
        hw = r[1]
        top = []
        bottom = []
        for j in range(5):
            u = -1.0 + j * 0.5
            dish = 0.022 * (1.0 - u * u)
            top.append(at(s, u * hw, dish + 0.008))
            bottom.append(at(s, u * hw, dish - 0.008))
        ring = top + list(reversed(bottom))
        rings.append(ring)
        cols.append(shade(head, 0.96 + 0.1 * (s - 0.25)))
    loft(b, rings, cols, 1.0, 0, False)
    return finish(b, "SM_Shovel")


def make_hoe():
    b = new_builder()
    head = HEAD
    handle(b, -0.62, 0.47, 0.029, 11, -0.6, -0.44)
    box(b, 0.39, 0.49, -0.05, 0.05, 0.04, shade(head, 0.92), 1.0, 0)
    # arm reaching across, then the blade dropping back along the handle
    box(b, 0.41, 0.475, 0.04, 0.25, 0.02, head, 1.0, 0)
    loft(b, [ring_rect_t(0.19, 0.2, 0.475, 0.016), ring_rect_t(0.235, 0.18, 0.475, 0.012), ring_rect_t(0.262, 0.17, 0.475, 0.004)],
         [head, shade(head, 1.08), shade(head, 1.16)], 1.0, 0, False)
    return finish(b, "SM_Hoe")


def make_spear():
    b = new_builder()
    head = HEAD
    handle(b, -0.66, 0.27, 0.025, 13, -0.2, -0.02)
    loft(b, [ring_circle(0.24, 0.0, 0.0, 0.034, 10), ring_circle(0.3, 0.0, 0.0, 0.031, 10), ring_circle(0.345, 0.0, 0.0, 0.022, 10)],
         [shade(head, 0.88), head, head], 1.0, 0, True)
    stations = [(0.34, 0.022, 0.013), (0.39, 0.058, 0.015), (0.46, 0.07, 0.014), (0.53, 0.058, 0.011), (0.6, 0.032, 0.008)]
    rings = []
    cols = []
    for st in stations:
        rings.append(ring_diamond_s(st[0], 0.0, st[1], st[2]))
        cols.append(head)
    rings.append([at(0.67, 0.0, 0.0)])
    cols.append(shade(head, 1.1))
    loft(b, rings, cols, 1.0, 0, False, [1.18, 0.9, 1.18, 0.9])
    return finish(b, "SM_Spear")


def make_mace():
    b = new_builder()
    rod = hexc(0xB9C7CC)
    iron = hexc(0x4B4B54)
    # breeze-rod handle with banding
    rings = []
    cols = []
    s = -0.56
    i = 0
    while s < 0.2:
        rings.append(ring_circle(s, 0.0, 0.0, 0.03 if i % 3 else 0.034, 10))
        cols.append(shade(rod, 0.85 if i % 3 == 0 else 1.0))
        s += 0.05
        i += 1
    rings.insert(0, [at(-0.58, 0.0, 0.0)])
    cols.insert(0, shade(rod, 0.8))
    loft(b, rings, cols, 0.0, 1, True)
    # heavy head: octagonal core, four flanges, a crown spike
    loft(b, [ring_circle(0.2, 0.0, 0.0, 0.05, 8), ring_circle(0.25, 0.0, 0.0, 0.11, 8), ring_circle(0.42, 0.0, 0.0, 0.11, 8), ring_circle(0.47, 0.0, 0.0, 0.06, 8)],
         [iron, shade(iron, 1.1), shade(iron, 1.1), iron], 0.0, 0, False)
    for k in range(4):
        a = k * math.pi * 0.5 + math.pi * 0.25
        ct = math.cos(a)
        cw = math.sin(a)
        ring0 = [at(0.24, ct * 0.1 - cw * 0.012, cw * 0.1 + ct * 0.012), at(0.24, ct * 0.1 + cw * 0.012, cw * 0.1 - ct * 0.012),
                 at(0.24, ct * 0.18 + cw * 0.008, cw * 0.18 - ct * 0.008), at(0.24, ct * 0.18 - cw * 0.008, cw * 0.18 + ct * 0.008)]
        ring1 = [at(0.43, ct * 0.1 - cw * 0.012, cw * 0.1 + ct * 0.012), at(0.43, ct * 0.1 + cw * 0.012, cw * 0.1 - ct * 0.012),
                 at(0.43, ct * 0.16 + cw * 0.008, cw * 0.16 - ct * 0.008), at(0.43, ct * 0.16 - cw * 0.008, cw * 0.16 + ct * 0.008)]
        loft(b, [ring0, ring1], [shade(iron, 1.2), shade(iron, 1.3)], 0.0, 0, False)
    loft(b, [ring_circle(0.47, 0.0, 0.0, 0.035, 8), [at(0.56, 0.0, 0.0)]], [shade(iron, 1.15), shade(iron, 1.3)], 0.0, 0, False)
    return finish(b, "SM_Mace")


def make_trident():
    b = new_builder()
    shaft = hexc(0x2F7F73)
    metal = hexc(0x4FB8A9)
    rings = []
    cols = []
    s = -0.66
    i = 0
    while s < 0.27:
        rings.append(ring_circle(s, 0.0, 0.0, 0.022 if i % 4 else 0.027, 8))
        cols.append(shade(shaft, 1.0 if i % 4 else 1.25))
        s += 0.05
        i += 1
    rings.insert(0, [at(-0.68, 0.0, 0.0)])
    cols.insert(0, shade(shaft, 0.8))
    loft(b, rings, cols, 0.0, 0, True)
    # crossbar and the three prongs
    loft(b, [ring_rect_t(-0.14, 0.255, 0.305, 0.018), ring_rect_t(0.14, 0.255, 0.305, 0.018)], [metal, metal], 0.0, 0, False)
    for side in (-1.0, 0.0, 1.0):
        t0 = side * 0.12
        top = 0.64 if side == 0.0 else 0.55
        rings = [ring_diamond_s(0.3, t0, 0.02, 0.016), ring_diamond_s((0.3 + top) * 0.5, t0 * 0.92, 0.017, 0.013), ring_diamond_s(top - 0.04, t0 * 0.86, 0.013, 0.01),
                 [at(top + 0.03, t0 * 0.82, 0.0)]]
        loft(b, rings, [metal, shade(metal, 1.1), shade(metal, 1.15), shade(metal, 1.3)], 0.0, 0, False, [1.15, 0.9, 1.15, 0.9])
        if side != 0.0:
            # barb on the outer side of each side prong
            loft(b, [ring_diamond_s(top - 0.12, t0 * 0.9 + side * 0.012, 0.01, 0.008), [at(top - 0.16, t0 * 0.9 + side * 0.055, 0.0)]],
                 [metal, shade(metal, 1.2)], 0.0, 0, False)
    return finish(b, "SM_Trident")


def make_bow():
    b = new_builder()
    wood = hexc(0x7A5130)
    stations = []
    s = -0.5
    while s <= 0.5001:
        stations.append(s)
        s += 0.0625
    rings = []
    cols = []
    for si in stations:
        u = si / 0.5
        tc = 0.1 - 0.26 * u * u
        half_t = 0.027 * (1.0 - 0.45 * abs(u))
        half_w = 0.022 * (1.0 - 0.3 * abs(u))
        grip = abs(si) < 0.08
        if grip:
            half_t *= 1.25
            half_w *= 1.3
        rings.append(ring_rect_s(si, tc - half_t, tc + half_t, half_w))
        cols.append(hexc(LEATHER) if grip else shade(wood, grain(int(si * 100), 17) * (0.8 if abs(u) > 0.92 else 1.0)))
    loft(b, rings, cols, 0.0, 1, False)
    # string between the nocks
    tip_t = 0.1 - 0.26
    loft(b, [ring_rect_s(-0.49, tip_t - 0.004, tip_t + 0.004, 0.004), ring_rect_s(0.49, tip_t - 0.004, tip_t + 0.004, 0.004)],
         [hexc(0xE9E4D6), hexc(0xE9E4D6)], 0.0, 1, False)
    return finish(b, "SM_Bow")


def make_crossbow():
    b = new_builder()
    wood = hexc(0x6B4A2B)
    iron = hexc(IRON)
    # stock with a flared butt
    loft(b, [ring_rect_s(-0.56, -0.06, 0.05, 0.04), ring_rect_s(-0.42, -0.05, 0.045, 0.036), ring_rect_s(-0.1, -0.036, 0.036, 0.032), ring_rect_s(0.4, -0.03, 0.03, 0.03)],
         [shade(wood, 0.85), wood, shade(wood, 1.05), wood], 0.0, 1, False)
    # iron fittings, trigger and latch
    box(b, 0.26, 0.33, -0.036, 0.036, 0.036, iron, 0.0, 0)
    box(b, -0.14, -0.08, -0.09, -0.036, 0.012, hexc(IRON_DARK), 0.0, 0)
    box(b, 0.0, 0.05, 0.03, 0.05, 0.02, hexc(IRON_DARK), 0.0, 0)
    # prod (limbs), curving back towards the tips
    rings = []
    cols = []
    for k in range(9):
        t = -0.4 + k * 0.1
        u = t / 0.4
        sc = 0.36 - 0.13 * u * u
        rings.append(ring_rect_t(t, sc - 0.022, sc + 0.022, 0.026 * (1.0 - 0.35 * abs(u))))
        cols.append(shade(hexc(0x5A4632), 1.0 + 0.1 * abs(u)))
    loft(b, rings, cols, 0.0, 0, False)
    # drawn string from the prod tips to the latch
    for side in (-1.0, 1.0):
        loft(b, [ring_rect_s(0.23, side * 0.395 - 0.004, side * 0.395 + 0.004, 0.004), ring_rect_s(0.03, side * 0.01 - 0.004, side * 0.01 + 0.004, 0.004)],
             [hexc(0xE9E4D6), hexc(0xE9E4D6)], 0.0, 1, False)
    return finish(b, "SM_Crossbow")


def make_shield():
    b = new_builder()
    plank = hexc(0xA07A4C)
    iron = hexc(0x7C7C85)
    # upright board (faces -Y like the sprite), three vertical planks
    for k in range(3):
        x0 = -0.3 + k * 0.2
        x1 = x0 + 0.2 - 0.004
        col = shade(plank, (0.94, 1.02, 0.97)[k])
        loft(b, [[xyz(x1, -0.03, -0.46), xyz(x0, -0.03, -0.46), xyz(x0, 0.03, -0.46), xyz(x1, 0.03, -0.46)],
                 [xyz(x1, -0.03, 0.46), xyz(x0, -0.03, 0.46), xyz(x0, 0.03, 0.46), xyz(x1, 0.03, 0.46)]],
             [shade(col, 0.95), col], 0.0, 1, False)
    # iron rim (left, right, top, bottom)
    rim = [(-0.33, -0.3, -0.49, 0.49), (0.3, 0.33, -0.49, 0.49), (-0.3, 0.3, 0.46, 0.49), (-0.3, 0.3, -0.49, -0.46)]
    for r in rim:
        loft(b, [[xyz(r[1], -0.04, r[2]), xyz(r[0], -0.04, r[2]), xyz(r[0], 0.04, r[2]), xyz(r[1], 0.04, r[2])],
                 [xyz(r[1], -0.04, r[3]), xyz(r[0], -0.04, r[3]), xyz(r[0], 0.04, r[3]), xyz(r[1], 0.04, r[3])]],
             [iron, shade(iron, 1.08)], 0.0, 0, False)
    # central boss on the front face
    rings = []
    cols = []
    for k in range(4):
        rr = (0.1, 0.095, 0.075, 0.04)[k]
        yy = (-0.03, -0.05, -0.068, -0.08)[k]
        ring = []
        for j in range(10):
            a = 2.0 * math.pi * j / 10
            ring.append(xyz(math.cos(a) * rr, yy, math.sin(a) * rr))
        rings.append(ring)
        cols.append(shade(iron, 1.0 + 0.08 * k))
    rings.append([xyz(0.0, -0.085, 0.0)])
    cols.append(shade(iron, 1.3))
    loft(b, rings, cols, 0.0, 0, True)
    return finish(b, "SM_Shield")


def make_fishing_rod():
    b = new_builder()
    cane = hexc(0x8C6B3E)
    rings = []
    cols = []
    for k in range(13):
        s = -0.6 + k * 0.1
        u = (s + 0.6) / 1.2
        t = -0.08 * u * u
        r = 0.02 * (1.0 - 0.62 * u)
        cork = s < -0.32
        if cork:
            r = 0.027
        rings.append(ring_circle(s, t, 0.0, r, 8))
        cols.append(hexc(0xB38E5B) if cork else shade(cane, 0.9 + 0.2 * (k % 2)))
    rings.insert(0, [at(-0.62, 0.0, 0.0)])
    cols.insert(0, hexc(0x8F6F48))
    loft(b, rings, cols, 0.0, 1, True)
    # reel and the line hanging from the tip
    ring0 = []
    ring1 = []
    for j in range(10):
        a = 2.0 * math.pi * j / 10
        ring0.append(at(-0.27 + math.cos(a) * 0.04, -0.05 + math.sin(a) * 0.04, -0.02))
        ring1.append(at(-0.27 + math.cos(a) * 0.04, -0.05 + math.sin(a) * 0.04, 0.02))
    loft(b, [ring0, ring1], [hexc(0x6E6E78), hexc(0x8A8A95)], 0.0, 0, True)
    tip_t = -0.08
    loft(b, [ring_rect_s(0.6, tip_t - 0.003, tip_t + 0.003, 0.003), ring_rect_s(0.42, tip_t - 0.3 - 0.003, tip_t - 0.3 + 0.003, 0.003)],
         [hexc(0xDADADA), hexc(0xDADADA)], 0.0, 1, False)
    return finish(b, "SM_FishingRod")


# ---------------------------------------------------------------- export

def export_object(obj, path):
    bpy.ops.object.select_all(action='DESELECT')
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.export_scene.fbx(
        filepath=path, use_selection=True, apply_unit_scale=True, global_scale=1.0,
        object_types={'MESH'}, mesh_smooth_type='FACE', use_mesh_modifiers=False,
        path_mode='AUTO', embed_textures=False, colors_type='LINEAR', add_leaf_bones=False)


def items_collection():
    c = bpy.data.collections.get("Opus55_Items")
    if c is None:
        c = bpy.data.collections.new("Opus55_Items")
        bpy.context.scene.collection.children.link(c)
    for obj in list(c.objects):
        bpy.data.objects.remove(obj, do_unlink=True)
    return c


PROP_COUNT = 12


def make_prop(k):
    # explicit dispatch: Safe Mode only accepts calls to plain names
    if k == 0:
        return make_sword()
    if k == 1:
        return make_pickaxe()
    if k == 2:
        return make_axe()
    if k == 3:
        return make_shovel()
    if k == 4:
        return make_hoe()
    if k == 5:
        return make_spear()
    if k == 6:
        return make_mace()
    if k == 7:
        return make_trident()
    if k == 8:
        return make_bow()
    if k == 9:
        return make_crossbow()
    if k == 10:
        return make_shield()
    return make_fishing_rod()


def main():
    coll = items_collection()
    out_dir = bpy.path.abspath(EXPORT_DIR)
    written = 0
    for k in range(PROP_COUNT):
        mesh = make_prop(k)
        obj = bpy.data.objects.new(mesh.name, mesh)
        coll.objects.link(obj)
        obj.location = (0.0, 0.0, 0.0)
        export_object(obj, "%s/%s.fbx" % (out_dir, mesh.name))
        # model sheet row
        obj.location = ((k % 6) * 1.6, SHEET_Y - (k // 6) * 1.6, 0.8)
        written += 1
    bpy.ops.wm.save_as_mainfile(filepath=bpy.data.filepath)
    print("OPUS55_ITEMS props=%d" % written)


main()
