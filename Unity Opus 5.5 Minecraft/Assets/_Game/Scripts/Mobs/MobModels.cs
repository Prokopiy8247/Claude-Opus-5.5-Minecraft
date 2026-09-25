using System.Collections.Generic;
using UnityEngine;

namespace MCR
{
    /// <summary>
    /// Cuboid rig definitions for every mob and authored entity (original proportions recreated in the Minecraft block-model style).
    /// Coordinates are model pixels: feet centre at origin, +Y up, +Z front, +X = the model's right side.
    /// The same definitions drive the runtime mesher, the skin painter and the Blender FBX builder.
    /// </summary>
    public static class MobModels
    {
        static readonly Dictionary<string, ModelDef> defs = new Dictionary<string, ModelDef>();
        static bool inited;

        public static ModelDef Get(string name)
        {
            Init();
            return name != null && defs.TryGetValue(name, out var d) ? d : null;
        }
        public static IEnumerable<ModelDef> All { get { Init(); return defs.Values; } }

        static ModelDef New(string name, RigKind rig, int tw = 64, int th = 64) { var d = new ModelDef(name, tw, th, rig); defs[name] = d; return d; }
        static void Done(ModelDef d) { d.PackUVs(); }

        static void Init()
        {
            if (inited) return; inited = true;
            // ---------------------------------------------------------------- humanoids
            Biped("player", 4, 12, 4, 4, true);
            Biped("zombie", 4, 12, 4, 4, true);
            Biped("husk", 4, 12, 4, 4, true).scale = 1.0625f;
            Biped("drowned", 4, 12, 4, 4, true);
            Biped("skeleton", 2, 12, 2, 2, false);
            Biped("stray", 2, 12, 2, 2, true);
            Biped("bogged", 2, 12, 2, 2, true);
            Biped("parched", 2, 12, 2, 2, true);
            Biped("wither_skeleton", 2, 12, 2, 2, false).scale = 1.2f;
            Biped("copper_golem", 3, 6, 3, 3, false, 6, 7).scale = 1f;
            Piglin("piglin"); Piglin("piglin_brute"); Piglin("zombified_piglin");
            Villager("villager", false); Villager("wandering_trader", false); Villager("zombie_villager", true);
            Illager("pillager"); Illager("vindicator"); Illager("evoker");
            Witch();
            Enderman();
            Creeper();
            Warden();
            Creaking();
            IronGolem();
            SnowGolem();
            Allay("allay"); Allay("vex");
            // ---------------------------------------------------------------- quadrupeds
            Pig(); Cow("cow"); Cow("mooshroom"); Sheep(); Goat(); Wolf(); Cat("cat"); Cat("ocelot"); Fox();
            PolarBear(); Panda(); Hoglin("hoglin"); Hoglin("zoglin"); Ravager(); Strider(); Armadillo(); Sniffer(); Camel("camel"); Camel("camel_husk");
            Horse("horse", 1f); Horse("donkey", 0.87f); Horse("mule", 0.92f); Horse("skeleton_horse", 1f); Horse("zombie_horse", 1f);
            Llama("llama"); Llama("trader_llama"); Turtle(); Frog(); Axolotl(); Rabbit();
            // ---------------------------------------------------------------- others
            Chicken(); Spider("spider", 1f); Spider("cave_spider", 0.7f); Slime("slime", false); Slime("magma_cube", true); Slime("sulfur_cube", false);
            Ghast("ghast", 1f); Ghast("happy_ghast", 1.25f); Ghast("ghastling", 0.35f);
            Blaze(); Shulker(); Bat(); Bee(); Parrot(); Squid("squid"); Squid("glow_squid"); Fish("cod", 2, 4, 7); Fish("salmon", 3, 5, 12); Fish("tropical_fish", 2, 5, 5); Pufferfish(); Dolphin();
            Guardian("guardian", 1f); Guardian("elder_guardian", 2.35f); Phantom(); Silverfish("silverfish"); Silverfish("endermite"); Breeze(); Tadpole(); Nautilus("nautilus"); Nautilus("zombie_nautilus");
            Dragon(); Wither();
            // ---------------------------------------------------------------- props / vehicles / held models
            Minecart(); Boat("boat", false); Boat("chest_boat", true); Raft(); EndCrystal(); ChestModel("chest"); ChestModel("ender_chest"); ChestModel("trapped_chest"); ChestModel("copper_chest");
            ShieldModel(); TridentModel(); SpearModel(); BowModel(); CrossbowModel();
            ArmorModel();
        }

        // ================================================================= humanoid families
        static ModelDef Biped(string name, float armW, float armH, float legW, float legD, bool jacket, float bodyH = 12, float headS = 8)
        {
            var d = New(name, RigKind.Biped);
            float legH = 12, bh = bodyH;
            if (name == "copper_golem") { legH = 5; bh = 6; }
            float bodyBottom = legH, top = legH + bh;
            d.Bone("body", null, 0, top, 0).C(-4, bodyBottom, -2, 8, bh, 4);
            var head = d.Bone("head", "body", 0, top, 0).C(-headS / 2, top, -headS / 2, headS, headS, headS);
            if (jacket) head.C(-headS / 2, top, -headS / 2, headS, headS, headS, 0.5f, "hat");
            if (name == "copper_golem") { head.C(-1, top + headS, -1, 2, 3, 2, 0, "rod"); head.C(-1.5f, top + headS + 3, -1.5f, 3, 3, 3, 0, "bulb"); head.C(-1, top + 2, headS / 2, 2, 3, 1, 0, "nose"); }
            float ax = 4;
            d.Bone("right_arm", "body", ax + armW / 2, top - 2, 0).C(ax, top - armH, -armW / 2, armW, armH, armW);
            d.Bone("left_arm", "body", -ax - armW / 2, top - 2, 0).C(-ax - armW, top - armH, -armW / 2, armW, armH, armW);
            float lx = legW <= 2 ? 2 : 2;
            d.Bone("right_leg", null, lx, legH, 0).C(lx - legW / 2, 0, -legD / 2, legW, legH, legD);
            d.Bone("left_leg", null, -lx, legH, 0).C(-lx - legW / 2, 0, -legD / 2, legW, legH, legD);
            if (jacket && name != "copper_golem") d.Get("body").C(-4, bodyBottom, -2, 8, bh, 4, 0.25f, "jacket");
            Done(d);
            return d;
        }

        static void Piglin(string name)
        {
            var d = New(name, RigKind.Biped);
            d.Bone("body", null, 0, 24, 0).C(-4, 12, -2, 8, 12, 4).C(-4, 12, -2, 8, 12, 4, 0.25f, "jacket");
            var h = d.Bone("head", "body", 0, 24, 0).C(-5, 24, -4, 10, 8, 8).C(-2, 24, 4, 4, 4, 1, 0, "snout");
            h.C(-1.5f, 24, 4, 1, 2, 1, 0, "tusk_r").C(0.5f, 24, 4, 1, 2, 1, 0, "tusk_l");
            d.Bone("right_ear", "head", 5, 30, 0, 0, 0, -30).C(5, 25, -2, 1, 5, 4);
            d.Bone("left_ear", "head", -5, 30, 0, 0, 0, 30).C(-6, 25, -2, 1, 5, 4);
            d.Bone("right_arm", "body", 6, 22, 0).C(4, 12, -2, 4, 12, 4);
            d.Bone("left_arm", "body", -6, 22, 0).C(-8, 12, -2, 4, 12, 4);
            d.Bone("right_leg", null, 2, 12, 0).C(0, 0, -2, 4, 12, 4);
            d.Bone("left_leg", null, -2, 12, 0).C(-4, 0, -2, 4, 12, 4);
            if (name == "piglin_brute") d.scale = 1.05f;
            Done(d);
        }

