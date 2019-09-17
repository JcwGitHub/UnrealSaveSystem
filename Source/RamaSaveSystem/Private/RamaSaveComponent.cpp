// Copyright 2015 by Nathan "Rama" Iyer. All Rights Reserved.
#include "RamaSaveSystemPrivatePCH.h"
#include "RamaSaveComponent.h"

//See PhysicsTimer()
#define PHYSICS_TIMER 0.05
     
#include "RamaSaveSystemSettings.h"

#include "RamaSaveEngine.h"
#include "StructuredArchiveFromArchive.h"

bool URamaSaveComponent::GetActorIsInPersistentLevel()
{
	AActor* Owner = GetOwner();
	if(!Owner) return false;
 
	return Owner->IsInPersistentLevel();
}

FString URamaSaveComponent::GetActorStreamingLevelPackageName()
{
	AActor* Owner = GetOwner();
	if(!Owner) return "No Owning Actor";
	
	//Consistency if it is the persistent level
	if(Owner->IsInPersistentLevel())
	{
		return "PersistentLevel";
	}
	 
	ULevel* Level = Owner->GetLevel();
	if(!Level) return "No level for Actor apparently! (should not ever see this unless actor pending delete perhaps)";
	
	//Rama Victory!!!
	UObject* Package = Level->GetOuter(); 
	if(!Package) return "PersistentLevel";
	return Package->GetName();
}

//This is Static
bool URamaSaveComponent::RamaSave_LoadFromFile(UWorld* World, int32 RamaSaveSystemVersion, const TArray<FString>& LoadActorsWithSaveTags,  FArchive &Ar, URamaSaveComponent*& LoadedComp, bool DontLoadPlayerPawns, FString LoadOnlyStreamingLevel)
{
	URamaSaveSystemSettings* Settings = URamaSaveSystemSettings::Get();
	if(!Settings) 
	{
		UE_LOG(RamaSave, Error,TEXT("URamaSaveComponent::RamaSave_LoadFromFile>> Huge big error! Tell Rama!"));
		return false;
	}
	
	bool GlobalLogging = Settings->Loading_GlobalVerboseLogging;
	
  
	LoadedComp = nullptr;
	
	//! #4 Actor Byte Chunk Skip Position
	int64 ActorArchiveEndPos = 0; 						
	Ar << ActorArchiveEndPos;
	 
	//! #4 String Actor Class
	//First Data in file should be the Object Class name
	FString ActorClassFromFile;
	Ar << ActorClassFromFile;			
  
	//! #4 String Actor Class Path
	FString ActorClassFullPath;
	Ar << ActorClassFullPath;	
	  
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//Ver 3 = FGUID and Actor Tags!
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

	FGuid PersistentActorUniqueID;
	TArray<FString> TempTags;
	if(RamaSaveSystemVersion > 2)
	{ 
		//! #4.5 FGUID !
		Ar << PersistentActorUniqueID;
		
		//! 4.7333 Actor Tags
		Ar << TempTags;
	}
	
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	// Streaming Levels Filter <3 Rama
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	 
	FString LoadedLevelPackageName = "Old File Version, Re-save this file to get level streaming info! <3 Rama";
	if(RamaSaveSystemVersion > 3)
	{ 
		//! 4.9 Level Streaming
		Ar << LoadedLevelPackageName;
	}
	 
	//Is this actor in the requested streaming level?
	if(LoadOnlyStreamingLevel != "" && LoadOnlyStreamingLevel != "Old File Version, Re-save this file to get level streaming info! <3 Rama")
	{ 
		//Not Match?
		if(LoadOnlyStreamingLevel != LoadedLevelPackageName)
		{
			//Skip! Essential to maintain integrity of load process!
			Ar.Seek(ActorArchiveEndPos);
			 
			return true;
		}
	}
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	
	
	//Does this actor archive entry contains any of the tags needed?
	if(LoadActorsWithSaveTags.Num() > 0)
	{
		bool SaveTagMatch = false;
		for(const FString& EachSuppliedTag : LoadActorsWithSaveTags)
		{
			if(!TempTags.Contains(EachSuppliedTag)) continue;
			 
			//Match!
			SaveTagMatch = true;
			break;
		}
		 
		if(!SaveTagMatch)
		{
			//Skip! Essential to maintain integrity of load process!
			Ar.Seek(ActorArchiveEndPos);
			
			return true;
		}
	}
	
	AActor* NewActor = nullptr;
	 
	//Doing lookup or creating new?
	if(PersistentActorUniqueID.IsValid())
	{
		for(TObjectIterator<URamaSaveComponent> Itr; Itr; ++Itr)
		{
			//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
			//     World must match <3 Rama
			if(Itr->GetWorld() != World) continue;
			//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
			
			if(Itr->RamaSave_PersistentActorUniqueID == PersistentActorUniqueID)
			{
				NewActor = Itr->GetOwner();
				if(!NewActor)
				{
					UE_LOG(RamaSave, Error,TEXT("Component with FGUID found, but GetOwner() returned nullptr!!!!!! %s %s"), *PersistentActorUniqueID.ToString(), *Itr->GetName() );
				}
				else
				{
					if(Itr->RamaSave_LogPersistentActorGUID) UE_LOG(RamaSave, Warning, TEXT("Rama Save ~ Actor with GUID found and loaded! %s"), *Itr->RamaSave_PersistentActorUniqueID.ToString() );
				}
				break;
				//~~~~
			}
		}
		
		if(!NewActor) 
		{
			UE_LOG(RamaSave, Error,TEXT("Actor was not found during lookup by FGUID! %s"), *PersistentActorUniqueID.ToString());
			
			//Skip! Essential to maintain integrity of load process!
			Ar.Seek(ActorArchiveEndPos);
			
			return false;
		}
	} 
	else
	{
		//Get the Class Name!
		UClass* LoadedActorOwnerClass = FindObject<UClass>(ANY_PACKAGE, *ActorClassFromFile);
		if(LoadedActorOwnerClass == NULL)
		{
			LoadedActorOwnerClass = LoadObject<UClass>(NULL, *ActorClassFromFile);
		}

		//Check Class
		if(LoadedActorOwnerClass == NULL)
		{ 
			UE_LOG(RamaSave, Warning,TEXT("Actor Class not found, loading from full class path... %s"), *ActorClassFromFile );
			
			//Load Static Class
			//		This works where the FindObject/LoadObject code pattern fails!
			LoadedActorOwnerClass = LoadClassFromPath(ActorClassFullPath);  
			//Note that StaticLoadClass expects as a UObject superclass, like UObject::StaticClass() or AActor::StaticClass() as the base class     
			
			if(LoadedActorOwnerClass == NULL)
			{
				UE_LOG(RamaSave, Error,TEXT("Actor Class not found, was it removed? %s"), *ActorClassFullPath );
				
				//Skip! Essential to maintain integrity of load process!
				Ar.Seek(ActorArchiveEndPos);
			
				return false;
			}
			else
			{   
				UE_LOG(RamaSave, Warning,TEXT(">>>> SUCCESS: Actor successfully loaded from full class path! ~ %s"), *ActorClassFullPath);
			}
		}  
		  
		//Create New Actor
		NewActor = SpawnBP<AActor>(World, LoadedActorOwnerClass, FVector::ZeroVector); 
		if(!NewActor) 
		{
			UE_LOG(RamaSave, Error,TEXT("Actor could not be spawned from class! %s"), *LoadedActorOwnerClass->GetName());
			
			//Skip! Essential to maintain integrity of load process!
			Ar.Seek(ActorArchiveEndPos);
			
			return false;
		}
	}
	  
	if(!NewActor) 
	{
		UE_LOG(RamaSave, Error,TEXT("RamaSave_LoadFromFile >> unforseen error, tell Rama!"));
		
		//Skip! Essential to maintain integrity of load process!
		Ar.Seek(ActorArchiveEndPos);
		 
		return false;
	}
	 
	//Check Component
	URamaSaveComponent* SaveComp = NewActor->FindComponentByClass<URamaSaveComponent>();
	if(!SaveComp)
	{
		UE_LOG(RamaSave, Error,TEXT("Actor Class loaded, but the save component was not yet initialized properly?"));
		
		//Skip! Essential to maintain integrity of load process!
		Ar.Seek(ActorArchiveEndPos);
		
		return false;
	} 
	
	//Ptr Reference <3 Rama
	LoadedComp = SaveComp;
	
	//Set Loaded Data
	SaveComp->LevelPackageName = LoadedLevelPackageName;
	 
	if(SaveComp->RamaSave_VerboseLog || GlobalLogging)
	{ 
		UE_LOG(RamaSave, Warning,TEXT("Actor Class loaded successfully! %s"), *ActorClassFullPath );
	}   
	 
	//If user adds more tags during runtime based on custom logic, those tags should be re-loaded!
	// (load of Self and subclass vars will probably also do this)
	SaveComp->RamaSave_SaveTags = TempTags;
	 
	//Set the Load Settings Struct (currently just 1 bool)
	SaveComp->DontLoadPlayerPawns = DontLoadPlayerPawns;
	
	//~~~~~~~~~~~~~~~~~~~~~~~~
	//~~~~~~~~~~~~~~~~~~~~~~~~
	//~~~~~~~~~~~~~~~~~~~~~~~~
	//! #5 Serialize Properties
	//Load into Save Comp! 
	SaveComp->LoadSelfAndSubclassVariables(Ar);
	SaveComp->LoadOwnerVariables(World,Ar);  								//Actor Transform
	
	if(SaveComp->RamaSave_SavePhysicsData)
	{
		SaveComp->LoadOwnerVariables_Physics(NewActor, World, Ar);	//Apply Physics
	}
	
	SaveComp->LoadSubComponentVariables(NewActor, World,Ar);
	//~~~~~~~~~~~~~~~~~~~~~~~~
	//~~~~~~~~~~~~~~~~~~~~~~~~
	//~~~~~~~~~~~~~~~~~~~~~~~~
	
	return true;
}

