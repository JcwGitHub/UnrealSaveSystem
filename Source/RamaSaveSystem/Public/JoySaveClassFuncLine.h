/*
	Joy String 
		Current Class, File, and Line Number!
			by Rama
			
	PreProcessor commands to get 
		a. Class name
		b. Function Name
		c. Line number 
		d. Function Signature (including parameters)
		
	Gives you a UE4 FString anywhere in your code that these macros are used!
	
	Ex: 
		You can use JOYSTR_CUR_CLASS_LINE anywhere to get a UE4 FString back telling you 
		what the current class is where you called this macro!
	
	Ex:
		This macro prints the class and line along with the message of your choosing!
		VSCREENMSG("Have fun today!");
	<3  Rama
*/
#pragma once

//Current Class Name + Function Name where this is called!
#define JOYSTR_CUR_CLASS_FUNC (FString(__FUNCTION__))

//Current Class where this is called!
#define JOYSTR_CUR_CLASS (FString(__FUNCTION__).Left(FString(__FUNCTION__).Find(TEXT(":"))) )

//Current Function Name where this is called!
#define JOYSTR_CUR_FUNC (FString(__FUNCTION__).Right(FString(__FUNCTION__).Len() - FString(__FUNCTION__).Find(TEXT("::")) - 2 ))
  
//Current Line Number in the code where this is called!
#define JOYSTR_CUR_LINE  (FString::FromInt(__LINE__))

//Current Class and Line Number where this is called!
#define JOYSTR_CUR_CLASS_LINE (JOYSTR_CUR_CLASS + "(" + JOYSTR_CUR_LINE + ")")
  
//Current Function Signature where this is called!
#define JOYSTR_CUR_FUNCSIG (FString(__FUNCSIG__))


//Victory Screen Message
// 	Gives you the Class name and exact line number where you print a message to yourself!
#define VSCREENMSG(Param1) (GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, *(JOYSTR_CUR_CLASS_LINE + ": " + Param1)) )

#define VSCREENMSG2(Param1,Param2) (GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, *(JOYSTR_CUR_CLASS_LINE + ": " + Param1 + " " + Param2)) )

#define VSCREENMSGSEC(Seconds,Param1) (GEngine->AddOnScreenDebugMessage(-1, Seconds, FColor::Red, *(JOYSTR_CUR_CLASS_LINE + ": " + Param1)) )
#define VSCREENMSG2SEC(Seconds,Param1,Param2) (GEngine->AddOnScreenDebugMessage(-1, Seconds, FColor::Red, *(JOYSTR_CUR_CLASS_LINE + ": " + Param1 + " " + Param2)) )

#define VSCREENMSGF(Param1,Param2) (GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, *(JOYSTR_CUR_CLASS_LINE + ": " + Param1 + " " + FString::SanitizeFloat(Param2))) )
   
#define RS_LOG(LogCat, Param1) 				UE_LOG(LogCat,Warning,TEXT("%s: %s"), *JOYSTR_CUR_CLASS_LINE, *FString(Param1))
#define RS_LOGM(LogCat, FormatString , ...) 	UE_LOG(LogCat,Warning,TEXT("%s: %s"), 	*JOYSTR_CUR_CLASS_LINE, *FString::Printf(TEXT(FormatString), ##__VA_ARGS__ ) )
#define RS_LOG2(LogCat, Param1,Param2) 	UE_LOG(LogCat,Warning,TEXT("%s: %s %s"), *JOYSTR_CUR_CLASS_LINE, *FString(Param1),*FString(Param2))
#define RS_LOGF(LogCat, Param1,Param2) 	UE_LOG(LogCat,Warning,TEXT("%s: %s %f"), *JOYSTR_CUR_CLASS_LINE, *FString(Param1),Param2)
#define RS_LOGD(LogCat, Param1,Param2) 	UE_LOG(LogCat,Warning,TEXT("%s: %s %d"), *JOYSTR_CUR_CLASS_LINE, *FString(Param1),Param2)

#define BOOLSTR(s) ((s) ? "True" : "False")