        static void Villager(string name, bool zombie)
        {
            var d = New(name, zombie ? RigKind.Biped : RigKind.Villager);
            d.Bone("body", null, 0, 24, 0).C(-4, 12, -3, 8, 12, 6).C(-4, 6, -3, 8, 18, 6, 0.5f, "robe");
            d.Bone("head", "body", 0, 24, 0).C(-4, 24, -4, 8, 10, 8).C(-1, 24 - 1, 4, 2, 4, 2, 0, "nose");
            if (name == "wandering_trader") d.Get("head").C(-4, 24, -4, 8, 10, 8, 0.5f, "hood");
            if (zombie)
            {
                d.Bone("right_arm", "body", 6, 22, 0).C(4, 10, -2, 4, 12, 4);
                d.Bone("left_arm", "body", -6, 22, 0).C(-8, 10, -2, 4, 12, 4);
            }
            else
            {
                var arms = d.Bone("arms", "body", 0, 20, 1, -45);
                arms.C(-4, 13, 1, 8, 4, 4, 0, "arms_mid").C(4, 13, -1, 4, 8, 4, 0, "arms_r").C(-8, 13, -1, 4, 8, 4, 0, "arms_l");
            }
            d.Bone("right_leg", null, 2, 12, 0).C(0, 0, -2, 4, 12, 4);
            d.Bone("left_leg", null, -2, 12, 0).C(-4, 0, -2, 4, 12, 4);
            Done(d);
        }

        static void Illager(string name)
        {
            var d = New(name, RigKind.Biped);
            d.Bone("body", null, 0, 24, 0).C(-4, 12, -3, 8, 12, 6).C(-4, 6, -3, 8, 18, 6, 0.5f, "robe");
            d.Bone("head", "body", 0, 24, 0).C(-4, 24, -4, 8, 10, 8).C(-1, 23, 4, 2, 4, 2, 0, "nose");
            d.Bone("right_arm", "body", 6, 22, 0).C(4, 12, -2, 4, 12, 4);
            d.Bone("left_arm", "body", -6, 22, 0).C(-8, 12, -2, 4, 12, 4);
            d.Bone("right_leg", null, 2, 12, 0).C(0, 0, -2, 4, 12, 4);
            d.Bone("left_leg", null, -2, 12, 0).C(-4, 0, -2, 4, 12, 4);
            Done(d);
        }

        static void Witch()
        {
            var d = New("witch", RigKind.Villager);
            d.Bone("body", null, 0, 24, 0).C(-4, 12, -3, 8, 12, 6).C(-4, 6, -3, 8, 18, 6, 0.5f, "robe");
            var h = d.Bone("head", "body", 0, 24, 0).C(-4, 24, -4, 8, 10, 8).C(-1, 23, 4, 2, 4, 2, 0, "nose").C(0, 22, 5.5f, 1, 1, 1, 0, "wart");
            h.C(-5, 33, -5, 10, 2, 10, 0, "hat_brim").C(-3.5f, 35, -3.5f, 7, 4, 7, 0, "hat_mid").C(-2, 39, -2, 4, 4, 4, 0, "hat_top").C(-0.5f, 43, -0.5f, 1, 2, 1, 0, "hat_tip");
            var arms = d.Bone("arms", "body", 0, 20, 1, -45);
            arms.C(-4, 13, 1, 8, 4, 4, 0, "arms_mid").C(4, 13, -1, 4, 8, 4, 0, "arms_r").C(-8, 13, -1, 4, 8, 4, 0, "arms_l");
            d.Bone("right_leg", null, 2, 12, 0).C(0, 0, -2, 4, 12, 4);
            d.Bone("left_leg", null, -2, 12, 0).C(-4, 0, -2, 4, 12, 4);
            Done(d);
        }

        static void Enderman()
        {
            var d = New("enderman", RigKind.Biped);
            d.Bone("body", null, 0, 38, 0).C(-4, 26, -2, 8, 12, 4);
            d.Bone("head", "body", 0, 38, 0).C(-4, 38, -4, 8, 8, 8).C(-4, 37, -4, 8, 8, 8, -0.5f, "jaw");
            d.Bone("right_arm", "body", 5, 36, 0).C(4, 6, -1, 2, 30, 2);
            d.Bone("left_arm", "body", -5, 36, 0).C(-6, 6, -1, 2, 30, 2);
            d.Bone("right_leg", null, 2, 26, 0).C(1, 0, -1, 2, 26, 2);
            d.Bone("left_leg", null, -2, 26, 0).C(-3, 0, -1, 2, 26, 2);
            d.Bone("held_block", "body", 0, 26, 8);
            Done(d);
        }

        static void Creeper()
        {
            var d = New("creeper", RigKind.Creeper);
            d.Bone("body", null, 0, 6, 0).C(-4, 6, -2, 8, 12, 4);
            d.Bone("head", "body", 0, 18, 0).C(-4, 18, -4, 8, 8, 8);
            d.Bone("leg_fr", null, 2, 6, 2).C(0, 0, 2, 4, 6, 4);
            d.Bone("leg_fl", null, -2, 6, 2).C(-4, 0, 2, 4, 6, 4);
            d.Bone("leg_br", null, 2, 6, -2).C(0, 0, -6, 4, 6, 4);
            d.Bone("leg_bl", null, -2, 6, -2).C(-4, 0, -6, 4, 6, 4);
            Done(d);
        }

        static void Warden()
        {
            var d = New("warden", RigKind.Biped, 128, 128);
            d.Bone("body", null, 0, 13, 0).C(-9, 13, -5, 18, 21, 11);
            d.Bone("head", "body", 0, 34, 0).C(-8, 34, -5, 16, 16, 10);
            d.Bone("right_tendril", "head", 8, 47, 0).C(8, 42, 0, 16, 16, 0.01f);
            d.Bone("left_tendril", "head", -8, 47, 0).C(-24, 42, 0, 16, 16, 0.01f);
            d.Bone("right_arm", "body", 13, 32, 1).C(9, 4, -3, 8, 28, 8);
            d.Bone("left_arm", "body", -13, 32, 1).C(-17, 4, -3, 8, 28, 8);
            d.Bone("right_leg", null, 5.5f, 13, 0).C(2.5f, 0, -3, 6, 13, 6);
            d.Bone("left_leg", null, -5.5f, 13, 0).C(-8.5f, 0, -3, 6, 13, 6);
            Done(d);
        }

        static void Creaking()
        {
            var d = New("creaking", RigKind.Biped, 64, 64);
            d.Bone("body", null, 0, 23, 0).C(-3, 23, -2, 6, 13, 5).C(-4, 34, -3, 8, 2, 6, 0, "shoulders");
            var h = d.Bone("head", "body", 0, 36, 0).C(-3, 36, -3, 6, 10, 6).C(-6, 42, -0.5f, 3, 6, 1, 0, "branch_l").C(3, 43, -0.5f, 3, 5, 1, 0, "branch_r");
            d.Bone("right_arm", "body", 5, 35, 0).C(4, 12, -1.5f, 3, 23, 3);
            d.Bone("left_arm", "body", -5, 35, 0).C(-7, 12, -1.5f, 3, 23, 3);
            d.Bone("right_leg", null, 2, 23, 0).C(0.5f, 0, -1.5f, 3, 23, 3);
            d.Bone("left_leg", null, -2, 23, 0).C(-3.5f, 0, -1.5f, 3, 23, 3);
            Done(d);
        }

        static void IronGolem()
        {
            var d = New("iron_golem", RigKind.Golem, 128, 128);
            d.Bone("body", null, 0, 17, 0).C(-9, 26, -5.5f, 18, 12, 11).C(-4.5f, 21, -3, 9, 5, 6, 0.5f, "waist");
            d.Bone("head", "body", 0, 38, -1).C(-4, 36, -3.5f, 8, 10, 8).C(-1, 36, 4.5f, 2, 4, 2, 0, "nose");
            d.Bone("right_arm", "body", 11, 37, 0).C(9, 7, -3, 4, 30, 6);
            d.Bone("left_arm", "body", -11, 37, 0).C(-13, 7, -3, 4, 30, 6);
            d.Bone("right_leg", null, 4, 21, 0).C(1.5f, 0, -2.5f, 6, 16, 5);
            d.Bone("left_leg", null, -4, 21, 0).C(-7.5f, 0, -2.5f, 6, 16, 5);
            Done(d);
        }

