#include "Render/MCMesher.h"
#include "World/MCWorld.h"
#include "Gen/MCBiomes.h"
#include "Blocks/MCTextures.h"

using namespace MCRender;

// Per-block "rendered by an authored static mesh" flags; filled by the renderer before meshing starts.
UNREAL_MINECRAFT_API TArray<uint8> GMCAuthoredMeshBlocks;

namespace MCBiomeColors
{
	FColor Grass(uint8 B) { return FMCBiomes::Get(B).Grass; }
	FColor Foliage(uint8 B) { return FMCBiomes::Get(B).Foliage; }
	FColor Water(uint8 B) { return FMCBiomes::Get(B).Water; }
}

// ---------------------------------------------------------------------------------------------------------------------
// Snapshot capture (game thread)

bool FMCMeshInput::Capture(const FMCWorld& World, const FMCChunkPos& C, int32 G)
{
	Chunk = C;
	Group = G;
	BaseZ = GroupBaseZ(G);
	Dim = World.Dim;
	States.SetNumUninitialized(SX * SY * SZ);
	Light.SetNumUninitialized(SX * SY * SZ);

	const FMCChunk* Near[3][3];
	for (int32 dy = -1; dy <= 1; ++dy)
		for (int32 dx = -1; dx <= 1; ++dx)
			Near[dx + 1][dy + 1] = World.GetChunk(FMCChunkPos(C.X + dx, C.Y + dy));

	const uint8 DefaultLight = (uint8)((World.Dim == EMCDimension::Overworld ? 15 : 0) << 4);
	bool bAny = false;
	for (int32 z = -1; z < SZ - 1; ++z)
	{
		const int32 WZ = BaseZ + z;
		for (int32 y = -1; y < SY - 1; ++y)
		{
			const int32 CYI = y < 0 ? 0 : (y >= 16 ? 2 : 1);
			const int32 LY = (y + 16) & 15;
			for (int32 x = -1; x < SX - 1; ++x)
			{
				const int32 CXI = x < 0 ? 0 : (x >= 16 ? 2 : 1);
				const int32 LX = (x + 16) & 15;
				const FMCChunk* Ch = Near[CXI][CYI];
				const int32 I = Index(x, y, z);
				if (Ch && FMCChunk::InRange(WZ))
				{
					const FMCState S = Ch->Get(LX, LY, WZ);
					States[I] = S;
					Light[I] = Ch->GetLightRaw(LX, LY, WZ);
					if (S != 0 && x >= 0 && x < 16 && y >= 0 && y < 16 && z >= 0 && z < GroupHeight) bAny = true;
				}
				else
				{
					States[I] = (!Ch && FMCChunk::InRange(WZ)) ? FMCBlocks::C.Stone : 0; // unknown neighbours act solid (no seams)
					Light[I] = WZ > MC::MaxZ ? DefaultLight : 0;
				}
			}
		}
	}

	// biome tints with a 5x5 blend (Minecraft-style smooth transitions)
	for (int32 y = -1; y < SY - 1; ++y)
	{
		for (int32 x = -1; x < SX - 1; ++x)
		{
			int32 R[3] = { 0, 0, 0 }, Gc[3] = { 0, 0, 0 }, Bc[3] = { 0, 0, 0 }, N = 0;
			for (int32 oy = -2; oy <= 2; oy += 2)
			{
				for (int32 ox = -2; ox <= 2; ox += 2)
				{
					const int32 WX = C.MinBlockX() + x + ox, WY = C.MinBlockY() + y + oy;
					const FMCChunk* Ch = World.GetChunk(FMCChunkPos::FromBlock(WX, WY));
					if (!Ch) continue;
					const int32 H = FMath::Clamp<int32>(Ch->GetHeight(WX & 15, WY & 15), BaseZ, BaseZ + GroupHeight - 1);
					const uint8 Bi = Ch->GetBiome(WX & 15, WY & 15, H);
					const FMCBiomeDef& D = FMCBiomes::Get(Bi);
					R[0] += D.Grass.R; Gc[0] += D.Grass.G; Bc[0] += D.Grass.B;
					R[1] += D.Foliage.R; Gc[1] += D.Foliage.G; Bc[1] += D.Foliage.B;
					R[2] += D.Water.R; Gc[2] += D.Water.G; Bc[2] += D.Water.B;
					++N;
				}
			}
			const int32 Ci = Col(x, y);
			if (N == 0) { GrassTint[Ci] = FColor(145, 189, 89); FoliageTint[Ci] = FColor(119, 171, 47); WaterTint[Ci] = FColor(63, 118, 228); continue; }
			GrassTint[Ci] = FColor(R[0] / N, Gc[0] / N, Bc[0] / N);
			FoliageTint[Ci] = FColor(R[1] / N, Gc[1] / N, Bc[1] / N);
			WaterTint[Ci] = FColor(R[2] / N, Gc[2] / N, Bc[2] / N);
		}
	}
	return bAny;
}

// ---------------------------------------------------------------------------------------------------------------------
// Mesher (worker thread)

namespace
{
	constexpr float BS = MC::BlockSizeF;

