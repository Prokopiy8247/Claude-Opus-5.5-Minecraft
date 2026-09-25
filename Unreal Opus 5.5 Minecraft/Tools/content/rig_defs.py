"""Single source of truth for the mob / player rigs.

UMCRigComponent attaches every part pivot to its parent part's pivot and places it
with SetRelativeLocation(Pivot * BlockSize) (MCRig.cpp, Build() and ApplyPose()), so
the pivots here are PARENT-RELATIVE.  Boxes are relative to the part's own pivot.

Two consumers are generated from this file by Tools/content/gen_rigs.py:
    Source/Unreal_Minecraft/Render/MCRigTable.inl   C++ rig registry (MCRigs::Init)
    Tools/BlenderMCP/rig_table.py                   part table for the Blender builder

Units are blocks, entity space: origin at the feet, +X forward, +Z up.  The tables
below were laid out with the creature's right side on -Y; build_all() mirrors every
rig so that "right_*" parts end up on +Y, which is the right-hand side in Unreal's
left-handed frame (the side MCRig attaches held items to).  Colours are 0xRRGGBB.
"""
import math

BODY, HEAD, JAW, NECK = "Body", "Head", "Jaw", "Neck"
ARM_R, ARM_L = "ArmR", "ArmL"
LEG_FR, LEG_FL, LEG_BR, LEG_BL, LEG_MR, LEG_ML = "LegFR", "LegFL", "LegBR", "LegBL", "LegMR", "LegML"
TAIL, WING_R, WING_L, FIN_R, FIN_L = "Tail", "WingR", "WingL", "FinR", "FinL"
ROD, MISC, SHELL, EAR_L, EAR_R = "Rod", "Misc", "Shell", "EarL", "EarR"


class Part(object):
    def __init__(self, name, parent, role, pivot, box_min, box_max, color, index, emissive):
        self.name, self.parent, self.role = name, parent, role
        self.pivot, self.box_min, self.box_max = pivot, box_min, box_max
        self.color, self.index, self.emissive = color, index, emissive


class Rig(object):
    def __init__(self, name):
        self.name = name
        self.parts = []
        self.head = -1
        self.right_arm = -1
        self.left_arm = -1
        self.hand_offset = (0.0, 0.0, 0.0)
        self.scale = 1.0

    def part(self, name, parent, role, pivot, box_min, box_max, color=0xB3B3B3, index=0, emissive=False):
        self.parts.append(Part(name, parent, role, pivot, box_min, box_max, color, index, emissive))
        return len(self.parts) - 1

    def absolute_pivot(self, i):
        """Entity-space pivot (only needed for previews: the game uses the relative one)."""
        p = self.parts[i]
        x, y, z = p.pivot
        while p.parent >= 0:
            p = self.parts[p.parent]
            x, y, z = x + p.pivot[0], y + p.pivot[1], z + p.pivot[2]
        return (x, y, z)


def mirror_y(r):
    """Reflect a rig across the XZ plane (pivots and boxes), keeping boxes min <= max."""
    for p in r.parts:
        p.pivot = (p.pivot[0], -p.pivot[1], p.pivot[2])
        lo, hi = p.box_min, p.box_max
        p.box_min = (lo[0], -hi[1], lo[2])
        p.box_max = (hi[0], -lo[1], hi[2])
    r.hand_offset = (r.hand_offset[0], -r.hand_offset[1], r.hand_offset[2])


SKIN = 0xD6A67D
DARK = 0x29292E


# ---------------------------------------------------------------- body plans

def humanoid(r, hair, skin, shirt, trousers, wide=False, thin=False):
    """12-px legs, 12-px torso, 8-px head. The torso pivots at the hips (its bottom), so
    sleeping / swimming rotations turn the whole upper body around the waist."""
    hw = 0.28 if wide else 0.25
    lw = 0.0625 if thin else 0.125          # skeletons have 2-px limbs
    r.part("body", -1, BODY, (0.0, 0.0, 0.75), (-0.125, -hw, 0.0), (0.125, hw, 0.75), shirt)
    r.part("head", 0, HEAD, (0.0, 0.0, 0.75), (-0.25, -0.25, 0.0), (0.25, 0.25, 0.5), skin)
    arm_y = hw + lw
    r.part("right_arm", 0, ARM_R, (0.0, -arm_y, 0.625), (-lw, -lw, -0.625), (lw, lw, 0.125), shirt)
    r.part("left_arm", 0, ARM_L, (0.0, arm_y, 0.625), (-lw, -lw, -0.625), (lw, lw, 0.125), shirt)
    r.part("right_leg", 0, LEG_FR, (0.0, -0.125, 0.0), (-lw, -lw, -0.75), (lw, lw, 0.0), trousers)
    r.part("left_leg", 0, LEG_FL, (0.0, 0.125, 0.0), (-lw, -lw, -0.75), (lw, lw, 0.0), trousers)
    r.head, r.right_arm, r.left_arm = 1, 2, 3
    r.hand_offset = (0.0, 0.0, -0.62)