        static void SnowGolem()
        {
            var d = New("snow_golem", RigKind.Biped);
            d.Bone("lower", null, 0, 0, 0).C(-6, 0, -6, 12, 12, 12);
            d.Bone("body", "lower", 0, 12, 0).C(-5, 11, -5, 10, 10, 10);
            d.Bone("head", "body", 0, 21, 0).C(-4, 21, -4, 8, 8, 8).C(-4, 21, -4, 8, 8, 8, 0.5f, "pumpkin");
            d.Bone("right_arm", "body", 5, 18, 0, 0, 0, -50).C(5, 17, -1, 12, 2, 2);
            d.Bone("left_arm", "body", -5, 18, 0, 0, 0, 50).C(-17, 17, -1, 12, 2, 2);
            Done(d);
        }

        static void Allay(string name)
        {
            var d = New(name, RigKind.Flyer, 32, 32);
            d.Bone("body", null, 0, 4, 0).C(-1.5f, 0, -1, 3, 4, 2).C(-1.5f, -1, -1, 3, 5, 2, -0.2f, "robe");
            d.Bone("head", "body", 0, 4, 0).C(-2.5f, 4, -2.5f, 5, 5, 5);
            d.Bone("right_arm", "body", 1.75f, 3.5f, 0).C(1.5f, 0, -0.5f, 1, 4, 1);
            d.Bone("left_arm", "body", -1.75f, 3.5f, 0).C(-2.5f, 0, -0.5f, 1, 4, 1);
            d.Bone("right_wing", "body", 0.5f, 3, -1).C(0.5f, -2, -1, 0.01f, 5, 8).Last.name = "wing";
            d.Bone("left_wing", "body", -0.5f, 3, -1).C(-0.5f, -2, -1, 0.01f, 5, 8);
            foreach (var b in new[] { "right_wing", "left_wing" }) d.Get(b).rotation = new Vector3(0, b == "right_wing" ? 150 : 210, 0);
            Done(d);
        }

        // ================================================================= quadrupeds
        static void Legs4(ModelDef d, float sx, float fz, float bz, float w, float h, float dz, float yTop)
        {
            d.Bone("leg_fr", null, sx, yTop, fz).C(sx - w / 2, yTop - h, fz - dz / 2, w, h, dz);
            d.Bone("leg_fl", null, -sx, yTop, fz).C(-sx - w / 2, yTop - h, fz - dz / 2, w, h, dz);
            d.Bone("leg_br", null, sx, yTop, bz).C(sx - w / 2, yTop - h, bz - dz / 2, w, h, dz);
            d.Bone("leg_bl", null, -sx, yTop, bz).C(-sx - w / 2, yTop - h, bz - dz / 2, w, h, dz);
        }

        static void Pig()
        {
            var d = New("pig", RigKind.Quadruped);
            d.Bone("body", null, 0, 11, 0).C(-5, 6, -8, 10, 8, 16);
            d.Bone("head", "body", 0, 12, 7).C(-4, 8, 7, 8, 8, 8).C(-2, 9, 15, 4, 3, 1, 0, "snout");
            Legs4(d, 3, 5, -5, 4, 6, 4, 6);
            Done(d);
        }

        static void Cow(string name)
        {
            var d = New(name, RigKind.Quadruped);
            d.Bone("body", null, 0, 17, 0).C(-6, 12, -9, 12, 10, 18).C(-2, 10.5f, -6, 4, 1.5f, 6, 0, "udder");
            var h = d.Bone("head", "body", 0, 20, 8).C(-4, 16, 8, 8, 8, 6).C(-3, 16, 14, 6, 3, 1, 0, "muzzle");
            h.C(4, 22, 10, 1, 3, 1, 0, "horn_r").C(-5, 22, 10, 1, 3, 1, 0, "horn_l");
            Legs4(d, 4, 6, -7, 4, 12, 4, 12);
            if (name == "mooshroom") d.Get("body").C(-3, 22, -3, 6, 5, 0.01f, 0, "mushroom1");
            Done(d);
        }

        static void Sheep()
        {
            var d = New("sheep", RigKind.Quadruped);
            d.Bone("body", null, 0, 16, 0).C(-4, 12, -8, 8, 6, 16).C(-4, 12, -8, 8, 6, 16, 1.75f, "wool");
            d.Bone("head", "body", 0, 18, 7).C(-3, 15, 7, 6, 6, 8).C(-3, 15, 7, 6, 6, 6, 0.6f, "wool_head");
            Legs4(d, 3, 5, -5, 4, 12, 4, 12);
            foreach (var l in new[] { "leg_fr", "leg_fl", "leg_br", "leg_bl" }) { var b = d.Get(l); var c = b.cubes[0]; b.C(c.origin.x, c.origin.y + 6, c.origin.z, c.size.x, 6, c.size.z, 0.5f, "wool_leg"); }
            Done(d);
        }

        static void Goat()
        {
            var d = New("goat", RigKind.Quadruped);
            d.Bone("body", null, 0, 17, 0).C(-4, 10, -7, 8, 7, 13).C(-4.5f, 11, -7, 9, 7, 7, 0, "fluff");
            var h = d.Bone("head", "body", 0, 18, 6, -30).C(-2.5f, 13, 6, 5, 7, 10).C(-0.5f, 11, 13, 1, 3, 1, 0, "beard");
            h.C(1, 19, 9, 2, 7, 2, 0, "horn_r").C(-3, 19, 9, 2, 7, 2, 0, "horn_l").C(2.5f, 17, 8, 3, 1, 2, 0, "ear_r").C(-5.5f, 17, 8, 3, 1, 2, 0, "ear_l");
            Legs4(d, 2.5f, 4, -5, 3, 10, 3, 10);
            Done(d);
        }

        static void Wolf()
        {
            var d = New("wolf", RigKind.Quadruped);
            d.Bone("body", null, 0, 10, 0).C(-3, 7, -6, 6, 6, 9).C(-4, 7, 1, 8, 7, 6, 0, "mane");
            var h = d.Bone("head", "body", 0, 11, 7).C(-3, 8, 7, 6, 6, 4).C(-1.5f, 8, 11, 3, 3, 4, 0, "snout");
            h.C(1, 14, 8, 2, 2, 1, 0, "ear_r").C(-3, 14, 8, 2, 2, 1, 0, "ear_l");
            d.Bone("tail", "body", 0, 12, -6, 40).C(-1, 4, -7, 2, 8, 2);
            Legs4(d, 1.5f, 3, -4, 2, 8, 2, 8);
            Done(d);
        }

        static void Cat(string name)
        {
            var d = New(name, RigKind.Quadruped);
            d.Bone("body", null, 0, 8, 0).C(-2, 5, -8, 4, 5, 13);
            var h = d.Bone("head", "body", 0, 10, 5).C(-2.5f, 7, 5, 5, 4, 5).C(-1.5f, 7, 10, 3, 2, 1, 0, "nose");
            h.C(1, 11, 7, 1, 1, 2, 0, "ear_r").C(-2, 11, 7, 1, 1, 2, 0, "ear_l");
            d.Bone("tail", "body", 0, 9, -8, 60).C(-0.5f, 1, -9, 1, 8, 1);
            Legs4(d, 1.1f, 3, -6, 2, 6, 2, 6);
            Done(d);
        }

        static void Fox()
        {
            var d = New("fox", RigKind.Quadruped);
            d.Bone("body", null, 0, 7, 0).C(-3, 4, -5, 6, 6, 11);
            var h = d.Bone("head", "body", 0, 8, 6).C(-4, 5, 6, 8, 6, 6).C(-2, 5, 12, 4, 2, 3, 0, "snout");
            h.C(1.5f, 11, 8, 2, 2, 1, 0, "ear_r").C(-3.5f, 11, 8, 2, 2, 1, 0, "ear_l");
            d.Bone("tail", "body", 0, 8, -5, 30).C(-2, 3, -14, 4, 9, 5);
            Legs4(d, 1.5f, 3, -3, 2, 4, 2, 4);
            Done(d);
        }

