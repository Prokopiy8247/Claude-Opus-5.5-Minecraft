// Opus 5.5 Minecraft Unreal - core voxel types and constants.
//
// Coordinate convention
// ---------------------
// Internally everything uses Unreal's Z-up convention. A block position is
// (X, Y, Z) with Z vertical. One block = 100 Unreal units (1 m).
// Minecraft-style display coordinates (x, y=height, z) are produced only for
// UI / commands:  mcX = X, mcY = Z, mcZ = Y.
// Directions: North = -Y, South = +Y, West = -X, East = +X, Up = +Z, Down = -Z.

#pragma once

#include "CoreMinimal.h"

DECLARE_LOG_CATEGORY_EXTERN(LogOpus55, Log, All);

namespace MC
{
	constexpr int32 ChunkSize = 16;
	constexpr int32 ChunkShift = 4;
	constexpr int32 ChunkMask = 15;
	constexpr int32 SectionVolume = 16 * 16 * 16;

	constexpr int32 MinZ = -64;              // Minecraft Y -64
	constexpr int32 MaxZ = 319;              // Minecraft Y 319 (top block)
	constexpr int32 WorldHeight = MaxZ - MinZ + 1; // 384
	constexpr int32 NumSections = WorldHeight / 16; // 24
	constexpr int32 SeaLevel = 63;

	constexpr double BlockSize = 100.0;
	constexpr float BlockSizeF = 100.0f;
	constexpr double InvBlockSize = 1.0 / 100.0;

	constexpr int32 TicksPerSecond = 20;
	constexpr double TickTime = 1.0 / 20.0;

	FORCEINLINE int32 FloorDiv16(int32 V) { return V >> 4; }
	FORCEINLINE int32 Mod16(int32 V) { return V & 15; }
	FORCEINLINE int32 FloorToInt(double V) { return (int32)FMath::FloorToDouble(V); }

	/** Index inside a 16x16x16 section: x + y*16 + z*256 (z vertical). */
	FORCEINLINE int32 SectionIndex(int32 LX, int32 LY, int32 LZ) { return LX | (LY << 4) | (LZ << 8); }
	/** Index of a column cell (x,y) in a 16x16 plane. */
	FORCEINLINE int32 ColumnIndex(int32 LX, int32 LY) { return LX | (LY << 4); }
}

enum class EMCDimension : uint8
{
	Overworld = 0,
	Nether = 1,
	End = 2,
	Count = 3
};

enum class EMCGameMode : uint8
{
	Survival = 0,
	Creative = 1,
	Adventure = 2,
	Spectator = 3
};

enum class EMCDifficulty : uint8
{
	Peaceful = 0,
	Easy = 1,
	Normal = 2,
	Hard = 3
};

/** Six block faces. Order is important (used for texture arrays per face). */
enum class EMCFace : uint8
{
	Down = 0,   // -Z
	Up = 1,     // +Z
	North = 2,  // -Y
	South = 3,  // +Y
	West = 4,   // -X
	East = 5,   // +X
	Count = 6
};

namespace MC
{
	extern const FIntVector FaceDir[6];
	FORCEINLINE EMCFace Opposite(EMCFace F) { return (EMCFace)((uint8)F ^ 1); }
	FORCEINLINE bool IsHorizontal(EMCFace F) { return (uint8)F >= 2; }
	/** Horizontal facing index 0..3 = North, South, West, East (same as EMCFace-2). */
	FORCEINLINE EMCFace HorizontalFace(int32 H) { return (EMCFace)(2 + (H & 3)); }
	EMCFace FaceFromYaw(float YawDegrees);                  // facing the player looks towards
	float YawFromFace(EMCFace Face);                         // yaw (deg) looking towards face
	EMCFace RotateY(EMCFace F, int32 QuarterTurnsClockwise); // rotate a horizontal face
	const TCHAR* FaceName(EMCFace F);
}

/** Integer block position. */
struct FMCBlockPos
{
	int32 X = 0, Y = 0, Z = 0;

