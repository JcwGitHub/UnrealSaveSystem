/*

	By Rama

*/
#include "RamaSaveSystemPrivatePCH.h"
#include "RamaSaveEngine.h"

#include "RamaSaveLibrary.h"
#include "RamaSaveSystemSettings.h"

//////////////////////////////////////////////////////////////////////////
// RamaSaveEngine

int32 ARamaSaveEngine::LoadedSaveVersion = 0;


//Name spaces tend to break in hot reload scenarios
namespace RamaSaveCompressedTask
{
	FGraphEventArray		VictoryMultithreadTest_CompletionEvents;
	
	//~~~~~~~~~~~~~~~
	//Are All Tasks Complete?
	//~~~~~~~~~~~~~~~
	bool TasksAreComplete()
	{
		//Check all thread completion events
		for (int32 Index = 0; Index < VictoryMultithreadTest_CompletionEvents.Num(); Index++)
		{
			//If  ! IsComplete()
			if (!VictoryMultithreadTest_CompletionEvents[Index]->IsComplete())
			{
				return false;
			}
		}
		return true;
	}
	
	//~~~~~~~~~~~
	//Each Task Thread
	//~~~~~~~~~~~
	class FRamaSaveTask
	{
 
	  public:
		TArray<uint8> Data;
		FString FileName = "";
		FRamaSaveTask(const TArray<uint8>& BinaryData, const FString& InFileName) //send in property defaults here
		{
			//Copy entire buffer (switch to ptr if feeling bold)
			Data = BinaryData;
			
			FileName = InFileName;
		}
 
		/** return the name of the task **/
		static const TCHAR* GetTaskName()
		{
			return TEXT("FRamaSaveTask");
		}
		FORCEINLINE static TStatId GetStatId()
		{
			RETURN_QUICK_DECLARE_CYCLE_STAT(FRamaSaveTask, STATGROUP_TaskGraphTasks);
		}
		/** return the thread for this task **/
		static ENamedThreads::Type GetDesiredThread()
		{
			return ENamedThreads::AnyThread;
		}
 
 
		/*
		namespace ESubsequentsMode
		{
			enum Type
			{
				//Necessary when another task will depend on this task. 
				TrackSubsequents,
				//Can be used to save task graph overhead when firing off a task that will not be a dependency of other tasks. 
				FireAndForget
			};
		}
		*/
		static ESubsequentsMode::Type GetSubsequentsMode() 
		{ 
			return ESubsequentsMode::TrackSubsequents; 
		}
 
                //~~~~~~~~~~~~~~~~~~~~~~~~
                //Main Function: Do Task!!
                //~~~~~~~~~~~~~~~~~~~~~~~~
		void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
		{
			URamaSaveUtility::CompressAndWriteToFile(Data,FileName);
		}
	};
	
	void Gooooo(const TArray<uint8>& Data, const FString& File)
	{
		VictoryMultithreadTest_CompletionEvents.Empty();
		VictoryMultithreadTest_CompletionEvents.Add(TGraphTask<FRamaSaveTask>::CreateTask(NULL, ENamedThreads::GameThread).ConstructAndDispatchWhenReady(Data,File));
	}
}


ARamaSaveEngine::ARamaSaveEngine(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	
} 

void ARamaSaveEngine::BeginPlay() 
{  
	Super::BeginPlay();
	//~~~~~~~~~

	UE_LOG(RamaSave, Log,TEXT("~~~ Rama Save Engine Created! ~~~"));
}
void ARamaSaveEngine::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	//Clear the archive ptr <3 Rama
	ClearAsyncArchive();
	
	Super::EndPlay(EndPlayReason);
}