        static void PolarBear()
        {
            var d = New("polar_bear", RigKind.Quadruped, 128, 64);
            d.Bone("body", null, 0, 14, 0).C(-7, 10, -12, 14, 14, 22).C(-6, 12, 7, 12, 11, 5, 0, "shoulder");
            var h = d.Bone("head", "body", 0, 20, 12).C(-3.5f, 15, 12, 7, 7, 7).C(-2.5f, 15, 19, 5, 3, 3, 0, "snout");
            h.C(2.5f, 21, 14, 2, 2, 1, 0, "ear_r").C(-4.5f, 21, 14, 2, 2, 1, 0, "ear_l");
            Legs4(d, 4.5f, 7, -8, 4, 10, 6, 10);
            Done(d);
        }

        static void Panda()
        {
            var d = New("panda", RigKind.Quadruped, 128, 64);
            d.Bone("body", null, 0, 13, 0).C(-9.5f, 9, -13, 19, 16, 26);
            var h = d.Bone("head", "body", 0, 16, 13).C(-6.5f, 10, 13, 13, 10, 9).C(-3.5f, 10, 22, 7, 5, 2, 0, "snout");
            h.C(3.5f, 19, 15, 5, 4, 1, 0, "ear_r").C(-8.5f, 19, 15, 5, 4, 1, 0, "ear_l");
            Legs4(d, 5.5f, 9, -9, 6, 9, 6, 9);
            Done(d);
        }

        static void Hoglin(string name)
        {
            var d = New(name, RigKind.Quadruped, 128, 64);
            d.Bone("body", null, 0, 19, 0).C(-8, 13, -10, 16, 14, 19).C(0, 27, -9, 0.01f, 10, 19, 0, "mane");
            var h = d.Bone("head", "body", 0, 21, 9, 50).C(-7, 12, 9, 14, 6, 19).C(7, 18, 9, 2, 11, 2, 0, "tusk_r").C(-9, 18, 9, 2, 11, 2, 0, "tusk_l");
            h.C(7, 17, 11, 6, 1, 4, 0, "ear_r").C(-13, 17, 11, 6, 1, 4, 0, "ear_l");
            Legs4(d, 5, 6, -6, 6, 14, 6, 14);
            Done(d);
        }

        static void Ravager()
        {
            var d = New("ravager", RigKind.Quadruped, 128, 128);
            d.Bone("body", null, 0, 22, 0).C(-7, 15, -14, 14, 16, 20).C(-6, 13, -14, 12, 13, 18, 0, "belly");
            d.Bone("neck", "body", 0, 26, 5).C(-5, 22, 5, 10, 10, 18);
            var h = d.Bone("head", "neck", 0, 28, 20).C(-8, 18, 20, 16, 20, 16).C(-2, 16, 36, 4, 8, 4, 0, "nose");
            h.C(8, 32, 26, 2, 14, 4, 0, "horn_r").C(-10, 32, 26, 2, 14, 4, 0, "horn_l");
            d.Bone("mouth", "head", 0, 20, 21).C(-8, 16, 21, 16, 3, 16);
            Legs4(d, 5, 7, -9, 8, 22, 8, 22);
            Done(d);
        }

        static void Strider()
        {
            var d = New("strider", RigKind.Quadruped, 64, 128);
            d.Bone("body", null, 0, 16, 0).C(-8, 16, -8, 16, 14, 16);
            for (int i = 0; i < 6; i++) d.Get("body").C(-8 + i * 3, 30, 0, 0.01f, 8, 12, 0, "bristle" + i);
            d.Bone("right_leg", null, 4, 16, 0).C(2, 0, -2, 4, 16, 4);
            d.Bone("left_leg", null, -4, 16, 0).C(-6, 0, -2, 4, 16, 4);
            d.Bone("head", "body", 0, 23, 0);
            Done(d);
        }

        static void Armadillo()
        {
            var d = New("armadillo", RigKind.Quadruped);
            d.Bone("body", null, 0, 5, 0).C(-4, 2, -5, 8, 6, 10).C(-4, 2, -5, 8, 6, 10, 0.3f, "shell");
            var h = d.Bone("head", "body", 0, 6, 5, -20).C(-1.5f, 3, 5, 3, 5, 2).C(1, 7, 5, 1, 3, 1, 0, "ear_r").C(-2, 7, 5, 1, 3, 1, 0, "ear_l");
            d.Bone("tail", "body", 0, 4, -5, 60).C(-0.5f, 0, -6, 1, 6, 1);
            Legs4(d, 2, 3, -3, 2, 3, 2, 3);
            Done(d);
        }

        static void Sniffer()
        {
            var d = New("sniffer", RigKind.Quadruped, 192, 192);
            d.Bone("body", null, 0, 19, 0).C(-12.5f, 9, -20, 25, 24, 40).C(-12.5f, 7, -20, 25, 12, 40, 0.5f, "fur");
            var h = d.Bone("head", "body", 0, 24, 20).C(-6.5f, 12, 20, 13, 18, 11).C(-6.5f, 11, 31, 13, 2, 9, 0, "nose");
            h.C(6.5f, 20, 22, 1, 19, 7, 0, "ear_r").C(-7.5f, 20, 22, 1, 19, 7, 0, "ear_l");
            d.Bone("leg_fr", null, 7.5f, 10, 12).C(4.5f, 0, 9, 7, 10, 8); d.Bone("leg_fl", null, -7.5f, 10, 12).C(-11.5f, 0, 9, 7, 10, 8);
            d.Bone("leg_mr", null, 7.5f, 10, 0).C(4.5f, 0, -4, 7, 10, 8); d.Bone("leg_ml", null, -7.5f, 10, 0).C(-11.5f, 0, -4, 7, 10, 8);
            d.Bone("leg_br", null, 7.5f, 10, -12).C(4.5f, 0, -16, 7, 10, 8); d.Bone("leg_bl", null, -7.5f, 10, -12).C(-11.5f, 0, -16, 7, 10, 8);
            Done(d);
        }

        static void Camel(string name)
        {
            var d = New(name, RigKind.Quadruped, 128, 128);
            d.Bone("body", null, 0, 25, 0).C(-7.5f, 20, -13, 15, 12, 27).C(-4.5f, 32, -5, 9, 5, 11, 0, "hump");
            d.Bone("tail", "body", 0, 31, -13).C(-1.5f, 17, -14, 3, 14, 0.01f);
            var h = d.Bone("head", "body", 0, 30, 12).C(-3.5f, 26, 12, 7, 8, 19).C(-3.5f, 34, 11, 7, 14, 7, 0, "neck").C(-2.5f, 44, 18, 5, 5, 6, 0, "snout");
            h.C(3.5f, 46, 13, 3, 1, 2, 0, "ear_r").C(-6.5f, 46, 13, 3, 1, 2, 0, "ear_l");
            Legs4(d, 4.9f, 9.5f, -9.5f, 5, 21, 5, 21);
            Done(d);
        }

        static void Horse(string name, float scale)
        {
            var d = New(name, RigKind.Horse, 64, 64);
            d.scale = scale;
            d.Bone("body", null, 0, 17, 0).C(-5, 13, -11, 10, 10, 22);
            d.Bone("tail", "body", 0, 21, -11, -30).C(-1.5f, 7, -14, 3, 14, 4);
            var neck = d.Bone("neck", "body", 0, 22, 10, 30).C(-2, 20, 7, 4, 12, 7).C(-1, 22, 5, 2, 16, 2, 0, "mane");
            var h = d.Bone("head", "neck", 0, 32, 12).C(-3, 28, 9, 6, 5, 7).C(-2, 28, 16, 4, 5, 5, 0, "muzzle");
            bool longEars = name == "donkey" || name == "mule";
            h.C(1, 33, 12, 2, longEars ? 7 : 3, 1, 0, "ear_r").C(-3, 33, 12, 2, longEars ? 7 : 3, 1, 0, "ear_l");
            Legs4(d, 3, 8, -8, 4, 13, 4, 13);
            d.Bone("saddle", "body", 0, 23, 0);
            Done(d);
        }

