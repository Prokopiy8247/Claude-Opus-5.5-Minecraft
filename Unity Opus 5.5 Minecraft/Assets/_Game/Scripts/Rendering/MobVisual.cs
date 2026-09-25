using System.Collections.Generic;
using UnityEngine;

namespace MCR
{
    /// <summary>
    /// Drives the procedural animation of a mob's block model: limb swing from walk distance, head aim,
    /// attack/death poses, per-rig special motion (legs of spiders, slime squash, dragon wings, squid tentacles)
    /// and held/equipped items. Bones are looked up by name, so one driver serves every rig.
    /// </summary>
    public sealed class MobVisual
    {
        public GameObject root;
        readonly LivingEntity mob;
        readonly ModelDef def;
        int Variant => mob is Mob m ? m.variant : 0;
        bool Sitting => mob is Mob m && m.sitting;
        bool Aggressive => mob is Mob m && m.aggressiveAnim;
        readonly Dictionary<string, Transform> bones = new Dictionary<string, Transform>();
        readonly Dictionary<string, Vector3> restRot = new Dictionary<string, Vector3>();
        readonly List<Transform> legList = new List<Transform>(), armList = new List<Transform>();
        readonly List<Transform> wingList = new List<Transform>(), tailList = new List<Transform>();
        readonly List<Transform> tentacleList = new List<Transform>(), segList = new List<Transform>(), rodList = new List<Transform>();
        readonly List<Transform> neckList = new List<Transform>();
        readonly List<Transform> eyes = new List<Transform>();
        readonly List<string> legNames = new List<string>();
        Transform body, head, jaw, armsBone, heldBone;
        float heightOffset;
        GameObject heldVisual, heldOffVisual, equipVisual;
        string heldKey, heldOffKey, equipKey;
        Renderer[] rends;
        float hurtPulse;
        bool dead;

        public MobVisual(Mob m) : this(m, m.def.model ?? m.def.id) { }

        public MobVisual(LivingEntity m, string modelName)
        {
            mob = m;
            def = MobModels.Get(modelName) ?? (m is Mob mm ? MobModels.Get(mm.def.id) : null);
            if (def == null) { root = new GameObject("Entity_" + modelName); return; }
            root = ModelRenderer.Build(def, null, Variant.ToString());
            if (root == null) { root = new GameObject("Entity_" + modelName); return; }
            Bind();
        }

        /// <summary>Fits the freshly built model to the mob's hitbox and indexes its bones for the rig.</summary>
        void Bind()
        {
            var m = mob;
            // measure the posed model in its own root space (bone meshes are stored relative to their pivots)
            root.transform.localScale = Vector3.one;
            float minY = float.MaxValue, maxY = float.MinValue;
            var inv = root.transform.worldToLocalMatrix;
            foreach (var r in root.GetComponentsInChildren<MeshFilter>(true))
            {
                if (r.sharedMesh == null) continue;
                var b = r.sharedMesh.bounds;
                var toRoot = inv * r.transform.localToWorldMatrix;
                for (int c = 0; c < 8; c++)
                {
                    var corner = new Vector3((c & 1) == 0 ? b.min.x : b.max.x, (c & 2) == 0 ? b.min.y : b.max.y, (c & 4) == 0 ? b.min.z : b.max.z);
                    float y = toRoot.MultiplyPoint3x4(corner).y;
                    minY = Mathf.Min(minY, y); maxY = Mathf.Max(maxY, y);
                }
            }
            if (minY == float.MaxValue) { minY = 0; maxY = 1.8f; }
            float modelHeight = Mathf.Max(0.05f, maxY - minY);
            float targetHeight = m.height > 0.02f ? m.height : (m is Mob mm2 ? mm2.def.height : 1.8f);
            // models are authored at one pixel = 1/16 block like the original; keep that scale unless the model is
            // clearly authored at a different size than its hitbox (then fit it). Players render at 15/16.
            float s = def.scale;
            if (def.name == "player") s *= 0.9375f;
            float fitted = modelHeight * s;
            if (fitted > targetHeight * 1.9f || fitted < targetHeight * 0.45f) s = targetHeight / modelHeight;
            root.transform.localScale = Vector3.one * s;
            heightOffset = -minY * s;

            bones.Clear(); restRot.Clear();
            legList.Clear(); armList.Clear(); wingList.Clear(); tailList.Clear();
            tentacleList.Clear(); segList.Clear(); rodList.Clear(); neckList.Clear(); eyes.Clear(); legNames.Clear();
            body = head = jaw = armsBone = heldBone = null;
            foreach (var b in def.bones)
            {
                var t = ModelRenderer.FindBone(root.transform, b.name);
                if (t == null) continue;
                bones[b.name] = t;
                restRot[b.name] = b.rotation;
                string n = b.name;
                if (n == "body" || n == "abdomen") body = t;
                else if (n == "head") head = t;
                else if (n == "jaw") jaw = t;
                else if (n == "arms") armsBone = t;
                else if (n == "held_block") heldBone = t;
                else if (n.StartsWith("wing")) wingList.Add(t);
                else if (n.StartsWith("tail")) tailList.Add(t);
                else if (n.StartsWith("tentacle")) tentacleList.Add(t);
                else if (n.StartsWith("seg")) segList.Add(t);
                else if (n.StartsWith("rod")) rodList.Add(t);
                else if (n.StartsWith("neck")) neckList.Add(t);
                else if (n.StartsWith("eye")) eyes.Add(t);
                else if (n.StartsWith("leg") || n.StartsWith("rear_leg") || n.StartsWith("front_leg") || n.EndsWith("_leg"))
                {
                    legList.Add(t);
                    legNames.Add(n);
                }
                else if (n.StartsWith("right_arm") || n.StartsWith("left_arm") || n.StartsWith("upper_limb") || n.StartsWith("lower_limb"))
                    armList.Add(t);
            }
            if (head == null) head = body;
            LogOnce();
        }

