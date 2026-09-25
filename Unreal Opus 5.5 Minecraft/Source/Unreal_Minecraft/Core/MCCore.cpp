#include "Core/MCCore.h"

DEFINE_LOG_CATEGORY(LogOpus55);

namespace MC
{
	const FIntVector FaceDir[6] = {
		FIntVector(0, 0, -1), // Down
		FIntVector(0, 0, 1),  // Up
		FIntVector(0, -1, 0), // North
		FIntVector(0, 1, 0),  // South
		FIntVector(-1, 0, 0), // West
		FIntVector(1, 0, 0),  // East
	};

	EMCFace FaceFromYaw(float YawDegrees)
	{
		// UE yaw: 0 = +X (east), 90 = +Y (south), 180 = -X (west), -90 = -Y (north)
		float Y = FMath::Fmod(YawDegrees, 360.f);
		if (Y < 0) Y += 360.f;
		if (Y >= 315.f || Y < 45.f) return EMCFace::East;
		if (Y < 135.f) return EMCFace::South;
		if (Y < 225.f) return EMCFace::West;
		return EMCFace::North;
	}

	float YawFromFace(EMCFace Face)
	{
		switch (Face)
		{
		case EMCFace::East: return 0.f;
		case EMCFace::South: return 90.f;
		case EMCFace::West: return 180.f;
		case EMCFace::North: return 270.f;
		default: return 0.f;
		}
	}

	EMCFace RotateY(EMCFace F, int32 Q)
	{
		if (!IsHorizontal(F)) return F;
		// clockwise seen from above (Z up, looking down): North -> East -> South -> West
		static const EMCFace Order[4] = { EMCFace::North, EMCFace::East, EMCFace::South, EMCFace::West };
		int32 Idx = 0;
		for (int32 i = 0; i < 4; ++i) if (Order[i] == F) Idx = i;
		return Order[((Idx + Q) % 4 + 4) % 4];
	}

	const TCHAR* FaceName(EMCFace F)
	{
		switch (F)
		{
		case EMCFace::Down: return TEXT("down");
		case EMCFace::Up: return TEXT("up");
		case EMCFace::North: return TEXT("north");
		case EMCFace::South: return TEXT("south");
		case EMCFace::West: return TEXT("west");
		case EMCFace::East: return TEXT("east");
		default: return TEXT("?");
		}
	}
}

FMCBox FMCBox::RotateY(int32 Quarter) const
{
	Quarter = ((Quarter % 4) + 4) % 4;
	FMCBox B = *this;
	for (int32 i = 0; i < Quarter; ++i)
	{
		// clockwise from above in a left-handed Z-up frame where North=-Y, East=+X:
		// North(-Y) -> East(+X): (x,y) -> (1-y, x)
		const FVector A(1.0 - B.Min.Y, B.Min.X, B.Min.Z);
		const FVector C(1.0 - B.Max.Y, B.Max.X, B.Max.Z);
		B.Min = FVector(FMath::Min(A.X, C.X), FMath::Min(A.Y, C.Y), A.Z);
		B.Max = FVector(FMath::Max(A.X, C.X), FMath::Max(A.Y, C.Y), C.Z);
	}
	return B;
}

double FMCBox::ClipX(const FMCBox& O, double D) const
{
	if (O.Max.Y <= Min.Y || O.Min.Y >= Max.Y || O.Max.Z <= Min.Z || O.Min.Z >= Max.Z) return D;
	if (D > 0 && O.Max.X <= Min.X) { const double Max_ = Min.X - O.Max.X; if (Max_ < D) D = Max_; }
	else if (D < 0 && O.Min.X >= Max.X) { const double Max_ = Max.X - O.Min.X; if (Max_ > D) D = Max_; }
	return D;
}

double FMCBox::ClipY(const FMCBox& O, double D) const
{
	if (O.Max.X <= Min.X || O.Min.X >= Max.X || O.Max.Z <= Min.Z || O.Min.Z >= Max.Z) return D;
	if (D > 0 && O.Max.Y <= Min.Y) { const double Max_ = Min.Y - O.Max.Y; if (Max_ < D) D = Max_; }
	else if (D < 0 && O.Min.Y >= Max.Y) { const double Max_ = Max.Y - O.Min.Y; if (Max_ > D) D = Max_; }
	return D;
}

double FMCBox::ClipZ(const FMCBox& O, double D) const
{
	if (O.Max.X <= Min.X || O.Min.X >= Max.X || O.Max.Y <= Min.Y || O.Min.Y >= Max.Y) return D;
	if (D > 0 && O.Max.Z <= Min.Z) { const double Max_ = Min.Z - O.Max.Z; if (Max_ < D) D = Max_; }
	else if (D < 0 && O.Min.Z >= Max.Z) { const double Max_ = Max.Z - O.Min.Z; if (Max_ > D) D = Max_; }
	return D;
}

bool FMCBox::RayHit(const FVector& Origin, const FVector& Dir, double MaxDist, double& OutT, EMCFace& OutFace) const
{
	double TMin = 0.0, TMax = MaxDist;
	EMCFace Face = EMCFace::Up;
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		const double O = Origin[Axis], D = Dir[Axis];
		const double BMin = Min[Axis], BMax = Max[Axis];
		if (FMath::Abs(D) < 1e-12)
		{
			if (O < BMin || O > BMax) return false;
			continue;
		}
		const double Inv = 1.0 / D;
		double T0 = (BMin - O) * Inv, T1 = (BMax - O) * Inv;
		EMCFace F0, F1;
		if (Axis == 0) { F0 = EMCFace::West; F1 = EMCFace::East; }
		else if (Axis == 1) { F0 = EMCFace::North; F1 = EMCFace::South; }
		else { F0 = EMCFace::Down; F1 = EMCFace::Up; }
		if (T0 > T1) { Swap(T0, T1); Swap(F0, F1); }
		if (T0 > TMin) { TMin = T0; Face = F0; }
		if (T1 < TMax) TMax = T1;
		if (TMin > TMax) return false;
	}
	OutT = TMin;
	OutFace = Face;
	return true;
}
