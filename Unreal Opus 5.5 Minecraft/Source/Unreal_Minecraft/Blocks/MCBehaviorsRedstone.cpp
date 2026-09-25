// Redstone components: dust networks, torches, repeaters, comparators, lamps, pistons, observers,
// dispensers/droppers, hoppers, targets, daylight detectors, note blocks, TNT, redstone ore, copper bulbs.
#include "Blocks/MCBehaviorsInternal.h"
#include "Crafting/MCRecipes.h"

using namespace MCBeh;

namespace
{
	const EMCFace GHoriz[4] = { EMCFace::North, EMCFace::South, EMCFace::West, EMCFace::East };

	bool IsWire(FMCState S) { return FMCBlocks::GetByState(S).Model == EMCModel::RedstoneWire; }

	/** Does a component at S connect visually/electrically towards side F (F = direction from the wire to the component)? */
	bool ComponentConnects(FMCState S, EMCFace F)
	{
		if (S == 0) return false;
		const FMCBlock& B = FMCBlocks::GetByState(S);
		if (B.Model == EMCModel::RedstoneWire) return true;
		if (B.Model == EMCModel::Repeater)
		{
			const EMCFace Facing = MCMeta::Facing4(FMCBlocks::MetaOf(S));
			return Facing == F || Facing == MC::Opposite(F);
		}
		if (B.Name == TEXT("observer")) return MCMeta::Facing6(FMCBlocks::MetaOf(S)) == F;
		if (!B.Has(MCB_Redstone)) return false;
		if (B.Model == EMCModel::Piston || B.Model == EMCModel::Door || B.Model == EMCModel::Trapdoor || B.Model == EMCModel::FenceGate || B.Model == EMCModel::Rail) return false;
		if (B.Name == TEXT("redstone_lamp") || B.Name == TEXT("note_block") || B.Name == TEXT("dispenser") || B.Name == TEXT("dropper") || B.Name == TEXT("hopper")
			|| B.Name == TEXT("tnt") || B.Family == TEXT("copper_bulb") || B.Name == TEXT("lectern") || B.Name == TEXT("bell") || B.Name == TEXT("crafter")) return false;
		return true;
	}

	/** Four horizontal connection flags + "goes up" flags (Minecraft dust shape rules). */
	void WireConnections(const FMCWorld& W, const FMCBlockPos& P, bool Conn[4], bool Up[4])
	{
		const bool bAboveOpaque = FMCBlocks::IsOpaque(W.GetState(P.Up()));
		for (int32 i = 0; i < 4; ++i)
		{
			Conn[i] = Up[i] = false;
			const FMCBlockPos Side = P.Offset(GHoriz[i]);
			const FMCState SS = W.GetState(Side);
			if (ComponentConnects(SS, GHoriz[i])) { Conn[i] = true; continue; }
			if (!bAboveOpaque && FMCBlocks::IsOpaque(SS) && IsWire(W.GetState(Side.Up()))) { Conn[i] = true; Up[i] = true; continue; }
			if (!FMCBlocks::IsOpaque(SS) && IsWire(W.GetState(Side.Down()))) Conn[i] = true;
		}
	}

	/** Directions the dust actually powers (dots power all four sides, a single line extends both ways). */
	void WireOutputs(const FMCWorld& W, const FMCBlockPos& P, bool Out[4])
	{
		bool Conn[4], Up[4];
		WireConnections(W, P, Conn, Up);
		const int32 N = Conn[0] + Conn[1] + Conn[2] + Conn[3];
		if (N == 0) { Out[0] = Out[1] = Out[2] = Out[3] = true; return; }
		for (int32 i = 0; i < 4; ++i) Out[i] = Conn[i];
		if (N == 1)
		{
			if (Conn[0] || Conn[1]) Out[0] = Out[1] = true;
			else Out[2] = Out[3] = true;
		}
	}

	// ================================================================================================================
	class FRedstoneWireBeh : public FMCBlockBehavior
	{
	public:
		virtual bool CanSurvive(const FMCBlock& B, FMCWorld& W, const FMCBlockPos& P, FMCState S) const override
		{
			const FMCBlockPos Below = P.Down();
			const FMCState BS = W.GetState(Below);
			if (MCBehaviorUtil::HasSolidTop(W, Below)) return true;
			const FMCBlock& BB = FMCBlocks::GetByState(BS);
			return BB.Name == TEXT("hopper") || (BB.Model == EMCModel::Slab && FMCBlocks::MetaOf(BS) >= 1) || (BB.Model == EMCModel::Stairs && MCMeta::Bit(FMCBlocks::MetaOf(BS), 2));
		}
		virtual FMCState GetPlacementState(const FMCBlock& B, const FMCPlaceContext& C) const override
		{
			if (!C.World) return B.BaseState;
			GWireSignalsDisabled = true;
			const int32 P = C.World->GetBestNeighborPower(C.Pos);
			GWireSignalsDisabled = false;
			return B.State((uint8)P);
		}
		virtual void OnPlaced(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCPlayer* Player) const override { UpdateNetwork(W, P); NotifyShape(W, P); }
		virtual void OnRemoved(FMCWorld& W, const FMCBlockPos& P, FMCState Old, FMCState New) const override
		{
			if (FMCBlocks::BlockOf(New) == FMCBlocks::BlockOf(Old)) return;
			// neighbouring dust re-evaluates (including diagonals up/down)
			for (EMCFace F : GHoriz)
			{
				const FMCBlockPos Q = P.Offset(F);
				for (int32 dz = -1; dz <= 1; ++dz) if (IsWire(W.GetState(Q.Up(dz)))) UpdateNetwork(W, Q.Up(dz));
			}
			W.NotifyNeighbors(P.Down());
			W.NotifyNeighbors(P.Up());
		}
		virtual void OnNeighborChanged(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCBlockPos& From) const override
		{
			if (!CanSurvive(FMCBlocks::GetByState(S), W, P, S)) { BreakUnsupported(W, P); return; }
			if (GWireSignalsDisabled) return;
			UpdateNetwork(W, P);
		}
		void NotifyShape(FMCWorld& W, const FMCBlockPos& P) const
		{
			// shape changes of neighbours are purely visual: remesh
			for (EMCFace F : GHoriz) for (int32 dz = -1; dz <= 1; ++dz) if (IsWire(W.GetState(P.Offset(F).Up(dz)))) W.MarkBlockDirty(P.Offset(F).Up(dz));
		}
		/** Recompute power over the connected dust network with a Dijkstra-style relaxation. */
		static void UpdateNetwork(FMCWorld& W, const FMCBlockPos& Start)
		{
			static bool bBusy = false;
			if (bBusy) return;
			TGuardValue<bool> Guard(bBusy, true);
			TArray<FMCBlockPos> Nodes;
			TMap<FMCBlockPos, int32> Index;
			TArray<FMCBlockPos> Queue;
			Queue.Add(Start);
			Index.Add(Start, 0);
			Nodes.Add(Start);
			for (int32 qi = 0; qi < Queue.Num() && Nodes.Num() < 1024; ++qi)
			{
				const FMCBlockPos P = Queue[qi];
				bool Conn[4], Up[4];
				WireConnections(W, P, Conn, Up);
				for (int32 i = 0; i < 4; ++i)
				{
					if (!Conn[i]) continue;
					const FMCBlockPos Side = P.Offset(GHoriz[i]);
					for (int32 dz = -1; dz <= 1; ++dz)
					{
						const FMCBlockPos Q = Side.Up(dz);
						if (!IsWire(W.GetState(Q)) || Index.Contains(Q)) continue;
						if (dz == 1 && FMCBlocks::IsOpaque(W.GetState(P.Up()))) continue;
						if (dz == -1 && FMCBlocks::IsOpaque(W.GetState(Side))) continue;
						Index.Add(Q, Nodes.Num());
						Nodes.Add(Q);
						Queue.Add(Q);
					}
				}
			}
			// external power (non-dust sources)
			TArray<int32> Power;
			Power.SetNumZeroed(Nodes.Num());
			GWireSignalsDisabled = true;
			for (int32 i = 0; i < Nodes.Num(); ++i) Power[i] = W.GetBestNeighborPower(Nodes[i]);
			GWireSignalsDisabled = false;
			// relax: power spreads with -1 per step (bucket queue from 15 down)
			TArray<int32> Order;
			Order.Reserve(Nodes.Num());
			for (int32 Level = 15; Level >= 1; --Level)
			{
				for (int32 i = 0; i < Nodes.Num(); ++i)
				{
					if (Power[i] != Level) continue;
					const FMCBlockPos P = Nodes[i];
					bool Conn[4], Up[4];
					WireConnections(W, P, Conn, Up);
					for (int32 d = 0; d < 4; ++d)
					{
						if (!Conn[d]) continue;
						const FMCBlockPos Side = P.Offset(GHoriz[d]);
						for (int32 dz = -1; dz <= 1; ++dz)
						{
							const int32* J = Index.Find(Side.Up(dz));
							if (J && Power[*J] < Level - 1) Power[*J] = Level - 1;
						}
					}
				}
			}
			// apply
			TArray<FMCBlockPos> Changed;
			for (int32 i = 0; i < Nodes.Num(); ++i)
			{
				const FMCState S = W.GetState(Nodes[i]);
				if ((FMCBlocks::MetaOf(S) & 15) == Power[i]) continue;
				W.SetState(Nodes[i], FMCBlocks::GetByState(S).State((uint8)Power[i]), MCSet_Render);
				Changed.Add(Nodes[i]);
			}
			// notify everything around changed dust (lamps, pistons, repeaters, blocks below/beside)
			GWireSignalsDisabled = false;
			TSet<FMCBlockPos> Notified;
			for (const FMCBlockPos& P : Changed)
			{
				for (int32 F = 0; F < 6; ++F)
				{
					const FMCBlockPos Q = P.Offset((EMCFace)F);
					if (!Notified.Contains(Q) && !Index.Contains(Q)) { Notified.Add(Q); W.UpdateNeighbor(Q, P); }
					// blocks powered by the dust relay to their neighbours
					if (FMCBlocks::IsOpaque(W.GetState(Q)))
						for (int32 G = 0; G < 6; ++G)
						{
							const FMCBlockPos R = Q.Offset((EMCFace)G);
							if (!Notified.Contains(R) && !Index.Contains(R)) { Notified.Add(R); W.UpdateNeighbor(R, Q); }
						}
				}
			}
		}
		virtual bool IsPowerSource(FMCState S) const override { return true; }
		virtual int32 GetWeakPower(FMCWorld& W, const FMCBlockPos& P, FMCState S, EMCFace Dir) const override
		{
			if (GWireSignalsDisabled) return 0;
			const int32 Pw = FMCBlocks::MetaOf(S) & 15;
			if (Pw == 0 || Dir == EMCFace::Up) return 0;
			if (Dir == EMCFace::Down) return Pw;
			bool Out[4];
			WireOutputs(W, P, Out);
			for (int32 i = 0; i < 4; ++i) if (GHoriz[i] == Dir) return Out[i] ? Pw : 0;
			return 0;
		}
		virtual int32 GetStrongPower(FMCWorld& W, const FMCBlockPos& P, FMCState S, EMCFace Dir) const override
		{
			return GetWeakPower(W, P, S, Dir);
		}
		virtual bool ConnectsToRedstone(FMCState S, EMCFace F) const override { return true; }
	};