def quadruped(r, by, bz, bl, lh, hz, body, head, snout=None, horns=False, ears=False, wool=None):
    r.part("body", -1, BODY, (0.0, 0.0, bz), (-bl, -by, -0.2), (bl, by, 0.2), body)
    r.part("head", 0, HEAD, (0.45, 0.0, hz - bz), (-0.25, -0.25, -0.25), (0.25, 0.25, 0.25), head)
    # legs share the coat colour; the Blender painter darkens the bottom texels into hooves / paws
    for name, role, sx, sy in (("right_front_leg", LEG_FR, bl, -by), ("left_front_leg", LEG_FL, bl, by),
                               ("right_back_leg", LEG_BR, -bl, -by), ("left_back_leg", LEG_BL, -bl, by)):
        r.part(name, 0, role, (sx * 0.7, sy * 0.7, -0.2), (-0.125, -0.125, -lh - 0.05), (0.125, 0.125, 0.05), body)
    # head features hang off the head so they turn with it
    if snout is not None:
        r.part("snout", 1, MISC, (0.25, 0.0, -0.08), (0.0, -0.125, -0.09), (0.0625, 0.125, 0.09), snout)
    if horns:
        r.part("right_horn", 1, MISC, (-0.05, -0.19, 0.25), (-0.04, -0.04, 0.0), (0.04, 0.04, 0.22), 0xEBE0BD)
        r.part("left_horn", 1, MISC, (-0.05, 0.19, 0.25), (-0.04, -0.04, 0.0), (0.04, 0.04, 0.22), 0xEBE0BD)
    if ears:
        r.part("right_ear", 1, EAR_R, (-0.08, -0.17, 0.25), (-0.05, -0.04, 0.0), (0.05, 0.04, 0.14), head)
        r.part("left_ear", 1, EAR_L, (-0.08, 0.17, 0.25), (-0.05, -0.04, 0.0), (0.05, 0.04, 0.14), head)
    if wool is not None:
        r.part("wool", 0, MISC, (0.0, 0.0, 0.0), (-bl - 0.06, -by - 0.06, -0.26), (bl + 0.06, by + 0.06, 0.27), wool)
    r.head = 1


def bird(r, by, bz, bl, lh, body, head, beak, legs, tail=True):
    r.part("body", -1, BODY, (0.0, 0.0, bz), (-bl, -by, -0.2), (bl, by, 0.2), body)
    r.part("head", 0, HEAD, (bl * 0.9, 0.0, 0.22), (-0.15, -0.15, -0.15), (0.15, 0.15, 0.15), head)
    r.part("beak", 1, MISC, (0.15, 0.0, -0.02), (0.0, -0.05, -0.04), (0.1, 0.05, 0.04), beak)
    r.part("right_wing", 0, WING_R, (0.0, -by, 0.05), (-0.2, -0.35, -0.03), (0.2, 0.05, 0.03), body)
    r.part("left_wing", 0, WING_L, (0.0, by, 0.05), (-0.2, -0.05, -0.03), (0.2, 0.35, 0.03), body)
    if lh > 0.0:
        r.part("right_leg", 0, LEG_FR, (0.0, -0.1, -0.2), (-0.04, -0.04, -lh), (0.04, 0.04, 0.0), legs)
        r.part("left_leg", 0, LEG_FL, (0.0, 0.1, -0.2), (-0.04, -0.04, -lh), (0.04, 0.04, 0.0), legs)
    if tail:
        r.part("tail", 0, TAIL, (-bl, 0.0, 0.0), (-0.25, -0.05, -0.05), (0.0, 0.05, 0.05), body)
    r.head = 1


def fish(r, bw, body, top, belly, flat=False):
    r.part("body", -1, BODY, (0.0, 0.0, 0.2), (-0.35, -bw, -0.2), (0.35, bw, 0.2), body)
    r.part("tail", 0, TAIL, (-0.35, 0.0, 0.0), (-0.3, -0.03, -0.15), (0.0, 0.03, 0.15), top)
    r.part("right_fin", 0, FIN_R, (0.05, -bw, -0.05), (-0.1, -0.2, -0.02), (0.1, 0.0, 0.02), top)
    r.part("left_fin", 0, FIN_L, (0.05, bw, -0.05), (-0.1, 0.0, -0.02), (0.1, 0.2, 0.02), top)
    r.part("head", 0, HEAD, (0.35, 0.0, 0.02), (-0.1, -bw * 0.9, -0.15), (0.15, bw * 0.9, 0.15), belly)
    if flat:
        r.part("top_fin", 0, FIN_L, (0.0, 0.0, 0.2), (-0.2, -0.02, 0.0), (0.2, 0.02, 0.2), top, index=1)
    r.head = 4


def serpentine(r, segments, seg, head, body, seg_a, seg_b):
    r.part("body", -1, BODY, (0.0, 0.0, 1.2), (-head, -head, head * 0.2), (head, head, head * 1.6), body)
    parent = 0
    for i in range(1, segments):
        parent = r.part("segment%d" % i, parent, TAIL, (0.0, 0.0, -seg),
                        (-seg * 0.5, -seg * 0.5, -seg / 3.0), (seg * 0.5, seg * 0.5, seg / 3.0),
                        seg_a if (i % 2) else seg_b, index=i)
    r.head = 0


