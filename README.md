# 🎤 WinSAPIOfflineSpeech (Unreal Engine Plugin)

Offline speech recognition plugin for Unreal Engine using **Windows SAPI (Speech API)**.

This plugin enables:

* 🎙️ Real-time microphone input
* 🔊 Offline speech recognition (no internet required)
* 🔥 Wake-word detection
* 🌍 Multi-language support (based on installed Windows speech packs)

---

# 🚀 Features

* ✅ Fully offline speech recognition (SAPI)
* ✅ Wake-word detection system
* ✅ Asynchronous background listening (no game thread blocking)
* ✅ Multi-language support (English, etc.)
* ✅ Lightweight (no external dependencies)
* ✅ Works with Unreal Engine C++

---

# 📦 Requirements

* Windows OS
* Unreal Engine 5.x
* Windows Speech Recognition enabled

### 🔹 Install Speech Language Packs

Go to:

```
Settings → Time & Language → Speech
```

Install required languages (e.g. English US).

---

# 🛠️ Installation

1. Copy plugin to:

```
YourProject/Plugins/WinSAPIOfflineSpeech
```

2. Enable plugin in Unreal:

```
Edit → Plugins → WinSAPIOfflineSpeech
```

3. Restart Unreal Engine

---

# 🎯 Usage

## 🔹 Start Listening

```cpp
StartWakeWordListening("hello", "en-US");
```

## 🔹 Stop Listening

```cpp
bStopListening = true;
```

---

## 🔹 Wake Word Event

Bind to event:

```cpp
OnWakeWordDetected.AddDynamic(this, &AMyActor::OnWakeDetected);
```

Example:

```cpp
void AMyActor::OnWakeDetected(const FString& Text)
{
    UE_LOG(LogTemp, Warning, TEXT("Wake word detected: %s"), *Text);
}
```

---

# 🌍 Language Support

Languages depend on **installed Windows recognizers**.

### 🔹 Get Installed Languages

```cpp
TArray<FString> Languages = GetInstalledLanguages();
```

Example output:

```
en-US
en-UK
```

---

### 🔹 Change Language

Language is set when starting recognition:

```cpp
StartWakeWordListening("hello", "en-US");
```

### ❗ Important

Language cannot be changed at runtime.

To switch language:

```cpp
StopListening();
Cleanup();
StartWakeWordListening("hello", "en-UK");
```

---

# 🧠 How It Works

1. Initializes COM (Windows)
2. Creates in-process SAPI recognizer
3. Attaches default microphone
4. Loads dictation grammar
5. Listens for speech events
6. Matches wake word
7. Triggers Unreal event

---

# ⚠️ Limitations

* ❌ Limited language support (depends on SAPI)
* ❌ Not as accurate as modern AI models (Whisper, etc.)
* ❌ Cannot hot-switch language
* ❌ Windows only

---

# 🧪 Debugging

### 🔹 Common Issues

**No recognition happening**

* Check microphone permissions
* Ensure speech recognition is enabled

**Language not found**

* Install language pack in Windows settings

**Crash on cleanup**

* Ensure proper COM release order
* Do not release objects from different threads

---

# 🔧 Developer Notes

* Uses `ISpRecognizer` (in-process)
* Uses dictation grammar (`LoadDictation`)
* Event-driven via `SPEI_RECOGNITION`
* Runs on background thread using `Async`

---

# 🚀 Future Improvements

* Keyword grammar (better wake word accuracy)
* Multi-language parallel recognition
* Blueprint support
* Noise filtering / confidence threshold
* Integration with modern offline engines (Vosk / Whisper)

---

# 📄 License

This project is released under the Creative Commons Zero v1.0 Universal (CC0 1.0).

---

# 🙌 Credits

* Microsoft SAPI

---