	struct FSnapshotGetter : public FMCBlockGetter
	{
		const FMCMeshInput& In;
		FMCBlockPos Origin;
		FSnapshotGetter(const FMCMeshInput& I) : In(I), Origin(I.Chunk.MinBlockX(), I.Chunk.MinBlockY(), I.BaseZ) {}
		virtual FMCState GetState(const FMCBlockPos& P) const override
		{
			const int32 X = P.X - Origin.X, Y = P.Y - Origin.Y, Z = P.Z - Origin.Z;
			if (X < -1 || X > 16 || Y < -1 || Y > 16 || Z < -1 || Z > GroupHeight) return 0;
			return In.Get(X, Y, Z);
		}
	};

	FORCEINLINE bool Opaque(FMCState S) { return (FMCBlocks::Info(S).Flags & MCB_Opaque) != 0; }

	/** Face tangent axes (a, b) for AO sampling. */
	const FIntVector FaceA[6] = { FIntVector(1, 0, 0), FIntVector(1, 0, 0), FIntVector(1, 0, 0), FIntVector(1, 0, 0), FIntVector(0, 1, 0), FIntVector(0, 1, 0) };
	const FIntVector FaceB[6] = { FIntVector(0, 1, 0), FIntVector(0, 1, 0), FIntVector(0, 0, 1), FIntVector(0, 0, 1), FIntVector(0, 0, 1), FIntVector(0, 0, 1) };

	FORCEINLINE FVector2f AutoUV(int32 Face, const FVector3f& P)
	{
		switch (Face)
		{
		case 0: return FVector2f(P.X, 1.f - P.Y);         // down
		case 1: return FVector2f(P.X, P.Y);               // up
		case 2: return FVector2f(1.f - P.X, 1.f - P.Z);   // north
		case 3: return FVector2f(P.X, 1.f - P.Z);         // south
		case 4: return FVector2f(P.Y, 1.f - P.Z);         // west
		default: return FVector2f(1.f - P.Y, 1.f - P.Z);  // east
		}
	}
	FORCEINLINE FVector2f RotUV(const FVector2f& UV, uint8 Rot)
	{
		switch (Rot & 3)
		{
		case 1: return FVector2f(1.f - UV.Y, UV.X);
		case 2: return FVector2f(1.f - UV.X, 1.f - UV.Y);
		case 3: return FVector2f(UV.Y, 1.f - UV.X);
		default: return UV;
		}
	}

	struct FBuilder
	{
		const FMCMeshInput& In;
		FMCChunkMeshData& Out;
		FSnapshotGetter Getter;
		FBuilder(const FMCMeshInput& I, FMCChunkMeshData& O) : In(I), Out(O), Getter(I) {}

		/** Emit a quad. P in block-local units relative to the group origin (x 0..16, y 0..16, z 0..64). */
		void Quad(int32 Layer, const FVector3f P[4], const FVector2f UV[4], const FVector3f& Normal, int16 Tex, uint32 Flags,
			const float Sky[4], const float Blk[4], const float AO[4], const FColor& Tint, const FVector2f* CustomUV0 = nullptr)
		{
			FMCMeshLayerData& L = Out.Layers[Layer];
			const uint32 Base = (uint32)L.Vertices.Num();
			// tangent from UV derivatives (triangle 0-1-2)
			const FVector3f E1 = P[1] - P[0], E2 = P[2] - P[0];
			const FVector2f D1 = UV[1] - UV[0], D2 = UV[2] - UV[0];
			const float Det = D1.X * D2.Y - D2.X * D1.Y;
			FVector3f T = FMath::Abs(Det) > 1e-8f ? (E1 * D2.Y - E2 * D1.Y) / Det : FVector3f(1, 0, 0);
			FVector3f Bt = FMath::Abs(Det) > 1e-8f ? (E2 * D1.X - E1 * D2.X) / Det : FVector3f(0, 1, 0);
			T = (T - Normal * FVector3f::DotProduct(Normal, T)).GetSafeNormal();
			if (T.IsNearlyZero()) T = FMath::Abs(Normal.Z) < 0.9f ? FVector3f(0, 0, 1) ^ Normal : FVector3f(1, 0, 0);
			const float Sign = FVector3f::DotProduct(FVector3f::CrossProduct(Normal, T), Bt) < 0.f ? -1.f : 1.f;

			for (int32 i = 0; i < 4; ++i)
			{
				FMCMeshVertex V;
				V.Pos = P[i] * BS;
				V.Normal = Normal;
				V.Tangent = T;
				V.TangentSign = Sign;
				V.UV0 = CustomUV0 ? CustomUV0[i] : UV[i];
				V.UV1 = FVector2f((float)Tex, (float)Flags);
				V.UV2 = FVector2f(Sky[i] / 15.f, Blk[i] / 15.f);
				V.Color = FColor(Tint.R, Tint.G, Tint.B, (uint8)FMath::Clamp(AO[i] * 255.f, 0.f, 255.f));
				L.Vertices.Add(V);
				Out.Bounds += FVector(V.Pos);
			}
			// orientation: front faces satisfy Cross(B-A, C-A) . N < 0 in UE's convention
			const FVector3f C = FVector3f::CrossProduct(P[1] - P[0], P[2] - P[0]);
			const bool bFlip = FVector3f::DotProduct(C, Normal) > 0.f;
			// choose the diagonal that interpolates AO best
			const bool bAlt = (AO[0] + AO[2]) < (AO[1] + AO[3]);
			uint32 Idx[6];
			if (!bAlt) { Idx[0] = 0; Idx[1] = 1; Idx[2] = 2; Idx[3] = 0; Idx[4] = 2; Idx[5] = 3; }
			else { Idx[0] = 1; Idx[1] = 2; Idx[2] = 3; Idx[3] = 1; Idx[4] = 3; Idx[5] = 0; }
			if (bFlip) { Swap(Idx[1], Idx[2]); Swap(Idx[4], Idx[5]); }
			for (int32 i = 0; i < 6; ++i) L.Indices.Add(Base + Idx[i]);
		}