        /// <summary>Diagnostics for the automated tests: where the model's face points compared with the mob's view.</summary>
        public string Probe()
        {
            if (root == null) return "no root";
            var h = head != null ? head : root.transform;
            var look = mob.LookDir; look.y = 0; look.Normalize();
            var fwd = h.forward; fwd.y = 0; fwd.Normalize();
            return mob.TypeId + " yaw=" + mob.yaw.ToString("0") + " body=" + mob.bodyYaw.ToString("0") + " head.fwd=" + fwd.ToString("0.00") + " look=" + look.ToString("0.00") + " dot=" + Vector3.Dot(fwd, look).ToString("0.00");
        }

        static readonly HashSet<string> logged = new HashSet<string>();
        /// <summary>One diagnostic line per model: which bones have geometry, in which layer, and their extents.</summary>
        void LogOnce()
        {
            if (!logged.Add(def.name)) return;
            var sb = new System.Text.StringBuilder("[Model] " + def.name + " bones=" + def.bones.Count + " height=" + mob.height.ToString("0.##") + " scale=" + root.transform.localScale.x.ToString("0.###") + "\n");
            foreach (var b in def.bones)
            {
                var t = ModelRenderer.FindBone(root.transform, b.name);
                if (t == null) { sb.Append("  ").Append(b.name).Append(": MISSING\n"); continue; }
                var list = new List<string>();
                foreach (var mf in t.GetComponentsInChildren<MeshFilter>())
                {
                    var mesh = mf.sharedMesh;
                    if (mesh == null) continue;
                    int tris = 0;
                    for (int sm = 0; sm < mesh.subMeshCount; sm++) tris += (int)(mesh.GetIndexCount(sm) / 3);
                    list.Add(mf.name + "[" + tris + "t v" + mesh.vertexCount + " b" + mesh.bounds.min.ToString("0.#") + ".." + mesh.bounds.max.ToString("0.#") + "]");
                }
                sb.Append("  ").Append(b.name).Append(" local=").Append(b.pivot.ToString("0.#")).Append(" parts=").Append(list.Count == 0 ? "none" : string.Join(",", list)).Append('\n');
            }
            Debug.Log(sb.ToString());
        }

        /// <summary>Rebuild the visuals after a change that is baked into the model (variant, shear, charge).</summary>
        public void Refresh()
        {
            if (root == null) return;
            var wasActive = root.activeSelf;
            var pos = root.transform.position;
            Object.Destroy(root);
            root = ModelRenderer.Build(def, null, Variant.ToString());
            if (root == null) return;
            // the old bones died with the old root: re-index them (and re-fit the scale) on the new model
            Bind();
            root.SetActive(wasActive);
            root.transform.position = pos;
            mob.go = root;
            rends = null;
            heldVisual = heldOffVisual = null;
            heldKey = heldOffKey = null;
            armorModels.Clear(); armorBones.Clear(); armorKey = null;
        }

