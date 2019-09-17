// Copyright 2015 by Nathan "Rama" Iyer. All Rights Reserved.
#include "RamaSaveSystemPrivatePCH.h"
#include "RamaSaveUtility.h"
#include "ArchiveSaveCompressedProxy.h"
#include "ArchiveLoadCompressedProxy.h"

////HTML Save and Load 
//#if PLATFORM_HTML5_BROWSER
//	#include "HTML5JavaScriptFx.h"
//#endif 

bool URamaSaveUtility::IsIllegalForSavingLoading(UClass* Class)
{
	if(!Class) return true;
	
	//Store arbitrary game data in an empty actor that is placed in world
	// or via the use of a UE4 Save Game Object
	return Class->IsChildOf(APlayerController::StaticClass())
			|| Class->IsChildOf(APlayerState::StaticClass())
			|| Class->IsChildOf(AGameState::StaticClass())
			|| Class->IsChildOf(AGameMode::StaticClass());
			 
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Save To Compressed File, by Rama
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool URamaSaveUtility::CompressAndWriteToFile(TArray<uint8>& Uncompressed, const FString& FullFilePath)
{
	//~~~ No Data ~~~ 
	if (Uncompressed.Num() <= 0) return false;
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	
	//~~~~~~~~~~~~~~~~~~~~~~~~~~
	//	Write to File

	//HTML Save and Load 
#if PLATFORM_HTML5_BROWSER
	FString FileName = FPaths::GetCleanFilename(*FullFilePath);
	  
	if (!UE_SaveGame(TCHAR_TO_ANSI(*FileName),0,(char*)Uncompressed.GetData(),Uncompressed.Num()))
	{
		return false;
	}
	
#else 
	//~~~~~~~~~~~~~~~~~~~~~~~~~~
	//					Compress
	//~~~ Compress File ~~~
	//tmp compressed data array
	TArray<uint8> CompressedData;
	FArchiveSaveCompressedProxy Compressor(CompressedData, ECompressionFlags::COMPRESS_ZLIB);
	
	//Send entire binary array/archive to compressor
	Compressor << Uncompressed;
	
	//send archive serialized data to binary array
	Compressor.Flush();
	
	if (!FFileHelper::SaveArrayToFile(CompressedData, *FullFilePath))
	{
		return false;
	}
	
	//~~~ Clean Up ~~~
	
	//~~~ Free Binary Arrays ~~~
	Compressor.FlushCache();
	CompressedData.Empty();
	Uncompressed.Empty();
#endif 
	
	return true;
} 

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// 	Decompress From File, by Rama
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool URamaSaveUtility::DecompressFromFile(const FString& FullFilePath, TArray<uint8>& Uncompressed)
{
	
	
#if PLATFORM_HTML5_BROWSER
	FString FileName = FPaths::GetCleanFilename(*FullFilePath);
	     
	if(!UE_DoesSaveGameExist(TCHAR_TO_ANSI(*FileName),0))
	{
		return false;
	}
	 
	char*	OutData;
	int		Size;
	bool Result = UE_LoadGame(TCHAR_TO_ANSI(*FileName),0,&OutData,&Size);
	if (!Result)
		return false; 
	Uncompressed.Append((uint8*)OutData,Size);
	::free (OutData);

#else
	//~~~ File Exists? ~~~
	if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*FullFilePath)) return false;
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	
	//tmp compressed data array
	TArray<uint8> CompressedData;
	
	if (!FFileHelper::LoadFileToArray(CompressedData, *FullFilePath))
	{
		return false;
		//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	}
	
	//~~~ Decompress File ~~~
	FArchiveLoadCompressedProxy Decompressor(CompressedData, ECompressionFlags::COMPRESS_ZLIB);
	
	//Decompression Error?
	if(Decompressor.GetError())
	{
		return false;
		//~~~~~~~~~~~~
	}
	
	//Send Data from Decompressor to Vibes array
	Decompressor << Uncompressed;
#endif
	
	return true;
}