		FColor TintFor(const FMCBlock& B, uint8 Meta, int32 LX, int32 LY) const
		{
			const int32 Ci = FMCMeshInput::Col(LX, LY);
			switch (B.Tint)
			{
			case EMCTint::Grass: return In.GrassTint[Ci];
			case EMCTint::Foliage: return In.FoliageTint[Ci];
			case EMCTint::Water: return In.WaterTint[Ci];
			case EMCTint::Birch: return FColor(128, 167, 85);
			case EMCTint::Spruce: return FColor(97, 153, 97);
			case EMCTint::Custom: return B.TintColor;
			case EMCTint::Stem:
			{
				const float A = (Meta & 7) / 7.f;
				return FColor((uint8)(A * 224), (uint8)(255 - A * 144), (uint8)(A * 36 + 16));
			}
			case EMCTint::Lily: return FColor(32, 128, 48);
			case EMCTint::Redstone:
			{
				const float Pw = (Meta & 15) / 15.f;
				return FColor((uint8)FMath::Lerp(90.f, 255.f, Pw), (uint8)(Pw * Pw * 60.f), (uint8)(Pw * 20.f));
			}
			default: return FColor::White;
			}
		}

		void LightAt(int32 X, int32 Y, int32 Z, float& Sky, float& Blk) const
		{
			const uint8 L = In.GetLight(X, Y, Z);
			Sky = (float)(L >> 4);
			Blk = (float)(L & 15);
		}

		// ------------------------------------------------------------------ full cube
		void Cube(int32 X, int32 Y, int32 Z, FMCState S, const FMCStateInfo& SI, const FMCBlock& B)
		{
			const int32 Layer = (int32)SI.Layer;
			const bool bCullSame = (SI.Flags & MCB_CullSame) != 0;
			const bool bLeaves = (SI.Flags & MCB_Leaves) != 0;
			const FColor Tint = TintFor(B, SI.Meta, X, Y);
			uint32 BaseFlags = 0;
			if (B.Tint != EMCTint::None) BaseFlags |= VF_Tint;
			if (SI.Light > 0) BaseFlags |= VF_Emissive;
			if (bLeaves) BaseFlags |= VF_Wave | VF_Foliage;
			const uint64 PosHash = MCHash::Hash3(0x51F0, In.Chunk.MinBlockX() + X, In.Chunk.MinBlockY() + Y, In.BaseZ + Z);

			for (int32 F = 0; F < 6; ++F)
			{
				const FIntVector& D = MC::FaceDir[F];
				const FMCState N = In.Get(X + D.X, Y + D.Y, Z + D.Z);
				if (N != 0)
				{
					const FMCStateInfo& NI = FMCBlocks::Info(N);
					if (NI.Flags & MCB_Opaque) continue;
					if (bCullSame && NI.Block == SI.Block) continue;
					if (!bLeaves && !bCullSame && NI.Shape == EMCShape::Cube && (NI.Flags & MCB_Opaque)) continue;
					// translucent / cutout cubes of the same kind (ice, stained glass)
					if ((Layer == (int32)EMCLayer::Translucent) && NI.Block == SI.Block) continue;
				}
				const int16 Tex = SI.Tex[F];
				if (Tex < 0) continue;

				// corners
				const FIntVector A = FaceA[F], Bv = FaceB[F];
				const FIntVector N0(X + D.X, Y + D.Y, Z + D.Z);
				FVector3f P[4]; FVector2f UV[4]; float Sky[4], Blk[4], AO[4];
				static const int32 SA[4] = { -1, 1, 1, -1 };
				static const int32 SB[4] = { -1, -1, 1, 1 };
				float S0, B0; LightAt(N0.X, N0.Y, N0.Z, S0, B0);
				uint8 Rot = SI.UVRot[F];
				if (B.bRandomRotateTop && (F == (int32)EMCFace::Up)) Rot = (uint8)(PosHash & 3);
				for (int32 c = 0; c < 4; ++c)
				{
					const FIntVector S1 = N0 + A * SA[c];
					const FIntVector S2 = N0 + Bv * SB[c];
					const FIntVector SC = N0 + A * SA[c] + Bv * SB[c];
					const FMCState St1 = In.Get(S1.X, S1.Y, S1.Z), St2 = In.Get(S2.X, S2.Y, S2.Z), StC = In.Get(SC.X, SC.Y, SC.Z);
					const bool O1 = Opaque(St1), O2 = Opaque(St2), OC = Opaque(StC);
					const int32 AOLevel = (O1 && O2) ? 0 : 3 - ((int32)O1 + (int32)O2 + (int32)OC);
					AO[c] = 0.42f + 0.1933f * AOLevel;
					float Ss = S0, Bs = B0; int32 Cnt = 1;
					float T1, T2;
					if (!O1) { LightAt(S1.X, S1.Y, S1.Z, T1, T2); Ss += T1; Bs += T2; ++Cnt; }
					if (!O2) { LightAt(S2.X, S2.Y, S2.Z, T1, T2); Ss += T1; Bs += T2; ++Cnt; }
					if (!OC && !(O1 && O2)) { LightAt(SC.X, SC.Y, SC.Z, T1, T2); Ss += T1; Bs += T2; ++Cnt; }
					Sky[c] = Ss / Cnt; Blk[c] = Bs / Cnt;
					// position of the corner
					FVector3f V((float)X, (float)Y, (float)Z);
					if (D.X > 0) V.X += 1; if (D.Y > 0) V.Y += 1; if (D.Z > 0) V.Z += 1;
					if (SA[c] > 0) V += FVector3f((float)A.X, (float)A.Y, (float)A.Z);
					if (SB[c] > 0) V += FVector3f((float)Bv.X, (float)Bv.Y, (float)Bv.Z);
					P[c] = V;
					const FVector3f Local(V.X - X, V.Y - Y, V.Z - Z);
					UV[c] = RotUV(AutoUV(F, Local), Rot);
				}
				const FVector3f Normal((float)D.X, (float)D.Y, (float)D.Z);
				Quad(Layer, P, UV, Normal, Tex, BaseFlags, Sky, Blk, AO, Tint);
			}
		}

