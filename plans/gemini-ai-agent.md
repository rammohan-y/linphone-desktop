# AI Agent — Call Automation & Transcription

## Goal
Integrate AI caller agents (starting with Google Gemini) that can make phone calls on behalf of the user. The user defines **AI Agents** (vendor configurations) and **AI Scenarios** (call scripts). When making an AI Call, the user picks a scenario, and the system uses the linked agent config + prompt to conduct the call and produce a transcript.

## Status
- [x] Research — audio capture, playback, available libraries
- [x] Research — streaming vs file I/O, SDK audio APIs
- [x] Phase 1a — Single-agent settings (API key, model, etc.) — **to be replaced by Phase 1b**
- [x] Phase 1b — Multi-agent + multi-scenario settings
- [x] Phase 2 — Core AI agent backend (audio capture → Gemini → audio inject)
- [x] Phase 3 — AI Call button & call window UI (arm-then-dial flow, live transcript panel)
- [ ] Phase 4 — Transcript export & history

---

## Data Model

### AI Agent (vendor configuration)
Stored in linphonerc as indexed INI sections: `[ai_agent_0]`, `[ai_agent_1]`, etc.

| Field | Config Key | Default | Description |
|-------|-----------|---------|-------------|
| Display Name | `name` | (required) | User-visible label, e.g., "Gemini Flash" |
| Provider | `provider` | `gemini` | For future multi-vendor support (gemini, openai, etc.) |
| API Key | `api_key` | (empty) | Vendor API key |
| Model | `model` | `gemini-2.0-flash-live-001` | Model identifier |
| Voice | `voice` | `Puck` | TTS voice name |
| Language | `language` | `en-US` | Language code |

Example in linphonerc:
```ini
[ai_agent_0]
name=Gemini Flash
provider=gemini
api_key=AIzaSy...
model=gemini-2.0-flash-live-001
voice=Puck
language=en-US

[ai_agent_1]
name=Gemini Pro
provider=gemini
api_key=AIzaSy...
model=gemini-2.5-pro-preview-03-25
voice=Aoede
language=en-US
```

### AI Scenario (call script)
Stored in linphonerc as indexed INI sections: `[ai_scenario_0]`, `[ai_scenario_1]`, etc.

| Field | Config Key | Default | Description |
|-------|-----------|---------|-------------|
| Name | `name` | (required) | User-visible label, e.g., "Book Dentist" |
| Agent Index | `agent_index` | `0` | Index of the AI Agent to use |
| System Prompt | `system_prompt` | (empty) | The instruction given to the AI |

Example in linphonerc:
```ini
[ai_scenario_0]
name=Book Dentist
agent_index=0
system_prompt=You are calling Dr. Sharma's clinic to book a general checkup...

[ai_scenario_1]
name=ISP Complaint
agent_index=0
system_prompt=You are calling a broadband provider's support line...
```

### Runtime Flow
```
User picks scenario "Book Dentist"
    → scenario.agent_index = 0
    → loads ai_agent_0 (Gemini Flash config)
    → loads scenario system_prompt
    → initiates AI Call with agent config + prompt
```

---

## Architecture

```
Callee speaks
    │
    ▼
linphone call audio (PCM s16le 16kHz, decoded by mediastreamer2)
    │
    ├──► Speaker (user hears callee — normal audio path)
    │
    ▼
call->startRecording() → growing WAV file (/tmp/ai_capture_XXXX.wav)
    │
    ▼
GeminiAgentModel (worker thread)
    │  reads new PCM chunks from WAV file via QTimer (every 200ms)
    │  strips WAV header, sends raw PCM as base64 over WebSocket
    ▼
Gemini Multimodal Live API (bidirectional WebSocket)
    │  Gemini's VAD detects end-of-speech
    │  returns: audio response chunks + transcript text
    ▼
GeminiAgentModel
    │
    ├──► Audio response: write PCM to temp WAV file
    │        │
    │        ▼
    │    call->getPlayer()->open(tempWav) + start()
    │    (callee hears Gemini's response)
    │
    └──► Transcript text
             │
             ▼
         GeminiAgentCore (UI thread) → QML live transcript panel
```

