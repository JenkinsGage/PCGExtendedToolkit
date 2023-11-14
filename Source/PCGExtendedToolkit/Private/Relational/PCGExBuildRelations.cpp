﻿// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Relational/PCGExBuildRelations.h"

#include "Data/PCGSpatialData.h"
#include "Data/PCGPointData.h"
#include "PCGContext.h"
#include "DrawDebugHelpers.h"
#include "Editor.h"
#include "Relational/PCGExRelationsHelpers.h"

#define LOCTEXT_NAMESPACE "PCGExBuildRelations"

int32 UPCGExBuildRelationsSettings::GetPreferredChunkSize() const { return 32; }

PCGEx::EIOInit UPCGExBuildRelationsSettings::GetPointOutputInitMode() const { return PCGEx::EIOInit::DuplicateInput; }

FPCGElementPtr UPCGExBuildRelationsSettings::CreateElement() const
{
	return MakeShared<FPCGExBuildRelationsElement>();
}

FPCGContext* FPCGExBuildRelationsElement::Initialize(
	const FPCGDataCollection& InputData,
	TWeakObjectPtr<UPCGComponent> SourceComponent,
	const UPCGNode* Node)
{
	FPCGExBuildRelationsContext* Context = new FPCGExBuildRelationsContext();
	InitializeContext(Context, InputData, SourceComponent, Node);
	return Context;
}

void FPCGExBuildRelationsElement::InitializeContext(
	FPCGExPointsProcessorContext* InContext,
	const FPCGDataCollection& InputData,
	TWeakObjectPtr<UPCGComponent> SourceComponent,
	const UPCGNode* Node) const
{
	FPCGExRelationsProcessorElement::InitializeContext(InContext, InputData, SourceComponent, Node);
	//FPCGExBuildRelationsContext* Context = static_cast<FPCGExBuildRelationsContext*>(InContext);
	// ...
}

bool FPCGExBuildRelationsElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExBuildRelationsElement::Execute);

	FPCGExBuildRelationsContext* Context = static_cast<FPCGExBuildRelationsContext*>(InContext);

	if (Context->IsCurrentOperation(PCGEx::EOperation::Setup))
	{
		if (Context->Params.IsEmpty())
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("MissingParams", "Missing Input Params."));
			return true;
		}

		if (Context->Points->IsEmpty())
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("MissingPoints", "Missing Input Points."));
			return true;
		}

		Context->SetOperation(PCGEx::EOperation::ReadyForNextPoints);
	}

	if (Context->IsCurrentOperation(PCGEx::EOperation::ReadyForNextPoints))
	{
		if (Context->CurrentIO)
		{
			//Cleanup current IO, indices won't be needed anymore.
			Context->CurrentIO->Flush();
		}

		if (!Context->AdvancePointsIO(true))
		{
			Context->SetOperation(PCGEx::EOperation::Done); //No more points
		}
		else
		{
			Context->CurrentIO->BuildMetadataEntriesAndIndices();
			Context->Octree = const_cast<UPCGPointData::PointOctree*>(&(Context->CurrentIO->Out->GetOctree())); // Not sure this really saves perf
			Context->SetOperation(PCGEx::EOperation::ReadyForNextParams);
		}
	}

	auto ProcessPoint = [&Context](
		const FPCGPoint& Point, int32 ReadIndex, UPCGExPointIO* IO)
	{
		Context->CachedIndex->SetValue(Point.MetadataEntry, ReadIndex); // Cache index

		TArray<PCGExRelational::FSocketCandidate> Candidates;
		const double MaxDistance = PCGExRelational::Helpers::PrepareCandidatesForPoint(Point, Context->CurrentParams, Candidates);

		auto ProcessPointNeighbor = [&ReadIndex, &Candidates, &IO](const FPCGPointRef& OtherPointRef)
		{
			const FPCGPoint* OtherPoint = OtherPointRef.Point;
			const int32 Index = IO->GetIndex(OtherPoint->MetadataEntry);

			if (Index == ReadIndex) { return; }

			for (PCGExRelational::FSocketCandidate& SocketCandidate : Candidates)
			{
				if (SocketCandidate.ProcessPoint(OtherPoint)) { SocketCandidate.Index = Index; }
			}
		};

		const FBoxCenterAndExtent Box = FBoxCenterAndExtent(Point.Transform.GetLocation(), FVector(MaxDistance));
		Context->Octree->FindElementsWithBoundsTest(Box, ProcessPointNeighbor);

		//Write results
		PCGMetadataEntryKey Key = Point.MetadataEntry;
		for (int i = 0; i < Candidates.Num(); i++)
		{
			Context->SocketInfos[i].Socket->SetIndex(Key, Candidates[i].Index);
		}
	};

	if (Context->IsCurrentOperation(PCGEx::EOperation::ReadyForNextParams))
	{
		if (!Context->AdvanceParams())
		{
			Context->SetOperation(PCGEx::EOperation::ReadyForNextPoints);
			return false;
		}
		else
		{
			Context->SetOperation(PCGEx::EOperation::ProcessingParams);
		}
	}

	auto Initialize = [&Context](UPCGExPointIO* IO)
	{
		Context->CurrentParams->PrepareForPointData(Context, IO->Out);
	};

	if (Context->IsCurrentOperation(PCGEx::EOperation::ProcessingParams))
	{
		if (Context->CurrentIO->OutputParallelProcessing(Context, Initialize, ProcessPoint, Context->ChunkSize))
		{
			Context->SetOperation(PCGEx::EOperation::ReadyForNextParams);
		}
	}

	if (Context->IsCurrentOperation(PCGEx::EOperation::Done))
	{
		Context->Points->OutputTo(Context);
		return true;
	}

	return false;
}

#undef LOCTEXT_NAMESPACE
