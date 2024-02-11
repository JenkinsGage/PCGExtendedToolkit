﻿// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExBuildConvexHull2D.h"

#include "Elements/Metadata/PCGMetadataElementCommon.h"
#include "Geometry/PCGExGeoDelaunay.h"
#include "Graph/PCGExCluster.h"

#define LOCTEXT_NAMESPACE "PCGExGraph"
#define PCGEX_NAMESPACE BuildConvexHull2D

int32 UPCGExBuildConvexHull2DSettings::GetPreferredChunkSize() const { return 32; }

PCGExData::EInit UPCGExBuildConvexHull2DSettings::GetMainOutputInitMode() const
{
	return bPrunePoints ? PCGExData::EInit::NewOutput : bMarkHull ? PCGExData::EInit::DuplicateInput : PCGExData::EInit::Forward;
}

FPCGExBuildConvexHull2DContext::~FPCGExBuildConvexHull2DContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE(GraphBuilder)
	PCGEX_DELETE(PathsIO)

	HullIndices.Empty();
}

TArray<FPCGPinProperties> UPCGExBuildConvexHull2DSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();

	FPCGPinProperties& PinClustersOutput = PinProperties.Emplace_GetRef(PCGExGraph::OutputEdgesLabel, EPCGDataType::Point);

#if WITH_EDITOR
	PinClustersOutput.Tooltip = FTEXT("Point data representing edges.");
#endif

	FPCGPinProperties& PinPathsOutput = PinProperties.Emplace_GetRef(PCGExGraph::OutputPathsLabel, EPCGDataType::Point);

#if WITH_EDITOR
	PinPathsOutput.Tooltip = FTEXT("Point data representing closed convex hull paths.");
#endif

	return PinProperties;
}

FName UPCGExBuildConvexHull2DSettings::GetMainOutputLabel() const { return PCGExGraph::OutputVerticesLabel; }

PCGEX_INITIALIZE_ELEMENT(BuildConvexHull2D)

bool FPCGExBuildConvexHull2DElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElementBase::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(BuildConvexHull2D)

	PCGEX_VALIDATE_NAME(Settings->HullAttributeName)

	PCGEX_FWD(ProjectionSettings)

	Context->GraphBuilderSettings.bPruneIsolatedPoints = Settings->bPrunePoints;

	Context->PathsIO = new PCGExData::FPointIOGroup();
	Context->PathsIO->DefaultOutputLabel = PCGExGraph::OutputPathsLabel;

	return true;
}

bool FPCGExBuildConvexHull2DElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExBuildConvexHull2DElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(BuildConvexHull2D)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		PCGEX_DELETE(Context->GraphBuilder)
		Context->HullIndices.Empty();

		if (!Context->AdvancePointsIO()) { Context->Done(); }
		else
		{
			if (Context->CurrentIO->GetNum() <= 3)
			{
				PCGE_LOG(Warning, GraphAndLog, FTEXT("(0) Some inputs have too few points to be processed (<= 3)."));
				return false;
			}

			Context->GraphBuilder = new PCGExGraph::FGraphBuilder(*Context->CurrentIO, &Context->GraphBuilderSettings, 6);
			Context->GetAsyncManager()->Start<FPCGExConvexHull2Task>(Context->CurrentIO->IOIndex, Context->CurrentIO, Context->GraphBuilder->Graph);

			//PCGE_LOG(Warning, GraphAndLog, FTEXT("(1) Some inputs generates no results. Are points coplanar? If so, use Convex Hull 2D instead."));

			Context->SetAsyncState(PCGExGeo::State_ProcessingHull);
		}
	}

	if (Context->IsState(PCGExGeo::State_ProcessingHull))
	{
		if (!Context->IsAsyncWorkComplete()) { return false; }

		if(Context->GraphBuilder->Graph->Edges.IsEmpty())
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("(1) Some inputs generates no results."));
			Context->SetState(PCGExMT::State_ReadyForNextPoints);
			return false;
		}
		
		Context->GraphBuilder->Compile(Context);
		Context->SetAsyncState(PCGExGraph::State_WritingClusters);
	}

	if (Context->IsState(PCGExGraph::State_WritingClusters))
	{
		if (!Context->IsAsyncWorkComplete()) { return false; }
		if (Context->GraphBuilder->bCompiledSuccessfully)
		{
			if (Settings->bMarkHull && !Settings->bPrunePoints)
			{
				PCGEx::TFAttributeWriter<bool>* HullMarkPointWriter = new PCGEx::TFAttributeWriter<bool>(Settings->HullAttributeName, false, false);
				HullMarkPointWriter->BindAndGet(*Context->CurrentIO);

				for (int i = 0; i < Context->CurrentIO->GetNum(); i++) { HullMarkPointWriter->Values[i] = Context->HullIndices.Contains(i); }

				HullMarkPointWriter->Write();
				PCGEX_DELETE(HullMarkPointWriter)
			}

			Context->BuildPath();
			Context->GraphBuilder->Write(Context);
		}
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsDone())
	{
		Context->OutputPoints();
		Context->PathsIO->OutputTo(Context);
	}

	return Context->IsDone();
}