## Key Findings from Research

### Audio Capture — Call Recording (chosen approach)
- `CallParams::setRecordFile(path)` — set before call or update during call
- `Call::startRecording()` / `Call::stopRecording()` — records callee's decoded audio
- Output: **16-bit linear WAV** (PCM s16le), mono
- We read the growing file from a separate thread, tracking read position

### Audio Capture — Why NOT "External Audio Device"
- `LinphoneAudioDeviceType` enum in SDK 5.5.0 has NO External type (stops at Hdmi=12)
- `Core::setUseFiles(true)` + `Core::setRecordFile()` is global — breaks user's mic/speaker
- Per-call recording via `CallParams::setRecordFile()` is the correct scoped approach

### Audio Injection — call->getPlayer()
- `Call::getPlayer()` returns the in-call `Player` object
- `Player::open(filePath)` + `Player::start()` — file-based only (seeks internally, no FIFOs)
- Write Gemini's response to a temp WAV, then play it

### Storing Lists in Config — Shortcuts Pattern
- Use indexed INI sections: `[prefix_0]`, `[prefix_1]`, etc.
- Read via `mConfig->getSectionsNamesList()` + prefix filtering
- Write via `mConfig->cleanSection()` + rewrite all
- Proven pattern used by shortcuts feature in SettingsModel.cpp

### WebSocket
- Qt6::WebSockets module provides `QWebSocket` — add to CMakeLists.txt

---

## Implementation Plan

### Phase 1b — Multi-Agent & Multi-Scenario Settings

**Step 1: Replace flat settings with list-based storage in SettingsModel**

Modify `Linphone/model/setting/SettingsModel.hpp/cpp`:
- Remove the 5 flat `gemini_*` GETSET macros
- Add methods:
  - `QVariantList getAiAgents() const` — reads all `[ai_agent_N]` sections
  - `void setAiAgents(const QVariantList &agents)` — cleans and rewrites all sections
  - `QVariantList getAiScenarios() const` — reads all `[ai_scenario_N]` sections
  - `void setAiScenarios(const QVariantList &scenarios)` — cleans and rewrites all sections
- Follow the exact shortcuts pattern (`getSectionsNamesList()`, prefix filter, indexed write)

**Step 2: Update SettingsCore to sync lists**

Modify `Linphone/core/setting/SettingsCore.hpp/cpp`:
- Remove the 5 flat gemini Q_PROPERTYs, getters, setters, signals, members
- Add:
  - `Q_PROPERTY(QVariantList aiAgents READ getAiAgents WRITE setAiAgents NOTIFY aiAgentsChanged)`
  - `Q_PROPERTY(QVariantList aiScenarios READ getAiScenarios WRITE setAiScenarios NOTIFY aiScenariosChanged)`
  - Corresponding members, signals, copy constructor entries, writeIntoModel/writeFromModel
- Invokable helpers for QML:
  - `Q_INVOKABLE void addAiAgent(QVariantMap agent)`
  - `Q_INVOKABLE void removeAiAgent(int index)`
  - `Q_INVOKABLE void updateAiAgent(int index, QVariantMap agent)`
  - `Q_INVOKABLE void addAiScenario(QVariantMap scenario)`
  - `Q_INVOKABLE void removeAiScenario(int index)`
  - `Q_INVOKABLE void updateAiScenario(int index, QVariantMap scenario)`
  - `Q_INVOKABLE QVariantList getAgentNames()` — for scenario dropdown

**Step 3: AI Agents Settings UI**

Replace `AIAgentSettingsLayout.qml` content with:
- List of configured agents (ListView with name, provider, model summary)
- "Add Agent" button → opens inline form or dialog
- Each agent row: edit/delete buttons
- Edit form: name, API key (hidden), model, voice, language fields
- Uses SettingsCpp.aiAgents / SettingsCpp.addAiAgent() etc.

