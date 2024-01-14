﻿// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "Data/PCGExAttributeHelpers.h"
#include "PCGExMT.h"
#include "PCGExEdge.h"
#include "Data/PCGExData.h"

struct FPCGExPointsProcessorContext;

namespace PCGExGraph
{
	const FName SourceParamsLabel = TEXT("Graph");
	const FName OutputParamsLabel = TEXT("➜");

	const FName SourceGraphsLabel = TEXT("In");
	const FName OutputGraphsLabel = TEXT("Out");

	const FName SourceVerticesLabel = TEXT("Vtx");
	const FName OutputVerticesLabel = TEXT("Vtx");

	const FName SourcePathsLabel = TEXT("Paths");
	const FName OutputPathsLabel = TEXT("Paths");

	constexpr PCGExMT::AsyncState State_ReadyForNextGraph = __COUNTER__;
	constexpr PCGExMT::AsyncState State_ProcessingGraph = __COUNTER__;

	constexpr PCGExMT::AsyncState State_CachingGraphIndices = __COUNTER__;
	constexpr PCGExMT::AsyncState State_SwappingGraphIndices = __COUNTER__;

	constexpr PCGExMT::AsyncState State_FindingEdgeTypes = __COUNTER__;

	constexpr PCGExMT::AsyncState State_BuildCustomGraph = __COUNTER__;
	constexpr PCGExMT::AsyncState State_FindingCrossings = __COUNTER__;
	constexpr PCGExMT::AsyncState State_WritingClusters = __COUNTER__;
	constexpr PCGExMT::AsyncState State_WaitingOnWritingClusters = __COUNTER__;

	constexpr PCGExMT::AsyncState State_PromotingEdges = __COUNTER__;

#pragma region Graph

	class FGraph;

	struct PCGEXTENDEDTOOLKIT_API FNode
	{
		FNode()
		{
		}

		bool bCrossing = false;
		bool bValid = false;

		int32 NodeIndex = -1;
		int32 PointIndex = -1;
		int32 NumExportedEdges = 0;

		TArray<int32> Edges;

		~FNode()
		{
			Edges.Empty();
		}

		void Add(const int32 EdgeIndex);
	};

	struct PCGEXTENDEDTOOLKIT_API FEdgeCrossing
	{
		FEdgeCrossing()
		{
		}

		int32 EdgeA = -1;
		int32 EdgeB = -1;
		FVector Center;
	};

	struct PCGEXTENDEDTOOLKIT_API FSubGraph
	{
		bool bConsolidated = false;
		int64 Id = -1;
		TSet<int32> Nodes;
		TSet<int32> Edges; //TODO : Test for TArray

		FSubGraph()
		{
		}

		~FSubGraph()
		{
			Nodes.Empty();
			Edges.Empty();
		}

		void Add(const FIndexedEdge& Edge, FGraph* InGraph);
		void Consolidate(FGraph* InGraph);
		void Invalidate(FGraph* InGraph);
	};

	class PCGEXTENDEDTOOLKIT_API FGraph
	{
		mutable FRWLock GraphLock;
		const int32 NumEdgesReserve;

	public:
		bool bRequiresConsolidation = false;

		TArray<FNode> Nodes;
		TArray<FIndexedEdge> Edges;

		TSet<uint64> UniqueEdges;

		TArray<FSubGraph*> SubGraphs;

		FGraph(const int32 InNumNodes, const int32 InNumEdgesReserve = 10)
			: NumEdgesReserve(InNumEdgesReserve)
		{
			Nodes.SetNum(InNumNodes);
			for (int i = 0; i < InNumNodes; i++)
			{
				FNode& Node = Nodes[i];
				Node.NodeIndex = Node.PointIndex = i;
				Node.bValid = false;
				Node.Edges.Reserve(NumEdgesReserve);
			}
		}

		bool InsertEdge(const int32 A, const int32 B, FIndexedEdge& OutEdge);
		void InsertEdges(const TArray<FUnsignedEdge>& InEdges);

		void BuildSubGraphs();
		void Consolidate(const bool bPrune, const int32 Min = 1, const int32 Max = TNumericLimits<int32>::Max());
		void ConsolidateIndices(bool bPrune);

		void ForEachCluster(TFunction<void(FSubGraph*)>&& Func)
		{
			for (FSubGraph* Cluster : SubGraphs)
			{
				if (Cluster->Nodes.IsEmpty() || Cluster->Edges.IsEmpty()) { continue; }
				Func(Cluster);
			}
		}

		~FGraph()
		{
			Nodes.Empty();
			UniqueEdges.Empty();
			Edges.Empty();

			for (const FSubGraph* Cluster : SubGraphs) { delete Cluster; }
			SubGraphs.Empty();
		}
	};

	struct PCGEXTENDEDTOOLKIT_API FEdgeCrossingsHandler
	{
		mutable FRWLock CrossingLock;

		FGraph* Graph;
		double Tolerance;
		double SquaredTolerance;

		TArray<FBox> SegmentBounds;
		TArray<FEdgeCrossing> Crossings;

		int32 NumEdges;
		int32 StartIndex;

