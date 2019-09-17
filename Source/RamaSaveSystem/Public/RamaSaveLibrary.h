// Copyright 2015 by Nathan "Rama" Iyer. All Rights Reserved.

#pragma once

#include "RamaSaveComponent.h"
#include "RamaSaveEngine.h"

#include "RamaSaveLibrary.generated.h"



UCLASS()
class RAMASAVESYSTEM_API URamaSaveLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	
	static ARamaSaveEngine* GetOrCreateRamaEngine(UWorld* World);
	
	/** 
		This returns actual level name after removing any path and any renaming that occurred for PIE (play in editor). 
		
		This is the name you can use with GetStreamingLevel for any level that is in your levels list! 
		
		This returns the same exact name whether you get the level name in PIE or in standalone or packaged game.
		
		<3 Rama 
	*/ 
	UFUNCTION(Category="Rama Save System", BlueprintPure)
	static FString RemoveLevelPIEPrefix(const FString& LevelName);

	/** 
		All Rama Save Components are found and their owning actors and all variables that you specify via the string array RamaSave_VarsToSave are saved into a single file! 
	
		@param SaveOnlyStreamingLevel Optional param to save only actors in a particular streaming level! Use "PersistentLevel" to save only non-streaming main level actors.
		
		@param StaticSaveData For any simple data that is not associated with an actor that you want to be able to load even before the world has finished being created,  make a BP of my RamaSaveObject class and put all your data there! You can use Construct Object to create an instance of the RamaSaveObject and use it any where you like, then pass that instance to this save function at the time of saving.
		
		<3 Rama 
	*/
	UFUNCTION(Category="Rama Save System", BlueprintCallable,meta=(WorldContext="WorldContextObject"))
	static void RamaSave_SaveToFile(
		UObject* WorldContextObject, 
		FString FileName, 
		bool& FileIOSuccess, 
		bool& AllComponentsSaved, 
		FString SaveOnlyStreamingLevel="",
		URamaSaveObject* StaticSaveData = nullptr
	);
	
	/** If you are using Async Saving then you can cancel after starting (and before it was going to finish) using this node! Returns true if an async save was in progress and was cancelled, false if no save was in process. */
	UFUNCTION(Category="Rama Save System", BlueprintCallable,meta=(WorldContext="WorldContextObject"))
	static bool RamaSave_CancelAsyncSaveProcess(UObject* WorldContextObject);
	
	/**
		Use this node with the same filename as your regular save data, but this node will extract only the RamaSaveObject Static Data, if you saved any with your file.
		
		The advantage of this node is that you can run this node before the world has been fully loaded, as this node loads only pure data!
		
		~ Tip ~
		You can cast the output of this node to your chosen RamaSaveObject BP or C++ class to retrieve all variable members!
		
		<3 Rama
	*/
	UFUNCTION(Category="Rama Save System", BlueprintCallable)
	static URamaSaveObject* RamaSave_LoadStaticDataFromFile(bool& FileIOSuccess,  FString FileName);
	
	
	/** 
		DestroyActorsBeforeLoad is how you specify whether actors that are being loaded should have any prior instances destroyed before load. 
		
		Actors are found and destroyed based on whether they have a Rama Save Component with DestroyBeforeLoad set to true. 
		
		@param LoadOnlyStreamingLevel Optional param to only load actors from a specific streaming level! Use "PersistentLevel" to load only non-streaming main level actors.
		
		<3 Rama
	*/
	UFUNCTION(Category="Rama Save System", BlueprintCallable,meta=(WorldContext="WorldContextObject"))
	static void RamaSave_LoadFromFile(UObject* WorldContextObject, bool& FileIOSuccess,  FString FileName, bool DestroyActorsBeforeLoad = true, bool DontLoadPlayerPawns = false, bool HandleStreamingLevelsLoadingAndUnloading = true, FString LoadOnlyStreamingLevel="");
	
	/** 
		~~~ Level Streaming File Information Acquisition (non destructive, informational only) ~~~
	
		Load just the streaming levels information from a file, without loading or unloading any levels and without loading any actors. 
		
		This is purely information acquisition so you can handle level streaming in your own very pretty way. 
		
		The format I give you is an array of
		
		LevelStreamingName=Visible
		
		so you will see
		
		YourLevel=true
		YourLevel2=false
		
		Use "Split String" on the resultant string with "=" as your text to split on and then you can test if the right result == "true" and use the Left result as the Streaming Level name to load or unload!
			
		@return The number of Streaming Level States :)
		
		<3 Rama 
	*/
	UFUNCTION(Category="Rama Save System", BlueprintCallable,meta=(WorldContext="WorldContextObject"))
	static int32 RamaSave_LoadStreamingStateFromFile(UObject* WorldContextObject, bool& FileIOSuccess, FString FileName, TArray<FString>& StreamingLevelsStates);
	
	/** If you supply any tags via LoadOnlyActorsWithSaveTags, only those actors will be loaded, otherwise all actors will be loaded.
	
		Please note these "Save Tags" are stored on the Rama Save Component and have no relationship to the actor tags stored at the actor level that come with UE4.
	
		The save tag system is all-inclusive, so if you supply multiple tags, if ANY of the tags are found in an actor's Rama Save Component, then the actor having those tags will be loaded.
		
		DestroyActorsBeforeLoad is how you specify whether actors that are being loaded should have any prior instances destroyed before load. Actors are found and destroyed based on whether they have a Rama Save Component with DestroyBeforeLoad set to true. 
		
		@param LoadOnlyStreamingLevel Optional param to only load actors from a specific streaming level! Use "PersistentLevel" to load only non-streaming main level actors.
		
		<3 RAma
	*/
	UFUNCTION(Category="Rama Save System", BlueprintCallable,meta=(WorldContext="WorldContextObject", AutoCreateRefTerm = "LoadOnlyActorsWithSaveTags"))
	static void RamaSave_LoadFromFileWithTags(UObject* WorldContextObject, const TArray<FString>& LoadOnlyActorsWithSaveTags, bool& FileIOSuccess, FString FileName, bool DestroyActorsBeforeLoad = true, bool DontLoadPlayerPawns = false, bool HandleStreamingLevelsLoadingAndUnloading = true, FString LoadOnlyStreamingLevel="");
	
	/** Get an array of all actors that have Rama Save Components! Useful for iterating over all actors that will be saved / have just been loaded. */
	UFUNCTION(Category="Rama Save System", BlueprintCallable,meta=(WorldContext="WorldContextObject"))
	static void GetAllRamaSaveActors(UObject* WorldContextObject, TArray<AActor*>& RamaSaveActors);
	
	/** Get an array of all actors that have Rama Save Components! Useful for iterating over all actors that will be saved / have just been loaded. 
	
		The save tag system is all-inclusive, so if you supply multiple tags, if ANY of the tags are found in an actor's Rama Save Component, then the actor will be returned.		
	*/
	UFUNCTION(Category="Rama Save System", BlueprintCallable,meta=(WorldContext="WorldContextObject", AutoCreateRefTerm = "SaveTags"))
	static void GetAllRamaSaveActorsWithTags(UObject* WorldContextObject, const TArray<FString>& SaveTags, TArray<AActor*>& RamaSaveActors);
	
	/** 
		Get an array of all Rama Save Components! 
		
		Useful for iterating over all components that will be saved / have just been loaded. 
		
		@param GetOnlyStreamingLevel Optional filter to only get components for actors in a specific sublevel that is present in your levels list! Use "PersistentLevel" for non-streaming main level.
		
		<3 Rama
	*/
	UFUNCTION(Category="Rama Save System", BlueprintCallable,meta=(WorldContext="WorldContextObject"))
	static void GetAllRamaSaveComponents(UObject* WorldContextObject, TArray<URamaSaveComponent*>& RamaSaveComponents, FString GetOnlyStreamingLevelName="");
	
	/** Get an array of all Rama Save Components! Useful for iterating over all components that will be saved / have just been loaded. 
	
		The save tag system is all-inclusive, so if you supply multiple tags, if ANY of the tags are found in an actor's Rama Save Component, then the component will be returned.	
		
		@param GetOnlyStreamingLevel Optional filter to only get components for actors in a specific sublevel that is present in your levels list! Use "PersistentLevel" for non-streaming main level.
		
		<3 Rama
	*/
	UFUNCTION(Category="Rama Save System", BlueprintCallable,meta=(WorldContext="WorldContextObject", AutoCreateRefTerm = "SaveTags"))
	static void GetAllRamaSaveComponentsWithTags(UObject* WorldContextObject, const TArray<FString>& SaveTags,  TArray<URamaSaveComponent*>& RamaSaveComponents, FString GetOnlyStreamingLevelName="");
	
	/** 
		Clear all actors out of the world that have a RamaSaveComponent with DestroyBeforeLoad set to true! 
	
		This gets called by RamaSave_LoadFromFile automatically if DestroyActorsBeforeLoad is set to true.
		
		@param ClearOnlyStreamingLevel Optional Param to clear only actors in a specific streaming level! Use "PersistentLevel" to only clear actors in main non-streaming level.
		
		This is done automatically if you use a streaming level with the LoadFromFile BP node, and if you also specify DestroyActorsBeforeLoad as true :)
		
		<3 Rama
	*/
	UFUNCTION(Category="Rama Save System", BlueprintCallable,meta=(WorldContext="WorldContextObject"))
	static void RamaSave_ClearLevel(UObject* WorldContextObject, bool DontDestroyPlayers = false, FString ClearOnlyStreamingLevel="");
	
