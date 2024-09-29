﻿// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExWriteIndex.h"


#define LOCTEXT_NAMESPACE "PCGExWriteIndexElement"
#define PCGEX_NAMESPACE WriteIndex

PCGExData::EInit UPCGExWriteIndexSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

PCGEX_INITIALIZE_ELEMENT(WriteIndex)

bool FPCGExWriteIndexElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(WriteIndex)

	PCGEX_VALIDATE_NAME(Settings->OutputAttributeName)

	if (Settings->bOutputCollectionIndex)
	{
		PCGEX_VALIDATE_NAME(Settings->CollectionIndexAttributeName)
	}

	return true;
}

bool FPCGExWriteIndexElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExWriteIndexElement::Execute);

	PCGEX_CONTEXT(WriteIndex)
	PCGEX_EXECUTION_CHECK
	
	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExWriteIndex::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExWriteIndex::FProcessor>>& NewBatch)
			{
			}))
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("Could not find any points to process."));
			return true;
		}
	}

	if (!Context->ProcessPointsBatch(PCGExMT::State_Done)) { return false; }

	Context->MainPoints->OutputToContext();

	return Context->TryComplete();
}

namespace PCGExWriteIndex
{
	bool FProcessor::Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExWriteIndex::Process);

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		NumPoints = PointDataFacade->GetNum();

		if (Settings->bOutputNormalizedIndex)
		{
			DoubleWriter = PointDataFacade->GetWritable<double>(Settings->OutputAttributeName, -1, false, false);
		}
		else
		{
			IntWriter = PointDataFacade->GetWritable<int32>(Settings->OutputAttributeName, -1, false, false);
		}

		if (Settings->bOutputCollectionIndex)
		{
			PCGExData::WriteMark(PointDataFacade->GetOut()->Metadata, Settings->CollectionIndexAttributeName, BatchIndex);
		}

		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 Count)
	{
		if (DoubleWriter) { DoubleWriter->GetMutable(Index) = static_cast<double>(Index) / NumPoints; }
		else { IntWriter->GetMutable(Index) = Index; }
	}

	void FProcessor::CompleteWork()
	{
		PointDataFacade->Write(AsyncManager);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