	FMCBlockPos() = default;
	FMCBlockPos(int32 InX, int32 InY, int32 InZ) : X(InX), Y(InY), Z(InZ) {}
	explicit FMCBlockPos(const FIntVector& V) : X(V.X), Y(V.Y), Z(V.Z) {}

	FORCEINLINE FMCBlockPos operator+(const FMCBlockPos& O) const { return FMCBlockPos(X + O.X, Y + O.Y, Z + O.Z); }
	FORCEINLINE FMCBlockPos operator-(const FMCBlockPos& O) const { return FMCBlockPos(X - O.X, Y - O.Y, Z - O.Z); }
	FORCEINLINE FMCBlockPos operator+(const FIntVector& O) const { return FMCBlockPos(X + O.X, Y + O.Y, Z + O.Z); }
	FORCEINLINE bool operator==(const FMCBlockPos& O) const { return X == O.X && Y == O.Y && Z == O.Z; }
	FORCEINLINE bool operator!=(const FMCBlockPos& O) const { return !(*this == O); }

	FORCEINLINE FMCBlockPos Offset(EMCFace F, int32 N = 1) const
	{
		const FIntVector& D = MC::FaceDir[(int32)F];
		return FMCBlockPos(X + D.X * N, Y + D.Y * N, Z + D.Z * N);
	}
	FORCEINLINE FMCBlockPos Up(int32 N = 1) const { return FMCBlockPos(X, Y, Z + N); }
	FORCEINLINE FMCBlockPos Down(int32 N = 1) const { return FMCBlockPos(X, Y, Z - N); }

	FORCEINLINE int32 ChunkX() const { return X >> 4; }
	FORCEINLINE int32 ChunkY() const { return Y >> 4; }

	/** World-space (UU) position of the block's minimum corner. */
	FORCEINLINE FVector ToWorld() const { return FVector(X * MC::BlockSize, Y * MC::BlockSize, Z * MC::BlockSize); }
	/** World-space (UU) position of the block center. */
	FORCEINLINE FVector Center() const { return FVector((X + 0.5) * MC::BlockSize, (Y + 0.5) * MC::BlockSize, (Z + 0.5) * MC::BlockSize); }
	FORCEINLINE FVector BottomCenter() const { return FVector((X + 0.5) * MC::BlockSize, (Y + 0.5) * MC::BlockSize, Z * MC::BlockSize); }

	static FORCEINLINE FMCBlockPos FromWorld(const FVector& W)
	{
		return FMCBlockPos(MC::FloorToInt(W.X * MC::InvBlockSize), MC::FloorToInt(W.Y * MC::InvBlockSize), MC::FloorToInt(W.Z * MC::InvBlockSize));
	}

	FORCEINLINE int64 DistSq(const FMCBlockPos& O) const
	{
		const int64 DX = X - O.X, DY = Y - O.Y, DZ = Z - O.Z;
		return DX * DX + DY * DY + DZ * DZ;
	}

	FString ToString() const { return FString::Printf(TEXT("%d %d %d"), X, Z, Y); } // Minecraft order (x y z)

	friend FORCEINLINE uint32 GetTypeHash(const FMCBlockPos& P)
	{
		uint32 H = (uint32)P.X * 73856093u;
		H ^= (uint32)P.Y * 19349663u;
		H ^= (uint32)P.Z * 83492791u;
		return H;
	}

	friend FArchive& operator<<(FArchive& Ar, FMCBlockPos& P)
	{
		Ar << P.X << P.Y << P.Z;
		return Ar;
	}
};

