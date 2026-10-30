// FQLearningAgent.cpp
// [Rebuilt Oct 2026 from coursework excerpts]

#include "FQLearningAgent.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"

float FQLearningAgent::GetQ(int32 State, ESmartAction Action) const
{
    const float* Found = QTable.Find(FStateActionPair{ State, (int32)Action });
    return Found ? *Found : 0.f;   // unseen pairs are 0
}

float FQLearningAgent::GetMaxQ(int32 State) const
{
    float Best = GetQ(State, ESmartAction::Rush);
    for (int32 A = 1; A < kNumActions; ++A)
    {
        Best = FMath::Max(Best, GetQ(State, (ESmartAction)A));
    }
    return Best;
}

ESmartAction FQLearningAgent::GetBestAction(int32 State) const
{
    // Random tie-break so an all-zero (unseen) state doesn't always pick Rush
    float Best = -TNumericLimits<float>::Max();
    TArray<int32, TInlineAllocator<kNumActions>> Ties;

    for (int32 A = 0; A < kNumActions; ++A)
    {
        const float Q = GetQ(State, (ESmartAction)A);
        if (Q > Best + KINDA_SMALL_NUMBER)
        {
            Best = Q;
            Ties.Reset();
            Ties.Add(A);
        }
        else if (FMath::IsNearlyEqual(Q, Best, KINDA_SMALL_NUMBER))
        {
            Ties.Add(A);
        }
    }
    return (ESmartAction)Ties[FMath::RandRange(0, Ties.Num() - 1)];
}

ESmartAction FQLearningAgent::SelectAction(int32 State) const
{
    if (FMath::FRand() < Epsilon)
    {
        return (ESmartAction)FMath::RandRange(0, kNumActions - 1);   // explore
    }
    return GetBestAction(State);                                       // exploit
}

void FQLearningAgent::UpdateQ(int32 State, ESmartAction Action, float Reward, int32 NextState)
{
    const float OldQ      = GetQ(State, Action);
    const float MaxFuture = GetMaxQ(NextState);
    const float NewQ      = OldQ + Alpha * (Reward + Gamma * MaxFuture - OldQ);
    QTable.Add(FStateActionPair{ State, (int32)Action }, NewQ);
}

void FQLearningAgent::SaveQTable(const FString& FilePath) const
{
    TSharedRef<FJsonObject> JsonObj = MakeShareable(new FJsonObject());
    for (const auto& Pair : QTable)
    {
        // Key format: "state_action" e.g. "42_3" = state 42, action 3
        const FString Key = FString::Printf(TEXT("%d_%d"), Pair.Key.State, Pair.Key.Action);
        JsonObj->SetNumberField(Key, Pair.Value);
    }

    FString OutputStr;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputStr);
    FJsonSerializer::Serialize(JsonObj, Writer);

    IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
    PF.CreateDirectoryTree(*FPaths::GetPath(FilePath));
    FFileHelper::SaveStringToFile(OutputStr, *FilePath);
}

void FQLearningAgent::LoadQTable(const FString& FilePath)
{
    FString InputStr;
    if (!FFileHelper::LoadFileToString(InputStr, *FilePath)) return;   // first session: no file

    TSharedPtr<FJsonObject> JsonObj;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(InputStr);
    if (!FJsonSerializer::Deserialize(Reader, JsonObj) || !JsonObj.IsValid()) return;   // corrupt file

    QTable.Empty();
    for (const auto& Field : JsonObj->Values)
    {
        int32 UnderIdx = INDEX_NONE;
        if (Field.Key.FindChar(TEXT('_'), UnderIdx))
        {
            const int32 S = FCString::Atoi(*Field.Key.Left(UnderIdx));
            const int32 A = FCString::Atoi(*Field.Key.Mid(UnderIdx + 1));
            if (A >= 0 && A < kNumActions && Field.Value.IsValid())
            {
                QTable.Add(FStateActionPair{ S, A }, (float)Field.Value->AsNumber());
            }
        }
    }
}

const TCHAR* FQLearningAgent::ActionName(ESmartAction Action)
{
    switch (Action)
    {
    case ESmartAction::Rush:         return TEXT("Rush");
    case ESmartAction::FlankLeft:    return TEXT("FlankLeft");
    case ESmartAction::FlankRight:   return TEXT("FlankRight");
    case ESmartAction::Retreat:      return TEXT("Retreat");
    case ESmartAction::Circle:       return TEXT("Circle");
    case ESmartAction::HoldPosition: return TEXT("HoldPosition");
    }
    return TEXT("?");
}