def spider(r, body, leg, eye, s=1.0):
    r.part("body", -1, BODY, (0.0, 0.0, 0.45 * s), (-0.2 * s, -0.25 * s, -0.15 * s), (0.2 * s, 0.25 * s, 0.15 * s), body)
    r.part("head", 0, HEAD, (0.35 * s, 0.0, 0.0), (-0.15 * s, -0.25 * s, -0.2 * s), (0.15 * s, 0.25 * s, 0.2 * s), body)
    r.part("abdomen", 0, TAIL, (-0.35 * s, 0.0, 0.05 * s), (-0.3 * s, -0.3 * s, -0.25 * s), (0.1 * s, 0.3 * s, 0.25 * s), body)
    r.part("eyes", 1, MISC, (0.15 * s, 0.0, 0.05 * s), (-0.01, -0.16 * s, -0.06 * s), (0.02, 0.16 * s, 0.06 * s), eye, emissive=True)
    legz = (0.35, 0.4, 0.4, 0.35)
    for i in range(4):
        x = (0.25 - i * 0.2) * s
        z = (legz[i] - 0.45) * s
        r.part("right_leg%d" % (i + 1), 0, LEG_MR, (x, -0.2 * s, z), (-0.06 * s, -0.55 * s, -0.06 * s), (0.06 * s, 0.06 * s, 0.06 * s), leg, index=i)
        r.part("left_leg%d" % (i + 1), 0, LEG_ML, (x, 0.2 * s, z), (-0.06 * s, -0.06 * s, -0.06 * s), (0.06 * s, 0.55 * s, 0.06 * s), leg, index=i)
    r.head = 1


def slime(r, body, core, eye=0x1F1F24):
    r.part("body", -1, BODY, (0.0, 0.0, 0.5), (-0.5, -0.5, -0.5), (0.5, 0.5, 0.5), body)
    r.part("eyes", 0, MISC, (0.5, 0.0, 0.12), (0.0, -0.3, -0.12), (0.02, 0.3, 0.12), eye)
    r.part("core", 0, MISC, (0.0, 0.0, 0.0), (-0.25, -0.25, -0.25), (0.25, 0.25, 0.25), core)
    r.head = 1


def ghast(r, s, body, tentacle):
    """Floating cube with nine hanging tentacles (ghast, happy ghast, ghastling)."""
    r.part("body", -1, BODY, (0.0, 0.0, 2.2 * s), (-2.0 * s, -2.0 * s, -2.0 * s), (2.0 * s, 2.0 * s, 2.0 * s), body)
    lengths = (1.4, 1.0, 1.6, 1.2, 1.8, 1.1, 1.5, 0.9, 1.3)
    k = 0
    for gx in (-1.2, 0.0, 1.2):
        for gy in (-1.2, 0.0, 1.2):
            r.part("tentacle%d" % k, 0, TAIL, (gx * s, gy * s, -2.0 * s),
                   (-0.14 * s, -0.14 * s, -lengths[k] * s), (0.14 * s, 0.14 * s, 0.0), tentacle, index=k)
            k += 1
    r.head = 0


def crate(r, color):
    r.part("body", -1, BODY, (0.0, 0.0, 0.5), (-0.4, -0.4, -0.4), (0.4, 0.4, 0.4), color)
    r.head = 0


# ---------------------------------------------------------------- data

HUMANOIDS = {
    #  rig                 hair       skin       shirt      trousers
    "player":             (DARK,     SKIN,     0x3389C7, 0x45336F),
    "steve":              (0x50301B, SKIN,     0x3389C7, 0x45336F),
    "alex":               (0x5C3318, SKIN,     0x47A167, 0x735233),
    "human":              (0x50301B, SKIN,     0x8C8C93, 0x40404D),
    "zombie":             (DARK,     0x709E70, 0x2E6B99, 0x38426B),
    "husk":               (DARK,     0x9E8F6B, 0xB8A87A, 0x8C7A57),
    "drowned":            (DARK,     0x618C80, 0x297070, 0x1F525C),
    "parched":            (DARK,     0xA89473, 0xB39E7A, 0x85765C),
    "zombie_villager":    (DARK,     0x759E7A, 0x4D5C4D, 0x3D3329),
    "zombie_horse_rider": (DARK,     0x709E70, 0x2E6B99, 0x38426B),
    "skeleton":           (DARK,     0xE0E0D6, 0xCCCCC7, 0xC2C2BD),
    "stray":              (DARK,     0xC7D1D6, 0xB8C2CC, 0xADB8C2),
    "bogged":             (DARK,     0x8C9966, 0x808C6B, 0x70805C),
    "wither_skeleton":    (DARK,     0x4D4D4D, 0x38383D, 0x2E2E33),
    "skeleton_horse_rider": (DARK,   0xE0E0D6, 0xCCCCC7, 0xC2C2BD),
    "villager":           (0x5C3D23, 0xCCA380, 0x6B4D33, 0x523D29),
    "wandering_trader":   (0x59524C, 0xC79E7A, 0x476B9E, 0x5C4D3D),
    "vindicator":         (0x47474C, 0xD1A880, 0x85858F, 0x4D4D54),
    "evoker":             (0x424247, 0xD6AD85, 0x5C526F, 0x4D475C),
    "pillager":           (0x4C4C52, 0xCCA380, 0x8F8A80, 0x525257),
    "illusioner":         (0x423D52, 0xD6B38A, 0x66709E, 0x524D66),
    "witch":              (0x333338, 0xC7A880, 0x573D75, 0x3D334D),
    "piglin":             (0x85523F, 0xB3755C, 0x9E8047, 0x66523D),
    "piglin_brute":       (0x8C5747, 0xB87A61, 0x756638, 0x574738),
    "zombified_piglin":   (DARK,     0x709E70, 0x9E7A57, 0x614D38),
    "creaking":           (0x3D2E1F, 0x705738, 0x57422B, 0x473623),
}