        // ------------------------------------------------------------------ per-frame
        public void Update(float partial)
        {
            if (root == null || def == null) return;
            var t = root.transform;
            t.position = mob.InterpPos(partial) + Vector3.up * heightOffset;
            t.rotation = Quaternion.Euler(0, Mathf.LerpAngle(mob.prevBodyYaw, mob.bodyYaw, partial), 0);

            float walk = Mathf.Lerp(mob.prevWalkAnimSpeed, mob.walkAnimSpeed, partial);
            float phase = mob.walkAnimPos + walk * partial;
            float swing = mob.attackAnim;
            float prevSwing = mob.prevAttackAnim;
            float at = Mathf.LerpAngle(prevSwing * 360f, swing * 360f, partial) / 360f;
            if (swing < prevSwing) at = swing; else at = prevSwing + (swing - prevSwing) * partial;
            float headYawAim = MathX.WrapAngle(Mathf.LerpAngle(mob.prevHeadYaw, mob.headYaw, partial) - Mathf.LerpAngle(mob.prevBodyYaw, mob.bodyYaw, partial));
            float headPitchAim = Mathf.Lerp(-mob.prevPitch, -mob.pitch, partial);

            dead = mob.dead || mob.health <= 0;
            if (dead)
            {
                float d = Mathf.Clamp01((mob.deathTime + partial) / 20f);
                t.rotation *= Quaternion.Euler(0, 0, 0);
                t.rotation = Quaternion.Euler(-90f * d, t.rotation.eulerAngles.y, 0);
                t.position += Vector3.up * (0.1f * d);
                for (int i = 0; i < legList.Count; i++) legList[i].localRotation = Rest(legList[i]);
                for (int i = 0; i < armList.Count; i++) armList[i].localRotation = Rest(armList[i]);
                goto light;
            }
            if (mob.sneaking) t.position += Vector3.down * 0.05f;
            if (Sitting) t.position += Vector3.down * 0.12f;

            ApplyRig(phase, walk, at, headYawAim, headPitchAim, partial);
        light:
            UpdateHeld();
            UpdateArmor();
            var light = EntityLight.Sample(mob.world, mob.position + Vector3.up * mob.height * 0.5f);
            if (rends == null) rends = root.GetComponentsInChildren<Renderer>();
            float flash = mob.hurtTime > 0 ? 0.55f : 0f;
            if (mob.glowing) flash = Mathf.Max(flash, 0.15f);
            Vector4 overlay = Vector4.zero;
            if (mob.hurtTime > 0) overlay = new Vector4(1f, 0.3f, 0.3f, 0.35f * (mob.hurtTime / (float)mob.hurtDuration));
            else if (mob.onFire) overlay = new Vector4(1f, 0.5f, 0.1f, 0.3f);
            else if (mob is CreeperMob cm && cm.swell > 0.01f) overlay = new Vector4(1f, 1f, 1f, Mathf.Min(0.8f, cm.swell));
            else if (mob is EndermanMob em && em.screaming) overlay = new Vector4(0.6f, 0.2f, 0.9f, 0.25f);
            else if (mob is SlimeMob sm && sm.absorbed != 0) overlay = new Vector4(1f, 0.9f, 0.4f, 0.2f);
            float alpha = 1f;
            if (mob is CreakingMob cr && cr.frozen) alpha = 0.6f;
            EntityLight.ApplyRenderers(rends, light, flash, overlay);
            if (alpha < 1f) SetAlpha(alpha);
            hurtPulse = Mathf.MoveTowards(hurtPulse, 0f, 0.05f);
        }
        bool swelledPose;

        void SetAlpha(float a)
        {
            if (rends == null) return;
            var mpb = new MaterialPropertyBlock();
            foreach (var r in rends)
            {
                if (r == null) continue;
                r.GetPropertyBlock(mpb);
                mpb.SetColor(EntityLight.ColorId, new Color(1, 1, 1, a));
                r.SetPropertyBlock(mpb);
            }
        }

