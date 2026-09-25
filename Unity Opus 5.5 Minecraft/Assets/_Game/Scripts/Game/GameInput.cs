using UnityEngine;
using UnityEngine.InputSystem;

namespace MCR
{
    /// <summary>
    /// Keyboard and mouse handling (Input System package): mouse look, movement keys, hotbar selection,
    /// the F-key debug combos, and feeding typed text and clicks to the interface.
    /// Held states are sampled every frame; edge-triggered actions are latched until the next game tick.
    /// </summary>
    public sealed partial class GameManager
    {
        // latched until consumed by the next tick
        bool attackPressedLatch, usePressedLatch, pickLatch;
        /// <summary>Scripted input for the automated tests: ticks left to hold a button or walk forward.</summary>
        public int scriptAttackTicks, scriptUseTicks, scriptWalkTicks;
        bool scriptAttackStarted, scriptUseStarted;
        bool attackHeld, useHeld;
        bool f3Down, f3Combo;
        int lastJumpTick = -100, lastForwardTick = -100;
        bool forwardWasDown;
        public bool showHitboxes, showChunkBorders, advancedTooltips;
        float scrollAccum;

        void OnEnable() { Keyboard.current?.SetIMEEnabled(false); if (Keyboard.current != null) Keyboard.current.onTextInput += OnTextInput; }
        void OnDisable() { if (Keyboard.current != null) Keyboard.current.onTextInput -= OnTextInput; }
        void OnTextInput(char c)
        {
            if (hud == null) return;
            if (c == '\b' || c == '\r' || c == '\n' || c == 27) return;
            if (char.IsControl(c)) return;
            hud.typedChars.Add(c);
        }

        /// <summary>Samples devices at the start of the frame and fills the interface's input fields.</summary>
        void PollInputFrame()
        {
            var kb = Keyboard.current;
            var mouse = Mouse.current;
            if (hud == null) return;
            if (mouse != null)
            {
                var mp = mouse.position.ReadValue();
                hud.mouseX = mp.x / hud.scale;
                hud.mouseY = (UnityEngine.Screen.height - mp.y) / hud.scale;
                hud.mouseDown = mouse.leftButton.isPressed;
                hud.mouseDownRight = mouse.rightButton.isPressed;
                hud.mouseClicked |= mouse.leftButton.wasPressedThisFrame;
                hud.mouseClickedRight |= mouse.rightButton.wasPressedThisFrame;
                hud.mouseReleased |= mouse.leftButton.wasReleasedThisFrame;
                hud.mouseReleasedRight |= mouse.rightButton.wasReleasedThisFrame;
                hud.mouseClickedMiddle |= mouse.middleButton.wasPressedThisFrame;
                float sc = mouse.scroll.ReadValue().y;
                if (Mathf.Abs(sc) > 0.01f)
                {
                    scrollAccum += sc / 120f;
                    int steps = (int)scrollAccum;
                    if (Mathf.Abs(sc) < 20f && steps == 0) steps = (int)Mathf.Sign(sc); // high-resolution wheels report small values
                    if (steps != 0) { hud.scrollDelta += steps; scrollAccum = 0f; }
                }
            }
            if (kb != null)
            {
                hud.shift = kb.leftShiftKey.isPressed || kb.rightShiftKey.isPressed;
                hud.ctrl = kb.leftCtrlKey.isPressed || kb.rightCtrlKey.isPressed;
                hud.alt = kb.leftAltKey.isPressed || kb.rightAltKey.isPressed;
                hud.pressedEnter |= kb.enterKey.wasPressedThisFrame || kb.numpadEnterKey.wasPressedThisFrame;
                hud.pressedEscape |= kb.escapeKey.wasPressedThisFrame;
                hud.pressedBackspace |= kb.backspaceKey.wasPressedThisFrame;
                hud.pressedDelete |= kb.deleteKey.wasPressedThisFrame;
                hud.keyUp |= kb.upArrowKey.wasPressedThisFrame;
                hud.keyDown |= kb.downArrowKey.wasPressedThisFrame;
                hud.keyLeft |= kb.leftArrowKey.wasPressedThisFrame;
                hud.keyRight |= kb.rightArrowKey.wasPressedThisFrame;
                hud.pressedQ |= kb.qKey.wasPressedThisFrame;
                hud.pressedF |= kb.fKey.wasPressedThisFrame;
                hud.pressedE |= kb.eKey.wasPressedThisFrame;
                hud.pressedTab |= kb.tabKey.wasPressedThisFrame;
                for (int i = 0; i < 9; i++) if (kb[Key.Digit1 + i].wasPressedThisFrame) hud.numberKey = i;
                if (kb.f11Key.wasPressedThisFrame) ToggleFullscreen();
                if (kb.f2Key.wasPressedThisFrame) TakeScreenshot();
            }
        }

