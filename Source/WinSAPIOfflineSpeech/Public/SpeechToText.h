// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#include <mmreg.h>
#include "Windows/HideWindowsPlatformTypes.h"
#include "SpeechToText.generated.h"

DECLARE_DYNAMIC_DELEGATE_OneParam(FOnSpeechRecognized, const FString&, RecognizedText);
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnSpeechRecognitionFailed, const FString&, ErrorReason);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class WINSAPIOFFLINESPEECH_API USpeechToText : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	USpeechToText();

	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	bool ContinueRecognize;

private:	
	UFUNCTION(BlueprintCallable, Category = "WinSAPIOfflineSpeech")
	void RecognizeAudioBytesAsync(const TArray<uint8>& AudioData, const FOnSpeechRecognized& OnSpeechRecognized, const FOnSpeechRecognitionFailed& OnFailed);
	
	bool LoadWavFileToPCM(const FString& FilePath, TArray<uint8>& OutPCMData, WAVEFORMATEX& OutFormat, FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "WinAPIOfflineSpeech")
	void StopRunningRecognization();
	
	UFUNCTION(BlueprintCallable, Category = "WinSAPIOfflineSpeech")
	void RecognizeWavFileAsync(const FString& FilePath, const FOnSpeechRecognized& OnSpeechRecognized, const FOnSpeechRecognitionFailed& OnFailed);
};