        // ------------------------------------------------------------------ rigs
        void ApplyRig(float phase, float walk, float at, float headYaw, float headPitch, float partial)
        {
            switch (def.rig)
            {
                case RigKind.Biped: BipedRig(phase, walk, at, headYaw, headPitch); break;
                case RigKind.Quadruped: case RigKind.Horse: QuadrupedRig(phase, walk, headYaw, headPitch); break;
                case RigKind.Villager: BipedRig(phase, walk, at, headYaw, headPitch); break;
                case RigKind.Spider: SpiderRig(phase, walk, headYaw); break;
                case RigKind.Creeper: CreeperRig(phase, walk, headYaw, headPitch); break;
                case RigKind.Slime: SlimeRig(walk); break;
                case RigKind.Chicken: ChickenRig(phase, walk, headYaw, headPitch); break;
                case RigKind.Squid: SquidRig(partial); break;
                case RigKind.Ghast: GhastRig(walk); break;
                case RigKind.Blaze: BlazeRig(phase, walk, partial); break;
                case RigKind.Fish: FishRig(phase, walk); break;
                case RigKind.Bat: FlyerWings(0.6f, walk); break;
                case RigKind.Flyer: FlyerRig(phase, walk, headYaw, headPitch, partial); break;
                case RigKind.Guardian: GuardianRig(walk, partial); break;
                case RigKind.Dragon: DragonRig(phase, walk, headYaw, partial); break;
                case RigKind.Wither: WitherRig(partial, headYaw, headPitch); break;
                case RigKind.Shulker: ShulkerRig(headYaw, headPitch); break;
                case RigKind.Snake: SnakeRig(partial); break;
                case RigKind.Golem: case RigKind.Static: case RigKind.Boat: case RigKind.Minecart: case RigKind.Crystal:
                    GenericIdle(phase, walk, headYaw); break;
                default: GenericIdle(phase, walk, headYaw); break;
            }
            // arms of everything else follow the generic swing
            if (armList.Count > 0 && def.rig != RigKind.Biped && def.rig != RigKind.Villager && def.rig != RigKind.Golem)
                for (int i = 0; i < armList.Count; i++) armList[i].localRotation = Rest(armList[i]) * Quaternion.Euler(ArmPitch(i, at), 0, 0);
        }

        void BipedRig(float phase, float walk, float at, float headYaw, float headPitch)
        {
            for (int i = 0; i < legList.Count; i++)
            {
                bool right = legNames[i].Contains("r");
                float a = Mathf.Cos(phase) * 40f * walk;
                legList[i].localRotation = Rest(legList[i]) * Quaternion.Euler(right ? a : -a, 0, 0);
            }
            for (int i = 0; i < armList.Count; i++)
            {
                bool right = armList[i].name.Contains("r") || armList[i].name.Contains("right");
                float a;
                if (Aggressive) { a = -95f + Mathf.Cos(phase) * (right ? 5f : -5f) * walk; }
                else a = -Mathf.Cos(phase) * 40f * walk * (right ? 1f : -1f);
                a += ArmPitch(i, at);
                float z = mob is ZombieMob ? (right ? -9f : 9f) : 0f;
                armList[i].localRotation = Rest(armList[i]) * Quaternion.Euler(a, 0, z);
            }
            AimHead(headYaw, headPitch, 0.5f);
            BobBody(phase, walk);
        }

        void QuadrupedRig(float phase, float walk, float headYaw, float headPitch)
        {
            for (int i = 0; i < legList.Count; i++)
            {
                var n = legNames[i];
                bool front = n.StartsWith("leg_f") || n.StartsWith("front_leg");
                bool right = n.Contains("_r") || n.Contains("fr") || n.Contains("br");
                float off = front ? 0f : Mathf.PI;
                float a = Mathf.Cos(phase + off) * 42f * walk;
                legList[i].localRotation = Rest(legList[i]) * Quaternion.Euler(a * (right ? 1f : -1f), 0, 0);
            }
            AimHead(headYaw, headPitch, 0.6f);
            BobBody(phase, walk);
            if (tailList.Count > 0) TailWave(tailList, 0.5f, 0.35f + walk * 0.3f);
        }

        void SpiderRig(float phase, float walk, float headYaw)
        {
            for (int i = 0; i < legList.Count; i++)
            {
                var n = legNames[i];
                // legs come in four pairs, front left/right and back left/right, each with a wider "knee"
                float off = n.Contains("f") ? 0f : Mathf.PI * 0.5f;
                float side = n.Contains("_r") || n.EndsWith("r") ? 1f : -1f;
                float a = Mathf.Cos(phase + off + (i % 2) * Mathf.PI) * 30f * (0.4f + walk);
                legList[i].localRotation = Rest(legList[i]) * Quaternion.Euler(0, a * side * 0.5f, a);
            }
            AnimateSegments(pid => Mathf.Sin(pid * 0.9f + mob.age * 0.2f) * 1.5f);
            AimHead(headYaw, 0f, 0.4f);
        }