		// ------------------------------------------------------------------ cross / crop plants
		void Plant(int32 X, int32 Y, int32 Z, FMCState S, const FMCStateInfo& SI, const FMCBlock& B, bool bCrop)
		{
			const int16 Tex = SI.Tex[0];
			const FColor Tint = TintFor(B, SI.Meta, X, Y);
			uint32 Flags = VF_Wave | VF_Foliage;
			if (B.Tint != EMCTint::None) Flags |= VF_Tint;
			if (SI.Light > 0) Flags |= VF_Emissive;
			float Sk, Bl; LightAt(X, Y, Z, Sk, Bl);
			const float Sky[4] = { Sk, Sk, Sk, Sk }, Blk[4] = { Bl, Bl, Bl, Bl };
			const float AOb[4] = { 0.7f, 0.7f, 1.f, 1.f };
			// deterministic random offset like Minecraft flowers/grass
			const uint64 H = MCHash::Hash3(0xF10E, In.Chunk.MinBlockX() + X, In.Chunk.MinBlockY() + Y, In.BaseZ + Z);
			float OX = 0.f, OY = 0.f, OZ = 0.f;
			const bool bOffset = !bCrop && (B.Has(MCB_Plant) && B.Name != TEXT("sugar_cane") && !B.Has(MCB_Waterlogged) && B.Orient != EMCCubeOrient::Upper);
			if (bOffset)
			{
				OX = ((float)((H >> 0) & 15) / 15.f - 0.5f) * 0.5f;
				OY = ((float)((H >> 4) & 15) / 15.f - 0.5f) * 0.5f;
				OZ = -((float)((H >> 8) & 15) / 15.f) * 0.2f * (B.Name == TEXT("short_grass") || B.Name == TEXT("fern") ? 1.f : 0.f);
			}
			const FVector3f O((float)X + OX, (float)Y + OY, (float)Z + OZ);
			auto EmitBoth = [&](const FVector3f P[4], const FVector2f UV[4])
			{
				FVector3f N = FVector3f::CrossProduct(P[1] - P[0], P[3] - P[0]).GetSafeNormal();
				Quad((int32)EMCLayer::Cutout, P, UV, -N, Tex, Flags, Sky, Blk, AOb, Tint);
			};
			const FVector2f UV[4] = { FVector2f(0, 1), FVector2f(1, 1), FVector2f(1, 0), FVector2f(0, 0) };
			if (!bCrop)
			{
				const float A = 0.1464f, Bq = 0.8536f;
				const FVector3f Q1[4] = { O + FVector3f(A, A, 0), O + FVector3f(Bq, Bq, 0), O + FVector3f(Bq, Bq, 1), O + FVector3f(A, A, 1) };
				const FVector3f Q2[4] = { O + FVector3f(A, Bq, 0), O + FVector3f(Bq, A, 0), O + FVector3f(Bq, A, 1), O + FVector3f(A, Bq, 1) };
				EmitBoth(Q1, UV); EmitBoth(Q2, UV);
			}
			else
			{
				const float Ks[2] = { 0.25f, 0.75f };
				for (float K : Ks)
				{
					const FVector3f QX[4] = { O + FVector3f(K, 0, -1.f / 16), O + FVector3f(K, 1, -1.f / 16), O + FVector3f(K, 1, 15.f / 16), O + FVector3f(K, 0, 15.f / 16) };
					const FVector3f QY[4] = { O + FVector3f(0, K, -1.f / 16), O + FVector3f(1, K, -1.f / 16), O + FVector3f(1, K, 15.f / 16), O + FVector3f(0, K, 15.f / 16) };
					EmitBoth(QX, UV); EmitBoth(QY, UV);
				}
			}
		}