//~~~~~~~~~~~~~~~~~~
// File Management
//  ♥ Rama
//~~~~~~~~~~~~~~~~~~
public:
	UFUNCTION(BlueprintPure, Category = "Rama Save System|File IO")
	static FORCEINLINE bool RamaSave_FileExists(const FString& File)
	{
		return FPlatformFileManager::Get().GetPlatformFile().FileExists(*File);
	}
	
	UFUNCTION(BlueprintPure, Category = "Rama Save System|File IO")
	static FORCEINLINE bool RamaSave_FolderExists(const FString& Dir)
	{
		return FPlatformFileManager::Get().GetPlatformFile().DirectoryExists(*Dir);
	}
	
	UFUNCTION(BlueprintPure, Category = "Rama Save System|File IO")
	static FORCEINLINE FDateTime RamaSave_GetFileTimeStamp(const FString& File)
	{
		return FPlatformFileManager::Get().GetPlatformFile().GetTimeStamp(*File);
	}
	  
	UFUNCTION(BlueprintPure, Category = "Rama Save System|File IO")
	static FORCEINLINE FText RamaSave_GetFileTimeStampText(const FString& File, bool Verbose)
	{
		FDateTime FileTimeStamp = FPlatformFileManager::Get().GetPlatformFile().GetTimeStamp(*File);
		
		//Turn UTC into local ♥ Rama
		FTimespan UTCOffset = FDateTime::Now() - FDateTime::UtcNow();
		FileTimeStamp += UTCOffset;
		//♥ Rama
		 
		if(Verbose)
		{
			return FText::AsDateTime(FileTimeStamp, EDateTimeStyle::Long, EDateTimeStyle::Medium, FText::GetInvariantTimeZone());
		} 
		return FText::AsDateTime(FileTimeStamp, EDateTimeStyle::Short, EDateTimeStyle::Short,FText::GetInvariantTimeZone());
	}
	
	UFUNCTION(BlueprintCallable, Category = "Rama Save System|File IO")
	static FORCEINLINE bool RamaSave_DeleteFile(const FString& File)
	{ 
		return FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*File);
	}
	
	UFUNCTION(BlueprintCallable, Category = "Rama Save System|File IO")
	static FORCEINLINE bool RamaSave_CopyFile(const FString& Dest,const FString& Src)
	{ 
		return FPlatformFileManager::Get().GetPlatformFile().CopyFile(*Dest,*Src);
	}
	
	UFUNCTION(BlueprintCallable, Category = "Rama Save System|File IO")
	static FORCEINLINE bool RenameFile(const FString& Dest, const FString& Source)
	{
		//Make sure file modification time gets updated!
		FPlatformFileManager::Get().GetPlatformFile().SetTimeStamp(*Source,FDateTime::Now());
		 
		return FPlatformFileManager::Get().GetPlatformFile().MoveFile(*Dest,*Source);
	}
	
	/**
	* This is my very own Get Files in Directory algorithm!
	*
	* Recursive ~ If you choose recursive, all files in all subfolders of the BaseDir will be included as well!
	* I also give you back absolute file paths.
	*
	* If recursive is false, then you are given only the file name itself and most concatenate that with the BaseDir to obtain the absolute path to your file.
	*
	* FilterByExtension ~ If this is not blank, then only files with exactly matching extension (regardless of case) will be returned.
	*
	* ♥ Rama
	*/
	UFUNCTION(BlueprintPure, Category = "Rama Save System|File IO")
	static bool RamaFileIO_GetFiles(
		const FString& FullPathOfBaseDir, 
		TArray<FString>& FilenamesOut, 
		bool Recursive=false, 
		const FString& FilterByExtension = ""
	);
	 