        void CreeperRig(float phase, float walk, float headYaw, float headPitch)
        {
            for (int i = 0; i < legList.Count; i++)
            {
                bool right = legNames[i].Contains("r") && !legNames[i].Contains("fl");
                float a = Mathf.Cos(phase + (i % 2 == 0 ? 0 : Mathf.PI)) * 36f * walk;
                legList[i].localRotation = Rest(legList[i]) * Quaternion.Euler(a, 0, 0);
            }
            float swell = 0f;
            if (mob is CreeperMob cm) swell = Mathf.Lerp(cm.prevSwell, cm.swell, 0.5f);
            float pulse = 1f + Mathf.Sin(mob.age * 0.9f) * 0.06f * swell;
            if (body != null) body.localScale = new Vector3(pulse, pulse, pulse);
            AimHead(headYaw, headPitch, 0.3f);
        }

        void SlimeRig(float walk)
        {
            float squash = 1f;
            if (mob is SlimeMob sm) squash = 1f + (Mathf.Lerp(sm.prevSquish, sm.squish, 0.5f) - 1f) * 0.5f;
            if (body != null) body.localScale = new Vector3(1f / Mathf.Max(0.5f, squash), squash, 1f / Mathf.Max(0.5f, squash));
            AnimateSegments(pid => 0f);
        }

        void ChickenRig(float phase, float walk, float headYaw, float headPitch)
        {
            for (int i = 0; i < legList.Count; i++)
            {
                bool right = legNames[i].Contains("_r") || legNames[i].Contains("br");
                float a = Mathf.Cos(phase + (right ? 0 : Mathf.PI)) * 38f * walk;
                legList[i].localRotation = Rest(legList[i]) * Quaternion.Euler(a, 0, 0);
            }
            float flap = 0f;
            if (mob is ChickenMob ck) flap = Mathf.Lerp(ck.prevFlap, ck.flap, 0.5f);
            for (int i = 0; i < wingList.Count; i++)
            {
                float dir = wingList[i].name.Contains("left") ? -1f : 1f;
                wingList[i].localRotation = Rest(wingList[i]) * Quaternion.Euler(0, 0, (dir * -25f) * (0.35f + flap));
            }
            AimHead(headYaw, headPitch, 0.4f);
            BobBody(phase, walk);
        }

        void SquidRig(float partial)
        {
            float a = (mob as SquidMob) != null ? Mathf.Lerp((mob as SquidMob).prevTentacle, (mob as SquidMob).tentacleAngle, partial) : 0f;
            for (int i = 0; i < tentacleList.Count; i++)
            {
                float ph = i * 0.8f;
                tentacleList[i].localRotation = Rest(tentacleList[i]) * Quaternion.Euler(Mathf.Sin(mob.age * 0.4f + ph) * 12f + a, 0, Mathf.Cos(mob.age * 0.4f + ph) * 12f);
            }
            if (body != null)
            {
                float pitchL = mob is SquidMob sq2 ? sq2.bodyPitch : 0f;
                body.localRotation = Rest(body) * Quaternion.Euler(pitchL, 0, 0);
            }
        }

        void GhastRig(float walk)
        {
            for (int i = 0; i < tentacleList.Count; i++)
            {
                float ph = i * 1.3f;
                tentacleList[i].localRotation = Rest(tentacleList[i]) * Quaternion.Euler(Mathf.Sin(mob.age * 0.15f + ph) * 8f, 0, Mathf.Cos(mob.age * 0.15f + ph) * 8f);
            }
        }

        void BlazeRig(float phase, float walk, float partial)
        {
            for (int i = 0; i < rodList.Count || i < segList.Count; i++)
            {
                if (i < rodList.Count)
                {
                    float a = mob.age * 20f + i * 45f;
                    rodList[i].localRotation = Rest(rodList[i]) * Quaternion.Euler(Mathf.Sin(a * Mathf.Deg2Rad) * 15f, a, 0);
                }
                if (i < segList.Count) segList[i].localRotation = Rest(segList[i]) * Quaternion.Euler(0, mob.age * 40f + i * 30f, 0);
            }
        }