void FPCGExBuildConvexHull2DContext::BuildPath() const
{
	TSet<uint64> UniqueEdges;
	const TArray<PCGExGraph::FIndexedEdge>& Edges = GraphBuilder->Graph->Edges;

	const PCGExData::FPointIO& PathIO = PathsIO->Emplace_GetRef(*CurrentIO, PCGExData::EInit::NewOutput);

	TArray<FPCGPoint>& MutablePathPoints = PathIO.GetOut()->GetMutablePoints();
	TSet<int32> VisitedEdges;
	VisitedEdges.Reserve(Edges.Num());

	int32 CurrentIndex = -1;
	int32 FirstIndex = -1;

	while (MutablePathPoints.Num() != Edges.Num())
	{
		int32 EdgeIndex = -1;

		if (CurrentIndex == -1)
		{
			EdgeIndex = 0;
			FirstIndex = Edges[EdgeIndex].Start;
			MutablePathPoints.Add(CurrentIO->GetInPoint(FirstIndex));
			CurrentIndex = Edges[EdgeIndex].End;
		}
		else
		{
			for (int i = 0; i < Edges.Num(); i++)
			{
				EdgeIndex = i;
				if (VisitedEdges.Contains(EdgeIndex)) { continue; }

				const PCGExGraph::FUnsignedEdge& Edge = Edges[EdgeIndex];
				if (!Edge.Contains(CurrentIndex)) { continue; }

				CurrentIndex = Edge.Other(CurrentIndex);
				break;
			}
		}

		if (CurrentIndex == FirstIndex) { break; }

		VisitedEdges.Add(EdgeIndex);
		MutablePathPoints.Add(CurrentIO->GetInPoint(CurrentIndex));
	}

	VisitedEdges.Empty();
}

bool FPCGExConvexHull2Task::ExecuteTask()
{
	FPCGExBuildConvexHull2DContext* Context = static_cast<FPCGExBuildConvexHull2DContext*>(Manager->Context);
	PCGEX_SETTINGS(BuildConvexHull2D)

	PCGExGeo::TDelaunay2* Delaunay = new PCGExGeo::TDelaunay2();

	const TArray<FPCGPoint>& Points = PointIO->GetIn()->GetPoints();
	const int32 NumPoints = Points.Num();

	TArray<FVector2D> Positions;
	Positions.SetNum(NumPoints);
	for (int i = 0; i < NumPoints; i++)
	{
		const FVector Pos = Points[i].Transform.GetLocation();
		Positions[i] = FVector2D(Pos.X, Pos.Y);
	}

	const TArrayView<FVector2D> View = MakeArrayView(Positions);
	if (!Delaunay->Process(View))
	{
		PCGEX_DELETE(Delaunay)
		return false;
	}

	if (Settings->bMarkHull) { Context->HullIndices.Append(Delaunay->DelaunayHull); }

	Graph->InsertEdges(Delaunay->DelaunayEdges, -1);

	PCGEX_DELETE(Delaunay)
	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
