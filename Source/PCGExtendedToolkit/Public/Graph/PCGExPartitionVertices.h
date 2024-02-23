﻿// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCluster.h"
#include "PCGExEdgesProcessor.h"

#include "PCGExPartitionVertices.generated.h"

/**
 * A Base node to process a set of point using GraphParams.
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph")
class PCGEXTENDEDTOOLKIT_API UPCGExPartitionVerticesSettings : public UPCGExEdgesProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(PartitionVertices, "Graph : Partition Vertices", "Split Vtx into per-cluster groups.");
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEx::NodeColorEdge; }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	//~Begin UPCGExEdgesProcessorSettings interface
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	virtual PCGExData::EInit GetEdgeOutputInitMode() const override;
	//~End UPCGExEdgesProcessorSettings interface
};

struct PCGEXTENDEDTOOLKIT_API FPCGExPartitionVerticesContext : public FPCGExEdgesProcessorContext
{
	friend class UPCGExPartitionVerticesSettings;
	friend class FPCGExPartitionVerticesElement;

	virtual ~FPCGExPartitionVerticesContext() override;

	PCGExData::FPointIOCollection* VtxPartitions = nullptr;
	TArray<PCGExGraph::FIndexedEdge> IndexedEdges;
};

class PCGEXTENDEDTOOLKIT_API FPCGExPartitionVerticesElement : public FPCGExEdgesProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};

class PCGEXTENDEDTOOLKIT_API FPCGExCreateVtxPartitionTask : public FPCGExNonAbandonableTask
{
public:
	FPCGExCreateVtxPartitionTask(FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO,
	                             PCGExData::FPointIO* InEdgeIO,
	                             TMap<int32, int32>* InNodeIndicesMap) :
		FPCGExNonAbandonableTask(InManager, InTaskIndex, InPointIO),
		EdgeIO(InEdgeIO),
		NodeIndicesMap(InNodeIndicesMap)
	{
	}

	PCGExData::FPointIO* EdgeIO = nullptr;
	TMap<int32, int32>* NodeIndicesMap = nullptr;

	virtual bool ExecuteTask() override;
};
