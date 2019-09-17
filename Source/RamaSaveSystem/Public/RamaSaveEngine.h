/*
	By Rama
*/

#pragma once

#include "RamaSaveObject.h"
#include "ObjectAndNameAsStringProxyArchive.h"
#include "RamaSaveEngine.generated.h"
 
//Version
#define JOY_SAVE_VERSION 6

#define JOY_SAVE_VERSION_STREAMINGLEVELS 4
#define JOY_SAVE_VERSION_MULTISUBCOMPONENT_SAMENAME 5
#define JOY_SAVE_VERSION_SAVEOBJECT 6

USTRUCT()
struct FRamaSaveEngineParams
{
	GENERATED_USTRUCT_BODY()
 
	UPROPERTY()
	TArray<FString> LoadOnlyActorsWithSaveTags;
	
	UPROPERTY()
	FString FileName = "";
	
	UPROPERTY()
	bool DestroyActorsBeforeLoad = true;
	
	UPROPERTY()
	bool DontLoadPlayerPawns = false;
	
	UPROPERTY()
	FString LoadOnlyStreamingLevel = "";
	
};

UCLASS()
class ARamaSaveEngine	: public AActor
{
	GENERATED_BODY()
public:
	ARamaSaveEngine(const FObjectInitializer& ObjectInitializer);
	
	
//Delegates / Events
public:
	/** Value proceeds from 0 to 1 during async save <3 Rama */
	UFUNCTION(BlueprintImplementableEvent, Category="Rama Save System")
	void Async_ProgressUpdate(float Progress);
	 
	UFUNCTION(BlueprintImplementableEvent, Category="Rama Save System")
	void Async_SaveStarted(const FString& FileName);
	
	UFUNCTION(BlueprintImplementableEvent, Category="Rama Save System")
	void Async_SaveFinished(const FString& FileName);
	
	UFUNCTION(BlueprintImplementableEvent, Category="Rama Save System")
	void Async_SaveCancelled(const FString& FileName);
	
//Saving
public:
	
	//SYNC
	void RamaSave_SaveToFile(FString FileName, bool& FileIOSuccess, bool& AllComponentsSaved, FString SaveOnlyStreamingLevel="", URamaSaveObject* StaticSaveData = nullptr);
	
	UPROPERTY()
	TArray<URamaSaveComponent*> RamaSaveComponents;
	
	//ASYNC
	void RamaSave_SaveToFile_ASYNC(FString FileName, bool& FileIOSuccess, bool& AllComponentsSaved, FString SaveOnlyStreamingLevel="", URamaSaveObject* StaticSaveData = nullptr);
	
	TArray<uint8> RamaSaveAsync_ToBinary;
	FObjectAndNameAsStringProxyArchive* AsyncArchive = nullptr;
	FMemoryWriter* AsyncMemoryWriter = nullptr;
	void ClearAsyncArchive();
	
	FTimerHandle TH_CheckCompressToFileFinished;
	void CheckCompressToFileFinished();
	bool RamaSaveAsync_SaveChecks = false;
	FString RamaSaveAsync_FileName;
	int32 RamaSaveAsync_TotalComponents;
	int32 RamaSaveAsync_Index = 0;
	int32 RamaSaveAsync_ChunkGoal = 1;
	FTimerHandle TH_RamaSaveAsync;
	void RamaSaveAsync();
	void RamaSaveAsync_Finish();
	
	//Returns true if a save was in progress and was cancelled
	bool RamaSaveAsync_Cancel();
	
//Loading
public:
	UPROPERTY()
	FRamaSaveEngineParams LoadParams;
	
	//Unload/Load appropriate Levels
	void Phase1(const FRamaSaveEngineParams& Params, bool HandleStreamingLevelsLoadingAndUnloading);
	
	float AsyncStartTime = 0;
	
	UPROPERTY()
	TArray<ULevelStreaming*> Load_StreamingLevels;
	
	FTimerHandle TH_AsyncStreamingLoad;
	void AsyncStreamingLoad();
	
	//Loooooooaaaaaaadddddd!!!
	void Phase2();
	
	//What file version is being loaded?
	static int32 LoadedSaveVersion;
	
	
	void SaveStaticData(FArchive& Ar, URamaSaveObject* StaticData);
	void SkipStaticData(FArchive& Ar);
	
	static URamaSaveObject* LoadStaticData(bool& FileIOSuccess,  FString FileName);
	
public:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
};

