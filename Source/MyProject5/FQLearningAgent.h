// FQLearningAgent.h
// Pure-algorithm tabular Q-learning: epsilon-greedy selection, Bellman update,
// JSON persistence. Knows nothing about the game world.
// [Rebuilt Oct 2026 from coursework excerpts - same structure, keys and hyperparameters]

#pragma once

#include "CoreMinimal.h"

enum class ESmartAction : uint8
{
    Rush = 0,
    FlankLeft,
    FlankRight,
    Retreat,
    Circle,
    HoldPosition
};

static constexpr int32 kNumActions = 6;

struct FStateActionPair
{
    int32 State  = 0;
    int32 Action = 0;

    bool operator==(const FStateActionPair& Other) const
    {
        return State == Other.State && Action == Other.Action;
    }

    friend uint32 GetTypeHash(const FStateActionPair& SAP)
    {
        return HashCombine(GetTypeHash(SAP.State), GetTypeHash(SAP.Action));
    }
};

class MYPROJECT5_API FQLearningAgent
{
public:
    float Alpha   = 0.10f;   // learning rate
    float Gamma   = 0.90f;   // discount factor
    float Epsilon = 0.15f;   // exploration rate

    ESmartAction SelectAction(int32 State) const;
    void UpdateQ(int32 State, ESmartAction Action, float Reward, int32 NextState);

    float GetQ(int32 State, ESmartAction Action) const;
    float GetMaxQ(int32 State) const;
    ESmartAction GetBestAction(int32 State) const;

    void SaveQTable(const FString& FilePath) const;
    void LoadQTable(const FString& FilePath);

    int32 NumEntries() const { return QTable.Num(); }

    static const TCHAR* ActionName(ESmartAction Action);

private:
    TMap<FStateActionPair, float> QTable;
};