        void FishRig(float phase, float walk)
        {
            float ph = mob.age * 0.6f;
            if (body != null) body.localRotation = Rest(body) * Quaternion.Euler(Mathf.Sin(ph) * 3f, 0, 0);
            for (int i = 0; i < tailList.Count; i++)
            {
                float s = Mathf.Sin(ph - i * 0.7f);
                tailList[i].localRotation = Rest(tailList[i]) * Quaternion.Euler(0, s * 22f, 0);
            }
            for (int i = 0; i < wingList.Count; i++) wingList[i].localRotation = Rest(wingList[i]) * Quaternion.Euler(0, 0, Mathf.Sin(ph * 1.4f + i) * 15f);
        }

        void FlyerWings(float speed, float walk)
        {
            float a = Mathf.Sin(mob.age * speed * 0.55f) * (20f + walk * 30f);
            for (int i = 0; i < wingList.Count; i++)
            {
                float dir = wingList[i].name.Contains("left") ? -1f : 1f;
                wingList[i].localRotation = Rest(wingList[i]) * Quaternion.Euler(0, 0, dir * a);
            }
            if (wingList.Count == 0)
                for (int i = 0; i < armList.Count; i++)
                {
                    float dir = armList[i].name.Contains("left") ? -1f : 1f;
                    armList[i].localRotation = Rest(armList[i]) * Quaternion.Euler(0, 0, dir * a);
                }
        }

        void FlyerRig(float phase, float walk, float headYaw, float headPitch, float partial)
        {
            FlyerWings(0.9f, walk);
            AimHead(headYaw * 0.5f, headPitch, 0.3f);
            if (mob is BeeMob bee) FlyerWings(bee.angry ? 3.2f : 0.7f, walk);
        }

        void GuardianRig(float walk, float partial)
        {
            if (body != null) body.localRotation = Rest(body) * Quaternion.Euler(0, Mathf.Sin(mob.age * 0.12f) * 6f, 0);
            float spikes = mob is GuardianMob g ? Mathf.Lerp(g.prevSpikes, g.spikes, partial) : 0f;
            for (int i = 0; i < segList.Count; i++)
                segList[i].localScale = Vector3.one * (1f + spikes * 0.35f);
            if (tailList.Count > 0) TailWave(tailList, 0.35f, 0.5f);
        }

        void DragonRig(float phase, float walk, float headYaw, float partial)
        {
            float flap = Mathf.Sin(mob.age * 0.25f) * 32f;
            for (int i = 0; i < wingList.Count; i++)
            {
                bool left = wingList[i].name.Contains("left");
                float dir = left ? -1f : 1f;
                float roll = wingList[i].name.Contains("tip") ? flap * 0.6f : flap;
                wingList[i].localRotation = Rest(wingList[i]) * Quaternion.Euler(0, dir * roll * 0.4f, dir * roll);
            }
            for (int i = 0; i < neckList.Count; i++)
                neckList[i].localRotation = Rest(neckList[i]) * Quaternion.Euler(Mathf.Sin(mob.age * 0.08f - i * 0.5f) * 4f, headYaw * 0.25f, 0);
            for (int i = 0; i < tailList.Count; i++)
                tailList[i].localRotation = Rest(tailList[i]) * Quaternion.Euler(Mathf.Sin(mob.age * 0.1f - i * 0.6f) * 6f, 0, 0);
            for (int i = 0; i < legList.Count; i++)
                legList[i].localRotation = Rest(legList[i]) * Quaternion.Euler(Mathf.Sin(mob.age * 0.25f + i) * 12f, 0, 0);
            if (jaw != null) jaw.localRotation = Rest(jaw) * Quaternion.Euler(0, 0, 0);
            for (int i = 0; i < eyes.Count; i++) { }
        }

        void WitherRig(float partial, float headYaw, float headPitch)
        {
            float idle = Mathf.Sin(mob.age * 0.1f) * 5f;
            if (head != null) head.localRotation = Rest(head) * Quaternion.Euler(-headPitch, headYaw, idle);
            var hr = bones.TryGetValue("head_right", out var r) ? r : null;
            var hl = bones.TryGetValue("head_left", out var l) ? l : null;
            if (hr != null) hr.localRotation = Rest(hr) * Quaternion.Euler(-headPitch * 0.6f, 0, idle * 1.5f);
            if (hl != null) hl.localRotation = Rest(hl) * Quaternion.Euler(-headPitch * 0.6f, 0, -idle * 1.5f);
            for (int i = 0; i < tailList.Count; i++)
                tailList[i].localRotation = Rest(tailList[i]) * Quaternion.Euler(0, Mathf.Sin(mob.age * 0.12f - i * 0.5f) * 8f, 0);
        }