	// ================================================================================================================
	class FRedstoneTorchBeh : public FMCBlockBehavior
	{
	public:
		static bool IsLit(FMCState S) { return !MCMeta::Bit(FMCBlocks::MetaOf(S), 3); }
		static FMCBlockPos Attached(const FMCBlockPos& P, FMCState S)
		{
			const int32 A = FMCBlocks::MetaOf(S) & 7;
			if (A == 0) return P.Down();
			return P.Offset(MC::Opposite((EMCFace)(1 + A)));
		}
		virtual FMCState GetPlacementState(const FMCBlock& B, const FMCPlaceContext& C) const override
		{
			const FMCState Base = MCBehaviors::Get(EMCBeh::Torch)->GetPlacementState(B, C);
			if (!Base || !C.World) return Base;
			const bool bPowered = PoweredAttach(*C.World, C.Pos, Base);
			return B.State(MCMeta::SetBit(FMCBlocks::MetaOf(Base), 3, bPowered));
		}
		static bool PoweredAttach(FMCWorld& W, const FMCBlockPos& P, FMCState S)
		{
			const FMCBlockPos A = Attached(P, S);
			// the attached block is powered by something other than this torch
			for (int32 F = 0; F < 6; ++F)
			{
				const FMCBlockPos N = A.Offset((EMCFace)F);
				if (N == P) continue;
				const FMCState NS = W.GetState(N);
				if (!(FMCBlocks::Info(NS).Flags & MCB_Redstone)) continue;
				const FMCBlockBehavior* Bh = FMCBlocks::GetByState(NS).Behavior;
				if (Bh->GetStrongPower(W, N, NS, MC::Opposite((EMCFace)F)) > 0) return true;
				if (!FMCBlocks::IsOpaque(W.GetState(A))) continue;
				if (Bh->GetWeakPower(W, N, NS, MC::Opposite((EMCFace)F)) > 0 && IsWire(NS)) return true;
			}
			return false;
		}
		virtual bool CanSurvive(const FMCBlock& B, FMCWorld& W, const FMCBlockPos& P, FMCState S) const override
		{
			return MCBehaviors::Get(EMCBeh::Torch)->CanSurvive(B, W, P, S);
		}
		virtual void OnNeighborChanged(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCBlockPos& From) const override
		{
			if (!CanSurvive(FMCBlocks::GetByState(S), W, P, S)) { BreakUnsupported(W, P); return; }
			const bool bShouldBeLit = !PoweredAttach(W, P, S);
			if (bShouldBeLit != IsLit(S) && !W.HasScheduledTick(P, FMCBlocks::BlockOf(S))) W.ScheduleTick(P, FMCBlocks::BlockOf(S), 2);
		}
		virtual void OnPlaced(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCPlayer* Player) const override
		{
			for (int32 F = 0; F < 6; ++F) W.NotifyNeighbors(P.Offset((EMCFace)F));
		}
		virtual void OnRemoved(FMCWorld& W, const FMCBlockPos& P, FMCState Old, FMCState New) const override
		{
			for (int32 F = 0; F < 6; ++F) W.NotifyNeighbors(P.Offset((EMCFace)F));
		}
		virtual void OnScheduledTick(FMCWorld& W, const FMCBlockPos& P, FMCState S) const override
		{
			const bool bShouldBeLit = !PoweredAttach(W, P, S);
			if (bShouldBeLit == IsLit(S)) return;
			// burnout protection (8 toggles within 60 ticks)
			TArray<int64>& Hist = Toggles.FindOrAdd(P);
			Hist.RemoveAll([&](int64 T) { return W.GameTick - T > 60; });
			if (bShouldBeLit && Hist.Num() >= 8)
			{
				W.PlaySound(TEXT("redstone_torch_burnout"), P.Center() * MC::InvBlockSize, 0.5f, 2.6f);
				W.SpawnParticles(TEXT("smoke"), P.Center() * MC::InvBlockSize, 5, 0.1f);
				W.ScheduleTick(P, FMCBlocks::BlockOf(S), 160);
				return;
			}
			Hist.Add(W.GameTick);
			W.SetState(P, WithMeta(S, MCMeta::SetBit(FMCBlocks::MetaOf(S), 3, !bShouldBeLit)));
			for (int32 F = 0; F < 6; ++F) W.NotifyNeighbors(P.Offset((EMCFace)F));
		}
		virtual bool IsPowerSource(FMCState S) const override { return true; }
		virtual int32 GetWeakPower(FMCWorld& W, const FMCBlockPos& P, FMCState S, EMCFace Dir) const override
		{
			if (!IsLit(S)) return 0;
			const FMCBlockPos A = Attached(P, S);
			if (P.Offset(Dir) == A) return 0;
			return 15;
		}
		virtual int32 GetStrongPower(FMCWorld& W, const FMCBlockPos& P, FMCState S, EMCFace Dir) const override
		{
			return (IsLit(S) && Dir == EMCFace::Up) ? 15 : 0;
		}
		virtual uint8 GetLightEmission(FMCState S, uint8 Default) const override { return IsLit(S) ? Default : 0; }
		virtual bool ConnectsToRedstone(FMCState S, EMCFace F) const override { return true; }
		static TMap<FMCBlockPos, TArray<int64>> Toggles;
	};
	TMap<FMCBlockPos, TArray<int64>> FRedstoneTorchBeh::Toggles;

	// ================================================================================================================
	/** Shared diode logic (repeater & comparator). Meta facing = output direction. */
	class FDiodeBase : public FMCBlockBehavior
	{
	public:
		virtual FMCState GetPlacementState(const FMCBlock& B, const FMCPlaceContext& C) const override
		{
			if (!C.World || !MCBehaviorUtil::HasSolidTop(*C.World, C.Pos.Down())) return 0;
			return B.State(MCMeta::FromFacing4(LookFacing(C)));
		}
		virtual bool CanSurvive(const FMCBlock& B, FMCWorld& W, const FMCBlockPos& P, FMCState S) const override { return MCBehaviorUtil::HasSolidTop(W, P.Down()); }
		static int32 InputFrom(FMCWorld& W, const FMCBlockPos& P, EMCFace Dir)
		{
			const FMCBlockPos N = P.Offset(Dir);
			const FMCState NS = W.GetState(N);
			int32 Pw = W.GetInputPower(P, Dir);
			if (IsWire(NS)) Pw = FMath::Max(Pw, (int32)(FMCBlocks::MetaOf(NS) & 15));
			return Pw;
		}
		static int32 SideInput(FMCWorld& W, const FMCBlockPos& P, EMCFace Dir)
		{
			const FMCBlockPos N = P.Offset(Dir);
			const FMCState NS = W.GetState(N);
			const FMCBlock& NB = FMCBlocks::GetByState(NS);
			if (IsWire(NS)) return FMCBlocks::MetaOf(NS) & 15;
			if (NB.Name == TEXT("redstone_block")) return 15;
			if (NB.Model == EMCModel::Repeater || NB.Model == EMCModel::Comparator)
				return NB.Behavior->GetWeakPower(W, N, NS, MC::Opposite(Dir));
			return 0;
		}
		static void NotifyFront(FMCWorld& W, const FMCBlockPos& P, FMCState S)
		{
			const EMCFace F = MCMeta::Facing4(FMCBlocks::MetaOf(S));
			const FMCBlockPos Front = P.Offset(F);
			W.UpdateNeighbor(Front, P);
			W.NotifyNeighbors(Front);
		}
	};

