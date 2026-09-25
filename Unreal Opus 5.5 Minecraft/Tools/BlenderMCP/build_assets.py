"""Opus 5.5 Minecraft hero-asset builder (runs inside Blender, UnrealMinecraft.blend).

Driven by Tools/BlenderMCP/run_build.py, which prepends a RIG_TABLE batch (generated
from Tools/content/rig_defs.py) and sends the result to Blender through the
blender_unreal MCP server (port 9878) with BLENDER_MCP_SAFE_MODE=1.  That is why this
file only imports bpy / bmesh / math and avoids lambdas, classes, os and open().

For every rig part it builds a texel-painted box (16 texels per block, the Minecraft
model resolution), exports it to

    //Saved/Opus55Fbx/Mobs/<rig>/SM_<rig>__<part>.fbx      -> /Game/Opus55Minecraft/Mobs/<rig>/

with the part PIVOT at the mesh origin, then keeps an assembled copy in the scene so
the .blend doubles as a model sheet.  Verified conventions (Tools/content/axis_probe.py):
Blender (x, y, z) arrives in Unreal as (100x, -100y, 100z) and object transforms are
baked into the vertices, so geometry is authored at the origin with y negated.

Vertex colour: rgb = albedo (linear), alpha = 1 mob tint applies, 0.5 self-lit, 0 fixed.
"""
import bpy
import bmesh
import math

EXPORT_DIR = "//Saved/Opus55Fbx"
PX = 16.0                 # texels per block
MAX_TEXELS = 32           # cap per face side (ghasts, dragon wings)
GRID = 7.0                # model-sheet spacing in blocks

# RIG_TABLE, BATCH_START, BATCH_LAST and ENTITY_PASS are prepended by run_build.py


# ---------------------------------------------------------------- colour helpers

def hexc(h):
    return (((h >> 16) & 255) / 255.0, ((h >> 8) & 255) / 255.0, (h & 255) / 255.0)


def mul(c, f):
    return (min(1.0, c[0] * f), min(1.0, c[1] * f), min(1.0, c[2] * f))


def mix(a, b, t):
    return (a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, a[2] + (b[2] - a[2]) * t)


def to_linear(c):
    out = []
    for v in c:
        if v <= 0.04045:
            out.append(v / 12.92)
        else:
            out.append(((v + 0.055) / 1.055) ** 2.4)
    return (out[0], out[1], out[2])


def noise(a, b, c):
    """Stable hash in [0, 1)."""
    h = (a * 73856093) ^ (b * 19349663) ^ (c * 83492791)
    h = h & 0xFFFFFFFF
    h = ((h ^ (h >> 13)) * 1274126177) & 0xFFFFFFFF
    return ((h ^ (h >> 16)) & 0xFFFF) / 65536.0


def name_seed(s):
    h = 7
    for ch in s:
        h = (h * 31 + ord(ch)) & 0xFFFFFF
    return h


WHITE = (0.95, 0.95, 0.95)
BLACK = (0.07, 0.07, 0.08)
EYE_BLUE = hexc(0x3D5CB8)
EYE_GREEN = hexc(0x2E8C3D)
EYE_BROWN = hexc(0x5C3D23)
EYE_RED = hexc(0xD92626)


# ---------------------------------------------------------------- rig styles

HUMAN_STYLE = {
    "player": "human", "steve": "human", "alex": "human", "human": "human",
    "zombie": "zombie", "husk": "zombie", "drowned": "zombie", "parched": "zombie",
    "zombie_villager": "zombie_villager", "zombie_horse_rider": "zombie",
    "skeleton": "skeleton", "stray": "skeleton", "bogged": "skeleton", "wither_skeleton": "skeleton",
    "skeleton_horse_rider": "skeleton",
    "villager": "villager", "wandering_trader": "villager",
    "vindicator": "illager", "evoker": "illager", "pillager": "illager", "illusioner": "illager",
    "witch": "witch", "piglin": "piglin", "piglin_brute": "piglin", "zombified_piglin": "piglin",
    "creaking": "creaking", "warden": "warden",
}

QUAD_RIGS = ("cow", "mooshroom", "sheep", "pig", "goat", "horse", "donkey", "mule", "skeleton_horse",
             "zombie_horse", "camel", "llama", "trader_llama", "wolf", "cat", "ocelot", "fox",
             "polar_bear", "panda", "hoglin", "zoglin", "ravager", "armadillo", "turtle", "sniffer",
             "strider", "axolotl", "rabbit", "frog")


