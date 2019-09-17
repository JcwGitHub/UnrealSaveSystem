// Copyright 2015 by Nathan "Rama" Iyer. All Rights Reserved.
#pragma once
 
#include "RamaSaveUtility.h"

#include "RamaSaveComponent.generated.h"
  
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams( FRamaSaveFullyLoadedSignature, class URamaSaveComponent*, RamaSaveComponent, FString, LevelPackageName );
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam( FRamaSavePreSaveSignature, class URamaSaveComponent*, RamaSaveComponent );
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams( FRamaPlayerLoadedSignature, APlayerController*, PC, APawn*, Pawn, int32, PlayerIndex);
  
//Only intended for Static Meshs support, not skeletal ragdoll
USTRUCT()
struct FRBSave
{
	GENERATED_USTRUCT_BODY()
	
	UPROPERTY()
	FVector_NetQuantize100 PhysicsLocation = FVector::ZeroVector;
	
	UPROPERTY()
	FQuat PhysicsRotation = FQuat::Identity;
	
	UPROPERTY()
	FVector_NetQuantize100 LinearVelocity = FVector::ZeroVector;
	
	UPROPERTY()
	FVector_NetQuantize100 AngularVelocity = FVector::ZeroVector;
	
	void FillFrom(UPrimitiveComponent* Comp)
	{
		if(!Comp) return;
		PhysicsLocation = Comp->GetComponentLocation();
		PhysicsRotation = Comp->GetComponentQuat();
		LinearVelocity = Comp->GetPhysicsLinearVelocity();
		AngularVelocity = Comp->GetPhysicsAngularVelocity();
	}
	void Apply(UPrimitiveComponent* Comp)
	{
		if(!Comp) return;
		Comp->SetAllPhysicsPosition(PhysicsLocation);
		Comp->SetWorldRotation(PhysicsRotation); 
		Comp->SetPhysicsLinearVelocity(LinearVelocity);
		Comp->SetPhysicsAngularVelocity(AngularVelocity);
	}
	
	FRBSave(){}
};

/*
//Serialization for Engine Type
FORCEINLINE FArchive &operator<<(FArchive &Ar, FRigidBodyState& TheStruct)
{ 
	Ar << TheStruct.Position;
	Ar << TheStruct.Quaternion;
	Ar << TheStruct.LinVel;
	Ar << TheStruct.AngVel;
	Ar << TheStruct.Flags;
	
	return Ar;
}
*/
  
//Custom Struct Serialization
FORCEINLINE FArchive &operator<<(FArchive &Ar, FRBSave& TheStruct)
{ 
	Ar << TheStruct.PhysicsLocation;
	Ar << TheStruct.PhysicsRotation;
	Ar << TheStruct.LinearVelocity;
	Ar << TheStruct.AngularVelocity;
	return Ar;
}

//Runtime Only
// Needed for the PhysicsTimer, see .cpp for explanation of PhysicsTimer
//		This struct allows me to not save RB State for every primitive component in the whole world ;)
//			by pairing each comp with the PhysState to be loaded
USTRUCT()
struct FRBLoad
{
	GENERATED_USTRUCT_BODY()
	
	//not saved to disk
	FRBSave PhysState;
	UPrimitiveComponent* Comp = nullptr;
	
	void Activate()
	{
		if(!Comp) return;
		PhysState.Apply(Comp);
	}
	FRBLoad() {}
	FRBLoad(UPrimitiveComponent* InComp, const FRBSave& InState )
	{
		Comp = InComp;
		PhysState = InState;
	}
};


/*
	~~~ Rama Save System ~~~

	1. When saving, all actors with Rama Save Components are saved to disk!
		These methods are called, in order:
			a. RamaSave_PreSave_CPP()
			b. RamaSave_PreSave()
			c. RamaSaveEvent_PreSave()
			 
	2. When loading, any existing actors with Rama Save Components are deleted!
		These methods are called, in order:
			a. RamaSave_PostLoad_CPP()
			a. RamaSave_PostLoad()
			b. RamaSaveEvent_ActorFullyLoaded()
*/ 
UCLASS(ClassGroup=Rama, BlueprintType, Blueprintable, meta=(BlueprintSpawnableComponent))
class RAMASAVESYSTEM_API URamaSaveComponent : public UActorComponent
{ 
	GENERATED_BODY()
public:
 
