// Fill out your copyright notice in the Description page of Project Settings.


#include "WakeWordDetector.h"

#include <sapi.h>

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(x) \
if (x) { \
x->Release(); \
x = nullptr; \
}
#endif

// Sets default values for this component's properties
UWakeWordDetector::UWakeWordDetector()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UWakeWordDetector::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}


// Called every frame
void UWakeWordDetector::TickComponent(float DeltaTime, ELevelTick TickType,
                                      FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

void UWakeWordDetector::PrepWakeWordListening(const FString& WakeWord)
{
    bStopListening = false;

    Async(EAsyncExecution::Thread, [this, WakeWord]()
    {
        if (!InitializeCOM()) return;

        FString LowerWakeWord = WakeWord.ToLower();

        if (!CreateRecognizer()) goto cleanup;
        if (!SetupAudioInput()) goto cleanup;
        if (!CreateRecognitionContext()) goto cleanup;
        if (!SetupGrammar()) goto cleanup;

        RunListeningLoop(LowerWakeWord);

    cleanup:
        Cleanup();
        ::CoUninitialize();
    });
}

void UWakeWordDetector::PrepWakeWordsListening(const TArray<FString>& WakeWords)
{
	bStopListening = false;

	Async(EAsyncExecution::Thread, [this, WakeWords]()
	{
		if (!InitializeCOM()) return;

		// Convert all wake words to lowercase once
		TArray<FString> LowerWakeWords;
		for (const FString& Word : WakeWords)
		{
			LowerWakeWords.Add(Word.ToLower());
		}

		if (!CreateRecognizer()) goto cleanup;
		if (!SetupAudioInput()) goto cleanup;
		if (!CreateRecognitionContext()) goto cleanup;
		if (!SetupGrammar()) goto cleanup;

		RunListeningLoop_Multi(LowerWakeWords);

	cleanup:
		Cleanup();
		::CoUninitialize();
	});
}

void UWakeWordDetector::RunListeningLoop_Multi(const TArray<FString>& LowerWakeWords)
{
	UE_LOG(LogTemp, Warning, TEXT("🎤 Listening Started (Multi Wake Words)"));

	while (!bStopListening)
	{
		DWORD waitResult = WaitForSingleObject(hEvent, 500);

		if (waitResult == WAIT_OBJECT_0)
		{
			SPEVENT event;
			ULONG fetched = 0;

			while (SUCCEEDED(pContext->GetEvents(1, &event, &fetched)) && fetched > 0)
			{
				if (event.eEventId == SPEI_RECOGNITION)
				{
					HandleRecognitionEvent_Multi(event, LowerWakeWords);
				}
			}
		}
		else if (waitResult == WAIT_FAILED)
		{
			UE_LOG(LogTemp, Error, TEXT("❌ Wait failed"));
			break;
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("🛑 Listening Stopped"));
}

void UWakeWordDetector::HandleRecognitionEvent_Multi(const SPEVENT& Event, const TArray<FString>& LowerWakeWords)
{
	ISpRecoResult* pResult = (ISpRecoResult*)Event.lParam;
	if (!pResult) return;

	LPWSTR pText = nullptr;

	if (SUCCEEDED(pResult->GetText(
		SP_GETWHOLEPHRASE,
		SP_GETWHOLEPHRASE,
		FALSE,
		&pText,
		nullptr)))
	{
		FString text = FString(pText);
		::CoTaskMemFree(pText);

		FString lower = text.ToLower();

		UE_LOG(LogTemp, Warning, TEXT("🎤 %s"), *text);

		// 🔥 Check against all wake words
		for (const FString& WakeWord : LowerWakeWords)
		{
			if (lower.Contains(WakeWord))
			{
				Async(EAsyncExecution::TaskGraphMainThread, [this, text]()
				{
					OnWakeWordDetected.Broadcast(text);
				});

				break; // stop after first match
			}
		}
	}
}

bool UWakeWordDetector::InitializeCOM()
{
	if (FAILED(::CoInitialize(nullptr)))
	{
		UE_LOG(LogTemp, Error, TEXT("❌ COM init failed"));
		return false;
	}
	return true;
}

bool UWakeWordDetector::CreateRecognizer()
{
	HRESULT hr = CoCreateInstance(
		CLSID_SpInprocRecognizer,
		nullptr,
		CLSCTX_ALL,
		IID_ISpRecognizer,
		(void**)&pRecognizer);

	return SUCCEEDED(hr);
}

bool UWakeWordDetector::SetupAudioInput()
{
	HRESULT hr;

	hr = CoCreateInstance(CLSID_SpObjectTokenCategory, nullptr, CLSCTX_ALL,
						  IID_ISpObjectTokenCategory, (void**)&pCategory);
	if (FAILED(hr)) return false;

	hr = pCategory->SetId(SPCAT_AUDIOIN, FALSE);
	if (FAILED(hr)) return false;

	hr = pCategory->EnumTokens(nullptr, nullptr, &pEnum);
	if (FAILED(hr)) return false;

	ULONG fetched = 0;
	hr = pEnum->Next(1, &pAudioToken, &fetched);
	if (FAILED(hr) || fetched == 0) return false;

	hr = pRecognizer->SetInput(pAudioToken, TRUE);
	if (FAILED(hr)) return false;

	SAFE_RELEASE(pAudioToken);
	SAFE_RELEASE(pEnum);
	SAFE_RELEASE(pCategory);

	return true;
}

bool UWakeWordDetector::CreateRecognitionContext()
{
	HRESULT hr;

	hr = pRecognizer->CreateRecoContext(&pContext);
	if (FAILED(hr)) return false;

	hr = pContext->SetNotifyWin32Event();
	if (FAILED(hr)) return false;

	hr = pContext->SetInterest(
		SPFEI(SPEI_RECOGNITION),
		SPFEI(SPEI_RECOGNITION));
	if (FAILED(hr)) return false;

	hEvent = pContext->GetNotifyEventHandle();

	return hEvent != nullptr;
}

bool UWakeWordDetector::SetupGrammar()
{
	HRESULT hr;

	hr = pContext->CreateGrammar(1, &pGrammar);
	if (FAILED(hr)) return false;

	hr = pGrammar->LoadDictation(nullptr, SPLO_STATIC);
	if (FAILED(hr)) return false;

	hr = pGrammar->SetDictationState(SPRS_ACTIVE);
	if (FAILED(hr)) return false;

	hr = pRecognizer->SetRecoState(SPRST_ACTIVE);
	if (FAILED(hr)) return false;

	return true;
}

void UWakeWordDetector::RunListeningLoop(const FString& LowerWakeWord)
{
	UE_LOG(LogTemp, Warning, TEXT("🎤 Listening Started"));

	while (!bStopListening)
	{
		DWORD waitResult = WaitForSingleObject(hEvent, WaitListening);

		if (waitResult == WAIT_OBJECT_0)
		{
			SPEVENT event;
			ULONG fetched = 0;

			while (SUCCEEDED(pContext->GetEvents(1, &event, &fetched)) && fetched > 0)
			{
				if (event.eEventId == SPEI_RECOGNITION)
				{
					HandleRecognitionEvent(event, LowerWakeWord);
				}
			}
		}
		else if (waitResult == WAIT_FAILED)
		{
			UE_LOG(LogTemp, Error, TEXT("❌ Wait failed"));
			break;
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("🛑 Listening Stopped"));
}

void UWakeWordDetector::HandleRecognitionEvent(const SPEVENT& Event, const FString& LowerWakeWord)
{
	ISpRecoResult* pResult = (ISpRecoResult*)Event.lParam;
	if (!pResult) return;

	LPWSTR pText = nullptr;

	if (SUCCEEDED(pResult->GetText(
		SP_GETWHOLEPHRASE,
		SP_GETWHOLEPHRASE,
		FALSE,
		&pText,
		nullptr)))
	{
		FString text = FString(pText);
		::CoTaskMemFree(pText);

		FString lower = text.ToLower();

		UE_LOG(LogTemp, Warning, TEXT("🎤 %s"), *text);

		if (lower.Contains(LowerWakeWord))
		{
			Async(EAsyncExecution::TaskGraphMainThread, [this, text]()
			{
				OnWakeWordDetected.Broadcast(text);
			});
		}
	}
}

void UWakeWordDetector::Cleanup()
{
	SAFE_RELEASE(pGrammar);
	SAFE_RELEASE(pContext);
	SAFE_RELEASE(pRecognizer);
	SAFE_RELEASE(pAudioToken);
	SAFE_RELEASE(pEnum);
	SAFE_RELEASE(pCategory);
}

void UWakeWordDetector::StartListening()
{
    bStopListening = false;
}

void UWakeWordDetector::StopListening()
{
	bStopListening = true;
}

TArray<FString> UWakeWordDetector::GetInstalledLanguages()
{
	TArray<FString> Languages;

	HRESULT hr = S_OK;

	ISpObjectTokenCategory* LocalCategory = nullptr;
	IEnumSpObjectTokens* LocalEnum = nullptr;
	ISpObjectToken* pToken = nullptr;

	ULONG fetched = 0;

	hr = CoCreateInstance(CLSID_SpObjectTokenCategory, nullptr, CLSCTX_ALL,
						  IID_ISpObjectTokenCategory, (void**)&LocalCategory);
	if (FAILED(hr)) return Languages;

	hr = LocalCategory->SetId(SPCAT_RECOGNIZERS, FALSE);
	if (FAILED(hr))
	{
		LocalCategory->Release();
		return Languages;
	}

	hr = LocalCategory->EnumTokens(nullptr, nullptr, &LocalEnum);
	if (FAILED(hr))
	{
		LocalCategory->Release();
		return Languages;
	}

	while (LocalEnum->Next(1, &pToken, &fetched) == S_OK)
	{
		WCHAR* lang = nullptr;

		// 🔥 Get ATTRIBUTES instead of direct value
		ISpDataKey* pAttributes = nullptr;
		hr = pToken->OpenKey(L"Attributes", &pAttributes);

		FString LangStr = TEXT("Unknown");

		if (SUCCEEDED(hr) && pAttributes)
		{
			if (SUCCEEDED(pAttributes->GetStringValue(L"Language", &lang)) && lang)
			{
				LangStr = FString(lang);
				::CoTaskMemFree(lang);
			}

			pAttributes->Release();
		}
		
		Languages.Add(LangStr);

		pToken->Release();
		pToken = nullptr;
	}

	if (LocalEnum) LocalEnum->Release();
	if (LocalCategory) LocalCategory->Release();

	return Languages;
}