def style_of(rig):
    if rig in HUMAN_STYLE:
        return "humanoid"
    if rig in QUAD_RIGS:
        return "quad"
    if rig in ("chicken", "parrot", "bat", "allay", "vex", "bee", "phantom", "breeze"):
        return "bird"
    if rig in ("cod", "salmon", "tropical_fish", "pufferfish", "tadpole", "dolphin", "guardian", "elder_guardian"):
        return "fish"
    if rig in ("squid", "glow_squid", "nautilus", "zombie_nautilus"):
        return "squid"
    if rig in ("spider", "cave_spider", "silverfish", "endermite"):
        return "spider"
    if rig in ("slime", "magma_cube", "sulfur_cube"):
        return "slime"
    if rig in ("ghast", "happy_ghast", "ghastling"):
        return "ghast"
    if rig in ("creeper", "enderman", "iron_golem", "snow_golem", "copper_golem", "shulker", "blaze",
               "ender_dragon", "wither"):
        return rig
    return "prop"


# ---------------------------------------------------------------- face painters
# Every painter returns (rgb, alpha) for one texel. i runs along the face's u axis,
# j along v (upwards on side faces). nu / nv are the texel counts.

def human_head(kind, face, i, j, nu, nv, skin, hair):
    """8x8 head. Faces: F front (+X), B back, S sides, U top, D bottom."""
    if face == "U":
        return (hair, 0.0)
    if face == "D":
        return (mul(skin, 0.85), 1.0)
    top = nv - 1 - j                     # rows counted from the top
    if kind in ("skeleton",):
        if face == "F":
            if top in (3, 4) and i in (1, 2, nu - 3, nu - 2):
                return (BLACK, 0.0)       # eye sockets
            if top == 5 and i in (3, 4):
                return (mul(skin, 0.45), 0.0)
            if top == 6 and 2 <= i <= nu - 3 and (i % 2 == 0):
                return (mul(skin, 0.55), 0.0)
        return (skin, 1.0)
    if face in ("S", "B"):
        if top < 2 or (face == "B" and top < 6) or (face == "S" and top < 3 and i < nu // 2):
            return (hair, 0.0)
        return (skin, 1.0)
    # front face
    if top < 2:
        return (hair, 0.0)
    if top == 2 and (i == 0 or i == nu - 1):
        return (hair, 0.0)
    if kind == "villager" or kind == "illager" or kind == "witch" or kind == "zombie_villager":
        if top == 3 and 1 <= i <= nu - 2:
            return (mul(hair, 0.8), 0.0)  # brow
        if top == 4 and i in (1, 2, nu - 3, nu - 2):
            if i in (2, nu - 3):
                return (EYE_GREEN if kind != "illager" else hexc(0x404040), 0.0)
            return (WHITE, 0.0)
        if 4 <= top <= 6 and i in (3, 4):
            return (mul(skin, 0.82), 1.0)  # long nose
        if top == 7 and 2 <= i <= nu - 3 and kind == "witch":
            return (mul(skin, 0.7), 1.0)
        return (skin, 1.0)
    if kind == "piglin":
        if top == 3 and i in (1, 2, nu - 3, nu - 2):
            return ((WHITE if i in (1, nu - 2) else BLACK), 0.0)
        if 4 <= top <= 6 and 2 <= i <= nu - 3:
            if top == 5 and i in (3, 4):
                return (mul(skin, 0.45), 0.0)
            return (mul(skin, 1.12), 1.0)  # snout
        if top == 7 and i in (1, nu - 2):
            return (hexc(0xEFE6CF), 0.0)   # tusks
        return (skin, 1.0)
    if kind == "creaking" or kind == "warden":
        if top == 4 and i in (2, nu - 3):
            return (hexc(0xFFA028) if kind == "creaking" else hexc(0x3FD1D9), 0.5)
        if top == 6 and 2 <= i <= nu - 3:
            return (mul(skin, 0.55), 0.0)
        return (mul(skin, 0.9 + 0.2 * noise(i, j, 17)), 1.0)
    # human / zombie
    if top == 3 and i in (1, 2, nu - 3, nu - 2):
        if kind == "zombie":
            return (BLACK, 0.0)
        return ((WHITE if i in (1, nu - 2) else EYE_BLUE), 0.0)
    if top == 5 and i in (3, 4):
        return (mul(skin, 0.86), 1.0)      # nose shading
    if top == 6 and 2 <= i <= nu - 3:
        return (mul(skin, 0.62 if kind == "zombie" else 0.72), 1.0)  # mouth
    return (skin, 1.0)


def animal_head(rig, face, i, j, nu, nv, base):
    top = nv - 1 - j
    if face == "F":
        if top == nv // 2 - 1 and (i == 1 or i == nu - 2):
            if rig in ("wolf", "fox", "cat", "ocelot"):
                return (hexc(0xE0C23A) if rig in ("cat", "ocelot") else BLACK, 0.0)
            return (BLACK, 0.0)
        if rig in ("cow", "mooshroom") and 1 <= i <= nu - 2 and top >= nv // 2:
            return (mix(base, WHITE, 0.75), 0.0)          # pale muzzle band
        if rig == "panda" and top == nv // 2 - 1 and i in (0, 2, nu - 3, nu - 1):
            return (BLACK, 0.0)                            # eye patches
        if rig == "fox" and top >= nv // 2:
            return (hexc(0xF2EDE6), 0.0)
        if rig == "sheep":
            return (mul(base, 0.95), 0.0)
        if rig in ("zombie_horse", "skeleton_horse") and top == nv // 2 - 1 and i in (1, nu - 2):
            return (hexc(0x1A1A1A), 0.0)
    if face == "U" and rig == "sheep":
        return (hexc(0xF0F0EB), 1.0)
    return (base, 1.0)


# ---------------------------------------------------------------- per part painter

def paint(rig, style, part, face, i, j, nu, nv, base, emissive):
    """Returns (srgb, alpha)."""
    seed = name_seed(rig + part)
    grain = 0.92 + 0.16 * noise(i + seed, j, 3)
    top = nv - 1 - j
    col = base
    alpha = 1.0

    if emissive:
        # self-lit parts: eyes plates, rods, tendrils
        if style == "spider" and part == "eyes":
            if (i % 2 == 1) and (j % 2 == 0 or nv <= 2):
                return (base, 0.5)
            return (BLACK, 0.0)
        return (mul(base, 0.9 + 0.2 * noise(i, j, seed)), 0.5)

    if style == "humanoid":
        kind = HUMAN_STYLE.get(rig, "human")
        hair = hexc(HAIR.get(rig, 0x29292E))
        skin = hexc(SKIN_C.get(rig, 0xD6A67D))
        if part == "head":
            c, a = human_head(kind, face, i, j, nu, nv, skin, hair)
            return (mul(c, 0.96 + 0.08 * noise(i, j, seed)), a)
        if part == "body":
            if kind == "skeleton":
                # ribs: alternating bone and gaps
                if face in ("F", "B") and (j % 3 == 0) and 1 <= i <= nu - 2 and top >= 2:
                    return (mul(base, 0.55), 0.0)
                return (mul(base, grain), 1.0)
            if face == "F" and top == 0 and nu // 2 - 2 <= i <= nu // 2 + 1:
                return (skin, 1.0)                        # collar opening
            if top == nv - 3 or top == nv - 4:
                return (mul(base, 0.55), 1.0)             # belt
            if kind in ("villager", "illager", "witch", "zombie_villager") and face == "F" and 2 <= i <= nu - 3 and top > 1:
                return (mul(base, 0.8 * grain), 1.0)      # robe panel
            return (mul(base, grain), 1.0)
        if part in ("right_arm", "left_arm"):
            if kind == "skeleton":
                return (mul(base, grain), 1.0)
            sleeve = 4 if kind in ("human", "villager", "illager", "witch") else 3
            if top < sleeve or kind in ("villager", "illager", "witch"):
                return (mul(base, grain), 1.0)
            return (mul(skin, 0.96 + 0.08 * noise(i, j, seed)), 1.0)
        if part in ("right_leg", "left_leg"):
            if j < 2:
                return (mul(hexc(0x4D4D52), grain), 0.0)  # shoes
            return (mul(base, grain), 1.0)
        return (mul(base, grain), 1.0)

    if style == "quad":
        if part == "head":
            c, a = animal_head(rig, face, i, j, nu, nv, base)
            return (mul(c, grain), a)
        if part == "snout":
            if face == "F" and (i == 1 or i == nu - 2) and nv >= 2 and j == nv // 2:
                return (mul(base, 0.45), 0.0)             # nostrils
            return (mul(base, grain), 0.0)
        if part in ("right_horn", "left_horn"):
            return (mul(base, 1.0 - 0.25 * (j / max(1, nv - 1))), 0.0)
        if part == "body":
            if rig in ("cow", "mooshroom"):
                if noise(i // 3, j // 3, seed + (0 if face in ("F", "B") else 5)) > 0.62:
                    return (mix(base, WHITE, 0.85), 0.0)  # patches
            if rig == "panda" and 0.3 < (j / max(1, nv - 1)) < 0.7:
                return (BLACK, 0.0)                        # shoulder band
            if rig == "turtle" and face == "U":
                return (mix(base, hexc(0x3D6B3D), 0.5 + 0.5 * noise(i // 2, j // 2, seed)), 1.0)
            if rig == "zombie_horse" and noise(i, j, seed) > 0.85:
                return (hexc(0x5C3333), 0.0)
            if rig == "skeleton_horse" and face == "S" and j % 3 == 0:
                return (mul(base, 0.6), 1.0)
            if face == "D":
                return (mul(base, 0.8), 1.0)
            return (mul(base, grain), 1.0)
        if part == "wool":
            fluff = 0.86 + 0.2 * noise(i, j, seed) - 0.06 * (1 if (i + j) % 2 else 0)
            return (mul(base, fluff), 1.0)
        if "leg" in part:
            if j < 2:
                return (mul(hexc(0x3D3530), grain), 0.0)  # hooves / paws
            return (mul(base, grain), 1.0)
        return (mul(base, grain), 1.0)

    if style == "bird":
        if part == "head" and face == "S" and top == nv // 2 - 1 and i == nu - 2:
            return (BLACK, 0.0)
        if part == "head" and rig == "chicken" and face == "F" and j < 2 and nu // 2 - 1 <= i <= nu // 2:
            return (hexc(0xD93333), 0.0)                   # wattle
        if part == "body" and rig == "bee" and face != "D" and (i // 2) % 2 == 1:
            return (mul(hexc(0x33291A), grain), 0.0)      # stripes
        if part in ("right_wing", "left_wing") and rig == "bee":
            return (hexc(0xDDE8F0), 0.0)
        if part in ("right_wing", "left_wing"):
            return (mul(base, 0.85 + 0.1 * (i / max(1, nu - 1))), 1.0)
        return (mul(base, grain), 1.0)

    if style == "fish":
        if part == "head" and face == "S" and top == nv // 2 - 1 and i == nu - 2:
            return (BLACK, 0.0)
        if part == "body":
            if rig == "tropical_fish" and (i // 2) % 3 == 1:
                return (hexc(0xF2F2F2), 0.0)
            if face == "D" or (face == "S" and j < nv // 3):
                return (mix(base, WHITE, 0.45), 1.0)       # pale belly
            if rig in ("guardian", "elder_guardian") and noise(i // 2, j // 2, seed) > 0.8:
                return (hexc(0xE69E4D), 0.0)               # spikes spots
        return (mul(base, grain), 1.0)

    if style == "squid":
        if part == "body" and face == "S" and top == nv // 2 and nu // 2 - 1 <= i <= nu // 2:
            return (BLACK, 0.0)
        if rig == "glow_squid" and noise(i, j, seed) > 0.8:
            return (hexc(0x9EF0DB), 0.5)
        return (mul(base, grain), 1.0)

    if style == "spider":
        if part == "head" and face == "F" and top < 3 and (i % 3 == 1):
            return (mul(base, 0.6), 0.0)
        if face == "U" and part in ("abdomen", "body") and noise(i, j, seed) > 0.8:
            return (mul(base, 1.35), 0.0)
        return (mul(base, grain), 1.0)

    if style == "slime":
        if part == "eyes":
            if face == "F" and nv >= 3:
                if top <= 1 and (i in (1, 2, nu - 3, nu - 2)):
                    return (BLACK, 0.0)
                if top == nv - 1 and nu // 2 - 1 <= i <= nu // 2:
                    return (BLACK, 0.0)
            return (hexc(BODY_OF.get(rig, 0x6BC76B)), 1.0)
        if part == "body" and rig == "magma_cube" and noise(i, j, seed + (name_seed(face) & 7)) > 0.82:
            return (hexc(0xF5B233), 0.5)                   # glowing cracks
        if part == "body":
            return (mul(base, 0.92 + 0.12 * noise(i, j, seed)), 1.0)
        return (base, 1.0)

    if style == "ghast":
        if part == "body" and face == "F":
            u = i / max(1, nu - 1)
            v = j / max(1, nv - 1)
            if 0.55 < v < 0.62 and (0.18 < u < 0.38 or 0.62 < u < 0.82):
                return (BLACK, 0.0)                        # closed eyes
            if 0.28 < v < 0.40 and 0.42 < u < 0.58:
                return (BLACK, 0.0)                        # mouth
            if rig == "ghast" and 0.35 < v < 0.55 and (0.24 < u < 0.28 or 0.72 < u < 0.76):
                return (hexc(0x9E9EAA), 0.0)               # tear streaks
        return (mul(base, 0.95 + 0.07 * noise(i, j, seed)), 1.0)

    if style == "creeper":
        if part == "head" and face == "F":
            if top in (2, 3) and i in (1, 2, nu - 3, nu - 2):
                return (BLACK, 0.0)
            if top in (4, 5) and nu // 2 - 1 <= i <= nu // 2:
                return (BLACK, 0.0)
            if top in (5, 6, 7) and i in (nu // 2 - 2, nu // 2 + 1) and top != 4:
                return (BLACK, 0.0)
        mottle = noise(i, j, seed + (name_seed(face) & 15))
        if mottle > 0.78:
            return (mix(base, hexc(0xB8E6A8), 0.45), 1.0)
        if mottle < 0.18:
            return (mul(base, 0.72), 1.0)
        return (mul(base, grain), 1.0)

    if style == "enderman":
        if part == "head" and face == "F" and top == 4 and (i in (0, 1, 2, nu - 3, nu - 2, nu - 1)):
            return ((hexc(0xE0A8F5) if i in (1, nu - 2) else hexc(0xB866E0)), 0.5)
        return (mul(base, 0.9 + 0.2 * noise(i, j, seed)), 1.0)

    if style in ("iron_golem", "copper_golem"):
        if part == "head" and face == "F" and top == 3 and i in (1, 2, nu - 3, nu - 2):
            return ((hexc(0xB82E2E) if style == "iron_golem" else BLACK), 0.0)
        if style == "iron_golem" and face in ("F", "S") and part == "body" and noise(i // 1, j // 2, seed) > 0.86:
            return (hexc(0x4D8C38), 0.0)                   # vines
        if style == "copper_golem" and noise(i, j, seed) > 0.8:
            return (hexc(0x5CB894), 0.0)                   # verdigris
        return (mul(base, 0.88 + 0.2 * noise(i, j, seed)), 1.0)

    if style == "snow_golem":
        if part == "head" and face == "F":
            if top == 3 and i in (1, 2, nu - 3, nu - 2):
                return (BLACK, 0.0)
            if top == 6 and 2 <= i <= nu - 3:
                return (mul(base, 0.5), 0.0)
            return (mul(base, grain), 0.0)                 # carved pumpkin
        if part == "head" and (face == "U" or face == "S"):
            return (mul(base, 0.9 if (i % 2) else 1.0), 0.0)
        return (mul(base, 0.95 + 0.06 * noise(i, j, seed)), 1.0)

    if style == "shulker":
        if part == "head":
            if face == "F" and top == 2 and i in (1, nu - 2):
                return (BLACK, 0.0)
            return (mul(base, grain), 0.0)
        if (face in ("S", "F", "B") and (j == 0 or j == nv - 1)):
            return (mul(base, 0.65), 1.0)                  # shell rim
        return (mul(base, grain), 1.0)

    if style == "blaze":
        if part == "head" and face == "F":
            if top == 3 and i in (1, 2, nu - 3, nu - 2):
                return (hexc(0x2E1F0F), 0.0)
            if top == 5 and 2 <= i <= nu - 3:
                return (hexc(0x5C2E0F), 0.0)
        return (mix(base, hexc(0xF58F1F), noise(i, j, seed) * 0.5), 1.0)

    if style == "ender_dragon":
        scale = noise(i // 2, j // 2, seed)
        if part in ("right_wing", "left_wing"):
            if face in ("U", "D") and (i % 6 == 0):
                return (mul(base, 0.6), 1.0)               # wing bones
            return (mul(base, 0.85 + 0.2 * noise(i, j, seed)), 1.0)
        if face == "U" and part in ("body", "neck0", "neck1", "neck2", "neck3", "neck4") and i == nu // 2 and j % 3 == 0:
            return (hexc(0x807399), 0.0)                   # dorsal plates
        return (mul(base, 0.85 + 0.3 * scale), 1.0)

    if style == "wither":
        if part.startswith("head") and face == "F":
            if top == 3 and i in (1, 2, nu - 3, nu - 2):
                return (BLACK, 0.0)
            if top == 5 and 2 <= i <= nu - 3 and i % 2 == 0:
                return (BLACK, 0.0)
        if part == "body" and face in ("F", "B") and j % 3 == 1 and 1 <= i <= nu - 2:
            return (mul(base, 1.6), 0.0)                   # ribs
        return (mul(base, 0.85 + 0.3 * noise(i, j, seed)), 1.0)

    # props: framed crate
    edge = (i == 0 or j == 0 or i == nu - 1 or j == nv - 1)
    if edge and nu > 2 and nv > 2:
        return (mul(base, 0.7), 1.0)
    return (mul(base, grain), 1.0)


HAIR = {"player": 0x2E2319, "steve": 0x50301B, "alex": 0xD9862E, "human": 0x50301B, "villager": 0x5C3D23,
        "wandering_trader": 0x59524C, "vindicator": 0x2E2E33, "evoker": 0x2E2E33, "pillager": 0x2E2E33,
        "illusioner": 0x2E2E3D, "witch": 0x33332E, "piglin": 0x4D2E1F, "piglin_brute": 0x3D2419,
        "zombified_piglin": 0x2E4D2E, "creaking": 0x2E2317, "warden": 0x1A2E33}
SKIN_C = {"player": 0xD6A67D, "steve": 0xD6A67D, "alex": 0xE6B891, "human": 0xD6A67D,
          "zombie": 0x709E70, "husk": 0x9E8F6B, "drowned": 0x618C80, "parched": 0xA89473,
          "zombie_villager": 0x759E7A, "zombie_horse_rider": 0x709E70, "skeleton": 0xE0E0D6,
          "stray": 0xC7D1D6, "bogged": 0x8C9966, "wither_skeleton": 0x4D4D4D, "skeleton_horse_rider": 0xE0E0D6,
          "villager": 0xCCA380, "wandering_trader": 0xC79E7A, "vindicator": 0xD1A880, "evoker": 0xD6AD85,
          "pillager": 0xCCA380, "illusioner": 0xD6B38A, "witch": 0xC7A880, "piglin": 0xE69E8A,
          "piglin_brute": 0xD99480, "zombified_piglin": 0x8CA880, "creaking": 0x705738, "warden": 0x1F3D47}
BODY_OF = {"slime": 0x6BC76B, "magma_cube": 0x9E3D24, "sulfur_cube": 0xE6D64D}


# ---------------------------------------------------------------- geometry

def face_defs(x0, y0, z0, x1, y1, z1):
    """(id, origin, u vector, v vector, normal) for the six faces, in Unreal space."""
    return [
        ("F", (x1, y0, z0), (0.0, y1 - y0, 0.0), (0.0, 0.0, z1 - z0), (1.0, 0.0, 0.0)),
        ("B", (x0, y1, z0), (0.0, y0 - y1, 0.0), (0.0, 0.0, z1 - z0), (-1.0, 0.0, 0.0)),
        ("S", (x0, y1, z0), (x1 - x0, 0.0, 0.0), (0.0, 0.0, z1 - z0), (0.0, 1.0, 0.0)),
        ("S", (x1, y0, z0), (x0 - x1, 0.0, 0.0), (0.0, 0.0, z1 - z0), (0.0, -1.0, 0.0)),
        ("U", (x0, y0, z1), (0.0, y1 - y0, 0.0), (x1 - x0, 0.0, 0.0), (0.0, 0.0, 1.0)),
        ("D", (x0, y0, z0), (0.0, y1 - y0, 0.0), (x1 - x0, 0.0, 0.0), (0.0, 0.0, -1.0)),
    ]


def ue_to_bl(p):
    return (p[0], -p[1], p[2])


def texel_count(length):
    n = int(round(abs(length) * PX))
    return max(1, min(MAX_TEXELS, n))


def build_part_mesh(rig, style, part, box_min, box_max, color, emissive):
    """Texel-grid box around the origin (= the part pivot), returns a new mesh."""
    bm = bmesh.new()
    col_layer = bm.loops.layers.float_color.new("Color")
    base = hexc(color)
    x0, y0, z0 = box_min
    x1, y1, z1 = box_max
    # keep degenerate boxes visible: a hairline thickness instead of zero-area faces
    if x1 - x0 < 0.004:
        x1 = x0 + 0.004
    if y1 - y0 < 0.004:
        y1 = y0 + 0.004
    if z1 - z0 < 0.004:
        z1 = z0 + 0.004
    for face in face_defs(x0, y0, z0, x1, y1, z1):
        fid, o, uvec, vvec, n = face
        ulen = math.sqrt(uvec[0] ** 2 + uvec[1] ** 2 + uvec[2] ** 2)
        vlen = math.sqrt(vvec[0] ** 2 + vvec[1] ** 2 + vvec[2] ** 2)
        nu = texel_count(ulen)
        nv = texel_count(vlen)
        verts = []
        for jv in range(nv + 1):
            row = []
            for iu in range(nu + 1):
                fu = iu / nu
                fv = jv / nv
                p = (o[0] + uvec[0] * fu + vvec[0] * fv,
                     o[1] + uvec[1] * fu + vvec[1] * fv,
                     o[2] + uvec[2] * fu + vvec[2] * fv)
                row.append(bm.verts.new(ue_to_bl(p)))
            verts.append(row)
        # winding: after the y flip, (u x v) must point along the Blender normal
        ub = ue_to_bl(uvec)
        vb = ue_to_bl(vvec)
        nb = ue_to_bl(n)
        cx = ub[1] * vb[2] - ub[2] * vb[1]
        cy = ub[2] * vb[0] - ub[0] * vb[2]
        cz = ub[0] * vb[1] - ub[1] * vb[0]
        flip = (cx * nb[0] + cy * nb[1] + cz * nb[2]) < 0.0
        # shading baked per face: top faces lit, bottoms in shadow, sides graded
        for jv in range(nv):
            for iu in range(nu):
                quad = [verts[jv][iu], verts[jv][iu + 1], verts[jv + 1][iu + 1], verts[jv + 1][iu]]
                if flip:
                    quad.reverse()
                f = bm.faces.new(quad)
                srgb, alpha = paint(rig, style, part, fid, iu, jv, nu, nv, base, emissive)
                if fid == "U":
                    shade = 1.04
                elif fid == "D":
                    shade = 0.78
                else:
                    shade = 0.9 + 0.1 * (jv + 0.5) / nv
                lin = to_linear(mul(srgb, shade))
                for loop in f.loops:
                    loop[col_layer] = (lin[0], lin[1], lin[2], alpha)
    mesh = bpy.data.meshes.new("SM_%s__%s" % (rig, part))
    bm.to_mesh(mesh)
    bm.free()
    mesh.shade_flat()
    mark_colors(mesh)
    return mesh


def mark_colors(mesh):
    """Flag the colour attribute as active + render colour: the viewport, Workbench and
    the FBX exporter all read those flags."""
    attr = mesh.color_attributes.get("Color")
    if attr is not None:
        mesh.color_attributes.active_color = attr
        mesh.color_attributes.render_color_index = mesh.color_attributes.active_color_index


def export_object(obj, path):
    bpy.ops.object.select_all(action='DESELECT')
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.export_scene.fbx(
        filepath=path, use_selection=True, apply_unit_scale=True, global_scale=1.0,
        object_types={'MESH'}, mesh_smooth_type='FACE', use_mesh_modifiers=False,
        path_mode='AUTO', embed_textures=False, colors_type='LINEAR', add_leaf_bones=False)


def rig_collection(rig):
    root = bpy.data.collections.get("Opus55_Rigs")
    if root is None:
        root = bpy.data.collections.new("Opus55_Rigs")
        bpy.context.scene.collection.children.link(root)
    name = "Rig_" + rig
    c = bpy.data.collections.get(name)
    if c is None:
        c = bpy.data.collections.new(name)
        root.children.link(c)
    for obj in list(c.objects):
        bpy.data.objects.remove(obj, do_unlink=True)
    return c


def build_rig(rig, parts, slot):
    style = style_of(rig)
    coll = rig_collection(rig)
    out_dir = bpy.path.abspath(EXPORT_DIR) + "/Mobs/" + rig
    gx = (slot % 12) * GRID
    gy = -(slot // 12) * GRID
    written = 0
    for entry in parts:
        name, parent, role, pivot, box_min, box_max, color, emissive = entry
        mesh = build_part_mesh(rig, style, name, box_min, box_max, color, emissive)
        obj = bpy.data.objects.new("SM_%s__%s" % (rig, name), mesh)
        coll.objects.link(obj)
        # exported at the origin: the pivot is the mesh origin
        obj.location = (0.0, 0.0, 0.0)
        export_object(obj, "%s/SM_%s__%s.fbx" % (out_dir, rig, name))
        # model sheet: assembled at the rest pose, one grid cell per rig
        p = ue_to_bl(pivot)
        obj.location = (gx + p[0], gy + p[1], p[2])
        written += 1
    return written


# ---------------------------------------------------------------- entity props

def box_list_mesh(name, boxes):
    """Several painted boxes merged into one mesh (props); boxes in Unreal space."""
    bm_all = bmesh.new()
    col_layer = bm_all.loops.layers.float_color.new("Color")
    for b in boxes:
        bmin, bmax, color, alpha, pattern = b
        m = build_part_mesh("prop", "prop", pattern, bmin, bmax, color, alpha == 0.5)
        tmp = bmesh.new()
        tmp.from_mesh(m)
        src_layer = tmp.loops.layers.float_color.get("Color")
        vmap = {}
        for v in tmp.verts:
            vmap[v.index] = bm_all.verts.new(v.co)
        for f in tmp.faces:
            nf = bm_all.faces.new([vmap[v.index] for v in f.verts])
            for k in range(len(f.loops)):
                c = f.loops[k][src_layer]
                if alpha != 1.0:
                    c = (c[0], c[1], c[2], alpha)
                nf.loops[k][col_layer] = c
        tmp.free()
        bpy.data.meshes.remove(m)
    mesh = bpy.data.meshes.new(name)
    bm_all.to_mesh(mesh)
    bm_all.free()
    mesh.shade_flat()
    mark_colors(mesh)
    return mesh


def build_entities(slot):
    coll = rig_collection("entities")
    out_dir = bpy.path.abspath(EXPORT_DIR) + "/Entities"
    light = 0xE6E6E6
    props = [
        # first-person arm: wrist at the origin, forearm running back along -X
        ("SM_PlayerArm", [((-0.44, -0.06, -0.06), (0.0, 0.06, 0.06), 0xD6A67D, 1.0, "arm"),
                          ((-0.44, -0.065, -0.065), (-0.30, 0.065, 0.065), 0x3389C7, 0.0, "sleeve")]),
        # boat hull (tinted by the wood colour at runtime)
        ("SM_Boat", [((-0.75, -0.45, 0.0), (0.75, 0.45, 0.1), light, 1.0, "planks"),
                     ((-0.75, -0.5, 0.0), (0.75, -0.42, 0.4), light, 1.0, "planks"),
                     ((-0.75, 0.42, 0.0), (0.75, 0.5, 0.4), light, 1.0, "planks"),
                     ((0.62, -0.45, 0.0), (0.8, 0.45, 0.35), light, 1.0, "planks"),
                     ((-0.8, -0.45, 0.0), (-0.62, 0.45, 0.35), light, 1.0, "planks"),
                     ((-0.1, -0.42, 0.2), (0.1, 0.42, 0.28), 0xC7C7C7, 1.0, "planks")]),
        # minecart: open iron tub on four wheels
        ("SM_Minecart", [((-0.5, -0.4, 0.1), (0.5, 0.4, 0.2), light, 1.0, "iron"),
                         ((-0.5, -0.45, 0.1), (0.5, -0.38, 0.62), light, 1.0, "iron"),
                         ((-0.5, 0.38, 0.1), (0.5, 0.45, 0.62), light, 1.0, "iron"),
                         ((0.43, -0.4, 0.1), (0.5, 0.4, 0.62), light, 1.0, "iron"),
                         ((-0.5, -0.4, 0.1), (-0.43, 0.4, 0.62), light, 1.0, "iron"),
                         ((0.25, -0.48, 0.0), (0.4, -0.44, 0.15), 0x333333, 0.0, "wheel"),
                         ((0.25, 0.44, 0.0), (0.4, 0.48, 0.15), 0x333333, 0.0, "wheel"),
                         ((-0.4, -0.48, 0.0), (-0.25, -0.44, 0.15), 0x333333, 0.0, "wheel"),
                         ((-0.4, 0.44, 0.0), (-0.25, 0.48, 0.15), 0x333333, 0.0, "wheel")]),
        # end crystal: glowing core inside a glass cage (centred, the entity spins it)
        ("SM_EndCrystal", [((-0.22, -0.22, -0.22), (0.22, 0.22, 0.22), 0xF08FF5, 0.5, "core")] +
                          [((-0.43, -0.43, -0.43), (0.43, -0.39, -0.39), 0xDDE8F5, 0.0, "cage"),
                           ((-0.43, 0.39, -0.43), (0.43, 0.43, -0.39), 0xDDE8F5, 0.0, "cage"),
                           ((-0.43, -0.43, 0.39), (0.43, -0.39, 0.43), 0xDDE8F5, 0.0, "cage"),
                           ((-0.43, 0.39, 0.39), (0.43, 0.43, 0.43), 0xDDE8F5, 0.0, "cage"),
                           ((-0.43, -0.43, -0.43), (-0.39, 0.43, -0.39), 0xDDE8F5, 0.0, "cage"),
                           ((0.39, -0.43, -0.43), (0.43, 0.43, -0.39), 0xDDE8F5, 0.0, "cage"),
                           ((-0.43, -0.43, 0.39), (-0.39, 0.43, 0.43), 0xDDE8F5, 0.0, "cage"),
                           ((0.39, -0.43, 0.39), (0.43, 0.43, 0.43), 0xDDE8F5, 0.0, "cage"),
                           ((-0.43, -0.43, -0.43), (-0.39, -0.39, 0.43), 0xDDE8F5, 0.0, "cage"),
                           ((0.39, -0.43, -0.43), (0.43, -0.39, 0.43), 0xDDE8F5, 0.0, "cage"),
                           ((-0.43, 0.39, -0.43), (-0.39, 0.43, 0.43), 0xDDE8F5, 0.0, "cage"),
                           ((0.39, 0.39, -0.43), (0.43, 0.43, 0.43), 0xDDE8F5, 0.0, "cage")]),
        # item frame: faces +Z, 12x12 px with a 1 px border, 1 px thick
        ("SM_ItemFrame", [((-0.375, -0.375, -0.03), (0.375, 0.375, 0.0), 0xB39973, 1.0, "back"),
                          ((-0.375, -0.375, 0.0), (0.375, -0.3125, 0.03), light, 1.0, "frame"),
                          ((-0.375, 0.3125, 0.0), (0.375, 0.375, 0.03), light, 1.0, "frame"),
                          ((-0.375, -0.3125, 0.0), (-0.3125, 0.3125, 0.03), light, 1.0, "frame"),
                          ((0.3125, -0.3125, 0.0), (0.375, 0.3125, 0.03), light, 1.0, "frame")]),
    ]
    written = 0
    for k in range(len(props)):
        name, boxes = props[k]
        mesh = box_list_mesh(name, boxes)
        obj = bpy.data.objects.new(name, mesh)
        coll.objects.link(obj)
        obj.location = (0.0, 0.0, 0.0)
        export_object(obj, "%s/%s.fbx" % (out_dir, name))
        obj.location = ((slot % 12) * GRID + k * 2.0, -(slot // 12) * GRID, 0.5)
        written += 1
    return written


# ---------------------------------------------------------------- main

def cleanup_defaults():
    for name in ("Cube", "SM_AxisTestA", "SM_AxisTestB"):
        obj = bpy.data.objects.get(name)
        if obj is not None:
            bpy.data.objects.remove(obj, do_unlink=True)


def main():
    cleanup_defaults()
    written = 0
    slot = BATCH_START
    for entry in RIG_TABLE:
        rig, parts = entry
        written += build_rig(rig, parts, slot)
        slot += 1
    if ENTITY_PASS:
        written += build_entities(slot)
    if BATCH_LAST:
        bpy.ops.wm.save_as_mainfile(filepath=bpy.data.filepath)
    print("OPUS55_BATCH rigs=%d parts=%d saved=%s" % (len(RIG_TABLE), written, str(BATCH_LAST)))


main()