**Step 4: AI Scenarios Settings UI**

Create `AIScenarioSettingsLayout.qml`:
- List of configured scenarios (ListView with name, agent name summary)
- "Add Scenario" button → opens inline form
- Each scenario row: edit/delete buttons
- Edit form: name, agent dropdown (populated from SettingsCpp.getAgentNames()), system prompt TextArea
- Register in view/CMakeLists.txt and SettingsPage.qml

### Phase 2 — Core AI Agent Backend
(unchanged — uses scenario's agent config + prompt at runtime)

New files:
- `Linphone/model/ai/AIAgentModel.hpp` — abstract base class
- `Linphone/model/ai/GeminiAgentModel.hpp/cpp` — Gemini WebSocket implementation
- `Linphone/core/ai/GeminiAgentCore.hpp/cpp` — UI thread core
- `Linphone/core/ai/GeminiAgentGui.hpp/cpp` — QML wrapper

### Phase 3 — AI Call Button & Call Window UI ✅

**Critical lesson: AI calls must follow the normal call navigation path.** An earlier approach that navigated to a separate AI call UI caused progressive call-window/control lag — each call loaded heavy QML components that never fully unwound, causing RSS growth (~150MB/call) and UI degradation.

**Final design — arm-then-dial:**
1. User selects a scenario from the "AI Call" popup on CallPage → `AICallControllerCpp.armAICall(index)`
2. Gemini WebSocket connects during the arming phase (pre-dial), shown via armed banner
3. User dials normally (history list, contact click, or dialer) — **same navigation path as any call**
4. On `StreamsRunning`, mixed recording starts and audio polling begins
5. AI transcript panel opens as a lightweight right panel in CallsWindow (deferred via `Qt.callLater`)

**Key implementation details:**
- **CaptureFilePoller**: Dedicated thread with 50ms PreciseTimer for file polling (off main thread)
- **Bidi-ready buffering**: `GeminiLiveClient` accumulates PCM in `mPendingInputPcm` before `setupComplete`, then flushes — prevents losing early callee audio
- **BlockingQueuedConnection for mixed_record_start**: Ensures capture is running before QML `activeChanged` triggers panel load
- **Deferred activeChanged**: `QMetaObject::invokeMethod(..., Qt::QueuedConnection)` so `rightPanel.replace()` never runs in the same stack frame as capture setup
- **Async Gemini teardown**: `cleanupGemini()` uses a joiner thread; `cleanupGeminiBlocking()` for destructor/disarm
- **CallCore cache in CallList**: `QHash<quintptr, QSharedPointer<CallCore>>` deduplicates wrappers by native `LinphoneCall*` — avoids recreating expensive CallCore/CallModel on each `lUpdate()` cycle
- **Proper mixed recording stop**: `audio_stream_mixed_record_stop()` (not `callModel->stopRecording()` which is a different mechanism)
- **Player close in cleanup**: `player->close()` on SDK thread before next call negotiates media
- **Env kill switches**: `LINPHONE_AI_NOOP`, `LINPHONE_AI_DISABLE_CAPTURE`, `LINPHONE_AI_DISABLE_POLLING`, `LINPHONE_AI_DISABLE_GEMINI`, `LINPHONE_AI_DISABLE_LOCAL_PLAYBACK`, `LINPHONE_AI_DISABLE_REMOTE_PLAYBACK`, `LINPHONE_AI_DISABLE_MIC_MUTE` for isolating regressions

**Files modified:**
- `Linphone/core/ai/AICallController.hpp/cpp` — Full rewrite with arm/disarm flow, async teardown, blocking capture start
- `Linphone/core/ai/CaptureFilePoller.hpp/cpp` — New: dedicated polling thread with pause/resume
- `Linphone/model/ai/GeminiLiveClient.hpp/cpp` — Bidi-ready buffering, debug instrumentation
- `Linphone/core/call/CallList.hpp/cpp` — CallCore cache by native pointer
- `Linphone/core/call/CallCore.hpp/cpp` — `getNativePtr()`, perf tracing
- `Linphone/view/Page/Main/Call/CallPage.qml` — AI Call popup button, armed banner
- `Linphone/view/Page/Window/Call/CallsWindow.qml` — AI Agent right panel, More Options entry, debug timing
- `Linphone/core/App.cpp` — AICallController singleton registration, UI init timing

### Phase 4 — Transcript Export & History
- Export as .txt / .json from panel
- Optionally store alongside recordings

---

## Gemini Multimodal Live API

- **Endpoint**: `wss://generativelanguage.googleapis.com/ws/google.ai.generativelanguage.v1beta.GenerativeService.BidiGenerateContent`
- **Auth**: API key as query param `?key=<API_KEY>`
- **Setup message** (first frame):
  ```json
  {
    "setup": {
      "model": "models/gemini-2.0-flash-live-001",
      "generationConfig": {
        "responseModalities": ["AUDIO", "TEXT"],
        "speechConfig": {
          "voiceConfig": { "prebuiltVoiceConfig": { "voiceName": "Puck" } }
        }
      },
      "systemInstruction": {
        "parts": [{ "text": "You are a caller..." }]
      }
    }
  }
  ```
- **Audio in**: `realtimeInput.mediaChunks[].data` (base64 PCM s16le 16kHz)
- **Audio out**: `serverContent.modelTurn.parts[].inlineData.data` (base64 PCM s16le 24kHz)
- **Text out**: `serverContent.modelTurn.parts[].text`
- **Turn detection**: Built-in VAD
- **Interruption**: Callee speech interrupts Gemini's response

---

## Design Decisions (resolved)
- **Multi-agent**: Users can configure multiple vendor profiles (agent configs)
- **Multi-scenario**: Users define reusable call scripts, each linked to an agent
- **Arm-then-dial**: User arms a scenario pre-call, then dials normally — AI call uses the same navigation/UI path as a regular call to avoid progressive QML degradation
- **Mic muted during AI playback**: Prevents barge-in detection on remote party hearing AI audio through speakers
- **Capture position advance after playback**: Skips AI's own played-back audio in the capture file to prevent Gemini hearing itself
- **Timestamps**: Transcript includes timestamps relative to call start
- **Prominent AI indicator**: Armed banner on CallPage, status in right panel during call
- **Multi-provider future**: Agent configs include `provider` field for OpenAI etc.
- **Storage**: Indexed INI sections in linphonerc (shortcuts pattern)
- **Local + remote audio**: `aplay` streams to speakers for local monitoring; `call->getPlayer()` sends to remote via RTP

## Future Enhancements

### Prompt Assistant — AI-powered system prompt generation
"Generate with AI" button next to system prompt TextArea. User types brief description, Gemini text API generates a full system prompt.

---

## Log
- 2026-04-23: Initial plan created after codebase research
- 2026-04-23: Phase 1a implemented — flat single-agent settings in SettingsModel/SettingsCore/QML
- 2026-04-23: Fixed AI Agent settings — moved to separate tab (AIAgentSettingsLayout.qml)
- 2026-04-24: Design change — multi-agent + multi-scenario architecture. Replacing flat settings with list-based storage using indexed INI sections (shortcuts pattern). Plan updated for Phase 1b.
- 2026-04-24: Phase 2 complete — AICallController + GeminiLiveClient with mixed recording, resampling, aplay streaming, remote playback via call player
- 2026-04-25: Phase 3 complete — arm-then-dial flow, CaptureFilePoller on dedicated thread, CallCore cache to prevent wrapper duplication, async Gemini teardown, proper mixed_record_stop, env kill switches for regression isolation. Key fix: keeping AI call on normal navigation path eliminated progressive UI lag and RSS growth.