	UFUNCTION(Category="Rama Save System", BlueprintPure)
	static void Test(UScriptStruct* Struct, TArray<uint8>& ByteData)
	{
		
	}
	
	/** 
		If you generate a unique ID for an instance of your actor that uses this component: 
		
		1. I will NOT destroy the actor during level loading!
	
		2. I will save the unique ID with the data chunk and during loading, I will find an actor that has the same unique id and load the data to that actor.
		
		All of my load events will still be called, like Actor Fully Loaded, but I will not be destroying the actor, only reloading data.
		
		This is great for actors in your level blueprint or persistent actors that you don't want to destroy, including player characters and items that never leave the world.
		
		~~~ Unique Ids Must be Unique! ~~~
		3. Please understand that id's must be unique per instance, not per class, because that is the only way my save system can know exactly which actor to load data into.
		
		For this reason it is best to generate a unique id in your constructor, if no valid GUID has been set yet.
		
		~~~ Actor Duplication ~~~
		4. Please note that during widget-drag actor duplication, the BP constructor does not run, you will have to manually click to generate a unique ID for duplicated actors.
		
		I am still investigating how to trigger a BP constructor activation during actor duplication without needing an editor plugin.
		
		~~~ Reversing the Process ~~~
		If you change your mind, just invalidate the guid so it goes back to 0's and my save system will delete/recreate the actor again.
		
		~~~ Debugging ~~~
		Check your log to see if I am ever ignoring two actors with the same GUID, and find the actors that are sharing the GUID and generate new unique ids for the actors.
		
		The only likely bug you can encounter is if you have 2+ actors using same GUID, then my save system will be confused :)
		
		Every actor instance in the world or spawned into the world MUST have a unique GUID :)
		
		♥ Rama
	*/
	UPROPERTY(Category="Rama Save System", EditAnywhere, BlueprintReadWrite)
	FGuid RamaSave_PersistentActorUniqueID = FGuid(); //Invalid
	 
	//! Auto-create new FGuid on Alt + Drag
	#if WITH_EDITOR
	virtual void PostEditImport() override
	{
		Super::PostEditImport();
		
		if(RamaSave_PersistentActorUniqueID != FGuid())
		{
			RamaSave_PersistentActorUniqueID = FGuid::NewGuid();
		}
	}
	#endif
	
	/** Whether to log information regarding Actor GUID Saving and Loading */
	UPROPERTY(Category="Rama Save System", EditDefaultsOnly, BlueprintReadWrite)
	bool RamaSave_LogPersistentActorGUID = true;
	
	/**
		Add tags to instances of an actor class / blueprint so you can identify to my save system which actors you want to load.
		
		These tags are optional, and are ideal for things like level streaming or any other game-specific conditions where you want to selectively load actors based on your own game's needs.
	*/
	UPROPERTY(Category="Rama Save System", EditAnywhere, BlueprintReadWrite)
	TArray<FString> RamaSave_SaveTags;
	
	UFUNCTION(Category="Rama Save System", BlueprintPure)
	bool RamaSave_HasSaveTag(FString SaveTag)
	{
		return RamaSave_SaveTags.Contains(SaveTag);
	}
	
	/** This function tells you which streaming level the owner of this component is part of!!!! Yes!!! Those of you using UE4's Level Streaming system will love me for this! <3 Rama */
	UFUNCTION(Category="Rama Save System", BlueprintPure)
	FString GetActorStreamingLevelPackageName();
	
	/** Is the owner of this component in the persistent level? :) <3 Rama */
	UFUNCTION(Category="Rama Save System", BlueprintPure)
	bool GetActorIsInPersistentLevel();
	