		// ------------------------------------------------------------------ fluids
		static bool IsFluidOf(FMCState S, FMCBlockId Fluid)
		{
			if (S == 0) return false;
			const FMCStateInfo& I = FMCBlocks::Info(S);
			if (I.Block == Fluid) return true;
			return Fluid == FMCBlocks::C.WaterId && (I.Flags & MCB_Waterlogged);
		}
		static float OwnHeight(FMCState S, FMCBlockId Fluid)
		{
			const FMCStateInfo& I = FMCBlocks::Info(S);
			if (I.Block != Fluid) return 8.f / 9.f; // waterlogged block = source
			const int32 Level = I.Meta & 15;
			if (Level >= 8 || Level == 0) return 8.f / 9.f;
			return (8.f - Level) / 9.f;
		}
		float CellHeight(int32 X, int32 Y, int32 Z, FMCBlockId Fluid) const
		{
			const FMCState S = In.Get(X, Y, Z);
			if (!IsFluidOf(S, Fluid)) return -1.f;
			if (IsFluidOf(In.Get(X, Y, Z + 1), Fluid)) return 1.f;
			return OwnHeight(S, Fluid);
		}
		float CornerHeight(int32 X, int32 Y, int32 Z, int32 DX, int32 DY, FMCBlockId Fluid) const
		{
			// average over the 4 cells touching the corner (X..X+DX, Y..Y+DY)
			float Sum = 0.f, W = 0.f;
			const int32 Xs[2] = { X, X + DX }, Ys[2] = { Y, Y + DY };
			for (int32 ix = 0; ix < 2; ++ix)
			{
				for (int32 iy = 0; iy < 2; ++iy)
				{
					const int32 CX = Xs[ix], CY = Ys[iy];
					if (IsFluidOf(In.Get(CX, CY, Z + 1), Fluid)) return 1.f;
					const float H = CellHeight(CX, CY, Z, Fluid);
					if (H >= 0.f)
					{
						const float Wt = H >= 0.8f ? 10.f : 1.f;
						Sum += H * Wt; W += Wt;
					}
					else if (!FMCBlocks::IsSolid(In.Get(CX, CY, Z)))
					{
						W += 1.f;
					}
				}
			}
			return W > 0.f ? Sum / W : 0.f;
		}