		FEdgeCrossingsHandler(FGraph* InGraph, const double InTolerance)
			: Graph(InGraph),
			  Tolerance(InTolerance),
			  SquaredTolerance(InTolerance * InTolerance)
		{
			NumEdges = InGraph->Edges.Num();
			StartIndex = InGraph->Nodes.Num();
			Crossings.Empty();
			SegmentBounds.Empty();
			SegmentBounds.Reserve(NumEdges);
		}

		~FEdgeCrossingsHandler()
		{
			SegmentBounds.Empty();
			Crossings.Empty();
			Graph = nullptr;
		}

		void Prepare(const TArray<FPCGPoint>& InPoints);
		void ProcessEdge(const int32 EdgeIndex, const TArray<FPCGPoint>& InPoints);
		void InsertCrossings();
	};

	class PCGEXTENDEDTOOLKIT_API FGraphBuilder
	{
	public:
		bool bPrunePoints = false;
		FString EdgeTagValue;

		PCGExData::FPointIO* PointIO = nullptr;

		FGraph* Graph = nullptr;
		FEdgeCrossingsHandler* EdgeCrossings = nullptr;

		PCGExData::FPointIOGroup* EdgesIO = nullptr;
		PCGExData::FPointIO* SourceEdgesIO = nullptr;

		bool bCompiledSuccessfully = false;

		FGraphBuilder(PCGExData::FPointIO& InPointIO, int32 NumEdgeReserve = 6, PCGExData::FPointIO* InSourceEdges = nullptr)
			: SourceEdgesIO(InSourceEdges)
		{
			PointIO = &InPointIO;
			PointIO->Tags->Set(PCGExGraph::Tag_Cluster, PointIO->GetInOut()->UID, EdgeTagValue);

			const int32 NumNodes = PointIO->GetOutNum();

			Graph = new FGraph(NumNodes, NumEdgeReserve);

			EdgesIO = new PCGExData::FPointIOGroup();
			EdgesIO->DefaultOutputLabel = OutputEdgesLabel;
		}

		void EnableCrossings(const double Tolerance);
		void EnablePointsPruning();

		void Compile(FPCGExPointsProcessorContext* InContext, int32 Min = 1, int32 Max = TNumericLimits<int32>::Max()) const;
		void Write(FPCGExPointsProcessorContext* InContext) const;

		~FGraphBuilder()
		{
			PCGEX_DELETE(Graph)
			PCGEX_DELETE(EdgeCrossings)
			PCGEX_DELETE(EdgesIO)
		}
	};

	static bool GetRemappedIndices(PCGExData::FPointIO& InPointIO, const FName AttributeName, TMap<int32, int32>& OutIndices)
	{
		OutIndices.Empty();

		PCGEx::TFAttributeReader<int32>* IndexReader = new PCGEx::TFAttributeReader<int32>(AttributeName);
		if (!IndexReader->Bind(const_cast<PCGExData::FPointIO&>(InPointIO)))
		{
			PCGEX_DELETE(IndexReader)
			return false;
		}

		OutIndices.Reserve(IndexReader->Values.Num());
		for (int i = 0; i < IndexReader->Values.Num(); i++) { OutIndices.Add(IndexReader->Values[i], i); }

		PCGEX_DELETE(IndexReader)
		return true;
	}

	static bool GetRemappedIndices(const PCGExData::FPointIO& InPointIO, const FName AttributeName, TMap<int32, int32>& OutIndices)
	{
		return GetRemappedIndices(const_cast<PCGExData::FPointIO&>(InPointIO), AttributeName, OutIndices);
	}

#pragma endregion
}

class PCGEXTENDEDTOOLKIT_API FPCGExWriteSubGraphEdgesTask : public FPCGExNonAbandonableTask
{
public:
	FPCGExWriteSubGraphEdgesTask(FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO,
	                             PCGExData::FPointIO* InClusterIO, PCGExGraph::FGraph* InGraph, PCGExGraph::FSubGraph* InSubGraph,
	                             const int32 InMin = 1, const int32 InMax = TNumericLimits<int32>::Max())
		: FPCGExNonAbandonableTask(InManager, InTaskIndex, InPointIO),
		  EdgeIO(InClusterIO), Graph(InGraph), SubGraph(InSubGraph),
		  Min(InMin), Max(InMax)
	{
	}

	PCGExData::FPointIO* EdgeIO = nullptr;
	PCGExGraph::FGraph* Graph = nullptr;
	PCGExGraph::FSubGraph* SubGraph = nullptr;
	int32 Min;
	int32 Max;

	virtual bool ExecuteTask() override;
};


class PCGEXTENDEDTOOLKIT_API FPCGExCompileGraphTask : public FPCGExNonAbandonableTask
{
public:
	FPCGExCompileGraphTask(FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO,
	                       PCGExGraph::FGraphBuilder* InGraphBuilder, const int32 InMin = 1, const int32 InMax = TNumericLimits<int32>::Max())
		: FPCGExNonAbandonableTask(InManager, InTaskIndex, InPointIO),
		  Builder(InGraphBuilder), Min(InMin), Max(InMax)
	{
	}

	PCGExGraph::FGraphBuilder* Builder = nullptr;
	int32 Min;
	int32 Max;

	virtual bool ExecuteTask() override;
};