void URamaSaveComponent::FullyLoaded()
{
	URamaSaveSystemSettings* Settings = URamaSaveSystemSettings::Get();
	if(Settings && Settings->Loading_GlobalVerboseLogging) 
	{
		if(GetOwner()) 
		{
			UE_LOG(RamaSave, Warning,TEXT("About to call Actor Fully Loaded Event in Blueprints for %s"), *GetOwner()->GetName() );
		} 
		else 
		{
			UE_LOG(RamaSave, Warning,TEXT("Want to call Actor Fully Loaded Event in Blueprints but no owner! %s"), *GetName() );
		}
	}
	 
	//C++
	RamaSave_PostLoad_CPP();
	 
	//BP Impl
	RamaSave_PostLoad();
	
	//BP Assignable
	if(RamaSaveEvent_ActorFullyLoaded.IsBound())
	{ 
		RamaSaveEvent_ActorFullyLoaded.Broadcast(this,LevelPackageName);
	}

} 

void URamaSaveComponent::RamaCPP_PreSave()
{
	//C++
	RamaSave_PreSave_CPP();
	
	//BP Impl
	RamaSave_PreSave();
	
	//BP Assignable
	if(RamaSaveEvent_PreSave.IsBound())
	{ 
		RamaSaveEvent_PreSave.Broadcast(this);
	}
}