//~~~~~~~~~~~~~~~~~~~
// 		SAVING
//~~~~~~~~~~~~~~~~~~~
void ARamaSaveEngine::RamaSave_SaveToFile(FString FileName, bool& FileIOSuccess, bool& AllComponentsSaved, FString SaveOnlyStreamingLevel, URamaSaveObject* StaticSaveData)
{
	
	 
	//~~~~~~~~~~~~~~~~~~~
	// Clear Any Async Save
	CLEARTIMER(TH_RamaSaveAsync);
	ClearAsyncArchive();
	//~~~~~~~~~~~~~~~~~~~
	
	//~~~
	
	UWorld* World = GetWorld();
	if (!World)
	{
		VSCREENMSG("Big error tell Rama");
		return;
	}

	URamaSaveSystemSettings* Settings = URamaSaveSystemSettings::Get();
	
	//Should always be valid!
	check(Settings);
	
	//~~~ Pre Checks ~~~
	
	//Asked to save only streaming but its not loaded?
	if(SaveOnlyStreamingLevel != "" && SaveOnlyStreamingLevel != "PersistentLevel")
	{
		bool FoundStreaming = false;
		const TArray<ULevelStreaming*>& Levels = World->GetStreamingLevels();
		for(ULevelStreaming* EachLevel : Levels)
		{
			if(!EachLevel) continue;
			
			FString NoPIELevelName = URamaSaveLibrary::RemoveLevelPIEPrefix(EachLevel->GetWorldAssetPackageName());
			if(SaveOnlyStreamingLevel == NoPIELevelName)
			{
				//Can Save?
				if(!EachLevel->IsLevelVisible() || !EachLevel->IsLevelLoaded())
				{
					VSCREENMSG2("Asked to save only 1 streaming level but that level was not loaded or was not visible!", SaveOnlyStreamingLevel);
					return;
				}
				
				//Success!
				FoundStreaming = true;
				break;
			}
		}
		
		//Not Found?
		if(!FoundStreaming)
		{
			VSCREENMSG2("Asked to save only 1 streaming level but that level was not found in streaming list!", SaveOnlyStreamingLevel);
			return;
		}
	}
	
	//~~~
	
	//ASYNC BRANCH
	if(Settings->AsyncSave)
	{
		RamaSave_SaveToFile_ASYNC(FileName, FileIOSuccess, AllComponentsSaved, SaveOnlyStreamingLevel,StaticSaveData);
		return;
		//~~~~
	}
	
	//~~~
	
	AllComponentsSaved = true;
	FileIOSuccess = false;
	
	//~~~~~~~~~~~~~~~
	//		Pre Save 
	//~~~~~~~~~~~~~~~
	RamaSaveComponents.Empty();
	
	//! FILTER OUT ACTORS by STREAMING LEVEL HERE!
	URamaSaveLibrary::GetAllRamaSaveComponents(World,RamaSaveComponents,SaveOnlyStreamingLevel);
		
	int32 CompCountNotBeingSaved = 0;
	
	for(URamaSaveComponent* EachSaveComp : RamaSaveComponents)
	{
		if(!EachSaveComp) continue;
		
		//Keep track of comps that dont want to be saved
		// Done this way for efficiency, to not loop over all comps again or restart/cycle back
		if(!EachSaveComp->RamaSave_ShouldSaveActor)
		{ 
			CompCountNotBeingSaved++;
		}
		
		AActor* ActorOwner = EachSaveComp->GetOwner();
		if(!ActorOwner) continue;
		
	
		//Verify all properties can be saved!
		if(Settings->Saving_PerformObjectValidityChecks) //Might want to skip for faster saving
		{
			if(!URamaSaveLibrary::VerifyActorAndComponentProperties(EachSaveComp))
			{
				AllComponentsSaved = false;
				UE_LOG(RamaSave,Error,TEXT("Rama Save System ~ Cancelling ~ Actor vars could not be saved for %s"), *ActorOwner->GetName());
				VSCREENMSG("Big big Save Error See Log!!!!!   <~~~~~    <~~~~    <~~~~");	
				return;
			}
		}
		
		//~~~~~~~~~~~~~~~~~~~
		//Before Serialization Process Event Starts
		//	 In case the user destroys an actor prior to the save process fully initiating
		EachSaveComp->RamaCPP_PreSave();
		//~~~~~~~~~~~~~~~~~~~
	
		UClass* ClassToCheck = ActorOwner->GetClass();
		 
		//Ensure Owner does not have multiple Rama Save Components
		TArray<URamaSaveComponent*> RSCs;
		ActorOwner->GetComponents<URamaSaveComponent>(RSCs);
		if(RSCs.Num() > 1)
		{
			VSCREENMSG2SEC(10,"Rama Save System ~ Cancelling ~ Actor has more than 1 Rama Save Component! ~ ", ActorOwner->GetName());
			UE_LOG(RamaSave,Error,TEXT("Rama Save System ~ Cancelling ~ Actor has more than 1 Rama Save Component! ~ %s"), *ActorOwner->GetName());
			return;
		}   
		
		//Illegal cases, Player Controller, Player State
		if(URamaSaveUtility::IsIllegalForSavingLoading(ClassToCheck))
		{
			VSCREENMSGSEC(30,"Store global game data in an empty actor class with a Rama Save Component that is spawned into level, or use UE4 Save Object system.");
			VSCREENMSGSEC(30," ");
			VSCREENMSGSEC(30,"The Rama Save Component for each actor can have lots of custom data, but it is meant to be per-instance data");
			VSCREENMSGSEC(30," ");
			VSCREENMSGSEC(30,"The Rama Save System is meant for saving/loading many instances of actors. ");
			VSCREENMSGSEC(30," ");
			VSCREENMSG2SEC(30,"Rama Save System ~ Illegal Class ~ This Class cannot have Rama Save Components ~~~> ", ClassToCheck->GetName());
			
			return; 
			//~~~~
		}
	}
	 
	//~~~ Create Directory Tree! ~~~
	if(!URamaSaveUtility::CreateDirectoryTreeForFile(FileName))
	{
		VSCREENMSG2("Rama Save System ~ File IO Error: Could not create directory for file!", FileName);
		return;
	}

	//~~~
	
	//Have to create Archive at this level, and save the total number of components
	//To then be loaded statically
	TArray<uint8> ToBinary;
	FMemoryWriter MemoryWriter(ToBinary, true);
	
	//~~~ Versioning ~~~
	
	//! #1
	
	// Write version for this file format
	int32 SavegameFileVersion = JOY_SAVE_VERSION;
	MemoryWriter << SavegameFileVersion;		//<~~~ Rama Custom Serialization Version
	
	//! #2
	 
	// Write out engine and UE4 version information
	int32 PackageFileUE4Version = GPackageFileUE4Version;
	MemoryWriter << PackageFileUE4Version;
	FEngineVersion SavedEngineVersion = FEngineVersion::Current();
	MemoryWriter << SavedEngineVersion;
	
	
	//~~~ End Versioning ~~~
	
	//Obj and Name as String
	FObjectAndNameAsStringProxyArchive Ar(MemoryWriter, false);
	
	//~~~
	
	//!#3 Level Streaming
	TArray<FString> Streaming;
	const TArray<ULevelStreaming*>& Levels = World->GetStreamingLevels();
	for(ULevelStreaming* EachLevel : Levels)
	{
		if(!EachLevel) continue;
		
		FString NoPIELevelName = URamaSaveLibrary::RemoveLevelPIEPrefix(EachLevel->GetWorldAssetPackageName());
		 
		/*
		//Only want to know about currently loaded and visible levels!
		if(!EachLevel->IsLevelVisible()) 
		{
			RS_LOG2(RamaSave, "found NON VISIBLE streaming level", NoPIELevelName);
			continue;
		} 
		RS_LOG2(RamaSave, "found visible streaming level", NoPIELevelName);
	 	*/
		
		//StreamingLevelName=Visible
		Streaming.Add(NoPIELevelName + FString("=") + BOOLSTR(EachLevel->IsLevelVisible()));
	}
	Ar << Streaming;
	
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//!#4 STATIC DATA
	uint8 HasStaticData = (StaticSaveData != nullptr) ? 1 : 0;
	Ar << HasStaticData;
	if(StaticSaveData)
	{
		SaveStaticData(Ar, StaticSaveData);
	}
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	
	
	//~~~~~~~~~~~~~~~~~~~
	//! FINAL DO THIS LAST
	//~~~~~~~~~~~~~~~~~~~
	
	//!#4 Component Total 
	int32 TotalComponents = RamaSaveComponents.Num() - CompCountNotBeingSaved;
	Ar << TotalComponents;
 
	//When not visible does not show at all
	/*
	const TArray<class ULevel*>& AllLevels = World->GetLevels();
	for(const ULevel* EachLevel : AllLevels)
	{ 
		if(!EachLevel) continue;
		if(EachLevel->GetOuter()) RS_LOG2(RamaSave,"Package is", EachLevel->GetOuter()->GetName() );
		RS_LOGF(RamaSave,"Actor count for level is", EachLevel->Actors.Num());
		
	}
	*/
	
	//!#5 Serialize All Comps!
	for(URamaSaveComponent* EachSaveComp : RamaSaveComponents)
	{  
		//FString LevelPackageName = EachSaveComp->GetActorStreamingLevelPackageName();
		//RS_LOG2(RamaSave,"Saving Actor in package", LevelPackageName);
		 
		//! CAN NOW FILTER BASED ON PACKAGE NAME IF USER ONLY WANT SAVE PARTICULAR
		 
		/*
		//Default
		FString LevelPackageName = "PersistentLevel"; 
		
		if(!EachSaveComp->GetOwner()->IsInPersistentLevel())
		{
			LevelPackageName = "Huge very big error, Outer for non persistent actor was not found!!!!";
			
			ULevel* LevelForActor = EachSaveComp->GetOwner()->GetLevel();
			 
			//Rama Victory!!!
			UObject* LevelOuter = LevelForActor->GetOuter();
			if(!LevelOuter) continue;
			
			//! LESSON, the only retention /acknowledgement of level streaming is via the outer name, the level name is always "Persistent"
			//! even when the actor says it is not part of persistent level
			LevelPackageName = LevelOuter->GetName();
		}
		*/
		 
		if(!EachSaveComp->RamaSave_SaveToFile(World,Ar))
		{
			AllComponentsSaved = false;
		}
	}
	
	//VSCREENMSGF("TOTAL COMPS SAVED", TotalComponents);
	 
	//IO Success?
	FileIOSuccess = URamaSaveUtility::CompressAndWriteToFile(ToBinary,FileName);
}