        static void Llama(string name)
        {
            var d = New(name, RigKind.Quadruped, 128, 64);
            d.Bone("body", null, 0, 19, 0).C(-6, 14, -9, 12, 10, 18);
            var h = d.Bone("head", "body", 0, 22, 8).C(-2, 22, 7, 4, 14, 6).C(-2, 34, 13, 4, 4, 5, 0, "snout");
            h.C(1, 36, 9, 2, 3, 2, 0, "ear_r").C(-3, 36, 9, 2, 3, 2, 0, "ear_l");
            Legs4(d, 3.5f, 6, -6, 4, 14, 4, 14);
            if (name == "trader_llama") d.Get("body").C(-6, 14, -9, 12, 10, 18, 0.5f, "carpet");
            Done(d);
        }

        static void Turtle()
        {
            var d = New("turtle", RigKind.Quadruped, 128, 64);
            d.Bone("body", null, 0, 3, 0).C(-9.5f, 2, -10, 19, 6, 20).C(-5.5f, 1, -8, 11, 2, 16, 0, "belly");
            d.Bone("head", "body", 0, 4, 10).C(-3, 2, 10, 6, 5, 6);
            d.Bone("leg_fr", null, 5, 2, 8).C(5, 1, 7, 13, 1, 5); d.Bone("leg_fl", null, -5, 2, 8).C(-18, 1, 7, 13, 1, 5);
            d.Bone("leg_br", null, 3.5f, 2, -8).C(2, 1, -18, 4, 1, 10); d.Bone("leg_bl", null, -3.5f, 2, -8).C(-6, 1, -18, 4, 1, 10);
            Done(d);
        }

        static void Frog()
        {
            var d = New("frog", RigKind.Quadruped, 48, 48);
            d.Bone("body", null, 0, 2, 0).C(-3.5f, 2, -4.5f, 7, 3, 9);
            var h = d.Bone("head", "body", 0, 5, 0).C(-3.5f, 5, -4.5f, 7, 3, 9).C(1.5f, 8, 2, 3, 2, 3, 0, "eye_r").C(-4.5f, 8, 2, 3, 2, 3, 0, "eye_l");
            d.Bone("leg_fr", null, 3, 2, 3).C(2, 0, 2, 2, 3, 3); d.Bone("leg_fl", null, -3, 2, 3).C(-4, 0, 2, 2, 3, 3);
            d.Bone("leg_br", null, 3, 2, -3).C(2.5f, 0, -5, 3, 3, 4); d.Bone("leg_bl", null, -3, 2, -3).C(-5.5f, 0, -5, 3, 3, 4);
            Done(d);
        }

        static void Axolotl()
        {
            var d = New("axolotl", RigKind.Fish, 64, 64);
            d.Bone("body", null, 0, 2, 0).C(-4, 0, -5, 8, 4, 10);
            var h = d.Bone("head", "body", 0, 2, 5).C(-4, 0, 5, 8, 5, 5).C(4, 3, 7, 3, 5, 0.01f, 0, "gill_r").C(-7, 3, 7, 3, 5, 0.01f, 0, "gill_l").C(-4, 5, 7, 8, 3, 0.01f, 0, "gill_top");
            d.Bone("tail", "body", 0, 2, -5).C(0, 0, -17, 0.01f, 5, 12);
            d.Bone("leg_fr", null, 4, 1, 3).C(4, 0, 2, 3, 0.01f, 3); d.Bone("leg_fl", null, -4, 1, 3).C(-7, 0, 2, 3, 0.01f, 3);
            d.Bone("leg_br", null, 4, 1, -3).C(4, 0, -4, 3, 0.01f, 3); d.Bone("leg_bl", null, -4, 1, -3).C(-7, 0, -4, 3, 0.01f, 3);
            Done(d);
        }

        static void Rabbit()
        {
            var d = New("rabbit", RigKind.Quadruped, 64, 32);
            d.Bone("body", null, 0, 4, 0).C(-3, 1, -4, 6, 5, 8);
            var h = d.Bone("head", "body", 0, 6, 3).C(-2.5f, 5, 3, 5, 4, 5).C(0.5f, 9, 5, 2, 5, 1, 0, "ear_r").C(-2.5f, 9, 5, 2, 5, 1, 0, "ear_l");
            d.Bone("tail", "body", 0, 4, -4).C(-1.5f, 3, -6, 3, 3, 2);
            d.Bone("leg_fr", null, 2, 3, 3).C(1, 0, 2, 2, 3, 2); d.Bone("leg_fl", null, -2, 3, 3).C(-3, 0, 2, 2, 3, 2);
            d.Bone("leg_br", null, 2.5f, 3, -2).C(1.5f, 0, -4, 2, 3, 5); d.Bone("leg_bl", null, -2.5f, 3, -2).C(-3.5f, 0, -4, 2, 3, 5);
            Done(d);
        }

        // ================================================================= others
        static void Chicken()
        {
            var d = New("chicken", RigKind.Chicken);
            d.Bone("body", null, 0, 8, 0).C(-3, 5, -4, 6, 6, 8);
            var h = d.Bone("head", "body", 0, 9, 3).C(-2, 9, 2, 4, 6, 3).C(-2, 11, 5, 4, 2, 2, 0, "beak").C(-1, 9, 5, 2, 2, 2, 0, "wattle");
            d.Bone("right_wing", "body", 3, 11, 0).C(3, 7, -3, 1, 4, 6);
            d.Bone("left_wing", "body", -3, 11, 0).C(-4, 7, -3, 1, 4, 6);
            d.Bone("right_leg", null, 1.5f, 5, 0).C(0.5f, 0, -1, 3, 5, 3);
            d.Bone("left_leg", null, -1.5f, 5, 0).C(-3.5f, 0, -1, 3, 5, 3);
            Done(d);
        }

        static void Spider(string name, float scale)
        {
            var d = New(name, RigKind.Spider);
            d.scale = scale;
            d.Bone("body", null, 0, 9, 0).C(-3, 6, -3, 6, 6, 6, 0, "neck");
            d.Bone("abdomen", "body", 0, 9, -3).C(-5, 5, -15, 10, 8, 12);
            d.Bone("head", "body", 0, 9, 3).C(-4, 5, 3, 8, 8, 8);
            float[] zs = { 2f, 0.7f, -0.7f, -2f };
            for (int i = 0; i < 4; i++)
            {
                d.Bone("leg_r" + i, "body", 3, 9, zs[i]).C(3, 8, zs[i] - 1, 16, 2, 2);
                d.Bone("leg_l" + i, "body", -3, 9, zs[i]).C(-19, 8, zs[i] - 1, 16, 2, 2);
            }
            Done(d);
        }

        static void Slime(string name, bool magma)
        {
            var d = New(name, RigKind.Slime);
            if (magma)
            {
                var b = d.Bone("body", null, 0, 0, 0);
                for (int i = 0; i < 8; i++) d.Bone("seg" + i, "body", 0, i + 0.5f, 0).C(-4, i, -4, 8, 1, 8, 0, "seg" + i);
                d.Get("body").C(-2, 2, -2, 4, 4, 4, 0, "core");
            }
            else
            {
                d.Bone("body", null, 0, 0, 0).C(-4, 0, -4, 8, 8, 8, 0, "outer", true).C(-3, 1, -3, 6, 6, 6, 0, "inner")
                    .C(1.25f, 4, 3.5f, 2, 2, 2, 0, "eye_r").C(-3.25f, 4, 3.5f, 2, 2, 2, 0, "eye_l").C(-0.5f, 2, 3.5f, 1, 1, 1, 0, "mouth");
                if (name == "sulfur_cube") d.Bone("absorbed", "body", 0, 4, 0);
            }
            Done(d);
        }