bool URamaSaveComponent::RamaSave_SaveToFile(UWorld* World, FArchive &Ar)
{ 
	if(!RamaSave_ShouldSaveActor)
	{
		//Actor is not being saved right now per user request.
		return true;
	}
	 
	AActor* ActorOwner = GetOwner();
	if(!ActorOwner)
	{ 
		//VSCREENMSG2("Component without an owner!",GetName());
		UE_LOG(RamaSave, Error,TEXT("Component without an owner! %s"),*GetName());
		return false;
	}

	//! #4 Actor Byte Chunk Skip Position
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//							Actor Byte Chunk Start
	int64 ActorArchiveStartPos = Ar.Tell();
	int64 ActorArchiveEndPos = 0; 						
	Ar << ActorArchiveEndPos;	//Set aside space to fill later
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	
	//! #4 String Actor Class
	//Save the Actor Class Name 
	//		so that during load, Actor can be created and then data loaded from file
	FString ActorOwnerClass = ActorOwner->GetClass()->GetName();
	Ar << ActorOwnerClass;
	 
	//! #4 String Actor Class Path
	FString ActorClassFullPath = GetClassPath(ActorOwner->GetClass()); 
	Ar << ActorClassFullPath;	
	
	if(RamaSave_VerboseLog)
	{ 
		UE_LOG(RamaSave, Warning,TEXT("Saving Actor class path %s"), *ActorClassFullPath);
	}
	
	//! #4.5 FGUID !
	Ar << RamaSave_PersistentActorUniqueID;
	
	//! 4.7333 Actor Tags
	Ar << RamaSave_SaveTags;
	
	//Set the Level Package Name, so that can filter when loading if that is desired
	LevelPackageName = GetActorStreamingLevelPackageName();
	
	//! 4.9 Level Streaming
	Ar << LevelPackageName;
	
	//~~~~~~~~~~~~~~~~~~~~~~~~
	//~~~~~~~~~~~~~~~~~~~~~~~~
	//~~~~~~~~~~~~~~~~~~~~~~~~
	//! #5 Serialize Properties
	SaveSelfAndSubclassVariables(Ar);
	SaveOwnerVariables(World,Ar); 									//Actor Transform
	
	if(RamaSave_SavePhysicsData)
	{
		SaveOwnerVariables_Physics(ActorOwner, World,Ar);		//Physics
	}
	
	SaveSubComponentVariables(ActorOwner, World,Ar);
	//~~~~~~~~~~~~~~~~~~~~~~~~
	//~~~~~~~~~~~~~~~~~~~~~~~~
	//~~~~~~~~~~~~~~~~~~~~~~~~
		    
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//								Actor Byte Chunk End
	ActorArchiveEndPos = Ar.Tell();	//End of all actor bytes
	Ar.Seek(ActorArchiveStartPos);	//Go back
	Ar << ActorArchiveEndPos;			//Record the number of bytes
	Ar.Seek(ActorArchiveEndPos);	// go back to end!
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	
	return true;
}