QUADRUPEDS = {
    #  rig           by    bz    bl    lh    hz    body      head      features
    "cow":          (0.30, 0.75, 0.50, 0.45, 0.80, 0x6B4A38, 0x57392A, dict(snout=0xB89E94, horns=True)),
    "mooshroom":    (0.30, 0.75, 0.50, 0.45, 0.80, 0xC72E26, 0x9E231D, dict(snout=0xDBCCCC, horns=True)),
    "sheep":        (0.28, 0.70, 0.45, 0.40, 0.75, 0xDBD8D2, 0x857066, dict(wool=0xF0F0EB)),
    "pig":          (0.28, 0.65, 0.45, 0.35, 0.68, 0xE79494, 0xF0A8A4, dict(snout=0xF59E9E)),
    "goat":         (0.25, 0.72, 0.45, 0.50, 0.78, 0xD6D1C4, 0xCCC7BA, dict(horns=True)),
    "horse":        (0.28, 0.95, 0.65, 0.60, 1.05, 0x855C3D, 0x754F34, dict(snout=0x61422E, ears=True)),
    "donkey":       (0.28, 0.90, 0.60, 0.55, 1.00, 0x8F857A, 0x80766C, dict(snout=0x6B6157, ears=True)),
    "mule":         (0.28, 0.90, 0.60, 0.55, 1.00, 0x66564D, 0x5B4C44, dict(snout=0x4D423A, ears=True)),
    "skeleton_horse": (0.28, 0.95, 0.65, 0.60, 1.05, 0xD1D1C7, 0xCCCCC2, dict(snout=0xBDBDB3, ears=True)),
    "zombie_horse": (0.28, 0.95, 0.65, 0.60, 1.05, 0x6B946B, 0x668A66, dict(snout=0x5C7D5C, ears=True)),
    "camel":        (0.30, 1.25, 0.60, 1.00, 1.40, 0xD6B370, 0xD1AD6B, dict(snout=0xC2A164, ears=True)),
    "llama":        (0.28, 1.00, 0.50, 0.80, 1.15, 0xD6C29E, 0xCCB894, dict(snout=0xBDAA85, ears=True)),
    "trader_llama": (0.28, 1.00, 0.50, 0.80, 1.15, 0x8F769E, 0x856B94, dict(snout=0x75617D, ears=True)),
    "wolf":         (0.22, 0.55, 0.40, 0.35, 0.60, 0xDBDBE0, 0xE6E6EB, dict(snout=0xCFCFD6, ears=True)),
    "cat":          (0.20, 0.45, 0.32, 0.30, 0.50, 0xC79E66, 0xD1A870, dict(ears=True)),
    "ocelot":       (0.20, 0.45, 0.32, 0.30, 0.50, 0xEBBE52, 0xE6B84D, dict(ears=True)),
    "fox":          (0.20, 0.45, 0.32, 0.30, 0.50, 0xDB7033, 0xE67F3D, dict(snout=0xF0E6DB, ears=True)),
    "polar_bear":   (0.32, 0.85, 0.55, 0.50, 0.90, 0xE6E6E0, 0xEBEBE6, dict(snout=0xD1D1CC, ears=True)),
    "panda":        (0.30, 0.70, 0.45, 0.35, 0.78, 0xE6E6E6, 0xEBEBEB, dict(snout=0xF0F0F0, ears=True)),
    "hoglin":       (0.30, 0.80, 0.50, 0.45, 0.85, 0x9E614C, 0xAD7057, dict(snout=0xBD7A5C, horns=True, ears=True)),
    "zoglin":       (0.30, 0.80, 0.50, 0.45, 0.85, 0x6B9A70, 0x75A87A, dict(snout=0x85B38A, horns=True, ears=True)),
    "ravager":      (0.40, 0.90, 0.60, 0.45, 0.95, 0x524D52, 0x605C60, dict(snout=0x4D474D, horns=True)),
    "armadillo":    (0.25, 0.35, 0.35, 0.20, 0.40, 0x9E7A5C, 0xB38F6B, dict(ears=True)),
    "turtle":       (0.35, 0.30, 0.40, 0.15, 0.35, 0x578557, 0x709E66, dict()),
    "sniffer":      (0.55, 1.00, 0.90, 0.55, 1.10, 0x708561, 0x80945E, dict(snout=0xDB99A8)),
    "strider":      (0.30, 0.90, 0.45, 0.55, 1.00, 0xC7616B, 0xD17075, dict()),
    "axolotl":      (0.20, 0.25, 0.35, 0.15, 0.30, 0xF5B8CC, 0xE09EB8, dict()),
}

BIRDS = {
    #  rig        by    bz    bl    lh    body      head      beak      legs
    "chicken":   (0.25, 0.55, 0.35, 0.35, 0xEBEBE6, 0xF0F0EB, 0xF2C733, 0xF2B826),
    "parrot":    (0.25, 0.55, 0.35, 0.35, 0xD1332E, 0xDB3D33, 0x3D3D3D, 0x8C8C8C),
    "bat":       (0.22, 0.50, 0.30, 0.0,  0x4D4240, 0x574D4A, 0x3D3330, 0x3D3330),
    "allay":     (0.22, 0.55, 0.30, 0.0,  0x6B9EEB, 0x80B3F0, 0xC7D6F5, 0xC7D6F5),
    "vex":       (0.22, 0.55, 0.30, 0.0,  0x6BADB8, 0x75B8C2, 0x9ECCD1, 0x9ECCD1),
    "bee":       (0.20, 0.50, 0.28, 0.0,  0xEBBD29, 0x66522A, 0x33291A, 0x33291A),
    "phantom":   (0.30, 0.60, 0.45, 0.0,  0x3D666B, 0x477578, 0xE6E6E0, 0xE6E6E0),
    "breeze":    (0.18, 0.80, 0.18, 0.0,  0xA8C7E0, 0xBDD6EB, 0xE6F0FA, 0xE6F0FA),
}