	class FRepeaterBeh : public FDiodeBase
	{
	public:
		static bool Powered(FMCState S) { return MCMeta::Bit(FMCBlocks::MetaOf(S), 4); }
		static bool Locked(FMCState S) { return MCMeta::Bit(FMCBlocks::MetaOf(S), 5); }
		static int32 Delay(FMCState S) { return (((FMCBlocks::MetaOf(S) >> 2) & 3) + 1) * 2; }
		bool ComputeLocked(FMCWorld& W, const FMCBlockPos& P, FMCState S) const
		{
			const EMCFace F = MCMeta::Facing4(FMCBlocks::MetaOf(S));
			for (EMCFace Side : { MC::RotateY(F, 1), MC::RotateY(F, 3) })
			{
				const FMCBlockPos N = P.Offset(Side);
				const FMCState NS = W.GetState(N);
				const FMCBlock& NB = FMCBlocks::GetByState(NS);
				if ((NB.Model == EMCModel::Repeater || NB.Model == EMCModel::Comparator) && MCMeta::Facing4(FMCBlocks::MetaOf(NS)) == MC::Opposite(Side)
					&& NB.Behavior->GetWeakPower(W, N, NS, MC::Opposite(Side)) > 0) return true;
			}
			return false;
		}
		virtual bool OnUse(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCPlayer* Player, EMCFace Face, const FVector& Hit) const override
		{
			const uint8 M = FMCBlocks::MetaOf(S);
			const uint8 D = (((M >> 2) & 3) + 1) & 3;
			W.SetState(P, WithMeta(S, (M & ~0x0C) | (D << 2)), MCSet_Render);
			W.PlaySound(TEXT("repeater_click"), P.Center() * MC::InvBlockSize, 0.3f, 1.f);
			return true;
		}
		virtual void OnNeighborChanged(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCBlockPos& From) const override
		{
			if (!CanSurvive(FMCBlocks::GetByState(S), W, P, S)) { BreakUnsupported(W, P); return; }
			const bool bLocked = ComputeLocked(W, P, S);
			if (bLocked != Locked(S)) { W.SetState(P, WithMeta(S, MCMeta::SetBit(FMCBlocks::MetaOf(S), 5, bLocked)), MCSet_Render); S = W.GetState(P); }
			if (bLocked) return;
			const EMCFace F = MCMeta::Facing4(FMCBlocks::MetaOf(S));
			const bool bInput = InputFrom(W, P, MC::Opposite(F)) > 0;
			if (bInput != Powered(S) && !W.HasScheduledTick(P, FMCBlocks::BlockOf(S))) W.ScheduleTick(P, FMCBlocks::BlockOf(S), Delay(S), bInput ? -1 : 0);
		}
		virtual void OnScheduledTick(FMCWorld& W, const FMCBlockPos& P, FMCState S) const override
		{
			if (Locked(S)) return;
			const EMCFace F = MCMeta::Facing4(FMCBlocks::MetaOf(S));
			const bool bInput = InputFrom(W, P, MC::Opposite(F)) > 0;
			if (Powered(S) && !bInput) W.SetState(P, WithMeta(S, MCMeta::SetBit(FMCBlocks::MetaOf(S), 4, false)), MCSet_Render);
			else if (!Powered(S))
			{
				W.SetState(P, WithMeta(S, MCMeta::SetBit(FMCBlocks::MetaOf(S), 4, true)), MCSet_Render);
				if (!bInput) W.ScheduleTick(P, FMCBlocks::BlockOf(S), Delay(S));
			}
			NotifyFront(W, P, W.GetState(P));
		}
		virtual void OnPlaced(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCPlayer* Player) const override { OnNeighborChanged(W, P, S, P); }
		virtual void OnRemoved(FMCWorld& W, const FMCBlockPos& P, FMCState Old, FMCState New) const override { NotifyFront(W, P, Old); }
		virtual bool IsPowerSource(FMCState S) const override { return true; }
		virtual int32 GetWeakPower(FMCWorld& W, const FMCBlockPos& P, FMCState S, EMCFace Dir) const override
		{
			return (Powered(S) && Dir == MCMeta::Facing4(FMCBlocks::MetaOf(S))) ? 15 : 0;
		}
		virtual int32 GetStrongPower(FMCWorld& W, const FMCBlockPos& P, FMCState S, EMCFace Dir) const override { return GetWeakPower(W, P, S, Dir); }
		virtual bool ConnectsToRedstone(FMCState S, EMCFace F) const override { const EMCFace Fc = MCMeta::Facing4(FMCBlocks::MetaOf(S)); return F == Fc || F == MC::Opposite(Fc); }
	};

	class FComparatorBeh : public FDiodeBase
	{
	public:
		static int32 RearInput(FMCWorld& W, const FMCBlockPos& P, FMCState S)
		{
			const EMCFace F = MCMeta::Facing4(FMCBlocks::MetaOf(S));
			const EMCFace Back = MC::Opposite(F);
			int32 Pw = InputFrom(W, P, Back);
			FMCBlockPos N = P.Offset(Back);
			FMCState NS = W.GetState(N);
			const FMCBlock* NB = &FMCBlocks::GetByState(NS);
			// read container-like blocks directly or through one solid block
			auto Reads = [](const FMCBlock& B) { return B.Has(MCB_Container) || B.Name == TEXT("composter") || B.Name == TEXT("cake") || B.Name == TEXT("end_portal_frame")
				|| B.Model == EMCModel::Cauldron || B.Name == TEXT("jukebox") || B.Name == TEXT("lectern") || B.Name == TEXT("respawn_anchor") || B.Name == TEXT("chiseled_bookshelf")
				|| B.Name == TEXT("beehive") || B.Name == TEXT("bee_nest") || B.Name == TEXT("crafter"); };
			if (Reads(*NB)) Pw = NB->Behavior->GetComparatorOutput(W, N, NS);
			else if (FMCBlocks::IsOpaque(NS) && Pw < 15)
			{
				const FMCBlockPos N2 = N.Offset(Back);
				const FMCState S2 = W.GetState(N2);
				const FMCBlock& B2 = FMCBlocks::GetByState(S2);
				if (Reads(B2)) Pw = FMath::Max(Pw, B2.Behavior->GetComparatorOutput(W, N2, S2));
			}
			return Pw;
		}
		static int32 Compute(FMCWorld& W, const FMCBlockPos& P, FMCState S)
		{
			const EMCFace F = MCMeta::Facing4(FMCBlocks::MetaOf(S));
			const int32 Rear = RearInput(W, P, S);
			const int32 Side = FMath::Max(SideInput(W, P, MC::RotateY(F, 1)), SideInput(W, P, MC::RotateY(F, 3)));
			const bool bSubtract = MCMeta::Bit(FMCBlocks::MetaOf(S), 2);
			if (bSubtract) return FMath::Max(Rear - Side, 0);
			return Rear >= Side ? Rear : 0;
		}
		static int32 Output(FMCWorld& W, const FMCBlockPos& P)
		{
			if (FMCComparatorEntity* BE = static_cast<FMCComparatorEntity*>(W.GetBlockEntity(P))) if (BE->Type == EMCBlockEntityType::Comparator) return BE->Output;
			return 0;
		}
		virtual TSharedPtr<FMCBlockEntity> CreateBlockEntity(const FMCBlockPos& P, FMCState S) const override { return MakeShared<FMCComparatorEntity>(); }
		virtual bool OnUse(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCPlayer* Player, EMCFace Face, const FVector& Hit) const override
		{
			const bool bSub = !MCMeta::Bit(FMCBlocks::MetaOf(S), 2);
			W.SetState(P, WithMeta(S, MCMeta::SetBit(FMCBlocks::MetaOf(S), 2, bSub)), MCSet_Render | MCSet_KeepEntity);
			W.PlaySound(TEXT("comparator_click"), P.Center() * MC::InvBlockSize, 0.3f, bSub ? 0.55f : 0.5f);
			Refresh(W, P, W.GetState(P));
			return true;
		}
		void Refresh(FMCWorld& W, const FMCBlockPos& P, FMCState S) const
		{
			const int32 New = Compute(W, P, S);
			FMCComparatorEntity* BE = static_cast<FMCComparatorEntity*>(W.GetBlockEntity(P));
			const int32 Old = BE ? BE->Output : 0;
			if (BE) BE->Output = New;
			const bool bPowered = New > 0;
			if (bPowered != MCMeta::Bit(FMCBlocks::MetaOf(S), 3)) W.SetState(P, WithMeta(S, MCMeta::SetBit(FMCBlocks::MetaOf(S), 3, bPowered)), MCSet_Render | MCSet_KeepEntity);
			if (New != Old) NotifyFront(W, P, W.GetState(P));
		}
		virtual void OnNeighborChanged(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCBlockPos& From) const override
		{
			if (!CanSurvive(FMCBlocks::GetByState(S), W, P, S)) { BreakUnsupported(W, P); return; }
			if (Compute(W, P, S) != Output(W, P) && !W.HasScheduledTick(P, FMCBlocks::BlockOf(S))) W.ScheduleTick(P, FMCBlocks::BlockOf(S), 2);
		}
		virtual void OnScheduledTick(FMCWorld& W, const FMCBlockPos& P, FMCState S) const override { Refresh(W, P, S); }
		virtual void OnPlaced(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCPlayer* Player) const override { Refresh(W, P, S); }
		virtual void OnRemoved(FMCWorld& W, const FMCBlockPos& P, FMCState Old, FMCState New) const override { NotifyFront(W, P, Old); }
		virtual bool IsPowerSource(FMCState S) const override { return true; }
		virtual int32 GetWeakPower(FMCWorld& W, const FMCBlockPos& P, FMCState S, EMCFace Dir) const override
		{
			return Dir == MCMeta::Facing4(FMCBlocks::MetaOf(S)) ? Output(W, P) : 0;
		}
		virtual int32 GetStrongPower(FMCWorld& W, const FMCBlockPos& P, FMCState S, EMCFace Dir) const override { return GetWeakPower(W, P, S, Dir); }
		virtual bool ConnectsToRedstone(FMCState S, EMCFace F) const override { return true; }
	};