        /// <summary>Per-frame gameplay input once the world tick has run: look, movement keys, hotkeys, screens.</summary>
        void UpdateInput(float dt)
        {
            var kb = Keyboard.current;
            var mouse = Mouse.current;
            var p = player;
            bool screenOpen = hud.AnyScreen;
            Cursor.lockState = screenOpen || session == null ? CursorLockMode.None : CursorLockMode.Locked;
            Cursor.visible = screenOpen || session == null;

            if (screenOpen)
            {
                hud.Input();
                ReleaseControls();
                return;
            }
            if (p == null || kb == null) { hud.Input(); return; }

            // ---- mouse look
            if (mouse != null && !p.sleeping)
            {
                var d = mouse.delta.ReadValue();
                float f = mouseSensitivity * 0.6f + 0.2f;
                float scale = f * f * f * 8f * 0.15f;
                if (IsZooming) scale *= 0.25f;
                p.yaw += d.x * scale;
                p.pitch = Mathf.Clamp(p.pitch - d.y * scale * (invertY ? -1f : 1f), -90f, 90f);
            }

            // ---- movement keys (applied every tick from the held state)
            attackHeld = mouse != null && mouse.leftButton.isPressed;
            useHeld = mouse != null && mouse.rightButton.isPressed;
            if (mouse != null)
            {
                if (mouse.leftButton.wasPressedThisFrame) attackPressedLatch = true;
                if (mouse.rightButton.wasPressedThisFrame) usePressedLatch = true;
                if (mouse.middleButton.wasPressedThisFrame) pickLatch = true;
            }

            // ---- hotbar
            for (int i = 0; i < 9; i++) if (kb[Key.Digit1 + i].wasPressedThisFrame) p.inventory.selected = i;
            if (hud.scrollDelta != 0)
            {
                int s = p.inventory.selected - hud.scrollDelta;
                p.inventory.selected = ((s % 9) + 9) % 9;
            }

            // ---- F3 combos
            if (kb.f3Key.wasPressedThisFrame) { f3Down = true; f3Combo = false; }
            if (kb.f3Key.isPressed)
            {
                if (kb.f4Key.wasPressedThisFrame) { f3Combo = true; hud.Push(new GameModeSwitcherScreen()); }
                if (kb.nKey.wasPressedThisFrame) { f3Combo = true; p.SetGameMode(p.IsSpectator ? GameMode.Creative : GameMode.Spectator); hud.Chat("Game mode set to " + p.gameMode); }
                if (kb.bKey.wasPressedThisFrame) { f3Combo = true; showHitboxes = !showHitboxes; hud.Chat("Hitboxes: " + (showHitboxes ? "shown" : "hidden")); }
                if (kb.gKey.wasPressedThisFrame) { f3Combo = true; showChunkBorders = !showChunkBorders; hud.Chat("Chunk borders: " + (showChunkBorders ? "shown" : "hidden")); }
                if (kb.aKey.wasPressedThisFrame) { f3Combo = true; chunks?.ReloadAll(); hud.Chat("Reloading all chunks"); }
                if (kb.dKey.wasPressedThisFrame) { f3Combo = true; hud.chat.Clear(); }
                if (kb.hKey.wasPressedThisFrame) { f3Combo = true; advancedTooltips = !advancedTooltips; hud.Chat("Advanced tooltips: " + (advancedTooltips ? "shown" : "hidden")); }
                if (kb.tKey.wasPressedThisFrame) { f3Combo = true; ItemIcons.Clear(); hud.Chat("Reloaded resource caches"); }
                if (kb.pKey.wasPressedThisFrame) { f3Combo = true; }
                if (kb.qKey.wasPressedThisFrame) { f3Combo = true; hud.Chat("F3+A reload chunks, F3+B hitboxes, F3+D clear chat, F3+G chunk borders, F3+H tooltips, F3+N spectator, F3+F4 game modes"); }
            }
            if (kb.f3Key.wasReleasedThisFrame)
            {
                if (!f3Combo) hud.showDebug = !hud.showDebug;
                f3Down = false;
            }
            if (kb.f3Key.isPressed) { hud.Input(); return; }

            // ---- single keys
            if (kb.f1Key.wasPressedThisFrame) hud.HideGui = !hud.HideGui;
            if (kb.f5Key.wasPressedThisFrame) p.cameraMode = (p.cameraMode + 1) % 3;
            if (kb.escapeKey.wasPressedThisFrame) { hud.Push(new PauseScreen()); hud.pressedEscape = false; }
            if (kb.eKey.wasPressedThisFrame && !p.IsSpectator) OpenPlayerInventory();
            if (kb.tKey.wasPressedThisFrame) { hud.OpenChat(); hud.typedChars.Clear(); }
            if (kb.slashKey.wasPressedThisFrame) { hud.OpenChat("/"); hud.typedChars.Clear(); }
            if (kb.lKey.wasPressedThisFrame) hud.Push(new AdvancementsScreen());
            if (kb.qKey.wasPressedThisFrame && !p.IsSpectator) { p.DropSelected(hud.ctrl); p.SwingArm(); }
            if (kb.fKey.wasPressedThisFrame && !p.IsSpectator)
            {
                var main = p.inventory.main[p.inventory.selected];
                p.inventory.main[p.inventory.selected] = p.inventory.offhand;
                p.inventory.offhand = main;
            }
            if (hud.ctrl && kb.sKey.wasPressedThisFrame) SaveAll();
            hud.Input();
        }

