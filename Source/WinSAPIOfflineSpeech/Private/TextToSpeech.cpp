// Fill out your copyright notice in the Description page of Project Settings.


#include "TextToSpeech.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#include <sapi.h>
#include "Windows/HideWindowsPlatformTypes.h"

// Sets default values for this component's properties
UTextToSpeech::UTextToSpeech()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UTextToSpeech::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}


// Called every frame
void UTextToSpeech::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

void UTextToSpeech::GenerateSpeechAsWavBytesAsync(const FString& Text, const FString& VoiceName,
	const FOnSpeechBytesGenerated& OnBytesGenerated)
{
	Async(EAsyncExecution::Thread, [Text, VoiceName, OnBytesGenerated]()
    {
        if (FAILED(::CoInitialize(NULL)))
        {
            Async(EAsyncExecution::TaskGraphMainThread, [OnBytesGenerated]()
            {
                OnBytesGenerated.ExecuteIfBound(TArray<uint8>());
            });
            return;
        }

        ISpVoice* pVoice = nullptr;
        ISpStream* pStream = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_SpVoice, NULL, CLSCTX_ALL, IID_ISpVoice, (void**)&pVoice);

        if (SUCCEEDED(hr))
        {
            ISpObjectTokenCategory* pCategory = nullptr;
            if (SUCCEEDED(CoCreateInstance(CLSID_SpObjectTokenCategory, NULL, CLSCTX_ALL,
                                           IID_ISpObjectTokenCategory, (void**)&pCategory)))
            {
                if (SUCCEEDED(pCategory->SetId(SPCAT_VOICES, false)))
                {
                    IEnumSpObjectTokens* pEnum = nullptr;
                    if (SUCCEEDED(pCategory->EnumTokens(NULL, NULL, &pEnum)))
                    {
                        ISpObjectToken* pToken = nullptr;
                        ULONG fetched = 0;
                        while (SUCCEEDED(pEnum->Next(1, &pToken, &fetched)) && fetched)
                        {
                            LPWSTR desc = nullptr;
                            pToken->GetStringValue(NULL, &desc);
                            if (desc)
                            {
                                if (FString(desc).Contains(VoiceName))
                                {
                                    pVoice->SetVoice(pToken);
                                    ::CoTaskMemFree(desc);
                                    break;
                                }
                                ::CoTaskMemFree(desc);
                            }
                            pToken->Release();
                        }
                        pEnum->Release();
                    }
                }
                pCategory->Release();
            }

            IStream* pMemoryStream = nullptr;
            hr = ::CreateStreamOnHGlobal(NULL, true, &pMemoryStream);
            if (SUCCEEDED(hr))
            {
                WAVEFORMATEX wfx = {};
                wfx.wFormatTag = WAVE_FORMAT_PCM;
                wfx.nChannels = 1;
                wfx.nSamplesPerSec = 44100;
                wfx.wBitsPerSample = 16;
                wfx.nBlockAlign = (wfx.wBitsPerSample / 8) * wfx.nChannels;
                wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;

                hr = CoCreateInstance(CLSID_SpStream, NULL, CLSCTX_ALL, IID_ISpStream, (void**)&pStream);
                if (SUCCEEDED(hr))
                {
                    pStream->SetBaseStream(pMemoryStream, SPDFID_WaveFormatEx, &wfx);

                    pVoice->SetOutput(pStream, true);
                    pVoice->Speak(*Text, SPF_DEFAULT, NULL);
                    pVoice->SetOutput(NULL, false);
                    pStream->Close();

                    HGLOBAL hGlobal = NULL;
                    GetHGlobalFromStream(pMemoryStream, &hGlobal);
                    if (hGlobal)
                    {
                        SIZE_T dwSize = GlobalSize(hGlobal);
                        void* pData = GlobalLock(hGlobal);
                        if (pData && dwSize > 0)
                        {
                            TArray<uint8> WavData;
                            SerializeWaveFile(WavData, (uint8*)pData, (int32)dwSize, wfx.nChannels, wfx.nSamplesPerSec);

                            Async(EAsyncExecution::TaskGraphMainThread, [WavData, OnBytesGenerated]()
                            {
                                OnBytesGenerated.ExecuteIfBound(WavData);
                            });
                        }
                        GlobalUnlock(hGlobal);
                    }

                    pStream->Release();
                }

                pMemoryStream->Release();
            }

            pVoice->Release();
        }

        ::CoUninitialize();

        // Ensure delegate is called even on failure
        Async(EAsyncExecution::TaskGraphMainThread, [OnBytesGenerated]()
        {
            OnBytesGenerated.ExecuteIfBound(TArray<uint8>());
        });
    });
}