	// ================================================================================================================
	class FRedstoneLampBeh : public FMCBlockBehavior
	{
	public:
		virtual FMCState GetPlacementState(const FMCBlock& B, const FMCPlaceContext& C) const override
		{
			return B.State(C.World && C.World->IsPowered(C.Pos) ? 1 : 0);
		}
		virtual void OnNeighborChanged(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCBlockPos& From) const override
		{
			const bool bLit = MCMeta::Bit(FMCBlocks::MetaOf(S), 0);
			const bool bPowered = W.IsPowered(P);
			if (bPowered && !bLit) W.SetState(P, WithMeta(S, 1), MCSet_Render | MCSet_Light);
			else if (!bPowered && bLit && !W.HasScheduledTick(P, FMCBlocks::BlockOf(S))) W.ScheduleTick(P, FMCBlocks::BlockOf(S), 4);
		}
		virtual void OnScheduledTick(FMCWorld& W, const FMCBlockPos& P, FMCState S) const override
		{
			if (MCMeta::Bit(FMCBlocks::MetaOf(S), 0) && !W.IsPowered(P)) W.SetState(P, WithMeta(S, 0), MCSet_Render | MCSet_Light);
		}
		virtual uint8 GetLightEmission(FMCState S, uint8 Default) const override { return MCMeta::Bit(FMCBlocks::MetaOf(S), 0) ? 15 : 0; }
	};

	class FRedstoneBlockBeh : public FMCBlockBehavior
	{
	public:
		virtual bool IsPowerSource(FMCState S) const override { return true; }
		virtual int32 GetWeakPower(FMCWorld& W, const FMCBlockPos& P, FMCState S, EMCFace Dir) const override { return 15; }
		virtual void OnPlaced(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCPlayer* Player) const override { W.NotifyNeighbors(P); }
		virtual void OnRemoved(FMCWorld& W, const FMCBlockPos& P, FMCState Old, FMCState New) const override { W.NotifyNeighbors(P); }
		virtual bool ConnectsToRedstone(FMCState S, EMCFace F) const override { return true; }
	};