void URamaSaveComponent::SaveOwnerVariables(UWorld* World, FArchive &Ar)
{
	URamaSaveSystemSettings* Settings = URamaSaveSystemSettings::Get();
	if(!Settings) 
	{
		return;
	}
	
	AActor* ActorOwner = GetOwner();

	//! #6 Total Count
	//Serialize the total count, even if it is 0! 
	int64 TotalProperties = 0;
	int64 ArchiveBegin = Ar.Tell();		//cant assume starting at 0!
	Ar << TotalProperties;
	
	if(!ActorOwner) 
	{ 
		//VSCREENMSG2("Component without an owner!",GetName());
		UE_LOG(RamaSave, Error,TEXT("Component without an owner! %s"),*GetName());
		return;
	}
	 
	//! #7 Pawn
	APawn* Pawn = Cast<APawn>(ActorOwner);
	if(Pawn)
	{
		SaveOwnerVariables_Pawn(Pawn,World,Ar);
	}
	 
	//! #8 Physics
	//
	
	//~~~ Diagnostic ~~~
	#if WITH_EDITOR
	if(RamaSave_VerboseLog)
	{
		UE_LOG(RamaSave, Log, TEXT("%s ~ Number of Actor Variables To Save To Disk: %d"), *ActorOwner->GetName(), RamaSave_OwningActorVarsToSave.Num() );
		
		//Find user-created BP vars and tell user they are not in the save list!
		UBlueprint* ActorBP = UBlueprint::GetBlueprintFromClass(ActorOwner->GetClass());
		if(ActorBP) 
		{
			for(const FBPVariableDescription& Each :  ActorBP->NewVariables)
			{ 
				FString ActorVarName = Each.VarName.ToString();
				UProperty* Property = FindField<UProperty>( ActorOwner->GetClass(), *ActorVarName );
				if(Property) 
				{ 
					//Ignore Delegates
					//Delegate? 
					if(Property->IsA(UMulticastDelegateProperty::StaticClass()))
					{
						//Dont remind user to serialize Delegates! (It breaks them)
						continue;
					}  
				} 
				 
				//Is this property in the list of properties to save to disk?
				if(!RamaSave_OwningActorVarsToSave.Contains(ActorVarName))
				{    
					UE_LOG(RamaSave, Warning, TEXT("%s ~ Actor variable not in save list: %s"), *ActorOwner->GetName(), *ActorVarName );
				}
			}
		}
	}
	#endif //WITH_EDITOR
	 
	//~~~ Diagnostic ~~~
	  
	//~~~~~~~~~~~~~~~~~~~~
	if(RamaSave_OwningActorVarsToSave.Num() < 1 && !Settings->SaveAllPropertiesMarkedAsSaveGame) return;
	//~~~~~~~~~~~~~~~~~~~~~~
	
	//!#9 Properties
	for (TFieldIterator<UProperty> It(ActorOwner->GetClass()); It; ++It)
	{
		UProperty* Property = *It;
		FString PropertyNameString = Property->GetFName().ToString();
	    
		//Is this property in the list of properties to save to disk?
		if(RamaSave_OwningActorVarsToSave.Contains(PropertyNameString) || (Settings->SaveAllPropertiesMarkedAsSaveGame && Property->HasAnyPropertyFlags(CPF_SaveGame)))
		{	
			//We want each property as pure binary data 
			//		so we can easily save it to disk and not worry about its exact type!
			uint8* InstanceValuePtr = Property->ContainerPtrToValuePtr<uint8>(ActorOwner);  //this = object instance that has this property!
			
			int64 StartAfterStringPos = 0; //Ar POSITION TO STORE THE END POS AT
			int64 EndPos = 0; 						//Postion after serializing property
			
			//Serialize Instance of Property!
			Ar << PropertyNameString;
			StartAfterStringPos = Ar.Tell();
			Ar << EndPos;
			 
			//Property->SerializeItem(FStructuredArchiveFromArchive(Ar).GetSlot(),InstanceValuePtr); 
			 
			Property->SerializeItem(FStructuredArchiveFromArchive(Ar).GetSlot(), InstanceValuePtr);
			
			EndPos = Ar.Tell();
			Ar.Seek(StartAfterStringPos);
			Ar << EndPos;
			Ar.Seek(EndPos);
			
			TotalProperties++;
		} 
	}  
	
	//Put the total at the beginning of archive so know how big of for loop to amke
	const int64 ArchiveEnd = Ar.Tell();
	Ar.Seek(ArchiveBegin);
	Ar << TotalProperties;
	
	//Move back to end of archive 
	//		so as to not ovewrite all data 
	//			when next archive entries occur!!
	Ar.Seek(ArchiveEnd); //<~~~ !
}
void URamaSaveComponent::SaveOwnerVariables_Pawn(APawn* Pawn, UWorld* World, FArchive &Ar)
{
	bool IsPlayer = Pawn->GetPlayerState() != nullptr;
	Ar << IsPlayer;
	   
	FVector PawnVelocity = FVector::ZeroVector;
	if(Pawn->GetMovementComponent() != nullptr)
	{
		PawnVelocity = Pawn->GetMovementComponent()->Velocity;
	}
	else
	{ 
		//VSCREENMSG2("Rama Save Component SAVING ~ Movement component was invalid for", Pawn->GetName());
		//UE_LOG(RamaSave,Warning,TEXT("Rama Save Component SAVING ~ Movement component was invalid for %s"), *Pawn->GetName());
	}
	Ar << PawnVelocity; 
	
	
	FRotator ControlRotation = Pawn->GetControlRotation();
	if(Pawn->GetController())
	{
		ControlRotation = Pawn->GetController()->GetControlRotation(); //For symmetry, could use Pawn::GetControlRotation but there's no Set
	}
	Ar << ControlRotation; 
	
	int32 PlayerIndex = -1; //Indicates its a non-player controlled unit
	if(IsPlayer)
	{ 
		PlayerIndex = 0;
		APlayerController* ToFind = Cast<APlayerController>(Pawn->GetController());
		for (FConstPlayerControllerIterator PCIt = World->GetPlayerControllerIterator(); PCIt; ++PCIt)
		{
			if(ToFind == Cast<APlayerController>(*PCIt)) break;
			PlayerIndex++; 
		}
	}  
	 
	//Always Serializing, -1 for non players
	Ar << PlayerIndex;
}
void URamaSaveComponent::SaveOwnerVariables_Physics(AActor* ActorOwner, UWorld* World, FArchive &Ar)
{
	TArray<UPrimitiveComponent*> PrimComps;
	ActorOwner->GetComponents<UPrimitiveComponent>(PrimComps);
	
	//Whether this file has any Physics save data
	bool HavePhysicsSaveData = true;
	Ar << HavePhysicsSaveData;
	
	//Save count, so if count doesnt match, know to skip section
	int32 PrimitiveCount = PrimComps.Num();
	int64 SkipIndex = 0;
	int64 SkipLocation = 0;
	Ar << PrimitiveCount;
	SkipLocation = Ar.Tell();
	Ar << SkipIndex;
	
	//Save RB States
	for(UPrimitiveComponent* Each : PrimComps)
	{
		bool IsSimulatingPhysics = Each->IsSimulatingPhysics();
		Ar << IsSimulatingPhysics;
		
		//Dont save RB state for every primitive comp in the world!
		if(IsSimulatingPhysics)
		{ 
			FRBSave PhysState;
			PhysState.FillFrom(Each);			//Not concerned about multiple bone setups, just root bone for the time being
			Ar << PhysState;
		}
	}
	  
	int64 EndIndex = Ar.Tell();
	Ar.Seek(SkipLocation);
	SkipIndex = EndIndex;
	Ar << SkipIndex;
	Ar.Seek(EndIndex);
}
	