	UFUNCTION(Category="Rama Save System", BlueprintPure)
	bool RamaSave_HasAnyOfSaveTags(const TArray<FString>& SaveTags)
	{
		for(const FString& Each : SaveTags)
		{
			if(RamaSave_SaveTags.Contains(Each))
			{
				return true;
			}
		}
		return false;
	}
	/** 
		Customize whether or not a particular actor should be teleported to its position at the time of saving after a load has completed! 
	
		If this is false, all the data will be loaded into the actor, but the actor's transform will not be changed (position, rotation, scaling)
		
		You are then free to apply custom logic as to what to do with a unit after a load event, inside of Actor Fully Loaded.
		
		<3 Rama 
	*/
	UPROPERTY(Category="Rama Save System", EditAnywhere, BlueprintReadWrite)
	bool RamaSave_ShouldLoadActorWorldPosition = true;
	
	/** Customize whether or not a particular actor should be saved based on game conditions, such as whether the unit is alive! */
	UPROPERTY(Category="Rama Save System", EditAnywhere, BlueprintReadWrite)
	bool RamaSave_ShouldSaveActor = true;
	    
	/** 
	 *	~~~ All variables of this component are saved automatically for you! ~~~
	 * 
	 *	This is a list of vars to save/load corresponding to the ~ owning actor ~ of this component.
	 *
	 *  Variables can be from C++ base classes of the owning Actor or can be Blueprint-created!
	 *
	 * Other actor component variables of the owning actor are ignored. 
	 * 
	 * It is a primary feature of the Rama Save System that you can simply enter Actor variable names 
	 * into this list and they will be saved for you, along with any variables you create for the Rama Save Component.
	 *
	 * Suggestion: Turn off ShowFriendlyNames via Editor Preferences to see accurate names to copy/paste.
	 *
	 * <3 Rama
	 */
	UPROPERTY(Category="Rama Save System", EditAnywhere, BlueprintReadWrite)
	TArray<FString> RamaSave_OwningActorVarsToSave;
	
	/** 
	 * If you want to save variables present in a subcomponent of this Actor
	 *  then add them to this list!
	 *
	 *
	 * Essential Note: Component properties to be saved MUST have unique names
	 *   among all the other components and their variable names of the owning Actor.
	 *
	 * If your variable is not getting saved/loaded properly, make sure it has a unique name!
	 * 
	 *
	 * If you later remove the component I will ignore the no-longer-valid saved data for the component!
	 *
	 * This prevents the save system from crashing!
	 * 
	 * <3 Rama
	 */
	UPROPERTY(Category="Rama Save System", EditAnywhere, BlueprintReadWrite)
	TArray<FString> RamaSave_ComponentVarsToSave;
	
	
	/** Whether to print extra information about the saving / loading process of this Rama Save Component, including reminders of which Actor Properties are not being saved in the list. */
	UPROPERTY(Category="Rama Save System", EditAnywhere, BlueprintReadWrite)
	bool RamaSave_VerboseLog = false;
	   
	/** Whether to print out all properties of this Rama Save Component that are being saved to disk! <3*/
	UPROPERTY(Category="Rama Save System", EditAnywhere, BlueprintReadWrite)
	bool RamaSave_LogAllSavedComponentProperties = false;
	 
	/** Whether to save/load Physics data for all primitive components of the Owning Actor. Please note that runtime dynamically added primitive components (static mesh, skel mesh) cannot be saved/loaded using my built in system. Custom BP or C++ code is required. */
	UPROPERTY(Category="Rama Save System", EditAnywhere, BlueprintReadWrite)
	bool RamaSave_SavePhysicsData = false;
	
//~~~~~~~~~~~~~~~~~~~~~~~
// BP or C++ For Customization
//~~~~~~~~~~~~~~~~~~~~~~~
public:
	
	UPROPERTY(Category="Rama Save System", BlueprintAssignable)
	FRamaSaveFullyLoadedSignature RamaSaveEvent_ActorFullyLoaded;
	
	UPROPERTY(Category="Rama Save System", BlueprintAssignable)
	FRamaSavePreSaveSignature RamaSaveEvent_PreSave;
	
	UPROPERTY(Category="Rama Save System", BlueprintAssignable)
	FRamaPlayerLoadedSignature RamaSaveEvent_PlayerLoaded;
	