		void Fluid(int32 X, int32 Y, int32 Z, FMCState S, const FMCStateInfo& SI)
		{
			const bool bLava = SI.Block == FMCBlocks::C.LavaId;
			const FMCBlockId Fl = bLava ? FMCBlocks::C.LavaId : FMCBlocks::C.WaterId;
			const int32 Layer = bLava ? (int32)EMCLayer::Lava : (int32)EMCLayer::Water;
			const int16 Tex = FMCTextures::Find(bLava ? TEXT("lava_still") : TEXT("water_still"));
			const FColor Tint = bLava ? FColor::White : In.WaterTint[FMCMeshInput::Col(X, Y)];
			const uint32 Flags = (bLava ? VF_Emissive : VF_Tint) | VF_Flow;

			const bool bAboveSame = IsFluidOf(In.Get(X, Y, Z + 1), Fl);
			float H00, H10, H11, H01;
			if (bAboveSame) { H00 = H10 = H11 = H01 = 1.f; }
			else
			{
				H00 = CornerHeight(X, Y, Z, -1, -1, Fl);
				H10 = CornerHeight(X, Y, Z, 1, -1, Fl);
				H11 = CornerHeight(X, Y, Z, 1, 1, Fl);
				H01 = CornerHeight(X, Y, Z, -1, 1, Fl);
			}
			float Sk, Bl; LightAt(X, Y, Z, Sk, Bl);
			float SkU, BlU; LightAt(X, Y, Z + 1, SkU, BlU);
			const float SkyT[4] = { FMath::Max(Sk, SkU), FMath::Max(Sk, SkU), FMath::Max(Sk, SkU), FMath::Max(Sk, SkU) };
			const float BlkT[4] = { FMath::Max(Bl, BlU), FMath::Max(Bl, BlU), FMath::Max(Bl, BlU), FMath::Max(Bl, BlU) };
			const float AO1[4] = { 1, 1, 1, 1 };

			// flow direction
			FVector2f Flow(0, 0);
			const float Own = CellHeight(X, Y, Z, Fl);
			const int32 Dx[4] = { 1, -1, 0, 0 }, Dy[4] = { 0, 0, 1, -1 };
			for (int32 i = 0; i < 4; ++i)
			{
				const float Hn = CellHeight(X + Dx[i], Y + Dy[i], Z, Fl);
				if (Hn >= 0.f) Flow += FVector2f((float)Dx[i], (float)Dy[i]) * (Own - Hn);
				else if (!FMCBlocks::IsSolid(In.Get(X + Dx[i], Y + Dy[i], Z))) Flow += FVector2f((float)Dx[i], (float)Dy[i]) * Own * 0.5f;
			}
			if ((SI.Meta & 15) >= 8 && !bAboveSame) Flow *= 0.5f;

			// top surface (both windings so it is visible from below the water)
			if (!bAboveSame || true)
			{
				const FMCState Up = In.Get(X, Y, Z + 1);
				if (!IsFluidOf(Up, Fl) && !(FMCBlocks::IsOpaque(Up) && H00 >= 1.f && H11 >= 1.f))
				{
					const FVector3f P[4] = { FVector3f(X, Y, Z + H00), FVector3f(X + 1, Y, Z + H10), FVector3f(X + 1, Y + 1, Z + H11), FVector3f(X, Y + 1, Z + H01) };
					const FVector2f UV[4] = { FVector2f(0, 0), FVector2f(1, 0), FVector2f(1, 1), FVector2f(0, 1) };
					const FVector2f F4[4] = { Flow, Flow, Flow, Flow };
					Quad(Layer, P, UV, FVector3f(0, 0, 1), Tex, Flags, SkyT, BlkT, AO1, Tint, F4);
					const FVector3f PB[4] = { P[3], P[2], P[1], P[0] };
					const FVector2f UVB[4] = { UV[3], UV[2], UV[1], UV[0] };
					Quad(Layer, PB, UVB, FVector3f(0, 0, -1), Tex, Flags, SkyT, BlkT, AO1, Tint, F4);
				}
			}
			// sides
			struct FSide { int32 DX, DY; int32 Face; };
			const FSide Sides[4] = { { 0, -1, 2 }, { 0, 1, 3 }, { -1, 0, 4 }, { 1, 0, 5 } };
			for (const FSide& Sd : Sides)
			{
				const FMCState N = In.Get(X + Sd.DX, Y + Sd.DY, Z);
				if (IsFluidOf(N, Fl) || FMCBlocks::IsOpaque(N)) continue;
				float Ha, Hb; FVector3f A, Bp;
				switch (Sd.Face)
				{
				case 2: A = FVector3f(X + 1, Y, Z); Bp = FVector3f(X, Y, Z); Ha = H10; Hb = H00; break;
				case 3: A = FVector3f(X, Y + 1, Z); Bp = FVector3f(X + 1, Y + 1, Z); Ha = H01; Hb = H11; break;
				case 4: A = FVector3f(X, Y, Z); Bp = FVector3f(X, Y + 1, Z); Ha = H00; Hb = H01; break;
				default: A = FVector3f(X + 1, Y + 1, Z); Bp = FVector3f(X + 1, Y, Z); Ha = H11; Hb = H10; break;
				}
				const FVector3f P[4] = { A, Bp, Bp + FVector3f(0, 0, Hb), A + FVector3f(0, 0, Ha) };
				const FVector2f UV[4] = { FVector2f(0, 1), FVector2f(1, 1), FVector2f(1, 1 - Hb), FVector2f(0, 1 - Ha) };
				float SkN, BlN; LightAt(X + Sd.DX, Y + Sd.DY, Z, SkN, BlN);
				const float SkyS[4] = { FMath::Max(Sk, SkN), FMath::Max(Sk, SkN), FMath::Max(Sk, SkN), FMath::Max(Sk, SkN) };
				const float BlkS[4] = { FMath::Max(Bl, BlN), FMath::Max(Bl, BlN), FMath::Max(Bl, BlN), FMath::Max(Bl, BlN) };
				const FVector2f Down[4] = { FVector2f(0, -1), FVector2f(0, -1), FVector2f(0, -1), FVector2f(0, -1) };
				const FIntVector& D = MC::FaceDir[Sd.Face];
				Quad(Layer, P, UV, FVector3f((float)D.X, (float)D.Y, (float)D.Z), Tex, Flags, SkyS, BlkS, AO1, Tint, Down);
			}
			// bottom
			{
				const FMCState N = In.Get(X, Y, Z - 1);
				if (!IsFluidOf(N, Fl) && !FMCBlocks::IsOpaque(N))
				{
					const FVector3f P[4] = { FVector3f(X, Y + 1, Z), FVector3f(X + 1, Y + 1, Z), FVector3f(X + 1, Y, Z), FVector3f(X, Y, Z) };
					const FVector2f UV[4] = { FVector2f(0, 1), FVector2f(1, 1), FVector2f(1, 0), FVector2f(0, 0) };
					const FVector2f Z4[4] = { FVector2f(0, 0), FVector2f(0, 0), FVector2f(0, 0), FVector2f(0, 0) };
					Quad(Layer, P, UV, FVector3f(0, 0, -1), Tex, Flags, SkyT, BlkT, AO1, Tint, Z4);
				}
			}
		}