void URamaSaveComponent::LoadOwnerVariables(UWorld* World, FArchive &Ar)
{ 
	AActor* ActorOwner = GetOwner();
	
	//! #6 Total Count
	int64 TotalProperties = 0;
	Ar << TotalProperties;
	
	//Has to be Valid
	check(ActorOwner);
	
	//! #7 Pawn
	APawn* Pawn = Cast<APawn>(ActorOwner);
	if(Pawn)
	{
		LoadOwnerVariables_Pawn(Pawn, World,Ar);
	} 
	
	//! #8 Physics
	//
	  
	//!#9 Properties
	for(int64 v = 0; v < TotalProperties; v++)
	{
		//Get info about each property before deciding whether to serialize
		FString PropertyNameString;
		int64 EndPosToSkip;
		Ar << PropertyNameString;
		Ar << EndPosToSkip; 
			
		UProperty* Property = FindField<UProperty>( ActorOwner->GetClass(), *PropertyNameString );
		if(Property) 
		{ 
			uint8* InstanceValuePtr = Property->ContainerPtrToValuePtr<uint8>(ActorOwner);  //this = object instance that has this property!
			
			//Serialize Instance!
			Property->SerializeItem(FStructuredArchiveFromArchive(Ar).GetSlot(),InstanceValuePtr);
		}
		else
		{
			//SKIP TO END OF THIS PROPERTY AS IT WAS NOT FOUND IN ACTUAL CLASS
			// MUST HAVE BEEN REMOVED
			Ar.Seek(EndPosToSkip);
			 
			UE_LOG(RamaSave,Warning,TEXT("Property in save file but not found in class, re-save to get rid of this message %s %s"), *ActorOwner->GetClass()->GetName(), *PropertyNameString);
		}
	}
}
void URamaSaveComponent::LoadOwnerVariables_Pawn(APawn* Pawn, UWorld* World, FArchive &Ar)
{
	bool IsPlayer = false;
	Ar << IsPlayer;
	
	FVector PawnVelocity = FVector::ZeroVector;
	Ar << PawnVelocity;

	FRotator ControlRotation;
	Ar << ControlRotation; 
	
	int32 PlayerIndex = 0;
	Ar << PlayerIndex;
	
	if(IsPlayer)
	{ 
		//To avoid doing this Destroy would have to pre-load all of the above data, especially IsPlayer before
		//  spawning the actor from the BP in the first part of load sequence
		if(DontLoadPlayerPawns)
		{
			//Move to origin
			UPrimitiveComponent* RootPrim = Cast<UPrimitiveComponent>(Pawn->GetRootComponent());
			if(RootPrim && RootPrim->IsSimulatingPhysics())
			{
				RootPrim->SetAllPhysicsPosition(FVector(0));
			}
			else
			{
				Pawn->SetActorLocation(FVector(0));
			}
			
			//Remove the Player Pawn from the World
			Pawn->Destroy();
			return; 
			//~~~~
		}
		
		int32 Count = 0;
		for (FConstPlayerControllerIterator PCIt = World->GetPlayerControllerIterator(); PCIt; ++PCIt)
		{
			if(Count == PlayerIndex)
			{
				APlayerController* PC = Cast<APlayerController>(*PCIt);
				if(PC)
				{
					PC->Possess(Pawn); 
					
					//C++ Hook
					RamaSave_PlayerLoaded_CPP(PC,Pawn,PlayerIndex);
					 
					//BP Event
					if(RamaSaveEvent_PlayerLoaded.IsBound())
					{
						RamaSaveEvent_PlayerLoaded.Broadcast(PC,Pawn,PlayerIndex);
					} 
				}
			}
			Count++; 
		}
	}
	else
	{
		Pawn->SpawnDefaultController();
	}
	
	//This probablly wont replicate for clients without special effort
	if(Pawn->GetController())
	{ 
		Pawn->GetController()->SetControlRotation(ControlRotation); 
	}
	 
	//Restore Velocity
	if(Pawn->GetMovementComponent() != nullptr)
	{
		Pawn->GetMovementComponent()->Velocity = PawnVelocity;		//Replicates because this code can only happen on server
	}
	else
	{
		//VSCREENMSG2("Rama Save Component LOADING ~ Movement component was invalid for", Pawn->GetName());
		UE_LOG(RamaSave,Warning,TEXT("Rama Save Component LOADING ~ Movement component was invalid for %s"), *Pawn->GetName());
	}
}
void URamaSaveComponent::LoadOwnerVariables_Physics(AActor* ActorOwner, UWorld* World, FArchive &Ar)
{
	TArray<UPrimitiveComponent*> PrimComps;
	ActorOwner->GetComponents<UPrimitiveComponent>(PrimComps);
	
	bool HavePhysicsSaveData;
	Ar << HavePhysicsSaveData;
	
	//Any Physics Data To Load?
	if(!HavePhysicsSaveData) 
	{ 
		return; 
	}
	//Save count, so if count doesnt match, know to skip section
	int32 PrimitiveCount = 0;
	int64 SkipIndex = -1;
	Ar << PrimitiveCount;
	Ar << SkipIndex;
	
	//Save file has Physics data but user does not want to load it.
	if(!RamaSave_SavePhysicsData)
	{
		//Skip physics!
		Ar.Seek(SkipIndex);
		return;
	}
	 
	//Does the saved count match the current count (ie, did user delete a primitive component from a BP) ?
	//		This also happens if components were added dynamically during runtime.
	//			For actors with dynamically added components, physics state must be saved/loaded by the User.
	if(PrimitiveCount != PrimComps.Num())
	{
		//Skip physics!
		Ar.Seek(SkipIndex);
		UE_LOG(RamaSave,Warning,TEXT("%s ~ Skipping loading of physics because components in a save file were removed from current BP class, or primitive components were added dynamically during runtime %s"), *ActorOwner->GetName(), *ActorOwner->GetClass()->GetName());
		return;
	}
	
	//Load RB States
	RBLoadStates.Empty(); //Timer
	//FVector DeltaPos(FVector::ZeroVector);
	for(UPrimitiveComponent* Each : PrimComps)
	{
		bool IsSimulatingPhysics = false;
		Ar << IsSimulatingPhysics;
		
		//Set!
		// why is this set sending thing to origin?
		//	Physics not having time to get set up properly, need a DELAY
		//Each->SetSimulatePhysics(IsSimulatingPhysics);
		 
		if(IsSimulatingPhysics) 	
		{
			FRBSave PhysState;
			Ar << PhysState;
			
			RBLoadStates.Add(FRBLoad(Each,PhysState)); //Timer
			//PhysState.Apply(Each);
			//VSCREENMSG2("applying position", PhysState.Location.ToString());
			
			//FRigidBodyState PhysState;
			//Ar << PhysState;
			//FVector DeltaPos(FVector::ZeroVector);
			//Each->ConditionalApplyRigidBodyState(PhysState, GEngine->PhysicErrorCorrection, DeltaPos);
		}
		else
		{ 
			//In case spawned BP defaults to true, but saved state is false
			//  <3 Rama
			Each->SetSimulatePhysics(false);
		}
	}
	  
	FTimerHandle TH;
	ActorOwner->GetWorldTimerManager().SetTimer(TH, this, &URamaSaveComponent::PhysicsTimer,PHYSICS_TIMER,false);
} 

/*
	Physics Timer
	
	This is a temp solution for the fact that setting SetSimulatePhysics in the same tick that the actor is spawned/serialized
	causes the root comp to go to the origin.
	
	This timer allows for a slight delay before the call to SetSimulatePhysics
*/
void URamaSaveComponent::PhysicsTimer()
{	
	AActor* ActorOwner = GetOwner();
	if(!ActorOwner) return;
	
	URamaSaveSystemSettings* Settings = URamaSaveSystemSettings::Get();
	if(!Settings) 
	{
		return;
	}
	if(Settings->Loading_GlobalDisablePhysicsLoad)
	{ 
		UE_LOG(RamaSave, Warning, TEXT("Loading of Physics State disabled in ProjectSettings->RamaSaveSystem for Actor %s"), *ActorOwner->GetName());
		return;
	}
	
	
	for(FRBLoad& Each : RBLoadStates)
	{  
		if(!Each.Comp) continue;
		
		if(Settings->Loading_GlobalVerboseLogging)
		{
			UE_LOG(RamaSave, Warning, TEXT("About to load Physics State for Actor %s and this comp %s"), *ActorOwner->GetName(), *Each.Comp->GetName());
		}
	 
		//RB state is only saved if it is simulating
		Each.Comp->SetSimulatePhysics(true); 
		Each.Activate(); //See .h
	} 
}
	
