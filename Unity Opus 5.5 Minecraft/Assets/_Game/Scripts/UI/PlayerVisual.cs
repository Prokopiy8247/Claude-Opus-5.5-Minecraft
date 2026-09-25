using System.Collections.Generic;
using UnityEngine;
using UnityEngine.Rendering;

namespace MCR
{
    /// <summary>
    /// The local player's body: the first-person hand and held item in the overlay camera, the third-person
    /// player model, the shield raised while blocking, and the bow/crossbow draw poses. The model reuses the same
    /// procedural rig as every other mob, so the player looks identical in first and third person.
    /// </summary>
    public static class PlayerVisual
    {
        static MobVisual visual;
        static Player bound;
        static Transform armRoot, itemRoot, offRoot;
        static GameObject armObject, itemObject, offObject;
        static string itemKey, offKey;
        static Material handMat;
        static Mesh armMesh;
        static float equipAnim = 1f;
        static readonly int FirstPersonLayer = 31;

        // camera-space poses (x right, y up, z forward). An empty hand shows the right arm reaching in from the
        // lower right corner; a held item takes the arm's place, the way the original presents it.
        // the fist sits right of and below the crosshair; the shoulder is off-screen, one arm length back
        static readonly Vector3 ArmFist = new Vector3(0.44f, -0.34f, 0.90f);
        static readonly Vector3 ArmDir = new Vector3(-0.40f, 0.60f, 0.70f);
        const float ArmLength = 0.62f;
        static readonly Vector3 ArmShoulder = ArmFist - ArmDir.normalized * ArmLength;
        const float ArmTwist = 62f;
        static readonly Vector3 ItemCenter = new Vector3(0.60f, -0.36f, 0.82f);
        static readonly Vector3 BlockCenter = new Vector3(0.56f, -0.42f, 0.80f);

        /// <summary>Called every frame from the camera update with the local player.</summary>
        public static void Update(Player p, float partial)
        {
            if (p == null) { Clear(); return; }
            if (bound != p) { Clear(); bound = p; }
            bool thirdPerson = p.cameraMode != 0;
            if (thirdPerson)
            {
                EnsureBody(p);
                SetFirstPersonActive(false);
                if (visual != null && visual.root != null) { visual.root.SetActive(!p.IsSpectator); visual.Update(partial); }
                return;
            }
            if (visual != null && visual.root != null) visual.root.SetActive(false);
            if (!EnsureHand()) return;
            UpdateHand(p, partial);
        }

        /// <summary>Orientation diagnostics of the third-person body (automated tests).</summary>
        public static string BodyProbe() => visual != null ? visual.Probe() : "no body";

        public static void Clear()
        {
            if (armObject != null) Object.Destroy(armObject);
            if (itemRoot != null) Object.Destroy(itemRoot.gameObject);
            if (offRoot != null) Object.Destroy(offRoot.gameObject);
            if (visual != null) visual.Destroy();
            armObject = null; itemObject = null; offObject = null;
            armRoot = itemRoot = offRoot = null;
            visual = null; bound = null;
            itemKey = offKey = null;
        }

        static void SetFirstPersonActive(bool on)
        {
            if (armObject != null) armObject.SetActive(on);
            if (itemRoot != null) itemRoot.gameObject.SetActive(on);
            if (offRoot != null) offRoot.gameObject.SetActive(on);
        }

        // ------------------------------------------------------------------ third person body
        static void EnsureBody(Player p)
        {
            if (visual != null && visual.root != null) return;
            visual = new MobVisual(p, "player");
            if (visual.root != null) visual.root.SetActive(true);
        }

        // ------------------------------------------------------------------ first person
        static bool EnsureHand()
        {
            var cam = GameManager.MainCamera;
            if (cam == null) return false;
            if (armObject != null) return true;
            armObject = new GameObject("FirstPersonArm") { layer = FirstPersonLayer };
            armObject.transform.SetParent(cam.transform, false);
            armRoot = armObject.transform;
            if (handMat == null) handMat = Res.EntityMaterial(ModelSkins.Get("player", 64, 64, null).opaque);
            if (armMesh == null) armMesh = BuildArmMesh();
            armObject.AddComponent<MeshFilter>().sharedMesh = armMesh;
            var mr = armObject.AddComponent<MeshRenderer>();
            mr.sharedMaterial = handMat;
            mr.shadowCastingMode = ShadowCastingMode.Off;
            mr.receiveShadows = false;

            itemRoot = new GameObject("FirstPersonItem") { layer = FirstPersonLayer }.transform;
            itemRoot.SetParent(cam.transform, false);
            offRoot = new GameObject("FirstPersonOffhand") { layer = FirstPersonLayer }.transform;
            offRoot.SetParent(cam.transform, false);
            return true;
        }

