﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "PCGExMacros.h"
#include "Kismet/KismetStringLibrary.h"

namespace PCGExTags
{
	class FTagValue : public TSharedFromThis<FTagValue>
	{
	public:
		virtual ~FTagValue() = default;
		EPCGMetadataTypes UnderlyingType = EPCGMetadataTypes::Unknown;

		explicit FTagValue()
		{
		}

		virtual FString Flatten(const FString& LeftSide) = 0;
	};

	template <typename T>
	class TTagValue : public FTagValue
	{
	public:
		T Value;

		explicit TTagValue(const T& InValue)
			: FTagValue(), Value(InValue)
		{
			UnderlyingType = PCGEx::GetMetadataType<T>();
		}

		virtual FString Flatten(const FString& LeftSide) override
		{
			if constexpr (std::is_same_v<T, float> || std::is_same_v<T, double>)
			{
				return FString::Printf(TEXT("%s:%.2f"), *LeftSide, Value);
			}
			else if constexpr (std::is_same_v<T, int32> || std::is_same_v<T, int64>)
			{
				return FString::Printf(TEXT("%s:%d"), *LeftSide, Value);
			}
			else if constexpr (std::is_same_v<T, FVector2D> || std::is_same_v<T, FVector> || std::is_same_v<T, FVector4>)
			{
				return FString::Printf(TEXT("%s:%s"), *LeftSide, *Value.ToString());
			}
			else if constexpr (std::is_same_v<T, FString>)
			{
				return FString::Printf(TEXT("%s:%s"), *LeftSide, *Value);
			}
			else
			{
				return LeftSide;
			}
		}
	};

	static TSharedPtr<FTagValue> TryGetValueTag(const FString& InTag, FString& OutLeftSide)
	{
		int32 DividerPosition = INDEX_NONE;
		if (!InTag.FindChar(':', DividerPosition))
		{
			return nullptr;
		}

		OutLeftSide = InTag.Left(DividerPosition);
		FString RightSide = InTag.RightChop(DividerPosition + 1);

		if (OutLeftSide.IsEmpty())
		{
			return nullptr;
		}
		if (RightSide.IsEmpty())
		{
			return nullptr;
		}
		if (RightSide.IsNumeric())
		{
			int32 FloatingPointPosition = INDEX_NONE;
			if (InTag.FindChar('.', FloatingPointPosition))
			{
				return MakeShared<TTagValue<double>>(FCString::Atod(*RightSide));
			}
			return MakeShared<TTagValue<int32>>(FCString::Atoi(*RightSide));
		}

		if (FVector ParsedVector; ParsedVector.InitFromString(RightSide))
		{
			return MakeShared<TTagValue<FVector>>(ParsedVector);
		}

		if (FVector2D ParsedVector2D; ParsedVector2D.InitFromString(RightSide))
		{
			return MakeShared<TTagValue<FVector2D>>(ParsedVector2D);
		}

		if (FVector4 ParsedVector4; ParsedVector4.InitFromString(RightSide))
		{
			return MakeShared<TTagValue<FVector4>>(ParsedVector4);
		}

		return MakeShared<TTagValue<FString>>(RightSide);

		return nullptr;
	}

	using IDType = TSharedPtr<TTagValue<int32>>;
}

namespace PCGExData
{
	using namespace PCGExTags;
	const FString TagSeparator = FSTRING(":");

	struct FTags
	{
		mutable FRWLock TagsLock;
		TSet<FString> RawTags;                          // Contains all data tag
		TMap<FString, TSharedPtr<FTagValue>> ValueTags; // Prefix:ValueTag

		int32 Num() const { return RawTags.Num() + ValueTags.Num(); }

		bool IsEmpty() const { return RawTags.IsEmpty() && ValueTags.IsEmpty(); }

		FTags()
		{
			RawTags.Empty();
			ValueTags.Empty();
		}

		explicit FTags(const TSet<FString>& InTags)
			: FTags()
		{
			for (const FString& TagString : InTags) { ParseAndAdd(TagString); }
		}