void URamaSaveComponent::SaveSelfAndSubclassVariables(FArchive &Ar)
{
	//Transform
	OwningActorTransform = GetOwner()->GetTransform();
	
	//~~~
	
	//Get superclass to compare with to ensure dont serialize properties belonging to base class
	UClass* SuperClass = Super::StaticClass();
	 
	//Serialize the total count, even if it is 0! 
	int64 TotalProperties = 0;
	int64 ArchiveBegin = Ar.Tell();		//cant assume starting at 0!
	Ar << TotalProperties;
	
	for (TFieldIterator<UProperty> It(this->GetClass()); It; ++It)
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
		 
		//Is this property not in super class? Then the user added it, so save it!
		UProperty* SuperClassProperty = FindField<UProperty>( SuperClass, *PropertyNameString );
		if (!SuperClassProperty)
		{ 
			//Here's how to check if something is being saved that shouldn't be
			if(RamaSave_LogAllSavedComponentProperties)
			{
				UE_LOG(RamaSave, Log, TEXT("%s ~ Serializing Save Component Property: %s"), *GetOwner()->GetName(), *PropertyNameString);
			}  
			 
			/*
			//what is the type of uber graph?
			if(PropertyNameString.Contains("Uber"))
			{
				V_LOG2(RamaSave, "Type of Ubergraph", Property->GetClass()->GetName());
			} 
			*/
			 
			
			//We want each property as pure binary data 
			//		so we can easily save it to disk and not worry about its exact type!
			uint8* InstanceValuePtr = Property->ContainerPtrToValuePtr<uint8>(this);  //this = object instance that has this property!
			
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
		/*
		else
		{
			V_LOG2(RamaSave,"Super class component property found and skipped", *PropertyNameString);
		}
		*/
	} 
	 
	//Put the total at the beginning of archive so know how big of for loop to amke
	const int64 ArchiveEnd = Ar.Tell();
	Ar.Seek(ArchiveBegin);
	Ar << TotalProperties;
	
	//Move back to end of archive 
	//		so as to not ovewrite all data 
	//			when next archive entries occur!!
	Ar.Seek(ArchiveEnd); //<~~~ !
}
void URamaSaveComponent::LoadSelfAndSubclassVariables(FArchive &Ar)
{
	int64 TotalProperties = 0;
	Ar << TotalProperties;
		
	for(int64 v = 0; v < TotalProperties; v++)
	{
		//Get info about each property before deciding whether to serialize
		FString PropertyNameString;
		int64 EndPosToSkip;
		Ar << PropertyNameString;
		Ar << EndPosToSkip; 
			
		UProperty* Property = FindField<UProperty>( this->GetClass(), *PropertyNameString );
		if(Property) 
		{ 
			uint8* InstanceValuePtr = Property->ContainerPtrToValuePtr<uint8>(this);  //this = object instance that has this property!
			
			//Serialize Instance!
			Property->SerializeItem(FStructuredArchiveFromArchive(Ar).GetSlot(),InstanceValuePtr);
		}
		else
		{
			//SKIP TO END OF THIS PROPERTY AS IT WAS NOT FOUND IN ACTUAL CLASS
			// MUST HAVE BEEN REMOVED
			Ar.Seek(EndPosToSkip);
			 
			UE_LOG(RamaSave,Warning,TEXT("Property in save file but not found in class, re-save to get rid of this message %s %s"), *GetClass()->GetName(), *PropertyNameString);
		}
	}
	
	//~~~ Transform ~~~
	// This gets loaded above since it is UPROPERTY() in .h
	 
	//Ensure not static mobility during loading
	UPrimitiveComponent* RootComp = Cast<UPrimitiveComponent>(GetOwner()->GetRootComponent());
	if(RootComp && RootComp->Mobility != EComponentMobility::Movable)
	{
		UE_LOG(RamaSave,Warning,TEXT("Had to set root component to be movable for loading %s %s"), *GetOwner()->GetName(), *RootComp->GetName());
		RootComp->SetMobility(EComponentMobility::Movable);
	}
	    
	//Transform
	if(RamaSave_ShouldLoadActorWorldPosition)
	{ 
		UPrimitiveComponent* RootPrim = Cast<UPrimitiveComponent>(GetOwner()->GetRootComponent());
		if(RootPrim && RootPrim->IsSimulatingPhysics())
		{ 
			RootPrim->SetAllPhysicsPosition(OwningActorTransform.GetTranslation());
			RootPrim->SetAllPhysicsRotation(OwningActorTransform.Rotator());
			//not setting scale
		}
		else
		{
			GetOwner()->SetActorTransform(OwningActorTransform);
			//sets scale!
		}
	}
}