        /// <summary>The player model's own right arm (same cubes and skin), so first and third person agree.</summary>
        static Mesh BuildArmMesh()
        {
            var def = MobModels.Get("player");
            if (def != null)
            {
                var meshes = ModelMesher.BoneMeshes(def);
                for (int i = 0; i < def.bones.Count; i++)
                    if (def.bones[i].name == "right_arm" && meshes[i] != null && meshes[i].main != null) return meshes[i].main;
            }
            // fallback: a plain 4x12x4 box hanging from its pivot
            var m = new Mesh { name = "fp_arm" };
            var v = new List<Vector3>();
            var t = new List<int>();
            Vector3 mn = new Vector3(-0.125f, -0.625f, -0.125f), mx = new Vector3(0.125f, 0.125f, 0.125f);
            for (int f = 0; f < 6; f++)
            {
                int s = v.Count;
                for (int k = 0; k < 4; k++)
                {
                    var c = new Vector3((k == 1 || k == 2) ? 1 : 0, (k >= 2) ? 1 : 0, 0);
                    Vector3 p;
                    switch (f)
                    {
                        case 0: p = new Vector3(c.x, 1, c.y); break;
                        case 1: p = new Vector3(c.x, 0, c.y); break;
                        case 2: p = new Vector3(c.x, c.y, 0); break;
                        case 3: p = new Vector3(c.x, c.y, 1); break;
                        case 4: p = new Vector3(0, c.x, c.y); break;
                        default: p = new Vector3(1, c.x, c.y); break;
                    }
                    v.Add(Vector3.Scale(p, mx - mn) + mn);
                }
                t.Add(s); t.Add(s + 1); t.Add(s + 2); t.Add(s); t.Add(s + 2); t.Add(s + 3);
            }
            m.SetVertices(v); m.SetTriangles(t, 0); m.RecalculateNormals(); m.RecalculateBounds();
            return m;
        }

        static void UpdateHand(Player p, float partial)
        {
            // swing progress: a quick reach toward the crosshair and back
            float s = p.attackAnim < p.prevAttackAnim ? p.attackAnim : Mathf.Lerp(p.prevAttackAnim, p.attackAnim, partial);
            float arc = Mathf.Sin(s * Mathf.PI);
            float reach = Mathf.Sin(Mathf.Sqrt(s) * Mathf.PI);
            float lift = Mathf.Sin(Mathf.Sqrt(s) * Mathf.PI * 2f);

            // walking bob shared by the arm and the item
            float walk = Mathf.Lerp(p.prevWalkDist, p.walkDist, partial);
            float bob = Mathf.Lerp(p.prevBob, p.bob, partial);
            var bobOffset = new Vector3(Mathf.Sin(walk * Mathf.PI) * bob * 0.05f, -Mathf.Abs(Mathf.Cos(walk * Mathf.PI) * bob) * 0.06f, 0f);

            equipAnim = Mathf.MoveTowards(equipAnim, 1f, Time.deltaTime * 5f);
            var equipDrop = Vector3.down * ((1f - equipAnim) * 0.55f);

            var main = p.inventory.Selected;
            bool holding = main != null && !main.IsEmpty && !p.IsSpectator;
            armObject.SetActive(!holding && !p.IsSpectator);
            itemRoot.gameObject.SetActive(holding);

            if (!holding)
            {
                var dir = ArmDir.normalized;
                var rest = Quaternion.AngleAxis(ArmTwist, dir) * Quaternion.FromToRotation(Vector3.down, dir);
                var swing = Quaternion.Euler(-arc * 25f, -reach * 32f, reach * 12f);
                armRoot.localRotation = swing * rest;
                armRoot.localPosition = ArmShoulder + bobOffset + equipDrop + new Vector3(-reach * 0.22f, lift * 0.08f, arc * 0.08f);
            }
            RefreshHeld(p, main);

            if (holding && itemObject != null)
            {
                bool block = IsBlockItem(main);
                var center = block ? BlockCenter : ItemCenter;
                var pose = HeldRotation(main);
                var anim = Quaternion.identity;
                var offset = Vector3.zero;
                var useAnim = p.IsUsingItem && p.usingStack != null && !p.usingOffhand ? p.usingStack.item.GetUseAnim(p.usingStack) : UseAnim.None;
                switch (useAnim)
                {
                    case UseAnim.Eat:
                    case UseAnim.Drink:
                    {
                        // bring the food to the mouth and nibble
                        float k = Mathf.Clamp01(p.useTicks / 6f);
                        offset = Vector3.Lerp(Vector3.zero, new Vector3(-0.42f, 0.18f, -0.12f), k) + Vector3.up * (Mathf.Abs(Mathf.Sin(p.useTicks * 0.9f)) * 0.04f * k);
                        anim = Quaternion.Euler(0f, 0f, 0f) * Quaternion.Slerp(Quaternion.identity, Quaternion.Euler(-10f, 30f, 10f), k);
                        break;
                    }
                    case UseAnim.Block:
                        offset = new Vector3(-0.30f, 0.06f, -0.08f);
                        anim = Quaternion.Euler(0f, -12f, 0f);
                        break;
                    case UseAnim.Bow:
                    case UseAnim.Crossbow:
                    {
                        float draw = Mathf.Clamp01(p.useTicks / (useAnim == UseAnim.Crossbow ? 25f : 20f));
                        offset = new Vector3(-0.32f, 0.10f, -0.06f - 0.10f * draw) + new Vector3(0f, Mathf.Sin(p.useTicks * 1.7f) * 0.004f * draw, 0f);
                        anim = Quaternion.Euler(-8f, 22f, -20f);
                        break;
                    }
                    case UseAnim.Spear:
                    case UseAnim.Trident:
                        offset = new Vector3(-0.10f, 0.12f, -0.25f * Mathf.Clamp01(p.useTicks / 10f));
                        anim = Quaternion.Euler(-45f, 0f, 0f);
                        break;
                    case UseAnim.Spyglass:
                        offset = new Vector3(-0.60f, 0.36f, -0.35f);
                        break;
                }
                var swing = Quaternion.Euler(-arc * 55f, -reach * 25f, -arc * 20f);
                itemRoot.localPosition = center + bobOffset + equipDrop + offset + new Vector3(-reach * 0.24f, lift * 0.10f, -arc * 0.06f);
                itemRoot.localRotation = swing * anim;
                itemObject.transform.localRotation = pose;
            }

            // off-hand: mirrored to the lower left
            var off = p.inventory.offhand;
            bool offHolding = off != null && !off.IsEmpty && !p.IsSpectator;
            offRoot.gameObject.SetActive(offHolding);
            if (offHolding && offObject != null)
            {
                var c = IsBlockItem(off) ? BlockCenter : ItemCenter;
                var offset = Vector3.zero;
                if (p.IsBlocking && p.usingOffhand) offset = new Vector3(0.30f, 0.06f, -0.08f);
                offRoot.localPosition = new Vector3(-c.x, c.y, c.z) + bobOffset + offset;
                offRoot.localRotation = Quaternion.identity;
                var r = HeldRotation(off).eulerAngles;
                offObject.transform.localRotation = Quaternion.Euler(r.x, -r.y, -r.z);
            }
        }

