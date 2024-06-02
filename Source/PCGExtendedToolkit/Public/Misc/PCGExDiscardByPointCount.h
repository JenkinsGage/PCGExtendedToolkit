﻿// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExPointsProcessor.h"

#include "PCGExDiscardByPointCount.generated.h"

/**
 * Calculates the distance between two points (inherently a n*n operation)
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class PCGEXTENDEDTOOLKIT_API UPCGExDiscardByPointCountSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(DiscardByPointCount, "Discard By Point Count", "Filter outputs by point count.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExEditorSettings>()->NodeColorMiscRemove; }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	//~Begin UPCGExPointsProcessorSettings interface
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings interface

public:
	/** Don't output Clusters if they have less points than a specified amount. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bRemoveBelow = false;

	/** Discarded if point count is less than */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bRemoveBelow", ClampMin=1))
	int32 MinPointCount = 1;

	/** Don't output Clusters if they have more points than a specified amount. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bRemoveAbove = false;

	/** Discarded if point count is more than */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bRemoveAbove", ClampMin=-1))
	int32 MaxPointCount = 500;

	bool OutsidePointCountFilter(const int32 InValue) const { return (MinPointCount > 0 && InValue < MinPointCount) || (MaxPointCount > 0 && InValue < MaxPointCount); }
};

class PCGEXTENDEDTOOLKIT_API FPCGExDiscardByPointCountElement : public FPCGExPointsProcessorElementBase
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