FISHES = {
    #  rig              width body      top       belly     top fin
    "cod":             (0.18, 0x9E855C, 0x856F4D, 0xC7B48C, False),
    "salmon":          (0.18, 0xC76B52, 0x9E5742, 0x8C4D3D, False),
    "tropical_fish":   (0.18, 0xEB9E29, 0x803D9A, 0xF0F0F0, False),
    "pufferfish":      (0.30, 0xDBBD52, 0xB3943D, 0xF0E6B8, True),
    "tadpole":         (0.14, 0x4D4238, 0x3D332C, 0x6B5C4D, False),
    "dolphin":         (0.25, 0x708FB8, 0x85A3CC, 0xD6E0EB, True),
    "guardian":        (0.45, 0x578F85, 0x669E94, 0x85B8AD, True),
    "elder_guardian":  (0.55, 0x758F6B, 0x85A37A, 0x9EB894, True),
}

SERPENTS = {
    #  rig               body      seg A     seg B     seg   count head
    "squid":           (0x6B759E, 0x667099, 0x5C668C, 0.25, 5, 0.35),
    "glow_squid":      (0x6BC7B8, 0x61B8A8, 0x57A89A, 0.25, 5, 0.35),
    "nautilus":        (0xDBBD8A, 0xD1B37F, 0xC7A875, 0.22, 6, 0.30),
    "zombie_nautilus": (0x708A66, 0x66805C, 0x5C7552, 0.22, 6, 0.32),
}

PROPS = {
    "armor_stand": 0x9E7A52, "item_frame_prop": 0xA88052, "painting_prop": 0xA88052,
    "boat": 0x9E7547, "chest_boat": 0x9E7547, "minecart": 0x707078,
    "chest_minecart": 0x707078, "furnace_minecart": 0x707078, "tnt_minecart": 0x707078,
    "hopper_minecart": 0x707078, "end_crystal": 0xDB8FEB, "lightning_bolt": 0xCCDBFF,
    "xp_orb": 0xB8EB4D, "arrow": 0xB8AD99, "trident": 0x4DA8A8,
    "snowball": 0xF0F2FA, "eye_of_ender": 0x5CB899, "firework": 0xE68066,
    "area_effect_cloud": 0xCCCCDB, "fishing_bobber": 0xDB4D47, "wind_charge": 0xC7DBE6,
    "shulker_bullet": 0x9966B3, "llama_spit": 0xCCD1C7, "dragon_fireball": 0x9E4DB8,
    "fireball": 0xEB8033, "small_fireball": 0xF5A83D, "wither_skull": 0x3D3D42,
    "potion_proj": 0xE6E6F0, "spear": 0xB39E70,
}