        void ShulkerRig(float headYaw, float headPitch)
        {
            float peek = 0f;
            if (mob is ShulkerMob sh) peek = Mathf.Lerp(sh.prevPeek, sh.peek, 0.5f);
            var lid = bones.TryGetValue("lid", out var l) ? l : null;
            if (lid != null) lid.localPosition = new Vector3(0, 0, 0) + Vector3.up * (peek * 0.4f);
            if (body != null) body.localRotation = Rest(body) * Quaternion.Euler(0, headYaw, 0);
        }

        void SnakeRig(float partial)
        {
            AnimateSegments(pid => Mathf.Sin(mob.age * 0.08f - pid * 0.5f) * 6f);
        }

        void GenericIdle(float phase, float walk, float headYaw)
        {
            AimHead(headYaw, 0f, 0.3f);
            if (tailList.Count > 0) TailWave(tailList, 0.3f, 0.4f);
            AnimateSegments(pid => Mathf.Sin(pid * 0.7f + mob.age * 0.1f) * 2f);
        }

        // ------------------------------------------------------------------ bone helpers
        Quaternion Rest(Transform t)
        {
            return restRot.TryGetValue(t.name, out var r) ? Quaternion.Euler(r) : t.localRotation;
        }

        void AimHead(float yaw, float pitch, float factor)
        {
            if (head == null) return;
            yaw = Mathf.Clamp(MathX.WrapAngle(yaw), -75f, 75f) * factor;
            pitch = Mathf.Clamp(pitch, -40f, 40f) * factor;
            head.localRotation = Rest(head) * Quaternion.Euler(pitch, yaw, 0);
        }

        void BobBody(float phase, float walk)
        {
            if (body == null) return;
            body.localRotation = Rest(body) * Quaternion.Euler(0, 0, Mathf.Cos(phase) * 1.2f * walk);
        }

        void TailWave(List<Transform> list, float amp, float phaseStep)
        {
            for (int i = 0; i < list.Count; i++)
                list[i].localRotation = Rest(list[i]) * Quaternion.Euler(0, Mathf.Sin(mob.age * 0.15f - i * phaseStep) * amp * 10f, 0);
        }

        void AnimateSegments(System.Func<float, float> f)
        {
            for (int i = 0; i < segList.Count; i++)
                segList[i].localRotation = Rest(segList[i]) * Quaternion.Euler(f(i), 0, 0);
        }

        float ArmPitch(int index, float at)
        {
            if (at <= 0f) return 0f;
            // one full swing over the attack duration, eased
            float f = at < 0.5f ? at * 2f : (1f - at) * 2f;
            float a = -f * 110f;
            return index == 0 ? a : a * 0.35f;
        }

        // ------------------------------------------------------------------ equipment
        void UpdateHeld()
        {
            var main = mob.MainHand;
            var off = mob.OffHand;
            string mk = main != null && !main.IsEmpty ? main.item.id + ":" + main.count : null;
            if (mk != heldKey)
            {
                heldKey = mk;
                if (heldVisual != null) { Object.Destroy(heldVisual); heldVisual = null; }
                var hand = HandBone(0);
                if (mk != null && hand != null)
                {
                    heldVisual = ItemRender.CreateHeldModelVisual(main, 1f);
                    if (heldVisual != null)
                    {
                        heldVisual.transform.SetParent(hand, false);
                        heldVisual.transform.localPosition = HeldOffset(main);
                        heldVisual.transform.localRotation = Quaternion.Euler(-90f, 0, 0);
                        heldVisual.transform.localScale = Vector3.one * 0.6f;
                    }
                }
            }
            string ok = off != null && !off.IsEmpty ? off.item.id + ":" + off.count : null;
            if (ok != heldOffKey)
            {
                heldOffKey = ok;
                if (heldOffVisual != null) { Object.Destroy(heldOffVisual); heldOffVisual = null; }
                var hand = HandBone(1);
                if (ok != null && hand != null)
                {
                    heldOffVisual = ItemRender.CreateHeldModelVisual(off, 1f);
                    if (heldOffVisual != null)
                    {
                        heldOffVisual.transform.SetParent(hand, false);
                        heldOffVisual.transform.localPosition = HeldOffset(off);
                        heldOffVisual.transform.localRotation = Quaternion.Euler(-90f, 0, 0);
                        heldOffVisual.transform.localScale = Vector3.one * 0.6f;
                    }
                }
            }
        }