        bool IsZooming => player != null && player.IsUsingItem && player.usingStack != null && player.usingStack.item.id == "spyglass";

        void OpenPlayerInventory()
        {
            var p = player;
            if (p.IsCreative) { hud.Push(new CreativeScreen()); return; }
            p.OpenMenu(p.inventoryMenu);
        }

        void ReleaseControls()
        {
            attackHeld = useHeld = false;
            if (player != null) { player.moveForward = 0; player.moveStrafe = 0; player.jumping = false; }
            interaction?.StopMining();
        }

        /// <summary>Copies the held keys onto the player right before the world tick.</summary>
        void ApplyPlayerControls()
        {
            var p = player;
            var kb = Keyboard.current;
            if (p == null || kb == null) return;
            if (hud.AnyScreen || p.dead)
            {
                p.moveForward = 0; p.moveStrafe = 0; p.jumping = false;
                attackPressedLatch = usePressedLatch = pickLatch = false;
                return;
            }
            float fwd = 0, strafe = 0;
            bool forward = kb.wKey.isPressed || scriptWalkTicks > 0;
            if (scriptWalkTicks > 0) scriptWalkTicks--;
            if (forward) fwd += 1;
            if (kb.sKey.isPressed) fwd -= 1;
            if (kb.aKey.isPressed) strafe += 1;
            if (kb.dKey.isPressed) strafe -= 1;
            bool sneak = kb.leftShiftKey.isPressed;
            bool jump = kb.spaceKey.isPressed;
            bool sprintKey = kb.leftCtrlKey.isPressed;

            // double-tap forward starts sprinting, just like holding the sprint key
            if (forward && !forwardWasDown)
            {
                if (TicksRun - lastForwardTick < 7) p.sprinting = true;
                lastForwardTick = (int)TicksRun;
            }
            forwardWasDown = forward;
            bool canSprint = p.hunger.food > 6 || p.IsCreative || p.abilities.flying;
            if (sprintKey && forward && canSprint && !sneak) p.sprinting = true;
            if (!forward || p.horizontalCollision || (sneak && !p.abilities.flying) || !canSprint) p.sprinting = false;

            // double-tap jump toggles creative flight
            if (kb.spaceKey.wasPressedThisFrame || jumpEdge)
            {
                if (p.abilities.mayFly && TicksRun - lastJumpTick < 7)
                {
                    p.abilities.flying = !p.abilities.flying;
                    lastJumpTick = -100;
                }
                else lastJumpTick = (int)TicksRun;
            }
            jumpEdge = false;
            if (sneak && !p.abilities.flying) { fwd *= 0.3f; strafe *= 0.3f; }
            if (p.IsUsingItem && !p.abilities.flying) { fwd *= 0.2f; strafe *= 0.2f; }
            p.moveForward = fwd;
            p.moveStrafe = strafe;
            p.jumping = jump;
            p.sneaking = sneak;
            if (p.vehicle != null && sneak) p.StopRiding();

            // elytra: pressing jump while falling deploys the glider
            if (kb.spaceKey.wasPressedThisFrame && !p.onGround && !p.abilities.flying && p.velocity.y < 0) p.TryStartGliding();

            // scripted holds behave like a real press on their first tick and a held button afterwards
            bool atkHeld = attackHeld || scriptAttackTicks > 0, atkPress = attackPressedLatch || (scriptAttackTicks > 0 && !scriptAttackStarted);
            bool useHeldNow = useHeld || scriptUseTicks > 0, usePress = usePressedLatch || (scriptUseTicks > 0 && !scriptUseStarted);
            scriptAttackStarted = scriptAttackTicks > 0; scriptUseStarted = scriptUseTicks > 0;
            if (scriptAttackTicks > 0) scriptAttackTicks--;
            if (scriptUseTicks > 0) scriptUseTicks--;
            interaction?.Tick(atkHeld, atkPress, useHeldNow, usePress, pickLatch);
            attackPressedLatch = usePressedLatch = pickLatch = false;
        }
        bool jumpEdge;

        void ToggleFullscreen()
        {
            if (UnityEngine.Screen.fullScreen)
            {
                UnityEngine.Screen.fullScreenMode = FullScreenMode.Windowed;
                UnityEngine.Screen.SetResolution(1600, 900, false);
            }
            else
            {
                var r = UnityEngine.Screen.currentResolution;
                UnityEngine.Screen.SetResolution(r.width, r.height, FullScreenMode.FullScreenWindow);
            }
        }

        void TakeScreenshot()
        {
            var dir = System.IO.Path.Combine(Application.persistentDataPath, "screenshots");
            System.IO.Directory.CreateDirectory(dir);
            var path = System.IO.Path.Combine(dir, System.DateTime.Now.ToString("yyyy-MM-dd_HH.mm.ss") + ".png");
            ScreenCapture.CaptureScreenshot(path);
            hud?.Chat("Saved screenshot as " + System.IO.Path.GetFileName(path));
        }
    }
}