	// ================================================================================================================
	/** Pistons: push up to 12 blocks, sticky pistons pull one back. Instant movement (no moving block animation). */
	class FPistonBeh : public FMCBlockBehavior
	{
	public:
		static bool IsSticky(FMCState S) { return FMCBlocks::GetByState(S).Name == TEXT("sticky_piston"); }
		static bool Extended(FMCState S) { return MCMeta::Bit(FMCBlocks::MetaOf(S), 3); }
		virtual FMCState GetPlacementState(const FMCBlock& B, const FMCPlaceContext& C) const override
		{
			return B.State((uint8)MC::Opposite(LookFacing6(C)));
		}
		bool HasPower(FMCWorld& W, const FMCBlockPos& P, EMCFace Facing) const
		{
			for (int32 F = 0; F < 6; ++F)
			{
				if ((EMCFace)F == Facing) continue;
				if (W.GetInputPower(P, (EMCFace)F) > 0) return true;
			}
			// quasi-connectivity: the space above powered
			const FMCBlockPos Up = P.Up();
			for (int32 F = 0; F < 6; ++F) if ((EMCFace)F != EMCFace::Down && W.GetInputPower(Up, (EMCFace)F) > 0) return true;
			return false;
		}
		static int32 PushReaction(const FMCWorld& W, const FMCBlockPos& P, FMCState S, EMCFace MoveDir, const FMCBlockPos& PistonPos)
		{
			// 0 = normal, 1 = destroy, 2 = block (immovable), 3 = push only (no pull)
			if (S == 0) return 1;
			const FMCBlock& B = FMCBlocks::GetByState(S);
			const FMCStateInfo& I = FMCBlocks::Info(S);
			if (B.Has(MCB_Unbreakable) || B.Hardness < 0) return 2;
			if (B.Name == TEXT("obsidian") || B.Name == TEXT("crying_obsidian") || B.Name == TEXT("respawn_anchor") || B.Name == TEXT("reinforced_deepslate")
				|| B.Name == TEXT("enchanting_table") || B.Name == TEXT("beacon") || B.Name == TEXT("ender_chest") || B.Name == TEXT("spawner") || B.Name == TEXT("netherite_block")) return 2;
			if (B.Model == EMCModel::Piston && Extended(S)) return 2;
			if (B.Model == EMCModel::PistonHead) return 2;
			if (B.Has(MCB_BlockEntity)) return 2;
			if (I.Flags & MCB_Fluid) return 1;
			if (B.Has(MCB_Replaceable) || B.Has(MCB_Plant) || B.Model == EMCModel::Torch || B.Model == EMCModel::Button || B.Model == EMCModel::Lever || B.Model == EMCModel::Carpet
				|| B.Model == EMCModel::RedstoneWire || B.Model == EMCModel::Repeater || B.Model == EMCModel::Comparator || B.Model == EMCModel::Door || B.Model == EMCModel::Ladder
				|| B.Model == EMCModel::Lantern || B.Model == EMCModel::Vine || B.Model == EMCModel::Cake || B.Model == EMCModel::Bed || B.Model == EMCModel::FlowerPot
				|| B.Model == EMCModel::Candle || B.Model == EMCModel::Skull || B.Model == EMCModel::DragonEgg || B.Model == EMCModel::Scaffolding || B.Name == TEXT("cobweb")
				|| B.Model == EMCModel::PressurePlate || B.Model == EMCModel::Fire || B.Model == EMCModel::Portal || B.Model == EMCModel::EndPortal) return 1;
			if (B.Name == TEXT("glazed_terracotta") || B.Variant == TEXT("glazed_terracotta")) return 3;
			return 0;
		}
		static bool IsStickyBlock(FMCState S)
		{
			const FName N = FMCBlocks::GetByState(S).Name;
			return N == TEXT("slime_block") || N == TEXT("honey_block");
		}
		/** Collect the blocks that move (slime/honey drag attached blocks). Returns false if blocked. */
		bool Resolve(FMCWorld& W, const FMCBlockPos& PistonPos, const FMCBlockPos& Start, EMCFace MoveDir, TArray<FMCBlockPos>& Move, TArray<FMCBlockPos>& Destroy) const
		{
			TArray<FMCBlockPos> Stack;
			TSet<FMCBlockPos> Seen;
			Stack.Add(Start);
			while (Stack.Num())
			{
				const FMCBlockPos Head = Stack.Pop();
				// walk the line in the moving direction
				FMCBlockPos P = Head;
				while (true)
				{
					if (Seen.Contains(P)) break;
					if (P == PistonPos) return false;
					const FMCState S = W.GetState(P);
					const int32 R = PushReaction(W, P, S, MoveDir, PistonPos);
					if (S == 0) break;
					if (R == 2) return false;
					if (R == 1) { Destroy.AddUnique(P); break; }
					Seen.Add(P);
					Move.Add(P);
					if (Move.Num() > 12) return false;
					if (IsStickyBlock(S))
					{
						for (int32 F = 0; F < 6; ++F)
						{
							const EMCFace D = (EMCFace)F;
							if (D == MoveDir || D == MC::Opposite(MoveDir)) continue;
							const FMCBlockPos N = P.Offset(D);
							const FMCState NS = W.GetState(N);
							if (NS == 0 || Seen.Contains(N) || N == PistonPos) continue;
							if (PushReaction(W, N, NS, MoveDir, PistonPos) != 0) continue;
							if (IsStickyBlock(NS) && FMCBlocks::GetByState(NS).Name != FMCBlocks::GetByState(S).Name) continue;
							Stack.Add(N);
						}
						// block behind a slime block is dragged along too
						const FMCBlockPos Behind = P.Offset(MC::Opposite(MoveDir));
						const FMCState BS = W.GetState(Behind);
						if (BS != 0 && Behind != PistonPos && !Seen.Contains(Behind) && PushReaction(W, Behind, BS, MoveDir, PistonPos) == 0) Stack.Add(Behind);
					}
					P = P.Offset(MoveDir);
				}
			}
			return true;
		}
		void MoveBlocks(FMCWorld& W, const TArray<FMCBlockPos>& Move, const TArray<FMCBlockPos>& Destroy, EMCFace Dir) const
		{
			for (const FMCBlockPos& D : Destroy) W.DestroyBlock(D, true);
			TArray<TPair<FMCBlockPos, FMCState>> Moved;
			for (const FMCBlockPos& P : Move) Moved.Add(TPair<FMCBlockPos, FMCState>(P, W.GetState(P)));
			for (const FMCBlockPos& P : Move) W.SetState(P, 0, MCSet_Render | MCSet_Light);
			for (const auto& M : Moved) W.SetState(M.Key.Offset(Dir), M.Value, MCSet_Render | MCSet_Light | MCSet_NoSupport);
			// push entities standing in the way
			for (const auto& M : Moved)
			{
				const FMCBlockPos Dst = M.Key.Offset(Dir);
				TArray<AMCEntity*> Ents;
				W.GetEntitiesInBox(FMCBox(Dst.X, Dst.Y, Dst.Z, Dst.X + 1, Dst.Y + 1, Dst.Z + 1.01), Ents);
				const FIntVector& D = MC::FaceDir[(int32)Dir];
				for (AMCEntity* E : Ents)
				{
					if (!E) continue;
					E->SetPosition(E->Pos + FVector(D.X, D.Y, D.Z) * 1.01, false);
					if (IsStickyBlock(M.Value) && Dir == EMCFace::Up) E->Vel.Z = FMath::Max(E->Vel.Z, 1.0);
				}
			}
			for (const auto& M : Moved) { W.NotifyNeighbors(M.Key); W.NotifyNeighbors(M.Key.Offset(Dir)); }
		}
		void Check(FMCWorld& W, const FMCBlockPos& P, FMCState S) const
		{
			const EMCFace F = MCMeta::Facing6(FMCBlocks::MetaOf(S));
			const bool bPowered = HasPower(W, P, F);
			if (bPowered && !Extended(S))
			{
				TArray<FMCBlockPos> Move, Destroy;
				if (!Resolve(W, P, P.Offset(F), F, Move, Destroy)) return;
				// order from the far end so blocks do not overwrite each other
				const FIntVector& D = MC::FaceDir[(int32)F];
				Move.Sort([&](const FMCBlockPos& A, const FMCBlockPos& B) { return (A.X * D.X + A.Y * D.Y + A.Z * D.Z) > (B.X * D.X + B.Y * D.Y + B.Z * D.Z); });
				MoveBlocks(W, Move, Destroy, F);
				W.SetState(P, WithMeta(S, MCMeta::SetBit(FMCBlocks::MetaOf(S), 3, true)), MCSet_Render | MCSet_Light);
				const uint8 HeadMeta = (uint8)F | (IsSticky(S) ? 8 : 0);
				W.SetState(P.Offset(F), StateOf(TEXT("piston_head"), HeadMeta), MCSet_Render | MCSet_Light | MCSet_NoSupport);
				W.PlaySound(TEXT("piston_extend"), P.Center() * MC::InvBlockSize, 0.5f, W.Rand.FRange(0.6f, 0.85f));
				W.NotifyNeighbors(P.Offset(F));
			}
			else if (!bPowered && Extended(S))
			{
				const FMCBlockPos HeadPos = P.Offset(F);
				if (FMCBlocks::GetByState(W.GetState(HeadPos)).Model == EMCModel::PistonHead) W.SetState(HeadPos, 0, MCSet_Render | MCSet_Light);
				W.SetState(P, WithMeta(S, MCMeta::SetBit(FMCBlocks::MetaOf(S), 3, false)), MCSet_Render | MCSet_Light);
				if (IsSticky(S))
				{
					const FMCBlockPos Pull = HeadPos.Offset(F);
					const FMCState PS = W.GetState(Pull);
					const int32 R = PushReaction(W, Pull, PS, MC::Opposite(F), P);
					if (PS != 0 && R == 0)
					{
						TArray<FMCBlockPos> Move, Destroy;
						if (Resolve(W, P, Pull, MC::Opposite(F), Move, Destroy))
						{
							const FIntVector& D = MC::FaceDir[(int32)MC::Opposite(F)];
							Move.Sort([&](const FMCBlockPos& A, const FMCBlockPos& B) { return (A.X * D.X + A.Y * D.Y + A.Z * D.Z) > (B.X * D.X + B.Y * D.Y + B.Z * D.Z); });
							MoveBlocks(W, Move, Destroy, MC::Opposite(F));
						}
					}
				}
				W.PlaySound(TEXT("piston_contract"), P.Center() * MC::InvBlockSize, 0.5f, W.Rand.FRange(0.6f, 0.75f));
				W.NotifyNeighbors(HeadPos);
			}
		}
		virtual void OnPlaced(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCPlayer* Player) const override { Check(W, P, S); }
		virtual void OnNeighborChanged(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCBlockPos& From) const override
		{
			if (!W.HasScheduledTick(P, FMCBlocks::BlockOf(S))) W.ScheduleTick(P, FMCBlocks::BlockOf(S), 1);
		}
		virtual void OnScheduledTick(FMCWorld& W, const FMCBlockPos& P, FMCState S) const override { Check(W, P, S); }
		virtual void OnRemoved(FMCWorld& W, const FMCBlockPos& P, FMCState Old, FMCState New) const override
		{
			if (Extended(Old) && FMCBlocks::BlockOf(New) != FMCBlocks::BlockOf(Old))
			{
				const FMCBlockPos HeadPos = P.Offset(MCMeta::Facing6(FMCBlocks::MetaOf(Old)));
				if (FMCBlocks::GetByState(W.GetState(HeadPos)).Model == EMCModel::PistonHead) W.SetState(HeadPos, 0);
			}
		}
	};

	class FPistonHeadBeh : public FMCBlockBehavior
	{
	public:
		virtual void OnNeighborChanged(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCBlockPos& From) const override
		{
			const EMCFace F = MCMeta::Facing6(FMCBlocks::MetaOf(S));
			const FMCState Base = W.GetState(P.Offset(MC::Opposite(F)));
			if (FMCBlocks::GetByState(Base).Model != EMCModel::Piston || !MCMeta::Bit(FMCBlocks::MetaOf(Base), 3)) W.SetState(P, 0);
		}
		virtual void OnRemoved(FMCWorld& W, const FMCBlockPos& P, FMCState Old, FMCState New) const override
		{
			const EMCFace F = MCMeta::Facing6(FMCBlocks::MetaOf(Old));
			const FMCBlockPos BasePos = P.Offset(MC::Opposite(F));
			const FMCState Base = W.GetState(BasePos);
			if (FMCBlocks::GetByState(Base).Model == EMCModel::Piston && MCMeta::Bit(FMCBlocks::MetaOf(Base), 3) && New == 0)
				W.DestroyBlock(BasePos, true);
		}
	};