		// ------------------------------------------------------------------ box / quad models
		void Model(int32 X, int32 Y, int32 Z, FMCState S, const FMCStateInfo& SI, const FMCBlock& B)
		{
			const FMCBlockModel* M = nullptr;
			FMCBlockModel Dyn;
			if (SI.StaticModel >= 0) M = FMCBlocks::GetStaticModel(S);
			else
			{
				const FMCBlockPos WP(In.Chunk.MinBlockX() + X, In.Chunk.MinBlockY() + Y, In.BaseZ + Z);
				FMCBlocks::BuildModel(S, &Getter, WP, Dyn);
				M = &Dyn;
			}
			if (!M) return;
			const FColor BlockTint = TintFor(B, SI.Meta, X, Y);
			float Sk, Bl; LightAt(X, Y, Z, Sk, Bl);
			const int32 BlockLayer = (int32)SI.Layer;
			const float AO1[4] = { 1, 1, 1, 1 };

			for (const FMCModelBox& Bx : M->Boxes)
			{
				const int32 Layer = Bx.Layer != 255 ? Bx.Layer : BlockLayer;
				for (int32 F = 0; F < 6; ++F)
				{
					const int16 Tex = Bx.Tex[F];
					if (Tex < 0) continue;
					const FIntVector& D = MC::FaceDir[F];
					float FS = Sk, FB = Bl;
					const bool bOnBoundary = (F == 0 && Bx.Min.Z <= 0.0001f) || (F == 1 && Bx.Max.Z >= 0.9999f) || (F == 2 && Bx.Min.Y <= 0.0001f)
						|| (F == 3 && Bx.Max.Y >= 0.9999f) || (F == 4 && Bx.Min.X <= 0.0001f) || (F == 5 && Bx.Max.X >= 0.9999f);
					if (bOnBoundary)
					{
						const FMCState N = In.Get(X + D.X, Y + D.Y, Z + D.Z);
						if ((Bx.CullFaces & (1 << F)) && FMCBlocks::IsOpaque(N)) continue;
						float NS, NB; LightAt(X + D.X, Y + D.Y, Z + D.Z, NS, NB);
						FS = FMath::Max(FS, NS); FB = FMath::Max(FB, NB);
					}
					// face rectangle
					const FVector3f Mn = Bx.Min, Mx = Bx.Max;
					FVector3f P[4];
					switch (F)
					{
					case 0: P[0] = FVector3f(Mn.X, Mn.Y, Mn.Z); P[1] = FVector3f(Mx.X, Mn.Y, Mn.Z); P[2] = FVector3f(Mx.X, Mx.Y, Mn.Z); P[3] = FVector3f(Mn.X, Mx.Y, Mn.Z); break;
					case 1: P[0] = FVector3f(Mn.X, Mn.Y, Mx.Z); P[1] = FVector3f(Mx.X, Mn.Y, Mx.Z); P[2] = FVector3f(Mx.X, Mx.Y, Mx.Z); P[3] = FVector3f(Mn.X, Mx.Y, Mx.Z); break;
					case 2: P[0] = FVector3f(Mn.X, Mn.Y, Mn.Z); P[1] = FVector3f(Mx.X, Mn.Y, Mn.Z); P[2] = FVector3f(Mx.X, Mn.Y, Mx.Z); P[3] = FVector3f(Mn.X, Mn.Y, Mx.Z); break;
					case 3: P[0] = FVector3f(Mn.X, Mx.Y, Mn.Z); P[1] = FVector3f(Mx.X, Mx.Y, Mn.Z); P[2] = FVector3f(Mx.X, Mx.Y, Mx.Z); P[3] = FVector3f(Mn.X, Mx.Y, Mx.Z); break;
					case 4: P[0] = FVector3f(Mn.X, Mn.Y, Mn.Z); P[1] = FVector3f(Mn.X, Mx.Y, Mn.Z); P[2] = FVector3f(Mn.X, Mx.Y, Mx.Z); P[3] = FVector3f(Mn.X, Mn.Y, Mx.Z); break;
					default: P[0] = FVector3f(Mx.X, Mn.Y, Mn.Z); P[1] = FVector3f(Mx.X, Mx.Y, Mn.Z); P[2] = FVector3f(Mx.X, Mx.Y, Mx.Z); P[3] = FVector3f(Mx.X, Mn.Y, Mx.Z); break;
					}
					FVector2f UV[4];
					for (int32 c = 0; c < 4; ++c)
					{
						if (Bx.bAutoUV) UV[c] = RotUV(AutoUV(F, P[c]), Bx.UVRot[F]);
						else
						{
							const FVector2f A = AutoUV(F, P[c]);
							const FVector4f& R = Bx.UV[F];
							UV[c] = FVector2f(FMath::Lerp(R.X, R.Z, A.X), FMath::Lerp(R.Y, R.W, A.Y));
						}
						P[c] += FVector3f((float)X, (float)Y, (float)Z);
					}
					uint32 Flags = 0;
					const bool bTint = (Bx.TintFaces & (1 << F)) || B.Tint != EMCTint::None;
					if (bTint) Flags |= VF_Tint;
					if (Bx.bEmissive || SI.Light > 0) Flags |= VF_Emissive;
					const FColor Tint = (Bx.Color != FColor::White) ? Bx.Color : BlockTint;
					const float Sky4[4] = { FS, FS, FS, FS }, Blk4[4] = { FB, FB, FB, FB };
					Quad(Layer, P, UV, FVector3f((float)D.X, (float)D.Y, (float)D.Z), Tex, Flags, Sky4, Blk4, AO1, Tint);
				}
			}

			for (const FMCModelQuad& Q : M->Quads)
			{
				const int32 Layer = Q.Layer != 255 ? Q.Layer : (BlockLayer == (int32)EMCLayer::Opaque ? (int32)EMCLayer::Cutout : BlockLayer);
				float FS = Sk, FB = Bl;
				if (Q.LightFace >= 0)
				{
					const FIntVector& D = MC::FaceDir[Q.LightFace];
					LightAt(X + D.X, Y + D.Y, Z + D.Z, FS, FB);
				}
				else
				{
					// thin decals: take the brightest neighbour so they are not black inside solid corners
					for (int32 F = 0; F < 6; ++F)
					{
						const FIntVector& D = MC::FaceDir[F];
						if (FMCBlocks::IsOpaque(In.Get(X + D.X, Y + D.Y, Z + D.Z))) continue;
						float NS, NB; LightAt(X + D.X, Y + D.Y, Z + D.Z, NS, NB);
						FS = FMath::Max(FS, NS - 1.f); FB = FMath::Max(FB, NB - 1.f);
					}
				}
				FVector3f P[4];
				for (int32 c = 0; c < 4; ++c) P[c] = Q.P[c] + FVector3f((float)X, (float)Y, (float)Z);
				FVector3f N = FVector3f::CrossProduct(P[1] - P[0], P[3] - P[0]).GetSafeNormal();
				N = -N;
				uint32 Flags = 0;
				if (Q.bTint || B.Tint != EMCTint::None) Flags |= VF_Tint;
				if (Q.bEmissive || SI.Light > 0) Flags |= VF_Emissive;
				if (Q.bWave) Flags |= VF_Wave | VF_Foliage;
				const FColor Tint = Q.Color != FColor::White ? Q.Color : BlockTint;
				const float Sky4[4] = { FS, FS, FS, FS }, Blk4[4] = { FB, FB, FB, FB };
				Quad(Layer, P, Q.UV, N, Q.Tex, Flags, Sky4, Blk4, AO1, Tint);
				if (Q.bDoubleSided && Layer != (int32)EMCLayer::Cutout)
				{
					const FVector3f PB[4] = { P[3], P[2], P[1], P[0] };
					const FVector2f UVB[4] = { Q.UV[3], Q.UV[2], Q.UV[1], Q.UV[0] };
					Quad(Layer, PB, UVB, -N, Q.Tex, Flags, Sky4, Blk4, AO1, Tint);
				}
			}
		}
	};
}

