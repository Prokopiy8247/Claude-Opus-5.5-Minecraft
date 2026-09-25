// Rotatable building helper for structure pieces.
#pragma once

#include "CoreMinimal.h"
#include "Gen/MCGenCommon.h"

/**
 * Local structure coordinates: x = right, y = forward (depth), z = up. Rot = quarter turns clockwise.
 * A piece authored with its front (door) at y = 0 facing "north" (towards -y) is rotated to face any direction.
 */
struct UNREAL_MINECRAFT_API FMCPieceBuilder
{
	FMCGenWriter& W;
	FMCRandom& R;
	FIntVector Origin;
	int32 Rot = 0;

	FMCPieceBuilder(FMCGenWriter& InW, FMCRandom& InR, const FIntVector& InOrigin, int32 InRot) : W(InW), R(InR), Origin(InOrigin), Rot(((InRot % 4) + 4) % 4) {}

	FIntVector ToWorld(int32 X, int32 Y, int32 Z) const;
	EMCFace RotFace(EMCFace F) const { return MC::RotateY(F, Rot); }
	/** Rotate facing/axis meta of a state authored for Rot = 0. */
	FMCState RotState(FMCState S) const;

	void Set(int32 X, int32 Y, int32 Z, FMCState S);
	void SetRaw(int32 X, int32 Y, int32 Z, FMCState S);   // no rotation of the state
	FMCState Get(int32 X, int32 Y, int32 Z) const;
	void Fill(int32 X0, int32 Y0, int32 Z0, int32 X1, int32 Y1, int32 Z1, FMCState S);
	void FillAir(int32 X0, int32 Y0, int32 Z0, int32 X1, int32 Y1, int32 Z1) { Fill(X0, Y0, Z0, X1, Y1, Z1, 0); }
	/** Hollow room: walls/floor/ceiling of S, interior air. */
	void Room(int32 X0, int32 Y0, int32 Z0, int32 X1, int32 Y1, int32 Z1, FMCState Wall, FMCState Floor = 0, FMCState Ceil = 0);
	/** Fill downwards from Z until solid ground (foundations on slopes). */
	void Foundation(int32 X0, int32 Y0, int32 X1, int32 Y1, int32 Z, FMCState S, int32 MaxDepth = 12);
	/** Randomly swap a block type for variants (cracked / mossy bricks). */
	void SetVariant(int32 X, int32 Y, int32 Z, FMCState A, FMCState B, float ChanceB, FMCState C = 0, float ChanceC = 0.f);
	void Chest(int32 X, int32 Y, int32 Z, EMCFace Facing, FName Loot, FName Block = TEXT("chest"));
	void Spawner(int32 X, int32 Y, int32 Z, FName Mob);
	void Mob(FName Mob, float X, float Y, float Z, int32 Variant = -1, FName Extra = NAME_None);
	/** Door (lower + upper half) facing F (authored). */
	void Door(int32 X, int32 Y, int32 Z, const TCHAR* DoorBlock, EMCFace Facing, bool bRightHinge = false);
	void Bed(int32 X, int32 Y, int32 Z, const TCHAR* BedBlock, EMCFace Facing);
	/** Stairs block with authored facing and optional upside-down. */
	void Stair(int32 X, int32 Y, int32 Z, FMCState Stairs, EMCFace Facing, bool bTop = false);
	void Torch(int32 X, int32 Y, int32 Z, EMCFace WallFacing = EMCFace::Up);
	/** Gable roof over [X0..X1] x [Y0..Y1] starting at Z, ridge along Y. */
	void GableRoof(int32 X0, int32 Y0, int32 X1, int32 Y1, int32 Z, FMCState Stairs, FMCState Fill);
};