//DateTime
public:
	
	UFUNCTION(BlueprintPure, Category = "Rama Save System|File IO")
	static void RamaSave_DateTimeToString(const FDateTime& TheDateTime, FString& AsString)
	{
		AsString = TheDateTime.ToString();
	}
	
	UFUNCTION(BlueprintPure, Category = "Rama Save System|File IO")
	static void RamaSave_DateTimeFromString(const FString& AsString, FDateTime& TheDateTime)
	{
		FDateTime::Parse(AsString,TheDateTime);
	}
	
//Relative Project Paths
public:
	/** Platform-specific user documents folder, not specific to Windows, works on any platform that Epic supports for this functionality. */
	UFUNCTION(BlueprintPure, Category = "Rama Save System|Paths")
	static FString GetDocumentsFolder()
	{ 
		return FPlatformProcess::UserDir();
	}
	
	/** The location of your game's .exe even if the user moves your entire game file structure at any time! InstallDir/GameName/Binaries/OS such as Win64 or Win32, different on Linux/Mac/Mobile */
	UFUNCTION(BlueprintPure, Category = "Rama Save System|Paths")
	static FString RamaSavePaths_BinaryLocation()
	{
		return FString(FPlatformProcess::BaseDir());
	}
	 
	/** Use these nodes to always save files relative to the actual location of your game's .exe! InstallDir/GameName */
	UFUNCTION(BlueprintPure, Category = "Rama Save System|Paths")
	static FString RamaSavePaths_GameRootDirectory()
	{
		return FPaths::ConvertRelativePathToFull(FPaths::GameDir());
	}

	/** Use these nodes to always save files relative to the actual location of your game's .exe! InstallDir/GameName/Saved */
	UFUNCTION(BlueprintPure, Category = "Rama Save System|Paths")
	static FString RamaSavePaths_SavedDir()
	{
		return FPaths::ConvertRelativePathToFull(FPaths::GameSavedDir());
	}
	
	
public:
	static bool VerifyActorAndComponentProperties(URamaSaveComponent* SaveComp);
	
};