        static void Ghast(string name, float scale)
        {
            var d = New(name, RigKind.Ghast);
            d.scale = scale;
            d.Bone("body", null, 0, 8, 0).C(-8, 0, -8, 16, 16, 16);
            var r = new RNG(Hash.StringHash(name));
            for (int i = 0; i < 9; i++)
            {
                float x = (i % 3 - 1) * 5f - 1, z = (i / 3 - 1) * 5f - 1;
                int len = 7 + r.Next(6);
                d.Bone("tentacle" + i, "body", x + 1, 0, z + 1).C(x, -len, z, 2, len, 2);
            }
            if (name == "happy_ghast") d.Bone("harness", "body", 0, 16, 0);
            Done(d);
        }

        static void Blaze()
        {
            var d = New("blaze", RigKind.Blaze);
            d.Bone("body", null, 0, 12, 0);
            d.Bone("head", "body", 0, 18, 0).C(-4, 18, -4, 8, 8, 8);
            for (int i = 0; i < 12; i++) d.Bone("rod" + i, "body", 0, 12, 0).C(-1, 4 + (i / 4) * 5, -1, 2, 8, 2);
            Done(d);
        }

        static void Shulker()
        {
            var d = New("shulker", RigKind.Shulker);
            d.Bone("base", null, 0, 0, 0).C(-8, 0, -8, 16, 8, 16);
            d.Bone("lid", "base", 0, 4, 0).C(-8, 4, -8, 16, 12, 16);
            d.Bone("head", "base", 0, 6, 0).C(-3, 6, -3, 6, 6, 6);
            Done(d);
        }

        static void Bat()
        {
            var d = New("bat", RigKind.Bat, 32, 32);
            d.Bone("body", null, 0, 6, 0).C(-1.5f, 1, -1, 3, 5, 2);
            d.Bone("head", "body", 0, 6, 0).C(-2, 6, -1.5f, 4, 3, 3).C(0.5f, 9, 0, 1, 2, 0.01f, 0, "ear_r").C(-1.5f, 9, 0, 1, 2, 0.01f, 0, "ear_l");
            d.Bone("right_wing", "body", 1.5f, 6, 0).C(1.5f, 0, 0, 7, 6, 0.01f);
            d.Bone("left_wing", "body", -1.5f, 6, 0).C(-8.5f, 0, 0, 7, 6, 0.01f);
            Done(d);
        }

        static void Bee()
        {
            var d = New("bee", RigKind.Flyer, 64, 64);
            d.Bone("body", null, 0, 5, 0).C(-3.5f, 2, -5, 7, 7, 10).C(-0.5f, 4, -6, 1, 1, 1, 0, "stinger").C(1, 9, 3, 1, 2, 3, 0, "antenna_r").C(-2, 9, 3, 1, 2, 3, 0, "antenna_l");
            d.Bone("right_wing", "body", 1.5f, 9, 0).C(1.5f, 9, -2, 9, 0.01f, 6, 0, "wing", true);
            d.Bone("left_wing", "body", -1.5f, 9, 0).C(-10.5f, 9, -2, 9, 0.01f, 6, 0, "wing", true);
            d.Bone("head", "body", 0, 6, 5);
            d.Bone("legs", "body", 0, 2, 0).C(-2.5f, 0, -2, 5, 2, 0.01f).C(-2.5f, 0, 0, 5, 2, 0.01f).C(-2.5f, 0, 2, 5, 2, 0.01f);
            Done(d);
        }

        static void Parrot()
        {
            var d = New("parrot", RigKind.Flyer, 32, 32);
            d.Bone("body", null, 0, 5, 0, 30).C(-1.5f, 2, -1.5f, 3, 6, 3);
            var h = d.Bone("head", "body", 0, 8, 0).C(-1, 8, -1, 2, 3, 2).C(-1, 8, 1, 2, 2, 2, 0, "beak").C(-0.5f, 11, -2, 1, 2, 3, 0, "crest");
            d.Bone("tail", "body", 0, 3, -1.5f, 60).C(-1.5f, -1, -2, 3, 4, 1);
            d.Bone("right_wing", "body", 1.5f, 7, 0).C(1.5f, 2, -1.5f, 1, 5, 3);
            d.Bone("left_wing", "body", -1.5f, 7, 0).C(-2.5f, 2, -1.5f, 1, 5, 3);
            d.Bone("right_leg", null, 1, 2, 0).C(0.5f, 0, -0.5f, 1, 2, 1); d.Bone("left_leg", null, -1, 2, 0).C(-1.5f, 0, -0.5f, 1, 2, 1);
            Done(d);
        }

        static void Squid(string name)
        {
            var d = New(name, RigKind.Squid);
            d.Bone("body", null, 0, 8, 0).C(-6, 8, -6, 12, 16, 12);
            for (int i = 0; i < 8; i++)
            {
                float a = i / 8f * Mathf.PI * 2;
                float x = Mathf.Cos(a) * 5, z = Mathf.Sin(a) * 5;
                d.Bone("tentacle" + i, "body", x, 8, z).C(x - 1, -10, z - 1, 2, 18, 2);
            }
            Done(d);
        }

        static void Fish(string name, float w, float h, float len)
        {
            var d = New(name, RigKind.Fish, 32, 32);
            d.Bone("body", null, 0, h / 2 + 1, 0).C(-w / 2, 1, -len / 2, w, h, len).C(0, 1 + h, -len / 2 + 1, 0.01f, 2, len * 0.6f, 0, "fin_top");
            d.Bone("head", "body", 0, h / 2 + 1, len / 2).C(-w / 2, 1, len / 2, w, h - 1, 3);
            d.Bone("tail", "body", 0, h / 2 + 1, -len / 2).C(0, 1, -len / 2 - 4, 0.01f, h, 4);
            Done(d);
        }

        static void Pufferfish()
        {
            var d = New("pufferfish", RigKind.Fish, 32, 32);
            d.Bone("body", null, 0, 4, 0).C(-4, 0, -4, 8, 8, 8).C(-4, 8, -4, 8, 1, 8, 0, "spikes", true);
            d.Bone("tail", "body", 0, 4, -4).C(0, 2, -7, 0.01f, 4, 3);
            d.Bone("head", "body", 0, 4, 4);
            Done(d);
        }

        static void Dolphin()
        {
            var d = New("dolphin", RigKind.Fish, 64, 64);
            d.Bone("body", null, 0, 4, 0).C(-4, 0, -6, 8, 7, 13).C(-0.5f, 7, -2, 1, 4, 5, 0, "fin_top");
            var h = d.Bone("head", "body", 0, 4, 7).C(-4, 0, 7, 8, 7, 6).C(-1, 0, 13, 2, 2, 4, 0, "nose");
            var t = d.Bone("tail", "body", 0, 3, -6).C(-2, 0, -17, 4, 5, 11);
            d.Bone("tail_fin", "tail", 0, 2, -17).C(-5, 1, -21, 10, 1, 4);
            d.Get("body").C(4, 0, 2, 6, 1, 3, 0, "fin_r").C(-10, 0, 2, 6, 1, 3, 0, "fin_l");
            Done(d);
        }

        static void Guardian(string name, float scale)
        {
            var d = New(name, RigKind.Guardian, 64, 64);
            d.scale = scale;
            d.Bone("body", null, 0, 8, 0).C(-6, 2, -8, 12, 12, 16).C(-8, 2, -6, 2, 12, 12, 0, "side_l").C(6, 2, -6, 2, 12, 12, 0, "side_r").C(-6, 14, -6, 12, 2, 12, 0, "top").C(-6, 0, -6, 12, 2, 12, 0, "bottom");
            for (int i = 0; i < 12; i++) d.Bone("spike" + i, "body", 0, 8, 0).C(-1, 14, -1, 2, 9, 2);
            d.Bone("eye", "body", 0, 8, 8).C(-1, 7, 8, 2, 2, 1);
            d.Bone("tail0", "body", 0, 8, -8).C(-2, 6, -16, 4, 4, 8);
            d.Bone("tail1", "tail0", 0, 8, -16).C(-1.5f, 6.5f, -22, 3, 3, 6);
            d.Bone("tail2", "tail1", 0, 8, -22).C(-1, 7, -27, 2, 2, 5).C(0, 4, -30, 0.01f, 8, 5, 0, "fin");
            d.Bone("head", "body", 0, 8, 8);
            Done(d);
        }