void ARamaSaveEngine::RamaSave_SaveToFile_ASYNC(FString FileName, bool& FileIOSuccess, bool& AllComponentsSaved, FString SaveOnlyStreamingLevel, URamaSaveObject* StaticSaveData)
{
	UWorld* World = GetWorld();
	if (!World) return;

	URamaSaveSystemSettings* Settings = URamaSaveSystemSettings::Get();
	
	//Should always be valid!
	check(Settings);
	
	RamaSaveAsync_FileName = FileName;
	RamaSaveAsync_ChunkGoal = Settings->AsyncSaveActorChunkSize;
	RamaSaveAsync_SaveChecks = Settings->Saving_PerformObjectValidityChecks;
		
	//~~~~~~~~~~~~~~~~~~~~~~~
	//In Async Case events report these status flags since it is not synchronous with the node
	AllComponentsSaved = true;
	FileIOSuccess = true;
	//~~~~~~~~~~~~~~~~~~~~~~~
	
	//~~~ Create Directory Tree! ~~~
	if(!URamaSaveUtility::CreateDirectoryTreeForFile(FileName))
	{
		VSCREENMSG2("Rama Save System ~ File IO Error: Could not create directory for file!", FileName);
		return;
	}
	
	//~~~~~~~~~~~~~~~~~~~~~
	//  Write File Header
	//~~~~~~~~~~~~~~~~~~~~~
	
	//Have to create Archive at this level, and save the total number of components
	//To then be loaded statically
	RamaSaveAsync_ToBinary.Empty();
	
	//~~~~~~~~~~~~~~~~~~
	//  Clear Any Prev
	ClearAsyncArchive();
	//~~~~~~~~~~~~~~~~~~
	
	AsyncMemoryWriter = new FMemoryWriter(RamaSaveAsync_ToBinary, false);
	
	//~~~ Versioning ~~~
	 
	//! #1
	
	FMemoryWriter& Ar = *AsyncMemoryWriter;
	
	// Write version for this file format
	int32 SavegameFileVersion = JOY_SAVE_VERSION;
	Ar << SavegameFileVersion;		//<~~~ Rama Custom Serialization Version
	
	//! #2
	 
	// Write out engine and UE4 version information
	int32 PackageFileUE4Version = GPackageFileUE4Version;
	Ar << PackageFileUE4Version;
	FEngineVersion SavedEngineVersion = FEngineVersion::Current();
	Ar << SavedEngineVersion;
	
	//~~~ End Versioning ~~~
	
	//!#3 Level Streaming
	TArray<FString> Streaming;
	const TArray<ULevelStreaming*>& Levels = World->GetStreamingLevels();
	for(ULevelStreaming* EachLevel : Levels)
	{
		if(!EachLevel) continue;
		
		FString NoPIELevelName = URamaSaveLibrary::RemoveLevelPIEPrefix(EachLevel->GetWorldAssetPackageName());
		 
		Streaming.Add(NoPIELevelName + FString("=") + BOOLSTR(EachLevel->IsLevelVisible()));
	}
	Ar << Streaming;
	
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//!#4 STATIC DATA
	uint8 HasStaticData = (StaticSaveData != nullptr) ? 1 : 0;
	Ar << HasStaticData;
	if(StaticSaveData)
	{
		SaveStaticData(Ar, StaticSaveData);
	}
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	
	//~~~~~~~~~~~~~~~~~
	// 		ASYNC
	//~~~~~~~~~~~~~~~~~
	
	RamaSaveComponents.Empty();
	
	//! FILTER OUT ACTORS by STREAMING LEVEL HERE!
	URamaSaveLibrary::GetAllRamaSaveComponents(World,RamaSaveComponents,SaveOnlyStreamingLevel);
	
	int32 CompCountNotBeingSaved = 0;
	for(URamaSaveComponent* EachSaveComp : RamaSaveComponents)
	{
		if(!EachSaveComp) continue;
		
		//Keep track of comps that dont want to be saved
		// Done this way for efficiency, to not loop over all comps again or restart/cycle back
		if(!EachSaveComp->RamaSave_ShouldSaveActor)
		{ 
			CompCountNotBeingSaved++;
		}
	}
	
	//!#5 Component Total 
	RamaSaveAsync_TotalComponents = RamaSaveComponents.Num() - CompCountNotBeingSaved;
	Ar << RamaSaveAsync_TotalComponents;
	
	
	//~~~
	
	Async_SaveStarted(RamaSaveAsync_FileName);
	
		  
	//Obj and Name as String
	AsyncArchive = new FObjectAndNameAsStringProxyArchive(*AsyncMemoryWriter, false);
	
	//! START ASYNC
	RamaSaveAsync_Index = 0;
	SETTIMERH(TH_RamaSaveAsync,ARamaSaveEngine::RamaSaveAsync,Settings->AsyncSaveTickInterval,true);
}