TUniquePtr<FMCChunkMeshData> FMCMesher::Build(const FMCMeshInput& In)
{
	TUniquePtr<FMCChunkMeshData> Out = MakeUnique<FMCChunkMeshData>();
	FBuilder Bd(In, *Out);
	Out->Layers[0].Vertices.Reserve(8192);
	Out->Layers[0].Indices.Reserve(12288);

	const bool bHasAuthored = GMCAuthoredMeshBlocks.Num() > 0;
	for (int32 Z = 0; Z < GroupHeight; ++Z)
	{
		for (int32 Y = 0; Y < 16; ++Y)
		{
			for (int32 X = 0; X < 16; ++X)
			{
				const FMCState S = In.Get(X, Y, Z);
				if (S == 0) continue;
				const FMCStateInfo& SI = FMCBlocks::Info(S);
				if (bHasAuthored && GMCAuthoredMeshBlocks.IsValidIndex(SI.Block) && GMCAuthoredMeshBlocks[SI.Block]) continue;
				const FMCBlock& B = FMCBlocks::Get(SI.Block);
				switch (SI.Shape)
				{
				case EMCShape::Cube: Bd.Cube(X, Y, Z, S, SI, B); break;
				case EMCShape::Cross: Bd.Plant(X, Y, Z, S, SI, B, false); break;
				case EMCShape::Crop: Bd.Plant(X, Y, Z, S, SI, B, true); break;
				case EMCShape::Liquid: Bd.Fluid(X, Y, Z, S, SI); break;
				case EMCShape::Model: Bd.Model(X, Y, Z, S, SI, B); break;
				default: break;
				}
				// waterlogged plants also render the surrounding water
				if ((SI.Flags & MCB_Waterlogged) && SI.Shape != EMCShape::Liquid)
				{
					Bd.Fluid(X, Y, Z, FMCBlocks::C.Water, FMCBlocks::Info(FMCBlocks::C.Water));
				}
			}
		}
	}
	return Out;
}