        static void Phantom()
        {
            var d = New("phantom", RigKind.Flyer, 64, 64);
            d.Bone("body", null, 0, 4, 0).C(-3, 2, -8, 5, 3, 9);
            d.Bone("head", "body", 0, 4, 1).C(-4, 2, 1, 7, 3, 5);
            d.Bone("right_wing", "body", 2, 5, 0).C(2, 4, -8, 6, 2, 9);
            d.Bone("right_wing_tip", "right_wing", 8, 5, 0).C(8, 4.5f, -8, 13, 1, 9);
            d.Bone("left_wing", "body", -3, 5, 0).C(-9, 4, -8, 6, 2, 9);
            d.Bone("left_wing_tip", "left_wing", -9, 5, 0).C(-22, 4.5f, -8, 13, 1, 9);
            d.Bone("tail", "body", 0, 4, -8).C(-1.5f, 2.5f, -14, 3, 2, 6);
            d.Bone("tail_tip", "tail", 0, 4, -14).C(-0.5f, 3, -20, 1, 1, 6);
            Done(d);
        }

        static void Silverfish(string name)
        {
            var d = New(name, RigKind.Snake, 64, 32);
            float[] w = name == "endermite" ? new[] { 4f, 6f, 3f, 1f } : new[] { 3f, 4f, 6f, 3f, 2f, 2f, 1f };
            float z = 0;
            string parent = null;
            for (int i = 0; i < w.Length; i++)
            {
                float len = i == 0 ? 2 : 2;
                d.Bone("seg" + i, parent, 0, 1, z).C(-w[i] / 2, 0, z - len, w[i], Mathf.Min(4, w[i] * 0.7f + 1), len);
                parent = "seg" + i; z -= len;
            }
            d.Bone("head", "seg0", 0, 1, 0);
            Done(d);
        }

        static void Breeze()
        {
            var d = New("breeze", RigKind.Blaze, 32, 64);
            d.Bone("body", null, 0, 8, 0).C(-1, 3, -1, 2, 11, 2, 0, "rod");
            d.Bone("head", "body", 0, 14, 0).C(-4, 14, -4, 8, 8, 8);
            d.Bone("wind0", "body", 0, 8, 0).C(-6, 8, -6, 12, 4, 12, 0, "wind_top", true);
            d.Bone("wind1", "body", 0, 4, 0).C(-4, 4, -4, 8, 4, 8, 0, "wind_mid", true);
            d.Bone("wind2", "body", 0, 1, 0).C(-2.5f, 0, -2.5f, 5, 4, 5, 0, "wind_bottom", true);
            Done(d);
        }

        static void Tadpole()
        {
            var d = New("tadpole", RigKind.Fish, 16, 16);
            d.Bone("body", null, 0, 1, 0).C(-1.5f, 0, -1, 3, 2, 3);
            d.Bone("tail", "body", 0, 1, -1).C(0, 0, -8, 0.01f, 2, 7);
            d.Bone("head", "body", 0, 1, 2);
            Done(d);
        }

        static void Nautilus(string name)
        {
            var d = New(name, RigKind.Fish, 64, 64);
            d.Bone("body", null, 0, 6, 0).C(-4, 0, -6, 8, 12, 10, 0, "shell").C(-3, 2, 4, 6, 6, 3, 0, "face");
            for (int i = 0; i < 4; i++) d.Bone("tentacle" + i, "body", -2 + i * 1.3f, 2, 7).C(-2.5f + i * 1.3f, 1, 7, 1, 1, 6);
            d.Bone("head", "body", 0, 5, 5);
            Done(d);
        }

        // ================================================================= bosses
        static void Dragon()
        {
            var d = New("ender_dragon", RigKind.Dragon, 256, 256);
            // body (MC-like proportions, recreated): long torso with dorsal scales
            d.Bone("body", null, 0, 24, 0).C(-12, 12, -32, 24, 24, 64).C(-1, 36, -28, 2, 6, 12, 0, "scale0").C(-1, 36, -8, 2, 6, 12, 0, "scale1").C(-1, 36, 12, 2, 6, 12, 0, "scale2");
            // neck: 5 segments leading forward (+Z)
            string parent = "body"; float z = 32, y = 28;
            for (int i = 0; i < 5; i++)
            {
                d.Bone("neck" + i, parent, 0, y, z).C(-5, y - 5, z, 10, 10, 10).C(-1, y + 5, z + 2, 2, 4, 6, 0, "nscale" + i);
                parent = "neck" + i; z += 10;
            }
            var head = d.Bone("head", parent, 0, y, z).C(-8, y - 8, z, 16, 16, 16).C(-6, y - 7, z + 16, 12, 5, 16, 0, "upper_jaw");
            head.C(3, y + 8, z + 3, 2, 4, 6, 0, "horn_r").C(-5, y + 8, z + 3, 2, 4, 6, 0, "horn_l").C(3, y - 2, z + 28, 2, 2, 4, 0, "nostril_r").C(-5, y - 2, z + 28, 2, 2, 4, 0, "nostril_l");
            d.Bone("jaw", "head", 0, y - 4, z + 16).C(-6, y - 8, z + 16, 12, 4, 16);
            // tail: 12 segments going back (-Z)
            parent = "body"; z = -32; y = 26;
            for (int i = 0; i < 12; i++)
            {
                d.Bone("tail" + i, parent, 0, y, z).C(-5, y - 5, z - 10, 10, 10, 10).C(-1, y + 5, z - 8, 2, 4, 6, 0, "tscale" + i);
                parent = "tail" + i; z -= 10;
            }
            // wings: arm bone + membrane, and outer tip
            d.Bone("right_wing", "body", 12, 34, 16).C(12, 30, 12, 56, 8, 8).C(12, 34, -44, 56, 0.01f, 56, 0, "membrane");
            d.Bone("right_wing_tip", "right_wing", 68, 34, 16).C(68, 32, 14, 56, 4, 4).C(68, 34, -40, 56, 0.01f, 56, 0, "tip_membrane");
            d.Bone("left_wing", "body", -12, 34, 16).C(-68, 30, 12, 56, 8, 8).C(-68, 34, -44, 56, 0.01f, 56, 0, "membrane");
            d.Bone("left_wing_tip", "left_wing", -68, 34, 16).C(-124, 32, 14, 56, 4, 4).C(-124, 34, -40, 56, 0.01f, 56, 0, "tip_membrane");
            // legs
            d.Bone("front_leg_r", "body", 12, 20, 20, 30).C(10, 0, 16, 8, 24, 8).C(9, -8, 18, 10, 8, 6, 0, "foot");
            d.Bone("front_leg_l", "body", -12, 20, 20, 30).C(-18, 0, 16, 8, 24, 8).C(-19, -8, 18, 10, 8, 6, 0, "foot");
            d.Bone("rear_leg_r", "body", 16, 16, -24, 30).C(12, -16, -32, 16, 32, 16).C(12, -24, -28, 16, 8, 12, 0, "foot");
            d.Bone("rear_leg_l", "body", -16, 16, -24, 30).C(-28, -16, -32, 16, 32, 16).C(-28, -24, -28, 16, 8, 12, 0, "foot");
            d.scale = 1f;
            Done(d);
        }