void URamaSaveComponent::SaveSubComponentVariables(AActor* ActorOwner, UWorld* World, FArchive &Ar)
{
	URamaSaveSystemSettings* Settings = URamaSaveSystemSettings::Get();
	if(!Settings) 
	{
		return;
	}
	
	//Get All Actor Components
	TArray<UActorComponent*> Comps;
	ActorOwner->GetComponents<UActorComponent>(Comps);
	
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	// Has to be enabled by user in Project Settings
	//  as it makes older save files un-loadable
	if(!Settings->SaveAllPropertiesMarkedAsSaveGame) 
	{
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	
	//~~~ Comps with Vars that must have unique names in the string var list in old way ~~~
	
	//! Total
	int64 TotalProperties = 0;
	int64 ArchiveBegin = Ar.Tell();		//cant assume starting at 0!
	Ar << TotalProperties;
	
	//For each property to save
	for(int32 v = 0; v < RamaSave_ComponentVarsToSave.Num(); v++)
	{
		FString& EachToSave = RamaSave_ComponentVarsToSave[v];
		for(int32 b = 0; b < Comps.Num(); b++)
		{
			UActorComponent* EachComp = Comps[b];
			UProperty* Property = FindField<UProperty>( EachComp->GetClass(), *EachToSave );
			if(Property) 
			{ 
				if(RamaSave_LogAllSavedComponentProperties)
				{
					UE_LOG(RamaSave,Warning,TEXT("Property found in component and saved to disk! %s %s %s"), *GetClass()->GetName(), *EachComp->GetName(), *EachToSave);
				}	
				 
				//We want each property as pure binary data 
				//		so we can easily save it to disk and not worry about its exact type!
				uint8* InstanceValuePtr = Property->ContainerPtrToValuePtr<uint8>(EachComp);  
				
				int64 StartAfterStringPos = 0; //Ar POSITION TO STORE THE END POS AT
				int64 EndPos = 0; 						//Postion after serializing property
				 
				//Serialize Instance of Property!
				Ar << EachToSave;
				StartAfterStringPos = Ar.Tell();
				Ar << EndPos;
				 
				Property->SerializeItem(FStructuredArchiveFromArchive(Ar).GetSlot(),InstanceValuePtr); 
				
				EndPos = Ar.Tell();
				Ar.Seek(StartAfterStringPos);
				Ar << EndPos;
				Ar.Seek(EndPos);
				
				TotalProperties++;
				 
				//~~~~
				//~~~~
				//~~~~
				break;		//! THIS BREAK MEANS COMPONENT PROPS TO BE SAVED MUST HAVE UNIQUE NAMES AMONG ALL THE OTHER COMPONENTS OF THE ACTOR!
				//~~~~		//! Would have to save how many comps had the same var name if wanted to avoid doing this
				//~~~~		//! and skip as many copies of the var name as needed if not all comps found during loading
				//~~~~
			}
		}
	}
	
	//Put the total at the beginning of archive so know how big of for loop to amke
	const int64 ArchiveEnd = Ar.Tell();
	Ar.Seek(ArchiveBegin);
	Ar << TotalProperties;
	
	//Move back to end of archive 
	//		so as to not ovewrite all data 
	//			when next archive entries occur!!
	Ar.Seek(ArchiveEnd); //<~~~ !
	
	return;
	} //Old Way
	//=============== 

	

	//~~~ Find All SaveGame Marked Properties in All Components ~~~
	
	int64 SaveGameArchiveBegin = Ar.Tell();		//cant assume starting at 0!
	
	//#SC_1
	int32 TotalCompEntries = 0;
	Ar << TotalCompEntries;
	  
	for(int32 b = 0; b < Comps.Num(); b++)
	{
		UActorComponent* EachComp = Comps[b];
		if(!EachComp)
		{
			UE_LOG(RamaSave,Error,TEXT("This should never happen, a comp found via GetComponents was null!"));
			continue;
		}
		
		//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
		//Preliminary Scan to See if saving this comp <3 Rama
		bool ShouldSave = false;
		for (TFieldIterator<UProperty> It(EachComp->GetClass()); It; ++It)
		{
			
			UProperty* Property = *It;
			FString PropertyNameString = Property->GetFName().ToString();
			
			if (RamaSave_ComponentVarsToSave.Contains(PropertyNameString) || Property->HasAnyPropertyFlags(CPF_SaveGame))
			{
				ShouldSave = true;
				TotalCompEntries++;
				break;
			}
		}
		//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
		
		//Clean Skip
		if(!ShouldSave) 
		{  
			//Don't store ANY info about comps that I am not saving
			continue;
		}
		
		
		FString CompName = EachComp->GetName();
		//#SC_2
		Ar << CompName;
		
		
		//~~~ might have to skip if comp removed ~~~
		int64 SkipPos_Seek = Ar.Tell();
		int64 SkipPos = 0;
		//#SC_3
		Ar << SkipPos;
		
		
		int64 CompEntryBegin = Ar.Tell();
		int32 CompPropertiesTotal = 0;
		//#SC_4
		Ar << CompPropertiesTotal;
		
		for (TFieldIterator<UProperty> It(EachComp->GetClass()); It; ++It)
		{

			UProperty* Property = *It;
			FString PropertyNameString = Property->GetFName().ToString();
			
			if (RamaSave_ComponentVarsToSave.Contains(PropertyNameString) || Property->HasAnyPropertyFlags(CPF_SaveGame))
			{
				//Serialize Instance of Property!
				//#SC_5
				Ar << PropertyNameString;
				int64 StartAfterPropertyStringPos = Ar.Tell();
				
				//#SC_6
				int64 PropertyEndPos = 0; 						//Postion after serializing property
				Ar << PropertyEndPos;

				//#SC_7	
				uint8* InstanceValuePtr = Property->ContainerPtrToValuePtr<uint8>(EachComp);
				Property->SerializeItem(FStructuredArchiveFromArchive(Ar).GetSlot(), InstanceValuePtr);

				//~~~ Save End of Entry BEFORE going back ~~~
				PropertyEndPos = Ar.Tell();
				
				Ar.Seek(StartAfterPropertyStringPos);
				
				Ar << PropertyEndPos;
				Ar.Seek(PropertyEndPos);

				CompPropertiesTotal++;
			}
		}
		
		
		//~~~ Update the total! ~~~
		int64 CompEntryEnd = Ar.Tell();
		Ar.Seek(CompEntryBegin);
		Ar << CompPropertiesTotal;
		
		//~~~ Back to End! ~~~
		Ar.Seek(CompEntryEnd);
		
		
		//~~~ Store Skip Pos in case have to pass by a binary chunk for no longer existing comp ~~~
	
		//The skip position is our current position after serializing all data
		SkipPos = Ar.Tell();
		Ar.Seek(SkipPos_Seek);
		Ar << SkipPos;
		
		//~~~ Back to current! ~~~
		Ar.Seek(SkipPos);	
	}
	
	  
	//~~~ Set Total! ~~~
	const int64 SaveGameArchiveEnd = Ar.Tell();
	Ar.Seek(SaveGameArchiveBegin);
	Ar << TotalCompEntries;
	 
	//Move back to end of archive 
	//		so as to not ovewrite all data 
	//			when next archive entries occur!!
	Ar.Seek(SaveGameArchiveEnd); //<~~~ !
}
void URamaSaveComponent::LoadSubComponentVariables(AActor* ActorOwner, UWorld* World, FArchive &Ar)
{
	URamaSaveSystemSettings* Settings = URamaSaveSystemSettings::Get();
	if(!Settings) 
	{
		return;
	}
	
	//Get All Actor Components
	TArray<UActorComponent*> Comps;
	ActorOwner->GetComponents<UActorComponent>(Comps);
	 
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	// Has to be enabled by user in Project Settings
	//  as it makes older save files un-loadable
	if(	ARamaSaveEngine::LoadedSaveVersion < JOY_SAVE_VERSION_MULTISUBCOMPONENT_SAMENAME 
		|| !Settings->SaveAllPropertiesMarkedAsSaveGame)
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	{
	int64 TotalProperties = 0;
	Ar << TotalProperties;
		
	if(RamaSave_LogAllSavedComponentProperties)
	{
		UE_LOG(RamaSave,Warning,TEXT("Component Properties To Load: %s %d"), *GetClass()->GetName(), TotalProperties );
	}
				
	for(int64 v = 0; v < TotalProperties; v++)
	{ 
		//Get info about each property before deciding whether to serialize
		FString PropertyNameString;
		int64 EndPosToSkip;
		Ar << PropertyNameString;
		Ar << EndPosToSkip; 
		
		bool Found = false;
		for(int32 b = 0; b < Comps.Num(); b++)
		{ 
			UActorComponent* EachComp = Comps[b];
			UProperty* Property = FindField<UProperty>( EachComp->GetClass(), *PropertyNameString );
			if(Property) 
			{ 
				if(RamaSave_LogAllSavedComponentProperties)
				{
					UE_LOG(RamaSave,Warning,TEXT("Property found in component and loaded from disk! %s %s %s"), *GetClass()->GetName(), *EachComp->GetName(), *PropertyNameString);
				}	
				uint8* InstanceValuePtr = Property->ContainerPtrToValuePtr<uint8>(EachComp);  //this = object instance that has this property!
				
				//Serialize Instance!
				Property->SerializeItem(FStructuredArchiveFromArchive(Ar).GetSlot(),InstanceValuePtr);
				Found = true;
				
				//~~~~
				//~~~~
				//~~~~
				break;		//! THIS BREAK MEANS COMPONENT PROPS TO BE SAVED MUST HAVE UNIQUE NAMES AMONG ALL THE OTHER COMPONENTS OF THE ACTOR!
				//~~~~	//! Would have to save how many comps had the same var name if wanted to avoid doing this
				//~~~~	//! and skip as many copies of the var name as needed if not all comps found during loading
				//~~~~
			}
		}
		
		if(!Found)
		{
			//SKIP TO END OF THIS PROPERTY AS IT WAS NOT FOUND IN ACTUAL CLASS
			// MUST HAVE BEEN REMOVED
			Ar.Seek(EndPosToSkip);
			 
			UE_LOG(RamaSave,Warning,TEXT("Property in save file but not found in class, re-save to get rid of this message %s %s"), *GetClass()->GetName(), *PropertyNameString);
		}
	}
	return;
	} //Old way
	
	//===================
	
	//``
	int32 TotalCompEntries = 0;
	//#SC_1
	Ar << TotalCompEntries;
	
	for(int32 v = 0; v < TotalCompEntries; v++)
	{
		//#SC_2
		FString CompName;
		Ar << CompName;
		  
		//#SC_3
		int64 SkipPos;
		Ar << SkipPos;
		 
		
		//#SC_4
		int32 CompPropertiesTotal;
		Ar << CompPropertiesTotal;
		
		//Find Comp
		UActorComponent* FoundComponent = nullptr;
		for(UActorComponent* Each : Comps)
		{
			if(Each && Each->GetName() == CompName)
			{
				FoundComponent = Each;
				break;
			}
		}
		
		//Verify Comp Exists!
		if(!FoundComponent)
		{
			UE_LOG(RamaSave,Error,TEXT("Save data found for a component that no longer exists! %s"), *CompName);
			Ar.Seek(SkipPos);
			continue;
			//~~~~~~
		}
		 
		for(int32 b = 0; b < CompPropertiesTotal; b++)
		{
			//Get info about each property before Loading it
			FString PropertyNameString;
			int64 EndPosToSkip;
			//#SC_5
			Ar << PropertyNameString;
			//#SC_6
			Ar << EndPosToSkip;

			UProperty* Property = FindField<UProperty>(FoundComponent->GetClass(), *PropertyNameString);
			if (Property)
			{
				if (RamaSave_LogAllSavedComponentProperties)
				{
					UE_LOG(RamaSave, Warning, TEXT("Property found in component and loaded from disk! %s %s %s"), *GetClass()->GetName(), *FoundComponent->GetName(), *PropertyNameString);
				}
				uint8* InstanceValuePtr = Property->ContainerPtrToValuePtr<uint8>(FoundComponent);  //this = object instance that has this property!

				//#SC_7																		  //Serialize Instance!
				Property->SerializeItem(FStructuredArchiveFromArchive(Ar).GetSlot(), InstanceValuePtr);

			}
			else
			{
				//SKIP TO END OF THIS PROPERTY AS IT WAS NOT FOUND IN ACTUAL CLASS
				// MUST HAVE BEEN REMOVED
				Ar.Seek(EndPosToSkip);

				UE_LOG(RamaSave, Warning, TEXT("Property in save file but not found in class, re-save to get rid of this message %s %s"), *GetClass()->GetName(), *PropertyNameString);
			}
		}
	} 
	
	
}
	