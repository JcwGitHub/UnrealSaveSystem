// Copyright 2015 by Nathan "Rama" Iyer. All Rights Reserved.

#pragma once
 
#include "JoySaveClassFuncLine.h"
#include "PlatformFilemanager.h"
#include "RamaSaveUtility.generated.h"

#define  PLATFORM_HTML5_BROWSER 0
/*
	C++ Static Function Library Class for Rama Save System
*/

//Timers!
#define SETTIMERH(handle, param1,param2,param3) (GetWorldTimerManager().SetTimer(handle, this, &param1, param2, param3))
#define CLEARTIMER(handle) (GetWorldTimerManager().ClearTimer(handle))
#define ISTIMERACTIVE(handle) (GetWorldTimerManager().IsTimerActive(handle))
#define GETTIMERTIMEALIVE(handle) (FMath::Abs(GetWorldTimerManager().GetTimerElapsed(handle)))

UCLASS()
class URamaSaveUtility : public UObject
{
	GENERATED_BODY()
public:
	
	//Illegal Cases
	static bool IsIllegalForSavingLoading(UClass* Class);

public:
	/*
		Destroy Object or Actor instantly
	*/
	static FORCEINLINE void VDestroy(UObject* ToDestroy)
	{
		if ( ! ToDestroy || !ToDestroy->IsValidLowLevel()) 	return;
		
		//~~~~~~~~~~~~~~~~~~~~
		//			Begin Destroy						
		ToDestroy->ConditionalBeginDestroy();
		//~~~~~~~~~~~~~~~~~~~~		
					
		ToDestroy = NULL;
	}
public:
 
	static FORCEINLINE bool VerifyObjectCanBeLoaded(UObject* Obj)
	{  
		return !FStringAssetReference(Obj).ToString().Contains("PersistentLevel");
	}
	
public:
	
	static FORCEINLINE bool FileExists(const FString& File)
	{
		return FPlatformFileManager::Get().GetPlatformFile().FileExists(*File);
	}
	static FORCEINLINE bool FolderExists(const FString& Dir)
	{
		return FPlatformFileManager::Get().GetPlatformFile().DirectoryExists(*Dir);
	}
	static FORCEINLINE bool CreateDirectoryTreeForFile(const FString& FullPath)
	{
		FString FolderPath = FPaths::GetPath(FullPath);
		
		if(FolderExists(FolderPath)) 
		{
			return true;
		}
		
		//Try to create it!
		return FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*FolderPath);
	}
	
	//! File Compression, by Rama
	static bool DecompressFromFile(const FString& FullFilePath, TArray<uint8>& Uncompressed);
	static bool CompressAndWriteToFile(TArray<uint8>& Uncompressed, const FString& FullFilePath);
	//! File Compression, by Rama
	
	
};