		explicit FTags(const TSharedPtr<FTags>& InTags)
			: FTags()
		{
			Reset(InTags);
		}

		void Append(const TSharedRef<FTags>& InTags)
		{
			Append(InTags->FlattenToArray());
		}

		void Append(const TArray<FString>& InTags)
		{
			FWriteScopeLock WriteScopeLock(TagsLock);
			for (const FString& TagString : InTags) { ParseAndAdd(TagString); }
		}

		void Append(const TSet<FString>& InTags)
		{
			FWriteScopeLock WriteScopeLock(TagsLock);
			for (const FString& TagString : InTags) { ParseAndAdd(TagString); }
		}

		void Reset()
		{
			FWriteScopeLock WriteScopeLock(TagsLock);
			RawTags.Empty();
			ValueTags.Empty();
		}

		void Reset(const TSharedPtr<FTags>& InTags)
		{
			Reset();
			if (InTags) { Append(InTags.ToSharedRef()); }
		}

		void DumpTo(TSet<FString>& InTags, const bool bFlatten = true) const
		{
			FReadScopeLock ReadScopeLock(TagsLock);

			InTags.Reserve(InTags.Num() + Num());
			InTags.Append(RawTags);
			if (bFlatten) { for (const TPair<FString, TSharedPtr<FTagValue>>& Pair : ValueTags) { InTags.Add(Pair.Value->Flatten(Pair.Key)); } }
			else { for (const TPair<FString, TSharedPtr<FTagValue>>& Pair : ValueTags) { InTags.Add(Pair.Key); } }
		}

		void DumpTo(TArray<FName>& InTags, const bool bFlatten = true) const
		{
			FReadScopeLock ReadScopeLock(TagsLock);
			InTags.Reserve(Num());
			InTags.Append(FlattenToArrayOfNames(bFlatten));
		}

		TSet<FString> Flatten()
		{
			FReadScopeLock ReadScopeLock(TagsLock);

			TSet<FString> Flattened;
			Flattened.Reserve(Num());
			Flattened.Append(RawTags);
			for (const TPair<FString, TSharedPtr<FTagValue>>& Pair : ValueTags) { Flattened.Add(Pair.Value->Flatten(Pair.Key)); }
			return Flattened;
		}

		TArray<FString> FlattenToArray(const bool bIncludeValue = true) const
		{
			FReadScopeLock ReadScopeLock(TagsLock);

			TArray<FString> Flattened;
			Flattened.Reserve(Num());
			for (const FString& Key : RawTags) { Flattened.Add(Key); }
			if (bIncludeValue) { for (const TPair<FString, TSharedPtr<FTagValue>>& Pair : ValueTags) { Flattened.Add(Pair.Value->Flatten(Pair.Key)); } }
			else { for (const TPair<FString, TSharedPtr<FTagValue>>& Pair : ValueTags) { Flattened.Add(Pair.Key); } }
			return Flattened;
		}

		TArray<FName> FlattenToArrayOfNames(const bool bIncludeValue = true) const
		{
			FReadScopeLock ReadScopeLock(TagsLock);

			TArray<FName> Flattened;
			Flattened.Reserve(Num());
			for (const FString& Key : RawTags) { Flattened.Add(FName(Key)); }
			if (bIncludeValue) { for (const TPair<FString, TSharedPtr<FTagValue>>& Pair : ValueTags) { Flattened.Add(FName(Pair.Value->Flatten(Pair.Key))); } }
			else { for (const TPair<FString, TSharedPtr<FTagValue>>& Pair : ValueTags) { Flattened.Add(FName(Pair.Key)); } }
			return Flattened;
		}

		~FTags() = default;

		/* Simple add to raw tags */
		void AddRaw(const FString& Key)
		{
			FWriteScopeLock WriteScopeLock(TagsLock);
			ParseAndAdd(Key);
		}