	// ================================================================================================================
	class FObserverBeh : public FMCBlockBehavior
	{
	public:
		virtual FMCState GetPlacementState(const FMCBlock& B, const FMCPlaceContext& C) const override
		{
			return B.State((uint8)LookFacing6(C));
		}
		virtual void OnNeighborChanged(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCBlockPos& From) const override
		{
			const EMCFace F = MCMeta::Facing6(FMCBlocks::MetaOf(S));
			if (From == P.Offset(F) && !MCMeta::Bit(FMCBlocks::MetaOf(S), 3) && !W.HasScheduledTick(P, FMCBlocks::BlockOf(S)))
				W.ScheduleTick(P, FMCBlocks::BlockOf(S), 2);
		}
		virtual void OnScheduledTick(FMCWorld& W, const FMCBlockPos& P, FMCState S) const override
		{
			const bool bActive = MCMeta::Bit(FMCBlocks::MetaOf(S), 3);
			W.SetState(P, WithMeta(S, MCMeta::SetBit(FMCBlocks::MetaOf(S), 3, !bActive)), MCSet_Render);
			if (!bActive) W.ScheduleTick(P, FMCBlocks::BlockOf(S), 2);
			const EMCFace F = MCMeta::Facing6(FMCBlocks::MetaOf(S));
			const FMCBlockPos Back = P.Offset(MC::Opposite(F));
			W.UpdateNeighbor(Back, P);
			W.NotifyNeighbors(Back);
		}
		virtual bool IsPowerSource(FMCState S) const override { return true; }
		virtual int32 GetWeakPower(FMCWorld& W, const FMCBlockPos& P, FMCState S, EMCFace Dir) const override
		{
			return (MCMeta::Bit(FMCBlocks::MetaOf(S), 3) && Dir == MC::Opposite(MCMeta::Facing6(FMCBlocks::MetaOf(S)))) ? 15 : 0;
		}
		virtual int32 GetStrongPower(FMCWorld& W, const FMCBlockPos& P, FMCState S, EMCFace Dir) const override { return GetWeakPower(W, P, S, Dir); }
		virtual bool ConnectsToRedstone(FMCState S, EMCFace F) const override { return true; }
	};

	// ================================================================================================================
	class FDispenserBeh : public FMCBlockBehavior
	{
	public:
		bool bDropper = false;
		virtual FMCState GetPlacementState(const FMCBlock& B, const FMCPlaceContext& C) const override
		{
			return B.State((uint8)MC::Opposite(LookFacing6(C)));
		}
		virtual TSharedPtr<FMCBlockEntity> CreateBlockEntity(const FMCBlockPos& P, FMCState S) const override
		{
			return MakeShared<FMCContainerEntity>(bDropper ? EMCBlockEntityType::Dropper : EMCBlockEntityType::Dispenser, 9);
		}
		virtual bool OnUse(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCPlayer* Player, EMCFace Face, const FVector& Hit) const override
		{
			return Player && Player->OpenBlockContainer(P);
		}
		virtual void OnNeighborChanged(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCBlockPos& From) const override
		{
			const bool bPowered = W.IsPowered(P) || W.IsPowered(P.Up());
			const bool bTriggered = MCMeta::Bit(FMCBlocks::MetaOf(S), 3);
			if (bPowered && !bTriggered)
			{
				W.ScheduleTick(P, FMCBlocks::BlockOf(S), 4);
				W.SetState(P, WithMeta(S, MCMeta::SetBit(FMCBlocks::MetaOf(S), 3, true)), MCSet_KeepEntity);
			}
			else if (!bPowered && bTriggered) W.SetState(P, WithMeta(S, MCMeta::SetBit(FMCBlocks::MetaOf(S), 3, false)), MCSet_KeepEntity);
		}
		virtual void OnScheduledTick(FMCWorld& W, const FMCBlockPos& P, FMCState S) const override { Dispense(W, P, S); }
		virtual int32 GetComparatorOutput(FMCWorld& W, const FMCBlockPos& P, FMCState S) const override
		{
			FMCBlockEntity* BE = W.GetBlockEntity(P);
			return BE && BE->GetContainer() ? BE->GetContainer()->ComparatorSignal() : 0;
		}
		void Dispense(FMCWorld& W, const FMCBlockPos& P, FMCState S) const
		{
			FMCBlockEntity* BE = W.GetBlockEntity(P);
			FMCContainer* Inv = BE ? BE->GetContainer() : nullptr;
			if (!Inv) return;
			TArray<int32> Filled;
			for (int32 i = 0; i < Inv->Num(); ++i) if (!(*Inv)[i].IsEmpty()) Filled.Add(i);
			if (Filled.Num() == 0) { W.PlaySound(TEXT("dispenser_fail"), P.Center() * MC::InvBlockSize, 1.f, 1.2f); return; }
			const int32 Slot = Filled[W.Rand.NextInt(Filled.Num())];
			FMCItemStack& Stack = (*Inv)[Slot];
			const EMCFace F = MCMeta::Facing6(FMCBlocks::MetaOf(S));
			const FIntVector& D = MC::FaceDir[(int32)F];
			const FMCBlockPos Front = P.Offset(F);
			const FVector Out = P.Center() * MC::InvBlockSize + FVector(D.X, D.Y, D.Z) * 0.7;
			if (bDropper)
			{
				// insert into a container in front, else drop
				if (FMCBlockEntity* Target = W.GetBlockEntity(Front))
				{
					if (FMCContainer* TI = Target->GetContainer())
					{
						FMCItemStack One = Stack.Copy(); One.Count = 1;
						if (TI->Insert(One) == 0) { Stack.Count -= 1; if (Stack.Count <= 0) Stack.Clear(); W.MarkModified(P); return; }
					}
				}
				DropItem(W, Stack, Out, F);
				return;
			}
			const FMCItem& I = Stack.Item();
			const FName N = I.Name;
			auto Consume = [&]() { Stack.Count -= 1; if (Stack.Count <= 0) Stack.Clear(); W.MarkModified(P); };
			if (I.Kind == EMCItemKind::Projectile || N == TEXT("arrow") || N == TEXT("spectral_arrow") || N == TEXT("tipped_arrow") || N == TEXT("snowball") || N == TEXT("egg")
				|| N == TEXT("fire_charge") || N == TEXT("splash_potion") || N == TEXT("lingering_potion") || N == TEXT("experience_bottle") || N == TEXT("wind_charge"))
			{
				EMCProjectile T = EMCProjectile::Arrow;
				float Speed = 1.1f, Inacc = 6.f;
				if (N == TEXT("snowball")) T = EMCProjectile::Snowball;
				else if (N == TEXT("egg")) T = EMCProjectile::Egg;
				else if (N == TEXT("fire_charge")) { T = EMCProjectile::SmallFireball; Speed = 0.5f; }
				else if (N == TEXT("splash_potion")) { T = EMCProjectile::Potion; Speed = 0.5f; }
				else if (N == TEXT("lingering_potion")) { T = EMCProjectile::LingeringPotion; Speed = 0.5f; }
				else if (N == TEXT("experience_bottle")) { T = EMCProjectile::ExpBottle; Speed = 0.5f; }
				else if (N == TEXT("spectral_arrow")) T = EMCProjectile::SpectralArrow;
				else if (N == TEXT("wind_charge")) T = EMCProjectile::WindCharge;
				if (W.Game)
				{
					AMCProjectile* Pr = W.Game->SpawnEntity<AMCProjectile>(&W, Out - FVector(0, 0, 0.1));
					if (Pr)
					{
						Pr->Type = T;
						Pr->Item = Stack.Copy(); Pr->Item.Count = 1;
						Pr->bPickup = T == EMCProjectile::Arrow;
						Pr->InitEntity();
						Pr->Shoot(FVector(D.X, D.Y, D.Z + (F == EMCFace::Up || F == EMCFace::Down ? 0 : 0.1)), Speed, Inacc);
					}
				}
				W.PlaySound(TEXT("dispenser_launch"), Out);
				Consume();
				return;
			}
			if (N == TEXT("water_bucket") || N == TEXT("lava_bucket"))
			{
				const FMCState Cur = W.GetState(Front);
				if (Cur == 0 || FMCBlocks::IsReplaceable(Cur))
				{
					W.SetState(Front, N == TEXT("water_bucket") ? FMCBlocks::C.Water : FMCBlocks::C.Lava);
					Stack = FMCItemStack::Of(TEXT("bucket"), 1);
					W.MarkModified(P);
					return;
				}
			}
			if (N == TEXT("bucket"))
			{
				const FMCState Cur = W.GetState(Front);
				if ((Cur == FMCBlocks::C.Water || Cur == FMCBlocks::C.Lava))
				{
					W.SetState(Front, 0);
					const FMCItemStack Filled2 = FMCItemStack::Of(Cur == FMCBlocks::C.Water ? TEXT("water_bucket") : TEXT("lava_bucket"), 1);
					if (Stack.Count == 1) Stack = Filled2; else { Consume(); FMCItemStack Tmp = Filled2; if (Inv->Insert(Tmp) > 0) DropItem(W, Tmp, Out, F); }
					W.MarkModified(P);
					return;
				}
			}
			if (N == TEXT("tnt"))
			{
				if (W.Game) if (AMCPrimedTNT* T = W.Game->SpawnEntity<AMCPrimedTNT>(&W, FVector(Front.X + 0.5, Front.Y + 0.5, Front.Z))) T->InitEntity();
				W.PlaySound(TEXT("tnt_primed"), Out);
				Consume();
				return;
			}
			if (N == TEXT("flint_and_steel"))
			{
				if (W.GetState(Front) == 0) { W.SetState(Front, FMCBlocks::C.Fire); FMCRandom& R = W.Rand; if (Stack.DamageItem(1, R)) Stack.Clear(); W.MarkModified(P); return; }
			}
			if (N == TEXT("bone_meal"))
			{
				if (ApplyBoneMeal(W, Front, nullptr)) { Consume(); return; }
			}
			if (I.Kind == EMCItemKind::SpawnEgg && !I.SpawnMob.IsNone())
			{
				W.SpawnMob(I.SpawnMob, FVector(Front.X + 0.5, Front.Y + 0.5, Front.Z));
				Consume();
				return;
			}
			if (I.ArmorSlot != EMCArmorSlot::None)
			{
				TArray<AMCEntity*> Ents;
				W.GetEntitiesInBox(FMCBox(Front.X, Front.Y, Front.Z, Front.X + 1, Front.Y + 1, Front.Z + 1), Ents);
				for (AMCEntity* E : Ents)
				{
					if (AMCLiving* L = Cast<AMCLiving>(E))
					{
						const EMCEquipSlot Slot2 = I.ArmorSlot == EMCArmorSlot::Head ? EMCEquipSlot::Head : I.ArmorSlot == EMCArmorSlot::Chest ? EMCEquipSlot::Chest : I.ArmorSlot == EMCArmorSlot::Legs ? EMCEquipSlot::Legs : EMCEquipSlot::Feet;
						if (L->GetItem(Slot2).IsEmpty()) { FMCItemStack One = Stack.Copy(); One.Count = 1; L->SetItem(Slot2, One); Consume(); return; }
					}
				}
			}
			if (I.Block && (FMCBlocks::Get(I.Block).Family == TEXT("shulker_box") || N == TEXT("carved_pumpkin")))
			{
				if (W.GetState(Front) == 0) { W.SetState(Front, FMCBlocks::Get(I.Block).BaseState); Consume(); return; }
			}
			DropItem(W, Stack, Out, F);
		}
		void DropItem(FMCWorld& W, FMCItemStack& Stack, const FVector& Out, EMCFace F) const
		{
			FMCItemStack One = Stack.Copy(); One.Count = 1;
			Stack.Count -= 1; if (Stack.Count <= 0) Stack.Clear();
			const FIntVector& D = MC::FaceDir[(int32)F];
			W.SpawnItem(Out - FVector(0, 0, 0.15), One, false, 0.25f);
			// give it the dispenser throw velocity
			if (W.Entities.Num())
			{
				AMCEntity* E = W.Entities.Last();
				if (E && E->Kind == EMCEntityKind::Item)
				{
					const double Sp = W.Rand.NextDouble() * 0.1 + 0.2;
					E->Vel = FVector(D.X * Sp + W.Rand.Gaussian() * 0.0075 * 6, D.Y * Sp + W.Rand.Gaussian() * 0.0075 * 6, 0.2 + W.Rand.Gaussian() * 0.0075 * 6);
				}
			}
			W.PlaySound(TEXT("dispenser_dispense"), Out);
			W.SpawnParticles(TEXT("smoke"), Out, 6, 0.1f);
		}
	};