bool ARamaSaveEngine::RamaSaveAsync_Cancel()
{
	bool WasActive = ISTIMERACTIVE(TH_RamaSaveAsync);
	CLEARTIMER(TH_RamaSaveAsync);
	 
	//~~~~~~~~~~~~~~~~~~~
	// Free the Data now  <3 Rama
	ClearAsyncArchive();
	//~~~~~~~~~~~~~~~~~~~
	 
	if(WasActive)
	{
		Async_SaveCancelled(RamaSaveAsync_FileName);
	}
	return WasActive;
}

void ARamaSaveEngine::RamaSaveAsync()
{	
	UWorld* World = GetWorld();
	if(!World) return;
	//~~~~~~~~~~~~~~~
	
	//Progress Update!
	Async_ProgressUpdate(float(RamaSaveAsync_Index)/float(RamaSaveAsync_TotalComponents));
	
	int32 ChunkCount = 0;
	//RamaSaveAsync_ChunkGoal
	
	//Done?
	if(RamaSaveAsync_Index >= RamaSaveComponents.Num())
	{
		Async_ProgressUpdate(1);
		CLEARTIMER(TH_RamaSaveAsync);
		RamaSaveAsync_Finish();
		return;
	}
	
	//Gooo!
	for(; RamaSaveComponents.IsValidIndex(RamaSaveAsync_Index); RamaSaveAsync_Index++)
	{
		if(ChunkCount >= RamaSaveAsync_ChunkGoal)
		{
			//Wait for next tick interval
			return;
		}
		
		//Inc!
		ChunkCount++;
		
		URamaSaveComponent* EachSaveComp = RamaSaveComponents[RamaSaveAsync_Index];
		if(!EachSaveComp) continue;
		
		AActor* ActorOwner = EachSaveComp->GetOwner();
		if(!ActorOwner) continue;
		
		//Verify all properties can be saved!
		if(RamaSaveAsync_SaveChecks) //Might want to skip for faster saving
		{
			if(!URamaSaveLibrary::VerifyActorAndComponentProperties(EachSaveComp))
			{
				UE_LOG(RamaSave,Error,TEXT("Rama Save System ~ Cancelling ~ Actor vars could not be saved for %s"), *ActorOwner->GetName());
				VSCREENMSG("Big big Save Error See Log!!!!!   <~~~~~    <~~~~    <~~~~");	
				RamaSaveAsync_Cancel();
				return;
			}
		}
		
		//~~~~~~~~~~~~~~~~~~~
		//Before Serialization Process Event Starts
		//	 In case the user destroys an actor prior to the save process fully initiating
		EachSaveComp->RamaCPP_PreSave();
		//~~~~~~~~~~~~~~~~~~~
	
		UClass* ClassToCheck = ActorOwner->GetClass();
		  
		//Ensure Owner does not have multiple Rama Save Components
		TArray<URamaSaveComponent*> RSCs;
		ActorOwner->GetComponents<URamaSaveComponent>(RSCs);
		if(RSCs.Num() > 1)
		{
			VSCREENMSG2SEC(10,"Rama Save System ~ Cancelling ~ Actor has more than 1 Rama Save Component! ~ ", ActorOwner->GetName());
			UE_LOG(RamaSave,Error,TEXT("Rama Save System ~ Cancelling ~ Actor has more than 1 Rama Save Component! ~ %s"), *ActorOwner->GetName());
			RamaSaveAsync_Cancel();
			return;
		}   
		
		//Illegal cases, Player Controller, Player State
		if(URamaSaveUtility::IsIllegalForSavingLoading(ClassToCheck))
		{
			VSCREENMSGSEC(30,"Store global game data in an empty actor class with a Rama Save Component that is spawned into level, or use UE4 Save Object system.");
			VSCREENMSGSEC(30," ");
			VSCREENMSGSEC(30,"The Rama Save Component for each actor can have lots of custom data, but it is meant to be per-instance data");
			VSCREENMSGSEC(30," ");
			VSCREENMSGSEC(30,"The Rama Save System is meant for saving/loading many instances of actors. ");
			VSCREENMSGSEC(30," ");
			VSCREENMSG2SEC(30,"Rama Save System ~ Illegal Class ~ This Class cannot have Rama Save Components ~~~> ", ClassToCheck->GetName());
			RamaSaveAsync_Cancel();
			return; 
			//~~~~
		}
		
		 
		//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
		//  RAMA SAVE COMPONENT ACTUAL SERIALIZATION IS HERE
		//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
		bool Success = EachSaveComp->RamaSave_SaveToFile(World,*AsyncArchive);
		//if(!Success) report this
		//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	}
}
	
void ARamaSaveEngine::RamaSaveAsync_Finish()
{  
	//URamaSaveUtility::CompressAndWriteToFile(RamaSaveAsync_ToBinary,RamaSaveAsync_FileName);
	 
	//! Yes much faster, no delay at all
	//FFileHelper::SaveArrayToFile(RamaSaveAsync_ToBinary, *RamaSaveAsync_FileName);
	
	//VSCREENMSG("Faster with no compression?");
	//Remember when I offer uncompressed I have to remember to also LOAD optionally compressed or uncompressed
	
	RamaSaveCompressedTask::Gooooo(RamaSaveAsync_ToBinary,RamaSaveAsync_FileName);
	SETTIMERH(TH_CheckCompressToFileFinished, ARamaSaveEngine::CheckCompressToFileFinished,0.01,true);
	
}

void ARamaSaveEngine::CheckCompressToFileFinished()
{ 
	if(RamaSaveCompressedTask::TasksAreComplete())
	{
		CLEARTIMER(TH_CheckCompressToFileFinished);
		
		//BP
		Async_SaveFinished(RamaSaveAsync_FileName);
		
		//~~~~~~~~~~~~~~~~~~~
		// Free the Data now  <3 Rama
		ClearAsyncArchive();
		//~~~~~~~~~~~~~~~~~~~
	}
}
	
void ARamaSaveEngine::ClearAsyncArchive()
{ 
	if(AsyncArchive) 
	{
		AsyncArchive->Close();
		delete AsyncArchive;
		AsyncArchive = nullptr;
	}
	if(AsyncMemoryWriter)
	{
		AsyncMemoryWriter->Close();
		delete AsyncMemoryWriter;
		AsyncMemoryWriter = nullptr;
	}
}

//~~~~~~~~~~~~~~~~~~~
// 		LOADING
//~~~~~~~~~~~~~~~~~~~
void ARamaSaveEngine::Phase1(const FRamaSaveEngineParams& Params, bool HandleStreamingLevelsLoadingAndUnloading)
{
	if(!GetWorld()) return;
	  
	//~~~~~~~~~~~~~~~~~~~
	// Clear Any Async Save
	CLEARTIMER(TH_RamaSaveAsync);
	ClearAsyncArchive();
	//~~~~~~~~~~~~~~~~~~~
	
	LoadParams = Params;
	
	//User doesnt want async level streaming handling?
	if(!HandleStreamingLevelsLoadingAndUnloading)
	{
		Phase2();
		return;
	}

	//Just handle Persistent?
	if(Params.LoadOnlyStreamingLevel == "PersistentLevel")
	{
		Phase2();
		return;
	}
		
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//Async load and unload of streaming levels
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

	TArray<FString> StreamingLevelsStates;
	bool FileIOSuccess = false;
	int32 TotalSublevels = URamaSaveLibrary::RamaSave_LoadStreamingStateFromFile(this, FileIOSuccess, Params.FileName, StreamingLevelsStates);
	
	//No streaming data
	if(TotalSublevels < 1)
	{
		Phase2();
		return;
	}
	
	//Only Persistent?
	if(TotalSublevels == 1 && StreamingLevelsStates[0] == "PersistentLevel")
	{
		//Only persistent was found!
		Phase2();
		return;
	}
	
	//Clear any prev
	Load_StreamingLevels.Empty();
	
	const TArray<ULevelStreaming*>& Levels = GetWorld()->GetStreamingLevels();
	for(ULevelStreaming* EachLevel : Levels)
	{
		if(!EachLevel) continue;
		
		FString NoPIELevelName = URamaSaveLibrary::RemoveLevelPIEPrefix(EachLevel->GetWorldAssetPackageName());
		
		//Iterate Levels!
		for(FString& EachSublevel : StreamingLevelsStates)
		{
			FString LevelName,State;
			EachSublevel.Split(TEXT("="),&LevelName,&State);
			 
			//Match?
			if(NoPIELevelName != LevelName) continue;
			//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
			 
			//Only Single?
			if(Params.LoadOnlyStreamingLevel != "" && Params.LoadOnlyStreamingLevel != NoPIELevelName)
			{
				continue;
			}
			
			//!TESTING
			//RS_LOG2(RamaSave,"ahm?", EachSublevel);
			//RS_LOG2(RamaSave,LevelName, State);
			  
			//Make Visible
			if(State.Contains("True"))
			{
				//Only do the async process if needed!
				if(!EachLevel->ShouldBeVisible() || !EachLevel->ShouldBeLoaded() )
				{
					Load_StreamingLevels.Add(EachLevel);
				}
				
				
				//!TESTING
				//RS_LOG2(RamaSave,"Told this level to load!", NoPIELevelName);
				 
				EachLevel->SetShouldBeVisible(true);
				EachLevel->SetShouldBeLoaded(true);
			}
			
			//Hide!
			else
			{ 
				//Modify all if none specified!
				if(Params.LoadOnlyStreamingLevel == "") 
				{
					EachLevel->SetShouldBeVisible(false);
				}
			}
		}
	}
	
	//Only unloading?
	if(Load_StreamingLevels.Num() < 1)
	{
		Phase2();
		return;
	}
	 
	
	//! START TIMER AND QUERY 
	AsyncStartTime = GetWorld()->GetTimeSeconds();
	GetWorldTimerManager().SetTimer(TH_AsyncStreamingLoad, this, &ARamaSaveEngine::AsyncStreamingLoad, 0.1, true);
}

void ARamaSaveEngine::AsyncStreamingLoad()
{
	if(!GetWorld()) return;
	 
	bool DoneLoading = true;
	for(ULevelStreaming* EachLevel : Load_StreamingLevels)
	{
		//Not loaded yet?
		if(EachLevel->GetLoadedLevel() == nullptr || !EachLevel->IsLevelVisible() || !EachLevel->IsLevelLoaded())
		{
			DoneLoading = false;
			 
			//TESTING
			//FString NoPIELevelName = URamaSaveLibrary::RemoveLevelPIEPrefix(EachLevel->GetWorldAssetPackageName());
			//RS_LOG2(RamaSave,"Rama Save System Async Load ~ Waiting for level to load ", NoPIELevelName);
			  
			//Max Time Out?
			if(GetWorld()->GetTimeSeconds() - AsyncStartTime > 30)
			{
				FString NoPIELevelName = URamaSaveLibrary::RemoveLevelPIEPrefix(EachLevel->GetWorldAssetPackageName());
				RS_LOG2(RamaSave,"ERROR >>>> Rama Save System Async Load ~ Reached max time out of 30 sec and loaded anyway! ", NoPIELevelName);
				DoneLoading = true;
				break;
			}
	
			return;
			//~~~~
			//~~~~
			//~~~~
		}
	}
	 
	if(DoneLoading)
	{
		GetWorldTimerManager().ClearTimer(TH_AsyncStreamingLoad);
		
		//~~~~~~~~
		//~~~~~~~~
		//~~~~~~~~
		Phase2();
		//~~~~~~~~
		//~~~~~~~~
		//~~~~~~~~
	}
}

void ARamaSaveEngine::Phase2()
{
	//Victory Decompress File
	TArray<uint8> Uncompressed_FromBinary;
	if( !URamaSaveUtility::DecompressFromFile(LoadParams.FileName,Uncompressed_FromBinary))
	{
		//File could not be loaded!
		return;
	}
	//~~~~~~~~~~~~~~~~~~~
	
	
	
	//~~~
	
	bool AllComponentsLoaded = true;
	
	FMemoryReader MemoryReader(Uncompressed_FromBinary, true);

	 
	//~~~ Versioning ~~~
	
	//! #1
	// Read version for this file format
	int32 SavegameFileVersion;
	MemoryReader << SavegameFileVersion;
 
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//For global access during load process
	ARamaSaveEngine::LoadedSaveVersion = SavegameFileVersion;
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	
	//Dont allow loading of older file versions
	//	Currently just aborting, can add more robust support in future
	if( SavegameFileVersion < JOY_SAVE_VERSION)
	{ 
		RS_LOGF(RamaSave,"This file was saved with older file version, file version found was: ", SavegameFileVersion );
		RS_LOGF(RamaSave,"Current Rama Save System version is:", (float)JOY_SAVE_VERSION);
		RS_LOG2(RamaSave,"File name is: ", LoadParams.FileName);
		RS_LOG(RamaSave,"This file will be updated the next time a save occurs, but if something crashes and/or your computer explodes let me know.");
	}
	 
	//Clear Level?
	if(LoadParams.DestroyActorsBeforeLoad)
	{
		URamaSaveLibrary::RamaSave_ClearLevel(GetWorld(),LoadParams.DontLoadPlayerPawns,LoadParams.LoadOnlyStreamingLevel); //Dont destroy existing player pawns because they are also not loaded.
	}
 	 
	//! #2
	// Read engine and UE4 version information
	
	int32 SavedUE4Version;
	MemoryReader << SavedUE4Version;

	FEngineVersion SavedEngineVersion;
	MemoryReader << SavedEngineVersion;

	//Set The Versions for the Memory Reader!
	MemoryReader.SetUE4Ver(SavedUE4Version);
	MemoryReader.SetEngineVer(SavedEngineVersion);
	
	//~~~ End Versioning ~~~
	
	//Obj and Name as String
	FObjectAndNameAsStringProxyArchive Ar(MemoryReader, true);
	 
	 
	//!#3 Level Streaming, have to process
	TArray<FString> Streaming;
	if(SavegameFileVersion >= JOY_SAVE_VERSION_STREAMINGLEVELS)
	{
		//Load Streaming Levels!
		Ar << Streaming;
	}
	
	//~~~~~~~~~~~~~~~~~ 
	//!#4
	if(SavegameFileVersion >= JOY_SAVE_VERSION_SAVEOBJECT)
	{ 
		uint8 HasStaticData = 0;
		Ar << HasStaticData;
		if(HasStaticData)
		{
			SkipStaticData(Ar);
		}
	}
	//~~~~~~~~~~~~~~~~~
	
	//!#5 Component Total
	int32 TotalComponents = 0;
	Ar << TotalComponents;
	
	
	//VSCREENMSGF("Load process got here! Comps to load is", TotalComponents);
	
	//!#6 All Comps!
	//For Loop to load each entry statically
	TArray<URamaSaveComponent*> LoadedComps;
	for(int32 v = 0; v < TotalComponents; v++)
	{
		LoadedComps.AddZeroed(1);
		if(!URamaSaveComponent::RamaSave_LoadFromFile(GetWorld(), SavegameFileVersion, LoadParams.LoadOnlyActorsWithSaveTags, Ar, LoadedComps.Last(), LoadParams.DontLoadPlayerPawns,LoadParams.LoadOnlyStreamingLevel))
		{
			//At least one component was not loaded!
			AllComponentsLoaded = false;
			LoadedComps.Pop();
		}
	}
	  
	//!Temp solution for the fact that some BP classes are not loading correctly the first time
	//! if all the actor instances of that BP were destroyed _during runtime_
	//! 		After GC period of time, the BP class itself seems to get unlaoded and it takes two calls
	//!			to StaticLoadClass to get it to be found
	if(!AllComponentsLoaded)
	{ 
		UE_LOG(RamaSave, Warning,TEXT("\n\n\n"));
		UE_LOG(RamaSave, Warning,TEXT("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"));
		UE_LOG(RamaSave, Warning,TEXT("Not all components were loaded from file: %s"), *LoadParams.FileName);
		UE_LOG(RamaSave, Warning,TEXT("Perhaps a class/BP was removed, but still present in the save game?"));
		UE_LOG(RamaSave, Warning,TEXT("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"));
		UE_LOG(RamaSave, Warning,TEXT("\n\n\n"));
	}
	 
	//~~~~~~~~~~~~~~~~
	//		Post Load 
	//~~~~~~~~~~~~~~~~
	
	//Fully Load Comps
	// 	Now Serialization is fully done! 
	//	User can do as they please with the created components. 
	for(URamaSaveComponent* EachSaveComp : LoadedComps)
	{ 
		if(!EachSaveComp) continue;
		 
		EachSaveComp->FullyLoaded();
	}
}



//~~~

void ARamaSaveEngine::SaveStaticData(FArchive& Ar, URamaSaveObject* StaticData)
{
	if(!StaticData->IsValidLowLevelFast() || StaticData->IsPendingKill())
	{
		return;
	}
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	
	URamaSaveSystemSettings* Settings = URamaSaveSystemSettings::Get();
	
	//Should always be valid!
	check(Settings);
	
	//~~~ Save the Object Class ~~~
	//! #1
	FString ClassFullPath = FStringClassReference(StaticData->GetClass()).ToString();
	Ar << ClassFullPath;
	
	//! #2
	int64 StaticDataSkipPos = Ar.Tell();
	Ar << StaticDataSkipPos;
	
	//! #3
	//Serialize the total count, even if it is 0! 
	int64 TotalProperties = 0;
	int64 PropertiesBegin = Ar.Tell();		//cant assume starting at 0!
	Ar << TotalProperties;
	
	for (TFieldIterator<UProperty> It(StaticData->GetClass()); It; ++It)
	{
		UProperty* Property = *It;
		FString PropertyNameString = Property->GetFName().ToString();
  
		//Delegate? 
		if(Property->IsA(UMulticastDelegateProperty::StaticClass()))
		{
			//Dont serialize Delegates! (It breaks them)
			continue;
		}  
		
		//The Uber Graph Frame
		if(PropertyNameString.Contains("UberGraphFrame"))
		{
			continue;
		}
		 
		//Here's how to check if something is being saved that shouldn't be
		if(Settings->LogSavingAndLoadingOfEachStaticDataProperty)
		{
			UE_LOG(RamaSave, Log, TEXT("%s ~ Saving Static Data Property: %s"), *StaticData->GetClass()->GetName(), *PropertyNameString);
		}
		 
		//We want each property as pure binary data 
		//		so we can easily save it to disk and not worry about its exact type!
		uint8* InstanceValuePtr = Property->ContainerPtrToValuePtr<uint8>(StaticData); 
		
		int64 StartAfterStringPos = 0; //Ar POSITION TO STORE THE END POS AT
		int64 EndPos = 0; 						//Postion after serializing property
		
		//Serialize Instance of Property!
		Ar << PropertyNameString;
		StartAfterStringPos = Ar.Tell();
		Ar << EndPos;
		 
		Property->SerializeItem(FStructuredArchiveFromArchive(Ar).GetSlot(),InstanceValuePtr); 
		
		EndPos = Ar.Tell();
		Ar.Seek(StartAfterStringPos);
		Ar << EndPos;
		Ar.Seek(EndPos);
		
		TotalProperties++;
	} 
	 
	//Put the total at the beginning of archive so know how big of for loop to amke
	int64 ArchiveEnd = Ar.Tell();
	Ar.Seek(PropertiesBegin);
	Ar << TotalProperties;
	
	Ar.Seek(StaticDataSkipPos);
	Ar << ArchiveEnd;
	
	//Move back to end of archive 
	//		so as to not ovewrite all data 
	//			when next archive entries occur!!
	Ar.Seek(ArchiveEnd); //<~~~ !
}

void ARamaSaveEngine::SkipStaticData(FArchive& Ar)
{
	//!#1
	FString ClassFullPath;
	Ar << ClassFullPath;

	//! #2
	int64 StaticDataSkipPos;
	Ar << StaticDataSkipPos;
	
	Ar.Seek(StaticDataSkipPos);
}

URamaSaveObject* ARamaSaveEngine::LoadStaticData(bool& FileIOSuccess,  FString FileName)
{
	URamaSaveSystemSettings* Settings = URamaSaveSystemSettings::Get();
	
	//Should always be valid!
	check(Settings);

	//~~~~
	
	FileIOSuccess = false;
	
	//Victory Decompress File
	TArray<uint8> Uncompressed_FromBinary;
	if( !URamaSaveUtility::DecompressFromFile(FileName,Uncompressed_FromBinary))
	{
		//File could not be loaded!
		return nullptr;
	}
	FMemoryReader MemoryReader(Uncompressed_FromBinary, true);
	
	//~~~ Versioning ~~~
	
	//! #1
	// Read version for this file format
	int32 SavegameFileVersion;
	MemoryReader << SavegameFileVersion;
 
	if(SavegameFileVersion < JOY_SAVE_VERSION_SAVEOBJECT)
	{
		VSCREENMSG("Save file is from a prior Rama Save System version, no Static Data to load!");
		return nullptr;
	}
	
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//For global access during load process
	ARamaSaveEngine::LoadedSaveVersion = SavegameFileVersion;
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	 
	//! #2
	// Read engine and UE4 version information
	
	int32 SavedUE4Version;
	MemoryReader << SavedUE4Version;

	FEngineVersion SavedEngineVersion;
	MemoryReader << SavedEngineVersion;

	//Set The Versions for the Memory Reader!
	MemoryReader.SetUE4Ver(SavedUE4Version);
	MemoryReader.SetEngineVer(SavedEngineVersion);
	
	//~~~ End Versioning ~~~
	
	//Obj and Name as String
	FObjectAndNameAsStringProxyArchive Ar(MemoryReader, true);
	 
	 
	//!#3 Level Streaming, have to process
	TArray<FString> Streaming;
	if(SavegameFileVersion > 3)
	{
		//Load Streaming Levels!
		Ar << Streaming;
	}
	
	//~~~
	
	FileIOSuccess = true;
	
	uint8 HasStaticData;
	Ar << HasStaticData;
	if(!HasStaticData)
	{
		UE_LOG(RamaSave, Error,TEXT("Tried to load Static Data for a file that did not have any! %s"), *FileName);
		return nullptr;
	}
	
	//~~~ Create the Object ~~~
	//! #1
	FString ClassFullPath;
	Ar << ClassFullPath;

	//! #2
	int64 StaticDataSkipPos;
	Ar << StaticDataSkipPos;
	
	if(ClassFullPath == "")
	{
		UE_LOG(RamaSave, Error,TEXT("Static Object Class was blank!!!"));
		
		//Skip! Essential to maintain integrity of load process!
		Ar.Seek(StaticDataSkipPos);
	
		return nullptr;
	}
	
	//Load Static Class
	UClass* LoadedRamaSaveObjectClass = StaticLoadClass(UObject::StaticClass(), NULL, *ClassFullPath, NULL, LOAD_None, NULL);  
	 
	if(LoadedRamaSaveObjectClass == NULL)
	{
		UE_LOG(RamaSave, Error,TEXT("Static Object Class not found, was it renamed or removed? %s"), *ClassFullPath);
		
		//Skip! Essential to maintain integrity of load process!
		Ar.Seek(StaticDataSkipPos);
	
		return nullptr;
	}
	   
	URamaSaveObject* RSO = NewObject<URamaSaveObject>(GetTransientPackage(),LoadedRamaSaveObjectClass); //nullptr = transient package
	if(!RSO)
	{
		UE_LOG(RamaSave, Error,TEXT("Static Object could not be created!!!! Tell Rama! %s"), *ClassFullPath);
		
		//Skip! Essential to maintain integrity of load process!
		Ar.Seek(StaticDataSkipPos);
	
		return nullptr;
	}
	
	//! #3
	int64 TotalProperties = 0;
	Ar << TotalProperties;
		
	for(int64 v = 0; v < TotalProperties; v++)
	{
		//Get info about each property before deciding whether to serialize
		FString PropertyNameString;
		int64 EndPosToSkip;
		Ar << PropertyNameString;
		Ar << EndPosToSkip; 
			 
		UProperty* Property = FindField<UProperty>( RSO->GetClass(), *PropertyNameString );
		if(Property) 
		{ 
			uint8* InstanceValuePtr = Property->ContainerPtrToValuePtr<uint8>(RSO); 
			
			//Serialize Instance!
			Property->SerializeItem(FStructuredArchiveFromArchive(Ar).GetSlot(),InstanceValuePtr);
			
			if(Settings->LogSavingAndLoadingOfEachStaticDataProperty)
			{
				UE_LOG(RamaSave, Log, TEXT("%s ~ Loading Static Data Property: %s"), *RSO->GetClass()->GetName(), *PropertyNameString);
			}
		}
		else
		{
			//SKIP TO END OF THIS PROPERTY AS IT WAS NOT FOUND IN ACTUAL CLASS
			// MUST HAVE BEEN REMOVED
			Ar.Seek(EndPosToSkip);
			 
			UE_LOG(RamaSave,Warning,TEXT("Property in save file but not found in class, re-save to get rid of this message %s %s"), *RSO->GetClass()->GetName(), *PropertyNameString);
		}
	}
	 
	return RSO;
}
	