/** Chunk column coordinate (X/Y plane). */
struct FMCChunkPos
{
	int32 X = 0, Y = 0;
	FMCChunkPos() = default;
	FMCChunkPos(int32 InX, int32 InY) : X(InX), Y(InY) {}
	FORCEINLINE bool operator==(const FMCChunkPos& O) const { return X == O.X && Y == O.Y; }
	FORCEINLINE bool operator!=(const FMCChunkPos& O) const { return !(*this == O); }
	FORCEINLINE FMCChunkPos operator+(const FMCChunkPos& O) const { return FMCChunkPos(X + O.X, Y + O.Y); }
	static FORCEINLINE FMCChunkPos FromBlock(int32 BX, int32 BY) { return FMCChunkPos(BX >> 4, BY >> 4); }
	static FORCEINLINE FMCChunkPos FromBlock(const FMCBlockPos& P) { return FMCChunkPos(P.X >> 4, P.Y >> 4); }
	FORCEINLINE int32 MinBlockX() const { return X << 4; }
	FORCEINLINE int32 MinBlockY() const { return Y << 4; }
	FORCEINLINE int64 Key() const { return ((int64)X << 32) | (uint32)Y; }
	friend FORCEINLINE uint32 GetTypeHash(const FMCChunkPos& P) { return HashCombineFast((uint32)P.X * 0x9E3779B1u, (uint32)P.Y * 0x85EBCA77u); }
	friend FArchive& operator<<(FArchive& Ar, FMCChunkPos& P) { Ar << P.X << P.Y; return Ar; }
	FString ToString() const { return FString::Printf(TEXT("%d %d"), X, Y); }
};

/** Axis aligned box in block units (1.0 = one block). */
struct FMCBox
{
	FVector Min = FVector::ZeroVector;
	FVector Max = FVector::ZeroVector;

	FMCBox() = default;
	FMCBox(const FVector& InMin, const FVector& InMax) : Min(InMin), Max(InMax) {}
	FMCBox(double X0, double Y0, double Z0, double X1, double Y1, double Z1) : Min(X0, Y0, Z0), Max(X1, Y1, Z1) {}

	/** Box from Minecraft-style 0..16 pixel coordinates (x, y=height, z) -> converted to Z-up block units. */
	static FMCBox Px(double X0, double H0, double Z0, double X1, double H1, double Z1)
	{
		return FMCBox(X0 / 16.0, Z0 / 16.0, H0 / 16.0, X1 / 16.0, Z1 / 16.0, H1 / 16.0);
	}

	FORCEINLINE FMCBox Offset(const FVector& D) const { return FMCBox(Min + D, Max + D); }
	FORCEINLINE FMCBox Offset(double X, double Y, double Z) const { return Offset(FVector(X, Y, Z)); }
	FORCEINLINE FMCBox Expand(const FVector& D) const
	{
		FMCBox B = *this;
		if (D.X < 0) B.Min.X += D.X; else B.Max.X += D.X;
		if (D.Y < 0) B.Min.Y += D.Y; else B.Max.Y += D.Y;
		if (D.Z < 0) B.Min.Z += D.Z; else B.Max.Z += D.Z;
		return B;
	}
	FORCEINLINE FMCBox Inflate(double V) const { return FMCBox(Min - FVector(V), Max + FVector(V)); }
	FORCEINLINE bool Intersects(const FMCBox& O) const
	{
		return Min.X < O.Max.X && Max.X > O.Min.X && Min.Y < O.Max.Y && Max.Y > O.Min.Y && Min.Z < O.Max.Z && Max.Z > O.Min.Z;
	}
	FORCEINLINE bool Contains(const FVector& P) const
	{
		return P.X >= Min.X && P.X <= Max.X && P.Y >= Min.Y && P.Y <= Max.Y && P.Z >= Min.Z && P.Z <= Max.Z;
	}
	FORCEINLINE FVector Center() const { return (Min + Max) * 0.5; }
	FORCEINLINE FVector Size() const { return Max - Min; }

	/** Rotate around the block center (0.5,0.5) by quarter turns clockwise (seen from above). */
	FMCBox RotateY(int32 Quarter) const;

	/** Swept collision helpers (Minecraft style): clip movement along an axis. */
	double ClipX(const FMCBox& Other, double Delta) const;
	double ClipY(const FMCBox& Other, double Delta) const;
	double ClipZ(const FMCBox& Other, double Delta) const;

	/** Ray intersection. Returns true and entry distance/face. */
	bool RayHit(const FVector& Origin, const FVector& Dir, double MaxDist, double& OutT, EMCFace& OutFace) const;