        static bool IsBlockItem(ItemStack s) => s.item.block != null && ItemRender.SpriteLayer(s.item) < 0;

        /// <summary>Resting orientation of an item in the right hand (the view looks along +Z).</summary>
        static Quaternion HeldRotation(ItemStack s)
        {
            if (IsBlockItem(s)) return Quaternion.Euler(0f, 45f, 0f);
            string model = ItemRender.ModelFor(s.item);
            if (model == "shield") return Quaternion.Euler(0f, 170f, 0f);
            if (model == "trident") return Quaternion.Euler(0f, 180f, 0f);
            // flat sprites: turned edge-on, tipped forward and leaned in so the tip points toward the crosshair
            return Quaternion.Euler(0f, 0f, 18f) * Quaternion.Euler(0f, -90f, 25f);
        }

        static void RefreshHeld(Player p, ItemStack main)
        {
            string key = main == null || main.IsEmpty ? null : main.item.id + (main.damage > 0 ? ":" + main.damage : "");
            if (key != itemKey)
            {
                bool slotChange = itemKey != null || key != null;
                itemKey = key;
                if (itemObject != null) Object.Destroy(itemObject);
                itemObject = key != null ? CreateHeld(main) : null;
                if (itemObject != null) { itemObject.transform.SetParent(itemRoot, false); SetLayer(itemObject, FirstPersonLayer); }
                if (slotChange) equipAnim = 0f;
            }
            var off = p.inventory.offhand;
            string okey = off == null || off.IsEmpty ? null : off.item.id;
            if (okey != offKey)
            {
                offKey = okey;
                if (offObject != null) Object.Destroy(offObject);
                offObject = okey != null ? CreateHeld(off) : null;
                if (offObject != null) { offObject.transform.SetParent(offRoot, false); SetLayer(offObject, FirstPersonLayer); }
            }
        }

        /// <summary>A pivot at the item's centre holding the visual, so rotations spin it in place.</summary>
        static GameObject CreateHeld(ItemStack s)
        {
            var pivot = new GameObject("Held_" + s.item.id);
            GameObject vis;
            if (IsBlockItem(s))
            {
                const float size = 0.40f;
                vis = ItemRender.CreateBlockVisual(s.item.block.DefaultState, size);
                if (vis != null) vis.transform.localPosition = new Vector3(0f, -size * 0.5f, 0f);
            }
            else
            {
                vis = ItemRender.CreateHeldModelVisual(s, 1f);
                if (vis != null) vis.transform.localScale = Vector3.one * 0.62f;
            }
            if (vis != null) vis.transform.SetParent(pivot.transform, false);
            return pivot;
        }

        static void SetLayer(GameObject go, int layer)
        {
            go.layer = layer;
            foreach (Transform t in go.transform) SetLayer(t.gameObject, layer);
        }

        /// <summary>Item in hand, exposed for the swing/block animation of other systems.</summary>
        public static void OnHeldChanged()
        {
            itemKey = null;
            equipAnim = 0f;
        }
    }
}
