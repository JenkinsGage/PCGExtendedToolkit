﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.h"
#include "PCGExPaths.h"
#include "PCGExPointsProcessor.h"

#include "PCGExPathProcessor.generated.h"

#define PCGEX_SKIP_INVALID_PATH_ENTRY \
if (Entry->GetNum() < 2){ \
if (!Settings->bOmitInvalidPathsOutputs) { Entry->InitializeOutput(PCGExData::EIOInit::Forward); } \
bHasInvalidInputs = true; return false; }

class UPCGExFilterFactoryData;

class UPCGExNodeStateFactory;

namespace PCGExCluster
{
	class FNodeStateHandler;
}

/**
 * 
 */
UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Path")
class PCGEXTENDEDTOOLKIT_API UPCGExPathProcessorSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	UPCGExPathProcessorSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings
#if WITH_EDITOR
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorPath; }
#endif
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
	virtual FName GetMainInputPin() const override { return PCGExPaths::SourcePathsLabel; }
	virtual FName GetMainOutputPin() const override { return PCGExPaths::OutputPathsLabel; }
	virtual FString GetPointFilterTooltip() const override { return TEXT("Path points processing filters"); }

	//~End UPCGExPointsProcessorSettings

	UPROPERTY()
	bool bSupportClosedLoops = true;

	/** If enabled, collections that have less than 2 points won't be processed and be omitted from the output. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable), AdvancedDisplay)
	bool bOmitInvalidPathsOutputs = true;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExPathProcessorContext : FPCGExPointsProcessorContext
{
	friend class FPCGExPathProcessorElement;
	TSharedPtr<PCGExData::FPointIOCollection> MainPaths;
};

class PCGEXTENDEDTOOLKIT_API FPCGExPathProcessorElement : public FPCGExPointsProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(PathProcessor)
	virtual bool Boot(FPCGExContext* InContext) const override;
};