	UFUNCTION(Category="Rama Save System", BlueprintImplementableEvent)
	void RamaSave_PreSave();
	
	UFUNCTION(Category="Rama Save System", BlueprintImplementableEvent)
	void RamaSave_PostLoad();
	  
	/** C++ hooks run _before_ the BP assignable events */
	virtual void RamaSave_PreSave_CPP() {}
	virtual void RamaSave_PostLoad_CPP() {}
	virtual void RamaSave_PlayerLoaded_CPP(APlayerController* PC, APawn* Pawn, int32 PlayerIndex) {}
  
	
public:
	UPROPERTY()
	FTransform OwningActorTransform;
	
public:
	bool DontLoadPlayerPawns;
	//Make above a struct 

	//Make a setting struct eventually instead of just passing the single bool of DontLoadPlayerPawns
	
	bool RamaSave_SaveToFile(UWorld* World, FArchive &Ar);
	static bool RamaSave_LoadFromFile(UWorld* World, int32 RamaSaveSystemVersion, const TArray<FString>& LoadActorsWithSaveTags, FArchive &Ar, URamaSaveComponent*& LoadedComp, bool DontLoadPlayerPawns, FString LoadOnlyStreamingLevel="");
	
public:
	void SaveSelfAndSubclassVariables(FArchive &Ar);
	void LoadSelfAndSubclassVariables(FArchive &Ar);
	
	void SaveOwnerVariables(UWorld* World, FArchive &Ar);
	void SaveOwnerVariables_Pawn(APawn* Pawn, UWorld* World, FArchive &Ar);
	void SaveOwnerVariables_Physics(AActor* ActorOwner, UWorld* World, FArchive &Ar);
	void SaveSubComponentVariables(AActor* ActorOwner, UWorld* World, FArchive &Ar);
	
	void LoadOwnerVariables(UWorld* World, FArchive &Ar);
	void LoadOwnerVariables_Pawn(APawn* Pawn, UWorld* World, FArchive &Ar);
	void LoadOwnerVariables_Physics(AActor* ActorOwner, UWorld* World, FArchive &Ar);
	void LoadSubComponentVariables(AActor* ActorOwner, UWorld* World, FArchive &Ar);
	
	//Name conflict with UObject::PreSave
	void RamaCPP_PreSave();
	void FullyLoaded(); 
	
	//Not saved to disk, runtime load porcess only. 
	//		Has no impact on save file sizes, loaded into memory only
	TArray<FRBLoad> RBLoadStates;
	void PhysicsTimer();
	
	//For Level Streaming, only valid after a load has occurred, valid thereafter.
	FString LevelPackageName = "Uninitialized Data";
	
public:
	template <typename VictoryObjType>
	static FORCEINLINE VictoryObjType* SpawnBP(
		UWorld* TheWorld, 
		UClass* TheBP,
		const FVector& Loc,
		const FRotator& Rot = FRotator::ZeroRotator,
		const bool bNoCollisionFail = true,
		AActor* Owner = NULL,
		APawn* Instigator = NULL
	){
		if(!TheWorld) return NULL;
		if(!TheBP) return NULL;
		//~~~~~~~~~~~
		
		FActorSpawnParameters SpawnInfo;
		if (bNoCollisionFail)
		{
			SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		}
		SpawnInfo.Owner 				= Owner;
		SpawnInfo.Instigator			= Instigator;
		SpawnInfo.bDeferConstruction 	= false;
		
		return TheWorld->SpawnActor<VictoryObjType>(TheBP, Loc ,Rot, SpawnInfo );
	}

	//************
	// LOAD CLASS
	//************
	 
	//Get Path
	static FORCEINLINE FString GetClassPath(UClass* ClassPtr)
	{
		return FStringClassReference(ClassPtr).ToString();
	}
	  
	//*************************
	//TEMPLATE Load Obj From Path
	//*************************
	static FORCEINLINE UClass* LoadClassFromPath(const FString& Path)
	{ 
		if(Path == "") return NULL;
		  
		return StaticLoadClass(UObject::StaticClass(), NULL, *Path, NULL, LOAD_None, NULL);
	}
	
};