	// ================================================================================================================
	class FHopperBeh : public FMCBlockBehavior
	{
	public:
		virtual FMCState GetPlacementState(const FMCBlock& B, const FMCPlaceContext& C) const override
		{
			const EMCFace D = MC::Opposite(C.ClickedFace);
			const EMCFace F = MC::IsHorizontal(D) ? D : EMCFace::Down;
			const bool bDisabled = C.World && C.World->IsPowered(C.Pos);
			return B.State((uint8)F | (bDisabled ? 8 : 0));
		}
		virtual TSharedPtr<FMCBlockEntity> CreateBlockEntity(const FMCBlockPos& P, FMCState S) const override { return MakeShared<FMCHopperEntity>(); }
		virtual bool OnUse(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCPlayer* Player, EMCFace Face, const FVector& Hit) const override
		{
			return Player && Player->OpenBlockContainer(P);
		}
		virtual void OnNeighborChanged(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCBlockPos& From) const override
		{
			const bool bPowered = W.IsPowered(P);
			if (bPowered != MCMeta::Bit(FMCBlocks::MetaOf(S), 3)) W.SetState(P, WithMeta(S, MCMeta::SetBit(FMCBlocks::MetaOf(S), 3, bPowered)), MCSet_KeepEntity | MCSet_Render);
		}
		virtual int32 GetComparatorOutput(FMCWorld& W, const FMCBlockPos& P, FMCState S) const override
		{
			FMCBlockEntity* BE = W.GetBlockEntity(P);
			return BE && BE->GetContainer() ? BE->GetContainer()->ComparatorSignal() : 0;
		}
	};

	// ================================================================================================================
	class FTargetBeh : public FMCBlockBehavior
	{
	public:
		virtual void OnProjectileHit(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCEntity* Projectile, const FVector& Hit) const override
		{
			// signal strength from distance to the face centre
			const FVector L = Hit - FVector(P.X + 0.5, P.Y + 0.5, P.Z + 0.5);
			const double Ax = FMath::Abs(L.X), Ay = FMath::Abs(L.Y), Az = FMath::Abs(L.Z);
			double Off;
			if (Ax >= Ay && Ax >= Az) Off = FMath::Max(Ay, Az);
			else if (Ay >= Ax && Ay >= Az) Off = FMath::Max(Ax, Az);
			else Off = FMath::Max(Ax, Ay);
			const int32 Power = FMath::Clamp(FMath::CeilToInt(15.0 * FMath::Clamp((0.5 - Off) / 0.5, 0.0, 1.0)), 1, 15);
			W.SetState(P, WithMeta(S, (uint8)Power));
			W.NotifyNeighbors(P);
			const AMCProjectile* Pr = Cast<AMCProjectile>(Projectile);
			const bool bArrow = Pr && (Pr->Type == EMCProjectile::Arrow || Pr->Type == EMCProjectile::SpectralArrow || Pr->Type == EMCProjectile::Trident);
			W.ScheduleTick(P, FMCBlocks::BlockOf(S), bArrow ? 20 : 8);
		}
		virtual void OnScheduledTick(FMCWorld& W, const FMCBlockPos& P, FMCState S) const override
		{
			if (FMCBlocks::MetaOf(S) != 0) { W.SetState(P, WithMeta(S, 0)); W.NotifyNeighbors(P); }
		}
		virtual bool IsPowerSource(FMCState S) const override { return true; }
		virtual int32 GetWeakPower(FMCWorld& W, const FMCBlockPos& P, FMCState S, EMCFace Dir) const override { return FMCBlocks::MetaOf(S) & 15; }
		virtual bool ConnectsToRedstone(FMCState S, EMCFace F) const override { return true; }
	};

	// ================================================================================================================
	class FDaylightBeh : public FMCBlockBehavior
	{
	public:
		virtual TSharedPtr<FMCBlockEntity> CreateBlockEntity(const FMCBlockPos& P, FMCState S) const override { return MakeShared<FMCDaylightEntity>(); }
		virtual void OnPlaced(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCPlayer* Player) const override
		{
			if (!W.GetBlockEntity(P)) W.SetBlockEntity(P, MakeShared<FMCDaylightEntity>());
		}
		virtual bool OnUse(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCPlayer* Player, EMCFace Face, const FVector& Hit) const override
		{
			const uint8 M = FMCBlocks::MetaOf(S) ^ 16;
			W.SetState(P, WithMeta(S, M), MCSet_KeepEntity | MCSet_Render);
			W.NotifyNeighbors(P);
			return true;
		}
		virtual bool IsPowerSource(FMCState S) const override { return true; }
		virtual int32 GetWeakPower(FMCWorld& W, const FMCBlockPos& P, FMCState S, EMCFace Dir) const override { return FMCBlocks::MetaOf(S) & 15; }
		virtual bool ConnectsToRedstone(FMCState S, EMCFace F) const override { return true; }
	};

