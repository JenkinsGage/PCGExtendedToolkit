﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExEdgeRefineOperation.h"
#include "Graph/PCGExCluster.h"
#include "PCGExEdgeRefineGabriel.generated.h"

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, meta=(DisplayName="Refine : Gabriel", PCGExNodeLibraryDoc="clusters/refine-cluster/gabriel"))
class UPCGExEdgeRefineGabriel : public UPCGExEdgeRefineOperation
{
	GENERATED_BODY()

public:
	virtual bool GetDefaultEdgeValidity() override { return !bInvert; }
	virtual bool WantsNodeOctree() override { return true; }

	virtual void CopySettingsFrom(const UPCGExInstancedFactory* Other) override
	{
		Super::CopySettingsFrom(Other);
		if (const UPCGExEdgeRefineGabriel* TypedOther = Cast<UPCGExEdgeRefineGabriel>(Other))
		{
			bInvert = TypedOther->bInvert;
		}
	}

	virtual bool WantsIndividualEdgeProcessing() override { return !bInvert; }

	virtual void PrepareForCluster(const TSharedPtr<PCGExCluster::FCluster>& InCluster, const TSharedPtr<PCGExHeuristics::FHeuristicsHandler>& InHeuristics) override
	{
		Super::PrepareForCluster(InCluster, InHeuristics);
		ExchangeValue = bInvert ? 1 : 0;
	}

	virtual void ProcessEdge(PCGExGraph::FEdge& Edge) override
	{
		Super::ProcessEdge(Edge);

		const FVector From = Cluster->GetStartPos(Edge);
		const FVector To = Cluster->GetEndPos(Edge);

		const FVector Center = FMath::Lerp(From, To, 0.5);
		const double SqrDist = FVector::DistSquared(Center, From);

		Cluster->NodeOctree->FindFirstElementWithBoundsTest(
			FBoxCenterAndExtent(Center, FVector(FMath::Sqrt(SqrDist))), [&](const PCGEx::FIndexedItem& Item)
			{
				if (FVector::DistSquared(Center, Cluster->GetPos(Item.Index)) < SqrDist)
				{
					FPlatformAtomics::InterlockedExchange(&Edge.bValid, ExchangeValue);
					return false;
				}
				return true;
			});
	}

	int8 ExchangeValue = 0;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bInvert = false;
};