def build_all():
    rigs = []

    thin = ("skeleton", "stray", "bogged", "wither_skeleton", "skeleton_horse_rider")
    for name in sorted(HUMANOIDS):
        r = Rig(name)
        humanoid(r, *HUMANOIDS[name], wide=(name == "piglin_brute"), thin=(name in thin))
        if name == "wither_skeleton":
            r.scale = 1.2
        rigs.append(r)

    for name in sorted(QUADRUPEDS):
        by, bz, bl, lh, hz, body, head, feat = QUADRUPEDS[name]
        r = Rig(name)
        quadruped(r, by, bz, bl, lh, hz, body, head, **feat)
        rigs.append(r)

    for name in sorted(BIRDS):
        by, bz, bl, lh, body, head, beak, legs = BIRDS[name]
        r = Rig(name)
        bird(r, by, bz, bl, lh, body, head, beak, legs, tail=(lh > 0.0))
        rigs.append(r)

    for name in sorted(FISHES):
        bw, body, top, belly, flat = FISHES[name]
        r = Rig(name)
        fish(r, bw, body, top, belly, flat)
        if name == "guardian":
            r.scale = 1.7
        elif name == "elder_guardian":
            r.scale = 2.6
        rigs.append(r)

    for name in sorted(SERPENTS):
        body, seg_a, seg_b, seg, count, head = SERPENTS[name]
        r = Rig(name)
        serpentine(r, count, seg, head, body, seg_a, seg_b)
        rigs.append(r)

    r = Rig("spider")
    spider(r, 0x38241F, 0x291A17, 0xD92626)
    rigs.append(r)
    for name, body, leg, eye in (("cave_spider", 0x2E6B45, 0x245238, 0xE63333),
                                 ("silverfish", 0x9E9EA3, 0x8F8F94, 0x4D4D57),
                                 ("endermite", 0x4D336B, 0x42295C, 0xC770F0)):
        r = Rig(name)
        spider(r, body, leg, eye, s=0.6)
        rigs.append(r)

    for name, body, core in (("slime", 0x6BC76B, 0x4D9E52), ("magma_cube", 0x9E3D24, 0xF08F29), ("sulfur_cube", 0xE6D64D, 0xBDA829)):
        r = Rig(name)
        slime(r, body, core)
        rigs.append(r)

    for name, s, body, tent in (("ghast", 1.0, 0xEBEBF0, 0xDBDBE3), ("happy_ghast", 1.0, 0xE3D1EB, 0xD6C2E0), ("ghastling", 0.25, 0xF0F0F5, 0xE0E0EB)):
        r = Rig(name)
        ghast(r, s, body, tent)
        rigs.append(r)

    # creeper: 8x12x4 body on four 4x6x4 feet, 8x8x8 head
    r = Rig("creeper")
    r.part("body", -1, BODY, (0.0, 0.0, 0.75), (-0.125, -0.25, -0.375), (0.125, 0.25, 0.375), 0x4D9E47)
    r.part("head", 0, HEAD, (0.0, 0.0, 0.375), (-0.25, -0.25, 0.0), (0.25, 0.25, 0.5), 0x57AD4D)
    for name, role, sx, sy in (("front_right_leg", LEG_FR, 0.25, -0.125), ("front_left_leg", LEG_FL, 0.25, 0.125),
                               ("back_right_leg", LEG_BR, -0.25, -0.125), ("back_left_leg", LEG_BL, -0.25, 0.125)):
        r.part(name, 0, role, (sx, sy, -0.375), (-0.125, -0.125, -0.375), (0.125, 0.125, 0.0), 0x428F3D)
    r.head = 1
    rigs.append(r)

    # enderman: 30-px legs and arms, 12-px body, 8-px head
    r = Rig("enderman")
    r.part("body", -1, BODY, (0.0, 0.0, 2.25), (-0.125, -0.25, -0.375), (0.125, 0.25, 0.375), 0x1F1A29)
    r.part("head", 0, HEAD, (0.0, 0.0, 0.375), (-0.25, -0.25, 0.0), (0.25, 0.25, 0.5), 0x241F2E)
    r.part("right_arm", 0, ARM_R, (0.0, -0.3125, 0.3125), (-0.0625, -0.0625, -1.875), (0.0625, 0.0625, 0.0625), 0x1F1A29)
    r.part("left_arm", 0, ARM_L, (0.0, 0.3125, 0.3125), (-0.0625, -0.0625, -1.875), (0.0625, 0.0625, 0.0625), 0x1F1A29)
    r.part("right_leg", 0, LEG_FR, (0.0, -0.125, -0.375), (-0.0625, -0.0625, -1.875), (0.0625, 0.0625, 0.0), 0x1F1A29)
    r.part("left_leg", 0, LEG_FL, (0.0, 0.125, -0.375), (-0.0625, -0.0625, -1.875), (0.0625, 0.0625, 0.0), 0x1F1A29)
    r.head, r.right_arm, r.left_arm = 1, 2, 3
    r.hand_offset = (0.0, 0.0, -1.8)
    rigs.append(r)

    # warden: broad biped with a heavy head
    r = Rig("warden")
    r.part("body", -1, BODY, (0.0, 0.0, 1.0), (-0.3, -0.56, 0.0), (0.3, 0.56, 1.3), 0x24474F)
    r.part("head", 0, HEAD, (0.0, 0.0, 1.3), (-0.3, -0.4, 0.0), (0.3, 0.4, 0.6), 0x1F3D47)
    r.part("right_arm", 0, ARM_R, (0.0, -0.75, 1.15), (-0.19, -0.19, -1.4), (0.19, 0.19, 0.1), 0x1F3D47)
    r.part("left_arm", 0, ARM_L, (0.0, 0.75, 1.15), (-0.19, -0.19, -1.4), (0.19, 0.19, 0.1), 0x1F3D47)
    r.part("right_leg", 0, LEG_FR, (0.0, -0.28, 0.0), (-0.19, -0.19, -1.0), (0.19, 0.19, 0.0), 0x1A333D)
    r.part("left_leg", 0, LEG_FL, (0.0, 0.28, 0.0), (-0.19, -0.19, -1.0), (0.19, 0.19, 0.0), 0x1A333D)
    r.part("right_tendril", 1, EAR_R, (0.0, -0.4, 0.45), (-0.02, -0.28, -0.12), (0.02, 0.0, 0.2), 0x33B8C2, emissive=True)
    r.part("left_tendril", 1, EAR_L, (0.0, 0.4, 0.45), (-0.02, 0.0, -0.12), (0.02, 0.28, 0.2), 0x33B8C2, emissive=True)
    r.head, r.right_arm, r.left_arm = 1, 2, 3
    r.hand_offset = (0.0, 0.0, -1.3)
    rigs.append(r)

    r = Rig("iron_golem")
    r.part("body", -1, BODY, (0.0, 0.0, 1.15), (-0.3, -0.45, -0.7), (0.3, 0.45, 0.7), 0xB8B3A8)
    r.part("head", 0, HEAD, (0.0, 0.0, 0.75), (-0.25, -0.25, 0.0), (0.25, 0.25, 0.5), 0xC7C2B8)
    r.part("nose", 1, MISC, (0.25, 0.0, 0.2), (0.0, -0.0625, -0.125), (0.125, 0.0625, 0.0), 0xA8A39A)
    r.part("right_arm", 0, ARM_R, (0.0, -0.6, 0.35), (-0.15, -0.15, -1.3), (0.15, 0.15, 0.25), 0xB3AEA3)
    r.part("left_arm", 0, ARM_L, (0.0, 0.6, 0.35), (-0.15, -0.15, -1.3), (0.15, 0.15, 0.25), 0xB3AEA3)
    r.part("right_leg", 0, LEG_FR, (0.0, -0.22, -0.7), (-0.18, -0.16, -0.45), (0.18, 0.16, 0.0), 0xA8A39A)
    r.part("left_leg", 0, LEG_FL, (0.0, 0.22, -0.7), (-0.18, -0.16, -0.45), (0.18, 0.16, 0.0), 0xA8A39A)
    r.head, r.right_arm, r.left_arm = 1, 3, 4
    r.hand_offset = (0.0, 0.0, -1.25)
    rigs.append(r)

    r = Rig("snow_golem")
    r.part("body", -1, BODY, (0.0, 0.0, 0.4), (-0.35, -0.35, -0.4), (0.35, 0.35, 0.3), 0xF0F2F5)
    r.part("torso", 0, MISC, (0.0, 0.0, 0.3), (-0.28, -0.28, 0.0), (0.28, 0.28, 0.5), 0xF5F7FA)
    r.part("head", 0, HEAD, (0.0, 0.0, 0.8), (-0.25, -0.25, 0.0), (0.25, 0.25, 0.5), 0xE6A33D)
    r.part("right_arm", 0, ARM_R, (0.0, -0.28, 0.65), (-0.03, -0.45, -0.03), (0.03, 0.0, 0.03), 0x94703F)
    r.part("left_arm", 0, ARM_L, (0.0, 0.28, 0.65), (-0.03, 0.0, -0.03), (0.03, 0.45, 0.03), 0x94703F)
    r.head, r.right_arm, r.left_arm = 2, 3, 4
    rigs.append(r)

    r = Rig("copper_golem")
    r.part("body", -1, BODY, (0.0, 0.0, 0.45), (-0.16, -0.2, -0.2), (0.16, 0.2, 0.2), 0xC27A4D)
    r.part("head", 0, HEAD, (0.0, 0.0, 0.2), (-0.18, -0.2, 0.0), (0.18, 0.2, 0.28), 0xD68F5C)
    r.part("nose", 1, MISC, (0.18, 0.0, 0.08), (0.0, -0.04, -0.06), (0.06, 0.04, 0.02), 0xB26E44)
    r.part("rod", 1, ROD, (0.0, 0.0, 0.28), (-0.03, -0.03, 0.0), (0.03, 0.03, 0.16), 0xE69E6B)
    r.part("right_arm", 0, ARM_R, (0.0, -0.25, 0.15), (-0.06, -0.06, -0.32), (0.06, 0.06, 0.04), 0xB87247)
    r.part("left_arm", 0, ARM_L, (0.0, 0.25, 0.15), (-0.06, -0.06, -0.32), (0.06, 0.06, 0.04), 0xB87247)
    r.part("right_leg", 0, LEG_FR, (0.0, -0.1, -0.2), (-0.07, -0.07, -0.25), (0.07, 0.07, 0.0), 0xB26E44)
    r.part("left_leg", 0, LEG_FL, (0.0, 0.1, -0.2), (-0.07, -0.07, -0.25), (0.07, 0.07, 0.0), 0xB26E44)
    r.head, r.right_arm, r.left_arm = 1, 4, 5
    rigs.append(r)

    r = Rig("shulker")
    r.part("body", -1, BODY, (0.0, 0.0, 0.25), (-0.5, -0.5, -0.25), (0.5, 0.5, 0.25), 0x8A57A0)
    r.part("head", 0, HEAD, (0.0, 0.0, 0.1), (-0.19, -0.19, 0.0), (0.19, 0.19, 0.38), 0xE6D6B3)
    r.part("lid", 0, JAW, (-0.5, 0.0, 0.25), (0.0, -0.5, 0.0), (1.0, 0.5, 0.5), 0x9A62B0)
    r.head = 1
    rigs.append(r)

    r = Rig("blaze")
    r.part("head", -1, HEAD, (0.0, 0.0, 1.3), (-0.25, -0.25, -0.25), (0.25, 0.25, 0.25), 0xF5C229)
    k = 0
    for ring, (dz, rad, phase) in enumerate(((-0.35, 0.5, 0.0), (-0.75, 0.4, 45.0), (-1.1, 0.3, 0.0))):
        for j in range(4):
            a = math.radians(phase + j * 90.0)
            r.part("rod%d" % k, 0, ROD, (math.cos(a) * rad, math.sin(a) * rad, dz),
                   (-0.0625, -0.0625, -0.25), (0.0625, 0.0625, 0.25), 0xFADB3D, index=k, emissive=True)
            k += 1
    r.head = 0
    rigs.append(r)

    r = Rig("rabbit")
    r.part("body", -1, BODY, (0.0, 0.0, 0.24), (-0.16, -0.1, -0.1), (0.16, 0.1, 0.1), 0x8B6A4A)
    r.part("head", 0, HEAD, (0.24, 0.0, 0.08), (-0.09, -0.09, -0.09), (0.09, 0.09, 0.09), 0x94724F)
    r.part("right_ear", 1, EAR_R, (-0.02, -0.04, 0.09), (-0.02, -0.025, 0.0), (0.02, 0.025, 0.16), 0x94724F)
    r.part("left_ear", 1, EAR_L, (-0.02, 0.04, 0.09), (-0.02, -0.025, 0.0), (0.02, 0.025, 0.16), 0x94724F)
    r.part("tail", 0, TAIL, (-0.17, 0.0, 0.03), (-0.05, -0.05, -0.05), (0.05, 0.05, 0.05), 0xF2F2F2)
    for name, role, sx, sy in (("right_front_leg", LEG_FR, 0.12, -0.07), ("left_front_leg", LEG_FL, 0.12, 0.07),
                               ("right_back_leg", LEG_BR, -0.12, -0.07), ("left_back_leg", LEG_BL, -0.12, 0.07)):
        r.part(name, 0, role, (sx, sy, -0.08), (-0.035, -0.03, -0.16), (0.035, 0.03, 0.0), 0x7D6144)
    r.head = 1
    rigs.append(r)

    r = Rig("frog")
    r.part("body", -1, BODY, (0.0, 0.0, 0.2), (-0.18, -0.16, -0.1), (0.18, 0.16, 0.1), 0x6B9E42)
    r.part("head", 0, HEAD, (0.14, 0.0, 0.06), (-0.04, -0.16, -0.05), (0.16, 0.16, 0.09), 0x75A84C)
    r.part("eyes", 1, MISC, (0.06, 0.0, 0.09), (-0.04, -0.12, 0.0), (0.04, 0.12, 0.05), 0xE6D933)
    for name, role, sx, sy in (("right_front_leg", LEG_FR, 0.12, -0.12), ("left_front_leg", LEG_FL, 0.12, 0.12),
                               ("right_back_leg", LEG_BR, -0.12, -0.14), ("left_back_leg", LEG_BL, -0.12, 0.14)):
        r.part(name, 0, role, (sx, sy, -0.06), (-0.05, -0.05, -0.14), (0.05, 0.05, 0.0), 0x5C8A38)
    r.head = 1
    rigs.append(r)

    # ---- bosses
    r = Rig("ender_dragon")
    r.part("body", -1, BODY, (0.0, 0.0, 1.0), (-1.2, -0.6, -0.5), (1.2, 0.6, 0.5), 0x29203D)
    parent = 0
    for i in range(5):
        parent = r.part("neck%d" % i, parent, NECK, (1.35 if i == 0 else 0.62, 0.0, 0.3 if i == 0 else 0.12),
                        (-0.31, -0.31, -0.31), (0.31, 0.31, 0.31), 0x332B45, index=i)
    head = r.part("head", parent, HEAD, (0.55, 0.0, 0.1), (-0.3, -0.45, -0.3), (0.7, 0.45, 0.35), 0x38304D)
    r.part("jaw", head, JAW, (0.1, 0.0, -0.3), (-0.3, -0.4, -0.15), (0.6, 0.4, 0.05), 0x2E2640)
    r.part("eyes", head, MISC, (0.55, 0.0, 0.18), (0.0, -0.38, -0.05), (0.16, 0.38, 0.07), 0xC74DEB, emissive=True)
    r.part("right_wing", 0, WING_R, (0.0, -0.55, 0.3), (-1.0, -4.0, -0.05), (1.0, 0.0, 0.05), 0x3D3352)
    r.part("left_wing", 0, WING_L, (0.0, 0.55, 0.3), (-1.0, 0.0, -0.05), (1.0, 4.0, 0.05), 0x3D3352)
    for name, role, sx, sy in (("right_front_leg", LEG_FR, 0.7, -0.45), ("left_front_leg", LEG_FL, 0.7, 0.45),
                               ("right_back_leg", LEG_BR, -0.8, -0.45), ("left_back_leg", LEG_BL, -0.8, 0.45)):
        r.part(name, 0, role, (sx, sy, -0.5), (-0.2, -0.2, -0.5), (0.2, 0.2, 0.0), 0x241C36)
    parent = 0
    for i in range(8):
        parent = r.part("tail%d" % i, parent, TAIL, (-1.35 if i == 0 else -0.45, 0.0, 0.1 if i == 0 else 0.0),
                        (-0.22, -0.22, -0.22), (0.22, 0.22, 0.22), 0x2E2640, index=i)
    r.head = head
    rigs.append(r)

    r = Rig("wither")
    r.part("body", -1, BODY, (0.0, 0.0, 1.5), (-0.5, -0.4, -0.5), (0.5, 0.4, 0.5), 0x33333A)
    r.part("head0", 0, HEAD, (0.5, 0.0, 0.5), (-0.3, -0.3, -0.3), (0.3, 0.3, 0.3), 0x3D3D45)
    r.part("head1", 0, HEAD, (0.3, -0.45, -0.2), (-0.22, -0.22, -0.22), (0.22, 0.22, 0.22), 0x38383F, index=1)
    r.part("head2", 0, HEAD, (0.3, 0.45, -0.2), (-0.22, -0.22, -0.22), (0.22, 0.22, 0.22), 0x38383F, index=2)
    r.part("tail", 0, TAIL, (0.0, 0.0, -0.3), (-0.2, -0.2, -1.0), (0.1, 0.2, 0.0), 0x2E2E35)
    r.part("right_arm", 0, ARM_R, (0.3, -0.4, -0.1), (-0.15, -0.4, -0.15), (0.15, 0.0, 0.15), 0x33333A, index=3)
    r.part("left_arm", 0, ARM_L, (0.3, 0.4, -0.1), (-0.15, 0.0, -0.15), (0.15, 0.4, 0.15), 0x33333A, index=4)
    r.head = 1
    rigs.append(r)

    for name in sorted(PROPS):
        r = Rig(name)
        crate(r, PROPS[name])
        rigs.append(r)

    for r in rigs:
        mirror_y(r)

    # every name must be unique: MCRigs::Find() resolves by id
    seen = set()
    for r in rigs:
        assert r.name not in seen, "duplicate rig " + r.name
        seen.add(r.name)
        for i, p in enumerate(r.parts):
            assert p.parent < i, "%s.%s: parent must be declared first" % (r.name, p.name)
    return rigs


if __name__ == "__main__":
    all_rigs = build_all()
    print("%d rigs, %d parts" % (len(all_rigs), sum(len(r.parts) for r in all_rigs)))