		template <typename T>
		TSharedPtr<TTagValue<T>> GetOrSet(const FString& Key, const T& Value)
		{
			{
				FReadScopeLock ReadScopeLock(TagsLock);

				TArray<T> ValuesForKey;
				if (const TSharedPtr<FTagValue>* ValueTagPtr = ValueTags.Find(Key))
				{
					if ((*ValueTagPtr)->UnderlyingType == PCGEx::GetMetadataType<T>())
					{
						return StaticCastSharedPtr<TTagValue<T>>(*ValueTagPtr);
					}
				}
			}
			{
				FWriteScopeLock WriteScopeLock(TagsLock);

				const TSharedPtr<TTagValue<T>> ValueTag = MakeShared<TTagValue<T>>(Value);
				ValueTags.Add(Key, ValueTag);
				return ValueTag;
			}
		}

		template <typename T>
		TSharedPtr<TTagValue<T>> Set(const FString& Key, const T& Value)
		{
			const TSharedPtr<TTagValue<T>> ValueTag = GetOrSet(Key, Value);
			ValueTag->Value = Value;
			return ValueTag;
		}

		template <typename T>
		TSharedPtr<TTagValue<T>> Set(const FString& Key, const TSharedPtr<TTagValue<T>>& Value)
		{
			return Set<T>(Key, Value->Value);
		}

		void Remove(const FString& Key)
		{
			FWriteScopeLock WriteScopeLock(TagsLock);
			ValueTags.Remove(Key);
			RawTags.Remove(Key);
		}

		void Remove(const TSet<FString>& InSet)
		{
			FWriteScopeLock WriteScopeLock(TagsLock);
			for (const FString& Tag : InSet)
			{
				ValueTags.Remove(Tag);
				RawTags.Remove(Tag);
			}
		}

		void Remove(const TSet<FName>& InSet)
		{
			FWriteScopeLock WriteScopeLock(TagsLock);
			for (const FName& Tag : InSet)
			{
				FString Key = Tag.ToString();
				ValueTags.Remove(Key);
				RawTags.Remove(Key);
			}
		}

		template <typename T>
		TSharedPtr<TTagValue<T>> GetTypedValue(const FString& Key)
		{
			FReadScopeLock ReadScopeLock(TagsLock);

			if (const TSharedPtr<FTagValue>* ValueTagPtr = ValueTags.Find(Key))
			{
				if ((*ValueTagPtr)->UnderlyingType == PCGEx::GetMetadataType<T>())
				{
					return StaticCastSharedPtr<TTagValue<T>>(*ValueTagPtr);
				}
			}

			return nullptr;
		}

		TSharedPtr<FTagValue> GetValue(const FString& Key)
		{
			FReadScopeLock ReadScopeLock(TagsLock);
			if (const TSharedPtr<FTagValue>* ValueTagPtr = ValueTags.Find(Key)) { return *ValueTagPtr; }
			return nullptr;
		}

		bool IsTagged(const FString& Key) const
		{
			FReadScopeLock ReadScopeLock(TagsLock);
			return ValueTags.Contains(Key) || RawTags.Contains(Key);
		}

	protected:
		void ParseAndAdd(const FString& InTag)
		{
			FString InKey = TEXT("");

			if (const TSharedPtr<FTagValue> TagValue = TryGetValueTag(InTag, InKey))
			{
				ValueTags.Add(InKey, TagValue);
				return;
			}

			RawTags.Add(InTag);
		}

		// NAME::VALUE
		static bool GetTagFromString(const FString& Input, FString& OutKey, FString& OutValue)
		{
			if (!Input.Contains(TagSeparator)) { return false; }

			TArray<FString> Split = UKismetStringLibrary::ParseIntoArray(Input, TagSeparator, true);

			if (Split.IsEmpty() || Split.Num() < 2) { return false; }

			OutKey = Split[0];
			Split.RemoveAt(0);
			OutValue = FString::Join(Split, TEXT(""));
			return true;
		}
	};
}
