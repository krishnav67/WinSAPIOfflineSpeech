// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TextToSpeech.generated.h"

DECLARE_DYNAMIC_DELEGATE_OneParam(FOnSpeechBytesGenerated, const TArray<uint8>&, WavData);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class WINSAPIOFFLINESPEECH_API UTextToSpeech : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UTextToSpeech();

	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;
protected:
	// Called when the game starts
	virtual void BeginPlay() override;

private:
	/** Generates speech asynchronously and returns the byte via delegate */
	UFUNCTION(BlueprintCallable, Category = "WinSAPIOfflineSpeech")
	void GenerateSpeechAsWavBytesAsync(const FString& Text, const FString& VoiceName, const FOnSpeechBytesGenerated& OnBytesGenerated);
};