        // ------------------------------------------------------------------ worn armour
        // one inflated "armor" model per material worn (pieces of different materials need different skins),
        // showing only the parts of the pieces in that material; its bones copy the wearer's pose every frame
        readonly List<GameObject> armorModels = new List<GameObject>();
        readonly List<Transform[]> armorBones = new List<Transform[]>();
        string armorKey;
        static readonly string[] ArmorBoneNames = { "body", "head", "right_arm", "left_arm", "right_leg", "left_leg" };
        static readonly string[][] ArmorParts =
        {
            new[] { "boot_r", "boot_l" },                  // feet
            new[] { "leggings_top", "leg_r", "leg_l" },    // legs
            new[] { "chest", "arm_r", "arm_l" },           // chest
            new[] { "helmet" },                            // head
        };

        void UpdateArmor()
        {
            if (def.rig != RigKind.Biped || !bones.ContainsKey("head") || !bones.ContainsKey("right_leg")) return;
            var sb = new System.Text.StringBuilder();
            for (int slot = 0; slot < 4; slot++)
            {
                var a = mob.GetArmor(slot);
                sb.Append(a != null && !a.IsEmpty && a.item is ArmorItem ? a.item.id : "-").Append('|');
            }
            string key = sb.ToString();
            if (key != armorKey)
            {
                armorKey = key;
                foreach (var g in armorModels) if (g != null) Object.Destroy(g);
                armorModels.Clear(); armorBones.Clear();
                var armorDef = MobModels.Get("armor");
                if (armorDef != null)
                {
                    var byMaterial = new Dictionary<string, List<int>>();
                    for (int slot = 0; slot < 4; slot++)
                    {
                        var a = mob.GetArmor(slot);
                        if (a == null || a.IsEmpty || !(a.item is ArmorItem)) continue;
                        string id = a.item.id;
                        int us = id.LastIndexOf('_');
                        string material = us > 0 ? id.Substring(0, us) : "iron";
                        if (!byMaterial.TryGetValue(material, out var list)) byMaterial[material] = list = new List<int>();
                        list.Add(slot);
                    }
                    foreach (var kv in byMaterial)
                    {
                        var model = ModelRenderer.Build(armorDef, root.transform, kv.Key);
                        if (model == null) continue;
                        model.name = "armor_" + kv.Key;
                        for (int slot = 0; slot < 4; slot++)
                            foreach (var part in ArmorParts[slot]) ModelRenderer.SetOptional(model, part, kv.Value.Contains(slot));
                        var map = new Transform[ArmorBoneNames.Length];
                        for (int i = 0; i < ArmorBoneNames.Length; i++) map[i] = ModelRenderer.FindBone(model.transform, ArmorBoneNames[i]);
                        armorModels.Add(model); armorBones.Add(map);
                        SetLayerLike(model, root.layer);
                    }
                }
                rends = null; // the light pass picks the new renderers up
            }
            // follow the wearer's pose
            for (int m = 0; m < armorModels.Count; m++)
            {
                var map = armorBones[m];
                for (int i = 0; i < ArmorBoneNames.Length; i++)
                {
                    if (map[i] == null || !bones.TryGetValue(ArmorBoneNames[i], out var src) || src == null) continue;
                    map[i].localPosition = src.localPosition;
                    map[i].localRotation = src.localRotation;
                    map[i].localScale = src.localScale;
                }
            }
        }

        static void SetLayerLike(GameObject go, int layer)
        {
            go.layer = layer;
            foreach (Transform c in go.transform) SetLayerLike(c.gameObject, layer);
        }

        Vector3 HeldOffset(ItemStack s)
        {
            var hand = HandBone(0);
            float drop = hand != null ? 0.09f : 0f;
            return new Vector3(0, -drop, 0.06f);
        }

        Transform HandBone(int side)
        {
            // the "arm" bones double as hand anchors; the model's own left/right naming decides which
            if (armList.Count > 0)
            {
                for (int i = 0; i < armList.Count; i++)
                {
                    bool right = armList[i].name.Contains("right") || armList[i].name.EndsWith("_r");
                    if ((side == 0) == right) return armList[i];
                }
                return armList[0];
            }
            return bones.TryGetValue("shaft", out var sh) ? sh : (body != null ? body : root.transform);
        }

        public void Destroy()
        {
            if (root != null) Object.Destroy(root);
            root = null;
            rends = null;
        }
    }
}
