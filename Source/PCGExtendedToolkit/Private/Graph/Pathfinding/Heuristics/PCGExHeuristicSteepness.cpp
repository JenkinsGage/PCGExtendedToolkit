﻿// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Pathfinding/Heuristics/PCGExHeuristicSteepness.h"

void UPCGExHeuristicSteepness::PrepareForCluster(PCGExCluster::FCluster* InCluster)
{
	UpwardVector = UpVector.GetSafeNormal();
	Super::PrepareForCluster(InCluster);
}

double UPCGExHeuristicSteepness::GetGlobalScore(
	const PCGExCluster::FNode& From,
	const PCGExCluster::FNode& Seed,
	const PCGExCluster::FNode& Goal) const
{
	const double Dot = GetDot(From.Position, Goal.Position);
	const double SampledDot = FMath::Max(0, ScoreCurveObj->GetFloatValue(Dot)) * ReferenceWeight;
	return SampledDot;
}

double UPCGExHeuristicSteepness::GetEdgeScore(
	const PCGExCluster::FNode& From,
	const PCGExCluster::FNode& To,
	const PCGExGraph::FIndexedEdge& Edge,
	const PCGExCluster::FNode& Seed,
	const PCGExCluster::FNode& Goal) const
{
	const double Dot = GetDot(From.Position, To.Position);
	const double SampledDot = FMath::Max(0, ScoreCurveObj->GetFloatValue(Dot)) * ReferenceWeight;
	return SampledDot;
}

double UPCGExHeuristicSteepness::GetDot(const FVector& From, const FVector& To) const
{
	return bInvert ?
		       1 - FMath::Abs(FVector::DotProduct((From - To).GetSafeNormal(), UpwardVector)) :
		       FMath::Abs(FVector::DotProduct((From - To).GetSafeNormal(), UpwardVector));
}

void UPCGExHeuristicSteepness::ApplyOverrides()
{
	Super::ApplyOverrides();
	PCGEX_OVERRIDE_OP_PROPERTY(bInvert, FName(TEXT("Heuristics/Invert")), EPCGMetadataTypes::Boolean);
	PCGEX_OVERRIDE_OP_PROPERTY(UpVector, FName(TEXT("Heuristics/UpVector")), EPCGMetadataTypes::Vector);
}

UPCGExHeuristicOperation* UPCGHeuristicsFactorySteepness::CreateOperation() const
{
	UPCGExHeuristicSteepness* NewOperation = NewObject<UPCGExHeuristicSteepness>();
	return NewOperation;
}

UPCGExParamFactoryBase* UPCGExHeuristicsSteepnessProviderSettings::CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const
{
	UPCGHeuristicsFactorySteepness* NewHeuristics = NewObject<UPCGHeuristicsFactorySteepness>();
	NewHeuristics->WeightFactor = Descriptor.WeightFactor;
	return Super::CreateFactory(InContext, NewHeuristics);
}