	FBox ToWorldBox(const FVector& BlockOrigin) const
	{
		return FBox(BlockOrigin + Min * MC::BlockSize, BlockOrigin + Max * MC::BlockSize);
	}
};

/** Simple 64-bit mixing hash utilities for deterministic generation. */
namespace MCHash
{
	FORCEINLINE uint64 SplitMix64(uint64 X)
	{
		X += 0x9E3779B97F4A7C15ull;
		X = (X ^ (X >> 30)) * 0xBF58476D1CE4E5B9ull;
		X = (X ^ (X >> 27)) * 0x94D049BB133111EBull;
		return X ^ (X >> 31);
	}
	FORCEINLINE uint64 Hash3(uint64 Seed, int32 X, int32 Y, int32 Z)
	{
		uint64 H = SplitMix64(Seed ^ ((uint64)(uint32)X * 0x9E3779B97F4A7C15ull));
		H = SplitMix64(H ^ ((uint64)(uint32)Y * 0xC2B2AE3D27D4EB4Full));
		H = SplitMix64(H ^ ((uint64)(uint32)Z * 0x165667B19E3779F9ull));
		return H;
	}
	FORCEINLINE uint64 Hash2(uint64 Seed, int32 X, int32 Y) { return Hash3(Seed, X, Y, 0x5bd1e995); }
	FORCEINLINE double ToUnit(uint64 H) { return (double)(H >> 11) * (1.0 / 9007199254740992.0); }
	FORCEINLINE uint64 StringHash(const TCHAR* S)
	{
		uint64 H = 1469598103934665603ull;
		while (*S) { H ^= (uint64)*S++; H *= 1099511628211ull; }
		return H;
	}
}

/** Small fast deterministic PRNG (xoroshiro128++). */
struct FMCRandom
{
	uint64 S0 = 0x1234567ull, S1 = 0x89ABCDEFull;

	FMCRandom() = default;
	explicit FMCRandom(uint64 Seed) { SetSeed(Seed); }

	void SetSeed(uint64 Seed)
	{
		S0 = MCHash::SplitMix64(Seed);
		S1 = MCHash::SplitMix64(S0 ^ 0xD1B54A32D192ED03ull);
		if ((S0 | S1) == 0) S1 = 1;
	}
	FORCEINLINE static uint64 Rotl(uint64 X, int K) { return (X << K) | (X >> (64 - K)); }
	FORCEINLINE uint64 Next()
	{
		const uint64 A = S0; uint64 B = S1;
		const uint64 R = Rotl(A + B, 17) + A;
		B ^= A;
		S0 = Rotl(A, 49) ^ B ^ (B << 21);
		S1 = Rotl(B, 28);
		return R;
	}
	FORCEINLINE int32 NextInt(int32 Bound) { return Bound <= 0 ? 0 : (int32)((Next() >> 33) % (uint64)Bound); }
	FORCEINLINE int32 Range(int32 MinInclusive, int32 MaxInclusive) { return MinInclusive + NextInt(MaxInclusive - MinInclusive + 1); }
	FORCEINLINE double NextDouble() { return (double)(Next() >> 11) * (1.0 / 9007199254740992.0); }
	FORCEINLINE float NextFloat() { return (float)NextDouble(); }
	FORCEINLINE float FRange(float A, float B) { return A + (B - A) * NextFloat(); }
	FORCEINLINE bool Chance(double P) { return NextDouble() < P; }
	FORCEINLINE bool NextBool() { return (Next() >> 63) != 0; }
	double Gaussian()
	{
		double U1 = FMath::Max(1e-12, NextDouble()), U2 = NextDouble();
		return FMath::Sqrt(-2.0 * FMath::Loge(U1)) * FMath::Cos(2.0 * PI * U2);
	}
};

/** Convert a world (UU) position to Minecraft-style display coordinates. */
FORCEINLINE FVector MCDisplayCoords(const FVector& World)
{
	return FVector(World.X * MC::InvBlockSize, World.Z * MC::InvBlockSize, World.Y * MC::InvBlockSize);
}
