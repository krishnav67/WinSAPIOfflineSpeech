// Fill out your copyright notice in the Description page of Project Settings.


#include "SpeechToText.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#include <sapi.h>
#include "Windows/HideWindowsPlatformTypes.h"

// Sets default values for this component's properties
USpeechToText::USpeechToText()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void USpeechToText::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}


// Called every frame
void USpeechToText::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

void USpeechToText::RecognizeAudioBytesAsync(
    const TArray<uint8>& AudioData,
    const FOnSpeechRecognized& OnSpeechRecognized,
    const FOnSpeechRecognitionFailed& OnFailed)
{
    Async(EAsyncExecution::Thread, [AudioData, OnSpeechRecognized, OnFailed]()
    {
        auto Fail = [&](const FString& Reason)
        {
            Async(EAsyncExecution::TaskGraphMainThread, [OnFailed, Reason]()
            {
                OnFailed.ExecuteIfBound(Reason);
            });
        };

        if (FAILED(::CoInitialize(NULL)))
        {
            Fail(TEXT("COM initialization failed"));
            return;
        }

        HRESULT hr;

        // 🔥 MOVE ALL DECLARATIONS HERE
        ISpRecognizer* pRecognizer = nullptr;
        ISpRecoContext* pContext = nullptr;
        ISpRecoGrammar* pGrammar = nullptr;
        ISpStream* pSpStream = nullptr;
        IStream* pMemoryStream = nullptr;
        WAVEFORMATEX wfx;

        HGLOBAL hGlobal = NULL;
        void* pData = NULL;
        HANDLE hEvent = NULL;

        // Create recognizer
        hr = CoCreateInstance(CLSID_SpInprocRecognizer, NULL, CLSCTX_ALL,
                              IID_ISpRecognizer, (void**)&pRecognizer);

        if (FAILED(hr))
        {
            Fail(TEXT("Failed to create SAPI recognizer"));
            ::CoUninitialize();
            return;
        }

        hr = pRecognizer->CreateRecoContext(&pContext);
        if (FAILED(hr))
        {
            Fail(TEXT("Failed to create recognition context"));
            goto cleanup;
        }

        hr = pContext->CreateGrammar(1, &pGrammar);
        if (FAILED(hr))
        {
            Fail(TEXT("Failed to create grammar"));
            goto cleanup;
        }

        hr = pGrammar->LoadDictation(NULL, SPLO_STATIC);
        if (FAILED(hr))
        {
            Fail(TEXT("Failed to load dictation grammar"));
            goto cleanup;
        }

        hr = pGrammar->SetDictationState(SPRS_ACTIVE);
        if (FAILED(hr))
        {
            Fail(TEXT("Failed to activate grammar"));
            goto cleanup;
        }

        // Create memory stream
        hGlobal = GlobalAlloc(GMEM_MOVEABLE, AudioData.Num());
        if (!hGlobal)
        {
            Fail(TEXT("Failed to allocate memory for audio"));
            goto cleanup;
        }

        pData = GlobalLock(hGlobal);
        if (!pData)
        {
            Fail(TEXT("Failed to lock global memory"));
            goto cleanup;
        }

        FMemory::Memcpy(pData, AudioData.GetData(), AudioData.Num());
        GlobalUnlock(hGlobal);

        if (FAILED(CreateStreamOnHGlobal(hGlobal, Windows::TRUE, &pMemoryStream)))
        {
            Fail(TEXT("Failed to create memory stream"));
            goto cleanup;
        }

        // Audio format
        FMemory::Memzero(&wfx, sizeof(WAVEFORMATEX));
        
        wfx.wFormatTag = WAVE_FORMAT_PCM;
        wfx.nChannels = 1;
        wfx.nSamplesPerSec = 16000;
        wfx.wBitsPerSample = 16;
        wfx.nBlockAlign = (wfx.wBitsPerSample / 8) * wfx.nChannels;
        wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;

        hr = CoCreateInstance(CLSID_SpStream, NULL, CLSCTX_ALL,
                              IID_ISpStream, (void**)&pSpStream);

        if (FAILED(hr))
        {
            Fail(TEXT("Failed to create SAPI stream"));
            goto cleanup;
        }

        hr = pSpStream->SetBaseStream(pMemoryStream, SPDFID_WaveFormatEx, &wfx);
        if (FAILED(hr))
        {
            Fail(TEXT("Failed to set stream format"));
            goto cleanup;
        }

        hr = pRecognizer->SetInput(pSpStream, Windows::TRUE);
        if (FAILED(hr))
        {
            Fail(TEXT("Failed to set recognizer input"));
            goto cleanup;
        }

        pContext->SetInterest(SPFEI(SPEI_RECOGNITION), SPFEI(SPEI_RECOGNITION));

        hEvent = pContext->GetNotifyEventHandle();
        if (!hEvent)
        {
            Fail(TEXT("Invalid event handle"));
            goto cleanup;
        }

        if (WaitForSingleObject(hEvent, 10000) != WAIT_OBJECT_0)
        {
            Fail(TEXT("Recognition timeout"));
            goto cleanup;
        }

        {
            SPEVENT event;
            ULONG fetched = 0;

            while (SUCCEEDED(pContext->GetEvents(1, &event, &fetched)) && fetched > 0)
            {
                if (event.eEventId == SPEI_RECOGNITION)
                {
                    ISpRecoResult* pResult = (ISpRecoResult*)event.lParam;

                    if (pResult)
                    {
                        LPWSTR pText = nullptr;

                        if (SUCCEEDED(pResult->GetText(
                                SP_GETWHOLEPHRASE,
                                SP_GETWHOLEPHRASE,
                                Windows::FALSE,
                                &pText,
                                NULL)))
                        {
                            FString RecognizedText = FString(pText);
                            ::CoTaskMemFree(pText);

                            Async(EAsyncExecution::TaskGraphMainThread,
                                [RecognizedText, OnSpeechRecognized]()
                            {
                                OnSpeechRecognized.ExecuteIfBound(RecognizedText);
                            });

                            break;
                        }
                    }
                }
            }
        }

    cleanup:

        if (pSpStream) pSpStream->Release();
        if (pMemoryStream) pMemoryStream->Release();
        if (pGrammar) pGrammar->Release();
        if (pContext) pContext->Release();
        if (pRecognizer) pRecognizer->Release();

        ::CoUninitialize();
    });
}

