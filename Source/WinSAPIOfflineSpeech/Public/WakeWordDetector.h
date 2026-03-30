// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include <sapi.h>

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WakeWordDetector.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWakeWordDetected, const FString&, DetectedText);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class WINSAPIOFFLINESPEECH_API UWakeWordDetector : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UWakeWordDetector();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(BlueprintAssignable)
	FOnWakeWordDetected OnWakeWordDetected;

	UFUNCTION(BlueprintCallable, Category = "WinSAPIOfflineWakeWord")
	void StartWakeWordListening(const FString& WakeWord);
	
	UFUNCTION(BlueprintCallable, Category = "WinSAPIOfflineWakeWord")
	void StartListening(bool bListen = true);

	UFUNCTION(BlueprintCallable, Category = "WinSAPIOfflineWakeWord")
	TArray<FString> GetInstalledLanguages();
private:
	bool InitializeCOM();
	bool CreateRecognizer();
	bool SetupAudioInput();
	bool CreateRecognitionContext();
	bool SetupGrammar();
	void RunListeningLoop(const FString& LowerWakeWord);
	void HandleRecognitionEvent(const SPEVENT& Event, const FString& LowerWakeWord);
	void Cleanup();

	// 🔹 SAPI Core
	ISpRecognizer* pRecognizer = nullptr;
	ISpRecoContext* pContext = nullptr;
	ISpRecoGrammar* pGrammar = nullptr;

	// 🔹 Audio Input
	ISpObjectToken* pAudioToken = nullptr;
	ISpObjectTokenCategory* pCategory = nullptr;
	IEnumSpObjectTokens* pEnum = nullptr;

	// 🔹 Event
	HANDLE hEvent = nullptr;

	// 🔹 Control
	bool bStopListening = false;
};