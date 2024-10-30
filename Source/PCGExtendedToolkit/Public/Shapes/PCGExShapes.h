﻿// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "AssetStaging/PCGExFitting.h"
#include "Data/PCGExData.h"
#include "PCGExShapes.generated.h"

UENUM(/*E--BlueprintType, meta=(DisplayName="[PCGEx] Shape Output Mode")--E*/)
enum class EPCGExShapeOutputMode : uint8
{
	PerDataset = 0 UMETA(DisplayName = "Per Dataset", ToolTip="Merge all shapes into the original dataset"),
	PerSeed    = 1 UMETA(DisplayName = "Per Seed", ToolTip="Create a new output per shape seed point"),
};


UENUM(/*E--BlueprintType, meta=(DisplayName="[PCGEx] Shape Output Mode")--E*/)
enum class EPCGExShapeResolutionMode : uint8
{
	Absolute = 0 UMETA(DisplayName = "Absolute", ToolTip="Resolution is absolute."),
	Scaled   = 1 UMETA(DisplayName = "Scaled", ToolTip="Resolution is scaled by the seed' scale"),
};

UENUM(/*E--BlueprintType, meta=(DisplayName="[PCGEx] Shape Output Mode")--E*/)
enum class EPCGExShapePointLookAt : uint8
{
	None = 0 UMETA(DisplayName = "None", ToolTip="Point look at will be as per 'canon' shape definition"),
	Seed = 1 UMETA(DisplayName = "Seed", ToolTip="Look At Seed"),
};

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExShapeConfigBase
{
	GENERATED_BODY()

	FPCGExShapeConfigBase()
	{
	}

	virtual ~FPCGExShapeConfigBase()
	{
	}

	/** Shape resolution, or "point per meter" -- how it's used depends on shape implementation. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayPriority=-1, ClampMin=0))
	double Resolution = 1;

	/** Axis on the source to remap to a target axis on the shape */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Align", meta=(PCG_Overridable))
	EPCGExAxisAlign SourceAxis = EPCGExAxisAlign::Forward;

	/** Shape axis to align to the source axis */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Align", meta=(PCG_Overridable))
	EPCGExAxisAlign TargetAxis = EPCGExAxisAlign::Forward;

	/** Points look at */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Align", meta=(PCG_Overridable))
	EPCGExShapePointLookAt PointsLookAt = EPCGExShapePointLookAt::None;

	/** Axis used to align the look at rotation */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Align", meta=(PCG_Overridable))
	EPCGExAxisAlign LookAtAxis = EPCGExAxisAlign::Forward;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FPCGExScaleToFitDetails ScaleToFit;

	/** Axis used to align the look at rotation */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FVector DefaultExtents = FVector::OneVector * 0.5;
	
	FTransform LocalTransform = FTransform::Identity;

	virtual void Init()
	{
		Resolution = Resolution * 0.01; // Per meter

		FQuat A = FQuat::Identity;
		FQuat B = FQuat::Identity;
		switch (SourceAxis)
		{
		case EPCGExAxisAlign::Forward:
			A = FRotationMatrix::MakeFromX(FVector::ForwardVector).ToQuat().Inverse();
			break;
		case EPCGExAxisAlign::Backward:
			A = FRotationMatrix::MakeFromX(FVector::BackwardVector).ToQuat().Inverse();
			break;
		case EPCGExAxisAlign::Right:
			A = FRotationMatrix::MakeFromX(FVector::RightVector).ToQuat().Inverse();
			break;
		case EPCGExAxisAlign::Left:
			A = FRotationMatrix::MakeFromX(FVector::LeftVector).ToQuat().Inverse();
			break;
		case EPCGExAxisAlign::Up:
			A = FRotationMatrix::MakeFromX(FVector::UpVector).ToQuat().Inverse();
			break;
		case EPCGExAxisAlign::Down:
			A = FRotationMatrix::MakeFromX(FVector::DownVector).ToQuat().Inverse();
			break;
		}

		switch (TargetAxis)
		{
		case EPCGExAxisAlign::Forward:
			B = FRotationMatrix::MakeFromX(FVector::ForwardVector).ToQuat();
			break;
		case EPCGExAxisAlign::Backward:
			B = FRotationMatrix::MakeFromX(FVector::BackwardVector).ToQuat();
			break;
		case EPCGExAxisAlign::Right:
			B = FRotationMatrix::MakeFromX(FVector::RightVector).ToQuat();
			break;
		case EPCGExAxisAlign::Left:
			B = FRotationMatrix::MakeFromX(FVector::LeftVector).ToQuat();
			break;
		case EPCGExAxisAlign::Up:
			B = FRotationMatrix::MakeFromX(FVector::UpVector).ToQuat();
			break;
		case EPCGExAxisAlign::Down:
			B = FRotationMatrix::MakeFromX(FVector::DownVector).ToQuat();
			break;
		}

		LocalTransform = FTransform(A * B, FVector::ZeroVector, FVector::OneVector);
	}
};

namespace PCGExShapes
{
	const FName OutputShapeBuilderLabel = TEXT("Shape Builder");
	const FName SourceShapeBuildersLabel = TEXT("Shape Builders");

	class FShape : public TSharedFromThis<FShape>
	{
	public:
		virtual ~FShape() = default;
		PCGExData::FPointRef Seed;
		int32 StartIndex = 0;
		int32 NumPoints = 0;
		FBox Fit = FBox(ForceInitToZero);
		FVector Extents = FVector::OneVector * 0.5;

		bool IsValid() const { return Fit.IsValid && NumPoints > 0; }

		explicit FShape(const PCGExData::FPointRef& InPointRef)
			: Seed(InPointRef)
		{
		}

		virtual void ComputeFit(const FPCGExShapeConfigBase& Config)
		{
			FVector OutScale = Seed.Point->Transform.GetScale3D();
			const FBox InBounds = Fit = FBox(FVector::OneVector * -0.5, FVector::OneVector * 0.5);
			Config.ScaleToFit.Process(*Seed.Point, InBounds, OutScale, Fit);

			Fit.Min *= OutScale;
			Fit.Max *= OutScale;
			Fit = Fit.TransformBy(Config.LocalTransform);
		}
	};
}