bool USpeechToText::LoadWavFileToPCM(const FString& FilePath, TArray<uint8>& OutPCMData, WAVEFORMATEX& OutFormat, FString& OutError)
{
    TArray<uint8> FileData;

    if (!FFileHelper::LoadFileToArray(FileData, *FilePath))
    {
        OutError = TEXT("Failed to read WAV file");
        return false;
    }

    if (FileData.Num() < 44)
    {
        OutError = TEXT("Invalid WAV file (too small)");
        return false;
    }

    const uint8* DataPtr = FileData.GetData();

    // Check RIFF header
    if (FMemory::Memcmp(DataPtr, "RIFF", 4) != 0 ||
        FMemory::Memcmp(DataPtr + 8, "WAVE", 4) != 0)
    {
        OutError = TEXT("Invalid WAV format (missing RIFF/WAVE)");
        return false;
    }

    int32 Offset = 12;

    bool bFoundFmt = false;
    bool bFoundData = false;

    uint16 AudioFormat = 0;
    uint16 NumChannels = 0;
    uint32 SampleRate = 0;
    uint16 BitsPerSample = 0;

    int32 DataChunkOffset = 0;
    int32 DataChunkSize = 0;

    // 🔍 Parse chunks
    while (Offset + 8 <= FileData.Num())
    {
        const char* ChunkId = (const char*)(DataPtr + Offset);
        uint32 ChunkSize = *(uint32*)(DataPtr + Offset + 4);

        if (FMemory::Memcmp(ChunkId, "fmt ", 4) == 0)
        {
            bFoundFmt = true;

            AudioFormat = *(uint16*)(DataPtr + Offset + 8);
            NumChannels = *(uint16*)(DataPtr + Offset + 10);
            SampleRate = *(uint32*)(DataPtr + Offset + 12);
            BitsPerSample = *(uint16*)(DataPtr + Offset + 22);
        }
        else if (FMemory::Memcmp(ChunkId, "data", 4) == 0)
        {
            bFoundData = true;
            DataChunkOffset = Offset + 8;
            DataChunkSize = ChunkSize;
        }

        Offset += 8 + ChunkSize;

        if (bFoundFmt && bFoundData)
            break;
    }

    if (!bFoundFmt || !bFoundData)
    {
        OutError = TEXT("Missing fmt or data chunk");
        return false;
    }

    if (AudioFormat != 1) // PCM only
    {
        OutError = TEXT("Only PCM WAV supported");
        return false;
    }

    // Fill format
    FMemory::Memzero(&OutFormat, sizeof(WAVEFORMATEX));
    OutFormat.wFormatTag = WAVE_FORMAT_PCM;
    OutFormat.nChannels = NumChannels;
    OutFormat.nSamplesPerSec = SampleRate;
    OutFormat.wBitsPerSample = BitsPerSample;
    OutFormat.nBlockAlign = (BitsPerSample / 8) * NumChannels;
    OutFormat.nAvgBytesPerSec = SampleRate * OutFormat.nBlockAlign;

    // Extract PCM
    OutPCMData.SetNumUninitialized(DataChunkSize);
    FMemory::Memcpy(OutPCMData.GetData(), DataPtr + DataChunkOffset, DataChunkSize);

    return true;
}

void USpeechToText::StopRunningRecognization()
{
    ContinueRecognize = false;
}

void USpeechToText::RecognizeWavFileAsync(const FString& FilePath, const FOnSpeechRecognized& OnSpeechRecognized, const FOnSpeechRecognitionFailed& OnFailed)
{
    TArray<uint8> PCMData;
    WAVEFORMATEX Format;
    FString Error;

    if (!LoadWavFileToPCM(FilePath, PCMData, Format, Error))
    {
        OnFailed.ExecuteIfBound(Error);
        return;
    }
    
    RecognizeAudioBytesAsync(PCMData, OnSpeechRecognized, OnFailed);
}