	// ================================================================================================================
	class FNoteBlockBeh : public FMCBlockBehavior
	{
	public:
		static FName Instrument(const FMCWorld& W, const FMCBlockPos& P)
		{
			const FMCState Below = W.GetState(P.Down());
			const FMCBlock& B = FMCBlocks::GetByState(Below);
			// mob heads on top override
			const FMCBlock& Above = FMCBlocks::GetByState(W.GetState(P.Up()));
			if (Above.Model == EMCModel::Skull) return FName(*FString::Printf(TEXT("note_mob_%s"), *Above.Name.ToString()));
			if (B.HasTag(TEXT("wool")) || B.Has(MCB_Wool)) return TEXT("note_guitar");
			if (B.Name == TEXT("clay")) return TEXT("note_flute");
			if (B.Name == TEXT("gold_block")) return TEXT("note_bell");
			if (B.Name == TEXT("packed_ice")) return TEXT("note_chime");
			if (B.Name == TEXT("bone_block")) return TEXT("note_xylophone");
			if (B.Name == TEXT("iron_block")) return TEXT("note_iron_xylophone");
			if (B.Name == TEXT("soul_sand")) return TEXT("note_cow_bell");
			if (B.Name == TEXT("pumpkin")) return TEXT("note_didgeridoo");
			if (B.Name == TEXT("emerald_block")) return TEXT("note_bit");
			if (B.Name == TEXT("hay_block")) return TEXT("note_banjo");
			if (B.Name == TEXT("glowstone")) return TEXT("note_pling");
			if (B.Has(MCB_Sand) || B.Name == TEXT("gravel") || B.Family == TEXT("concrete_powder")) return TEXT("note_snare");
			if (B.Name == TEXT("glass") || B.Sound == EMCSound::Glass) return TEXT("note_hat");
			if (B.Has(MCB_Stone) || B.Tool == EMCTool::Pickaxe) return TEXT("note_basedrum");
			if (B.Sound == EMCSound::Wood) return TEXT("note_bass");
			return TEXT("note_harp");
		}
		static void Play(FMCWorld& W, const FMCBlockPos& P, FMCState S)
		{
			if (W.GetState(P.Up()) != 0 && FMCBlocks::GetByState(W.GetState(P.Up())).Model != EMCModel::Skull) return;
			const int32 Note = FMCBlocks::MetaOf(S) & 31;
			const float Pitch = FMath::Pow(2.f, (Note - 12) / 12.f);
			W.PlaySound(Instrument(W, P), P.Center() * MC::InvBlockSize, 3.f, Pitch);
			const float Hue = Note / 24.f;
			const FLinearColor C = FLinearColor::MakeFromHSV8((uint8)(Hue * 255), 220, 255);
			W.SpawnParticles(TEXT("note"), P.Center() * MC::InvBlockSize + FVector(0, 0, 0.7), 1, 0.f, FVector::ZeroVector, C.ToFColor(true));
		}
		virtual bool OnUse(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCPlayer* Player, EMCFace Face, const FVector& Hit) const override
		{
			const uint8 M = FMCBlocks::MetaOf(S);
			const uint8 Note = ((M & 31) + 1) % 25;
			W.SetState(P, WithMeta(S, (M & 32) | Note), MCSet_None);
			Play(W, P, W.GetState(P));
			return true;
		}
		virtual void OnAttack(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCPlayer* Player) const override { Play(W, P, S); }
		virtual void OnNeighborChanged(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCBlockPos& From) const override
		{
			const bool bPowered = W.IsPowered(P);
			const uint8 M = FMCBlocks::MetaOf(S);
			if (bPowered != MCMeta::Bit(M, 5))
			{
				W.SetState(P, WithMeta(S, MCMeta::SetBit(M, 5, bPowered)), MCSet_None);
				if (bPowered) Play(W, P, S);
			}
		}
	};

	// ================================================================================================================
	class FTNTBeh : public FMCBlockBehavior
	{
	public:
		static void Prime(FMCWorld& W, const FMCBlockPos& P, AMCEntity* Igniter, int32 Fuse = 80)
		{
			W.SetState(P, 0);
			if (!W.Game) return;
			if (AMCPrimedTNT* T = W.Game->SpawnEntity<AMCPrimedTNT>(&W, FVector(P.X + 0.5, P.Y + 0.5, P.Z)))
			{
				T->Fuse = Fuse;
				T->Igniter = Igniter;
				T->InitEntity();
			}
			W.PlaySound(TEXT("tnt_primed"), P.Center() * MC::InvBlockSize);
		}
		virtual void OnPlaced(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCPlayer* Player) const override
		{
			if (W.IsPowered(P)) Prime(W, P, Player);
		}
		virtual void OnNeighborChanged(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCBlockPos& From) const override
		{
			if (W.IsPowered(P)) Prime(W, P, nullptr);
		}
		virtual bool OnUse(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCPlayer* Player, EMCFace Face, const FVector& Hit) const override
		{
			if (HeldIs(Player, TEXT("flint_and_steel")) || HeldIs(Player, TEXT("fire_charge")))
			{
				Prime(W, P, Player);
				if (!Player->IsCreative())
				{
					if (HeldIs(Player, TEXT("fire_charge"))) Player->ConsumeHeld(1); else Player->DamageHeld(1);
				}
				return true;
			}
			return false;
		}
		virtual void OnExploded(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCEntity* Source) const override
		{
			Prime(W, P, Source, 10 + W.Rand.NextInt(20));
		}
		virtual bool OnIgnite(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCEntity* Igniter) const override { Prime(W, P, Igniter); return true; }
		virtual void OnProjectileHit(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCEntity* Projectile, const FVector& Hit) const override
		{
			if (Projectile && Projectile->IsOnFire()) Prime(W, P, Projectile);
		}
	};

	// ================================================================================================================
	class FRedstoneOreBeh : public FMCBlockBehavior
	{
	public:
		static void Light(FMCWorld& W, const FMCBlockPos& P, FMCState S)
		{
			W.SpawnParticles(TEXT("dust"), P.Center() * MC::InvBlockSize, 6, 0.55f, FVector::ZeroVector, FColor(255, 20, 20));
			if (!MCMeta::Bit(FMCBlocks::MetaOf(S), 0)) W.SetState(P, WithMeta(S, 1), MCSet_Render | MCSet_Light);
		}
		virtual void OnAttack(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCPlayer* Player) const override { Light(W, P, S); }
		virtual void OnSteppedOn(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCEntity* E) const override { Light(W, P, S); }
		virtual bool OnUse(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCPlayer* Player, EMCFace Face, const FVector& Hit) const override { Light(W, P, S); return false; }
		virtual void OnRandomTick(FMCWorld& W, const FMCBlockPos& P, FMCState S, FMCRandom& R) const override
		{
			if (MCMeta::Bit(FMCBlocks::MetaOf(S), 0)) W.SetState(P, WithMeta(S, 0), MCSet_Render | MCSet_Light);
		}
		virtual uint8 GetLightEmission(FMCState S, uint8 Default) const override { return MCMeta::Bit(FMCBlocks::MetaOf(S), 0) ? 9 : 0; }
	};

	class FCopperBulbBeh : public FMCBlockBehavior
	{
	public:
		virtual void OnNeighborChanged(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCBlockPos& From) const override
		{
			const bool bPowered = W.IsPowered(P);
			const uint8 M = FMCBlocks::MetaOf(S);
			if (bPowered == MCMeta::Bit(M, 1)) return;
			uint8 NM = MCMeta::SetBit(M, 1, bPowered);
			if (bPowered) { NM ^= 1; W.PlaySound(MCMeta::Bit(NM, 0) ? TEXT("copper_bulb_on") : TEXT("copper_bulb_off"), P.Center() * MC::InvBlockSize); }
			W.SetState(P, WithMeta(S, NM), MCSet_Render | MCSet_Light | MCSet_Neighbors);
		}
		virtual uint8 GetLightEmission(FMCState S, uint8 Default) const override
		{
			if (!MCMeta::Bit(FMCBlocks::MetaOf(S), 0)) return 0;
			const FString V = FMCBlocks::GetByState(S).Variant.ToString();
			const int32 Stage = FCString::Atoi(*V);
			static const uint8 Levels[4] = { 15, 12, 8, 4 };
			return Levels[FMath::Clamp(Stage, 0, 3)];
		}
		virtual int32 GetComparatorOutput(FMCWorld& W, const FMCBlockPos& P, FMCState S) const override { return MCMeta::Bit(FMCBlocks::MetaOf(S), 0) ? 15 : 0; }
	};
}

void MCBeh::RegisterRedstone()
{
	Register(EMCBeh::RedstoneWire, new FRedstoneWireBeh());
	Register(EMCBeh::RedstoneTorch, new FRedstoneTorchBeh());
	Register(EMCBeh::Repeater, new FRepeaterBeh());
	Register(EMCBeh::Comparator, new FComparatorBeh());
	Register(EMCBeh::RedstoneLamp, new FRedstoneLampBeh());
	Register(EMCBeh::RedstoneBlock, new FRedstoneBlockBeh());
	Register(EMCBeh::Piston, new FPistonBeh());
	Register(EMCBeh::PistonHead, new FPistonHeadBeh());
	Register(EMCBeh::Observer, new FObserverBeh());
	FDispenserBeh* Disp = new FDispenserBeh(); Disp->bDropper = false;
	FDispenserBeh* Drop = new FDispenserBeh(); Drop->bDropper = true;
	Register(EMCBeh::Dispenser, Disp);
	Register(EMCBeh::Dropper, Drop);
	Register(EMCBeh::Hopper, new FHopperBeh());
	Register(EMCBeh::Target, new FTargetBeh());
	Register(EMCBeh::DaylightDetector, new FDaylightBeh());
	Register(EMCBeh::NoteBlock, new FNoteBlockBeh());
	Register(EMCBeh::TNT, new FTNTBeh());
	Register(EMCBeh::RedstoneOre, new FRedstoneOreBeh());
	Register(EMCBeh::CopperBulb, new FCopperBulbBeh());
}