        static void Wither()
        {
            var d = New("wither", RigKind.Wither, 64, 64);
            d.Bone("body", null, 0, 26, 0).C(-10, 30, -1.5f, 20, 3, 3, 0, "shoulders");
            d.Bone("spine", "body", 0, 30, 0, 12).C(-1.5f, 20, -1, 3, 10, 3).C(-4, 26, -0.5f, 8, 1.5f, 2, 0, "rib0").C(-4, 23.5f, -0.5f, 8, 1.5f, 2, 0, "rib1").C(-4, 21, -0.5f, 8, 1.5f, 2, 0, "rib2");
            d.Bone("tail", "spine", 0, 20, 0, 30).C(-1.5f, 14, -1, 3, 6, 3);
            d.Bone("head", "body", 0, 33, 0).C(-4, 33, -4, 8, 8, 8);
            d.Bone("head_right", "body", 9, 32, 0).C(6, 31, -3, 6, 6, 6);
            d.Bone("head_left", "body", -9, 32, 0).C(-12, 31, -3, 6, 6, 6);
            Done(d);
        }

        // ================================================================= props
        static void Minecart()
        {
            var d = New("minecart", RigKind.Minecart);
            d.Bone("root", null, 0, 0, 0).C(-10, 1, -8, 20, 2, 16, 0, "bottom").C(-10, 3, -8, 20, 8, 2, 0, "back").C(-10, 3, 6, 20, 8, 2, 0, "front").C(-10, 3, -6, 2, 8, 12, 0, "left").C(8, 3, -6, 2, 8, 12, 0, "right");
            Done(d);
        }

        static void Boat(string name, bool chest)
        {
            var d = New(name, RigKind.Boat, 128, 64);
            d.Bone("root", null, 0, 0, 0).C(-14, 0, -8, 28, 3, 16, 0, "bottom").C(-15, 3, -9, 30, 6, 2, 0, "side_back").C(-15, 3, 7, 30, 6, 2, 0, "side_front").C(-15, 3, -7, 2, 6, 14, 0, "stern").C(13, 3, -7, 2, 6, 14, 0, "bow");
            if (chest) d.Get("root").C(-12, 3, -6, 12, 10, 12, 0, "chest");
            d.Bone("paddle_right", "root", 3, 8, 8).C(2, 7, 8, 2, 2, 18).C(2.5f, 6, 20, 1, 6, 7, 0, "blade");
            d.Bone("paddle_left", "root", 3, 8, -8).C(2, 7, -26, 2, 2, 18).C(2.5f, 6, -27, 1, 6, 7, 0, "blade");
            // boats are longer along X in the model: rotate so front is +Z
            d.Get("root").rotation = new Vector3(0, 90, 0);
            Done(d);
        }

        static void Raft()
        {
            var d = New("raft", RigKind.Boat, 128, 64);
            d.Bone("root", null, 0, 0, 0).C(-14, 0, -8, 28, 4, 16, 0, "deck");
            d.Bone("paddle_right", "root", 3, 6, 8).C(2, 5, 8, 2, 2, 18).C(2.5f, 4, 20, 1, 6, 7, 0, "blade");
            d.Bone("paddle_left", "root", 3, 6, -8).C(2, 5, -26, 2, 2, 18).C(2.5f, 4, -27, 1, 6, 7, 0, "blade");
            d.Get("root").rotation = new Vector3(0, 90, 0);
            Done(d);
        }

        static void EndCrystal()
        {
            var d = New("end_crystal", RigKind.Crystal, 64, 32);
            d.Bone("root", null, 0, 0, 0);
            d.Bone("base", "root", 0, 0, 0).C(-6, 0, -6, 12, 4, 12);
            d.Bone("glass_outer", "root", 0, 12, 0).C(-4, 8, -4, 8, 8, 8, 0, "glass", true);
            d.Bone("glass_inner", "root", 0, 12, 0).C(-3.5f, 8.5f, -3.5f, 7, 7, 7, 0, "glass2", true);
            d.Bone("cube", "root", 0, 12, 0).C(-2.5f, 9.5f, -2.5f, 5, 5, 5, 0, "core");
            Done(d);
        }

        static void ChestModel(string name)
        {
            var d = New(name, RigKind.Static, 64, 64);
            d.Bone("bottom", null, 0, 0, 0).C(-7, 0, -7, 14, 10, 14);
            d.Bone("lid", "bottom", 0, 10, -7).C(-7, 10, -7, 14, 5, 14);
            d.Bone("lock", "lid", 0, 10, 7).C(-1, 7, 7, 2, 4, 1);
            Done(d);
        }

        static void ShieldModel()
        {
            var d = New("shield", RigKind.Static, 64, 64);
            d.Bone("plate", null, 0, 0, 0).C(-6, -11, -1, 12, 22, 1);
            d.Bone("handle", "plate", 0, 0, 0).C(-1, -3, -7, 2, 6, 6);
            Done(d);
        }

        static void TridentModel()
        {
            var d = New("trident", RigKind.Static, 32, 32);
            d.Bone("shaft", null, 0, 0, 0).C(-0.5f, -12, -0.5f, 1, 25, 1);
            d.Bone("prongs", "shaft", 0, 13, 0).C(-1.5f, 12, -0.5f, 3, 2, 1, 0, "base").C(-2.5f, 13, -0.5f, 1, 4, 1, 0, "left").C(1.5f, 13, -0.5f, 1, 4, 1, 0, "right").C(-0.5f, 13, -0.5f, 1, 5, 1, 0, "mid");
            Done(d);
        }

        static void SpearModel()
        {
            var d = New("spear", RigKind.Static, 32, 32);
            d.Bone("shaft", null, 0, 0, 0).C(-0.5f, -12, -0.5f, 1, 22, 1);
            d.Bone("tip", "shaft", 0, 10, 0).C(-1, 10, -0.5f, 2, 4, 1).C(-0.5f, 14, -0.5f, 1, 2, 1, 0, "point");
            d.Get("shaft").C(-1, -1, -1, 2, 3, 2, 0, "grip");
            Done(d);
        }

        static void BowModel()
        {
            var d = New("bow", RigKind.Static, 32, 32);
            d.Bone("grip", null, 0, 0, 0).C(-0.5f, -1.5f, -0.5f, 1, 3, 1);
            d.Bone("upper_limb", "grip", 0, 1.5f, 0, -20).C(-0.5f, 1.5f, -0.5f, 1, 6, 1);
            d.Bone("lower_limb", "grip", 0, -1.5f, 0, 20).C(-0.5f, -7.5f, -0.5f, 1, 6, 1);
            d.Bone("string", "grip", 0, 0, -2).C(-0.1f, -7, -2.5f, 0.2f, 14, 0.2f);
            Done(d);
        }

        static void CrossbowModel()
        {
            var d = New("crossbow", RigKind.Static, 32, 32);
            d.Bone("stock", null, 0, 0, 0).C(-0.5f, -0.5f, -6, 1, 1, 12);
            d.Bone("limbs", "stock", 0, 0, 5).C(-6, -0.5f, 4.5f, 12, 1, 1);
            d.Bone("string", "stock", 0, 0, 3).C(-5.5f, -0.1f, 2, 11, 0.2f, 0.2f);
            Done(d);
        }

        /// <summary>Armor overlay for bipeds: helmet, chestplate, arm guards, leggings, boots (inflated player boxes).</summary>
        static void ArmorModel()
        {
            var d = New("armor", RigKind.Biped, 64, 64);
            d.Bone("body", null, 0, 24, 0).C(-4, 12, -2, 8, 12, 4, 1f, "chest").C(-4, 12, -2, 8, 12, 4, 0.5f, "leggings_top");
            d.Bone("head", "body", 0, 24, 0).C(-4, 24, -4, 8, 8, 8, 1f, "helmet");
            d.Bone("right_arm", "body", 6, 22, 0).C(4, 12, -2, 4, 12, 4, 1f, "arm_r");
            d.Bone("left_arm", "body", -6, 22, 0).C(-8, 12, -2, 4, 12, 4, 1f, "arm_l");
            d.Bone("right_leg", null, 2, 12, 0).C(0, 0, -2, 4, 12, 4, 0.5f, "leg_r").C(0, 0, -2, 4, 6, 4, 1f, "boot_r");
            d.Bone("left_leg", null, -2, 12, 0).C(-4, 0, -2, 4, 12, 4, 0.5f, "leg_l").C(-4, 0, -2, 4, 6, 4, 1f, "boot_l");
            Done(d);
        }
    }
}
