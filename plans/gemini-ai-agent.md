# AI Vendor — Call Automation & Transcription

## Goal
Integrate AI caller vendors (starting with Google Gemini) that can make phone calls on behalf of the user. The user defines **AI Vendors** (provider configurations) and **AI Scenarios** (call scripts). When making an AI Call, the user picks a scenario, and the system uses the linked vendor config + prompt to conduct the call and produce a transcript.

## Status
- [x] Research — audio capture, playback, available libraries
- [x] Research — streaming vs file I/O, SDK audio APIs
- [x] Phase 1a — Single-vendor settings (API key, model, etc.) — **replaced by Phase 1b**
- [x] Phase 1b — Multi-vendor + multi-scenario settings
- [x] Phase 2 — Core AI backend (audio capture → Gemini → audio inject)
- [x] Phase 3 — AI Call button & call window UI (arm-then-dial flow, live transcript panel)
- [x] Audio fixes — 48kHz→16kHz downsampling, sample rate detection, Gemini language support
- [x] Branding — CallForge rebrand, purple theme, lavender tint, space background
- [x] UI polish — Rename "AI Agent" → "AI Vendor", show scenario name in call, feature-gated visibility
- [ ] Phase 4 — Transcript export & history

---

## Data Model

### AI Vendor (provider configuration)
Stored in linphonerc as indexed INI sections: `[ai_agent_0]`, `[ai_agent_1]`, etc.

| Field | Config Key | Default | Description |
|-------|-----------|---------|-------------|
| Display Name | `name` | (required) | User-visible label, e.g., "Gemini Flash" |
| Provider | `provider` | `gemini` | For future multi-vendor support (gemini, openai, etc.) |
| API Key | `api_key` | (empty) | Vendor API key |
| Model | `model` | `gemini-2.0-flash-live-001` | Model identifier |
| Voice | `voice` | `Puck` | TTS voice name |
| Language | `language` | `en-US` | Language code sent as `speechConfig.languageCode` to Gemini |

### AI Scenario (call script)
Stored in linphonerc as indexed INI sections: `[ai_scenario_0]`, `[ai_scenario_1]`, etc.

| Field | Config Key | Default | Description |
|-------|-----------|---------|-------------|
| Name | `name` | (required) | User-visible label, e.g., "Book Dentist" |
| Agent Index | `agent_index` | `0` | Index of the AI Vendor to use |
| System Prompt | `system_prompt` | (empty) | The instruction given to the AI |

### Runtime Flow
```
User picks scenario "Book Dentist"
    → scenario.agent_index = 0
    → loads ai_agent_0 (Gemini Flash config)
    → loads scenario system_prompt + vendor language
    → initiates AI Call with vendor config + prompt
```

---

## Architecture

```
Callee speaks
    │
    ▼
linphone call audio (PCM s16le, 16kHz or 48kHz depending on codec)
    │
    ├──► Speaker (user hears callee — normal audio path)
    │
    ▼
mixed_record_start() → growing WAV file (/tmp/ai_capture_XXXX.wav)
    │
    ▼
CaptureFilePoller (dedicated thread, 50ms PreciseTimer)
    │  reads new PCM chunks from WAV file
    │  conditionally downsamples 48kHz→16kHz (detected from audio stream)
    │  sends raw PCM as base64 over WebSocket
    ▼
Gemini Multimodal Live API (bidirectional WebSocket)
    │  Setup includes languageCode from vendor config
    │  Gemini's VAD detects end-of-speech
    │  returns: audio response chunks + transcript text
    ▼
AICallController
    │
    ├──► Audio response: resample 24kHz→16kHz, write PCM to temp WAV file
    │        │
    │        ▼
    │    call->getPlayer()->open(tempWav) + start()
    │    (callee hears Gemini's response)
    │
    └──► Transcript text
             │
             ▼
         AICallController → QML live transcript panel
         Panel title shows "AI Scenario - <name>"
         Status shows "<scenario name> — listening..."
```

## Key Findings from Research

### Audio Capture — Mixed Recording (chosen approach)
- `audio_stream_mixed_record_start()` — records both local and remote audio
- Output: **16-bit linear WAV** (PCM s16le), mono, rate depends on codec (16kHz or 48kHz)
- CaptureFilePoller reads growing file from dedicated thread

### Audio Sample Rate Handling
- **Problem**: Codec negotiation determines stream sample rate (16kHz with video disabled, 48kHz otherwise)
- **Solution**: Detect actual `stream->sample_rate` via `BlockingQueuedConnection`, conditionally downsample 48kHz→16kHz before sending to Gemini
- **Gemini expects**: `audio/pcm;rate=16000` — sending 48kHz as 16kHz caused 3x speed distortion and sporadic VAD failures
- **Playback resampling**: SDK's `linphone_player_open/start` handles resampling automatically via MSResample filter

### Audio Injection — call->getPlayer()
- `Call::getPlayer()` returns the in-call `Player` object
- `Player::open(filePath)` + `Player::start()` — file-based only (seeks internally, no FIFOs)
- Write Gemini's response to a temp WAV, then play it

---

## Implementation Details

### Phase 3 — AI Call Button & Call Window UI ✅

**Critical lesson: AI calls must follow the normal call navigation path.** An earlier approach that navigated to a separate AI call UI caused progressive call-window/control lag — each call loaded heavy QML components that never fully unwound, causing RSS growth (~150MB/call) and UI degradation.

**Final design — arm-then-dial:**
1. User selects a scenario from the "AI Call" popup on CallPage → `AICallControllerCpp.armAICall(index)`
2. Gemini WebSocket connects during the arming phase (pre-dial), shown via armed banner
3. User dials normally (history list, contact click, or dialer) — **same navigation path as any call**
4. On `StreamsRunning`, mixed recording starts and audio polling begins
5. AI transcript panel opens as a lightweight right panel in CallsWindow (deferred via `Qt.callLater`)

**Key implementation details:**
- **CaptureFilePoller**: Dedicated thread with 50ms PreciseTimer for file polling
- **Bidi-ready buffering**: `GeminiLiveClient` accumulates PCM before `setupComplete`, flushes after
- **Conditional downsampling**: Detect audio stream sample rate, downsample 48kHz→16kHz only when needed
- **Language support**: `speechConfig.languageCode` sent in Gemini setup message from vendor config
- **activeScenarioName**: Persists through active call (unlike `armedScenarioIndex` which resets on disarm)
- **Async Gemini teardown**: Joiner thread for non-blocking cleanup
- **CallCore cache in CallList**: Deduplicates wrappers by native `LinphoneCall*`
- **Env kill switches**: `LINPHONE_AI_NOOP`, `DISABLE_CAPTURE`, `DISABLE_POLLING`, `DISABLE_GEMINI`, etc.

### Audio Fixes ✅
- **48kHz→16kHz downsampling**: Averages every 3 consecutive int16 samples
- **Sample rate detection**: `stream->sample_rate` captured via pointer in SDK thread lambda
- **Conditional logic**: `mCaptureSampleRate > 16000 ? downsample48kTo16k(pcmData) : pcmData`
- **Validated**: 5/5 (100%) Gemini response rate vs 3/14 (21%) before fix

### Branding & UI Polish ✅
- **CallForge rebrand**: Logo, splash, login image, translations all updated
- **Purple theme**: `#6C3FBF` primary, secondary palette shifted to purple tints
- **Lavender background**: Grey scale tinted lavender (#EEEAF6 base) for reduced eye strain
- **Space background**: Dark space with milky way band replaces mountain waves on login
- **Login flow**: Skip wizard, go directly to third-party SIP login
- **Feature gating**: Video codecs, Hide FPS, Video Call button respect `videoEnabled` setting
- **Tray menu**: Removed "Mark All Read" and "Check for Update" (Linphone-specific)
- **Naming**: "AI Agent" → "AI Vendor" everywhere, scenario name shown in call panel

### Files Modified
- `Linphone/core/ai/AICallController.hpp/cpp` — Full controller with arm/disarm, downsampling, activeScenarioName
- `Linphone/core/ai/CaptureFilePoller.hpp/cpp` — Dedicated polling thread
- `Linphone/model/ai/GeminiLiveClient.hpp/cpp` — Bidi buffering, languageCode support
- `Linphone/core/call/CallList.hpp/cpp` — CallCore cache
- `Linphone/core/call/CallCore.hpp/cpp` — getNativePtr()
- `Linphone/core/setting/SettingsCore.hpp/cpp` — AI vendor/scenario properties
- `Linphone/model/setting/SettingsModel.hpp/cpp` — Indexed INI storage
- `Linphone/core/App.cpp` — Singleton registration, tray menu cleanup
- `Linphone/view/Page/Main/Call/CallPage.qml` — AI Call popup, armed banner
- `Linphone/view/Page/Window/Call/CallsWindow.qml` — AI Scenario panel, scenario name title
- `Linphone/view/Page/Layout/Settings/AIAgentSettingsLayout.qml` — Vendor settings UI
- `Linphone/view/Page/Layout/Settings/AIScenarioSettingsLayout.qml` — Scenario settings UI
- `Linphone/view/Page/Layout/Settings/AdvancedSettingsLayout.qml` — Feature-gated video sections
- `Linphone/view/Style/DefaultStyle.qml` — Lavender tint, purple secondary palette
- `Linphone/view/Style/Themes.qml` — Purple theme colors
- `Linphone/data/image/*.svg` — New CallForge branding assets
- `Linphone/data/languages/*.ts` — "Linphone" → "CallForge" in translations
- `Linphone/data/config/linphonerc-factory` — Video disabled by default

### Phase 4 — Transcript Export & History (TODO)
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
        "responseModalities": ["AUDIO"],
        "speechConfig": {
          "voiceConfig": { "prebuiltVoiceConfig": { "voiceName": "Puck" } },
          "languageCode": "en-US"
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
- **Multi-vendor**: Users can configure multiple provider profiles (vendor configs)
- **Multi-scenario**: Users define reusable call scripts, each linked to a vendor
- **Arm-then-dial**: User arms a scenario pre-call, then dials normally — AI call uses the same navigation/UI path as a regular call to avoid progressive QML degradation
- **Mic muted during AI playback**: Prevents barge-in detection on remote party hearing AI audio through speakers
- **Capture position advance after playback**: Skips AI's own played-back audio in the capture file
- **Conditional downsampling**: Detect stream sample rate before downsampling to avoid corrupting already-16kHz audio
- **Language passthrough**: Vendor language config sent as Gemini `speechConfig.languageCode`
- **Lavender theme**: Softer than pure white for desktop eye comfort, preserves text readability
- **Feature gating**: Video-related settings/UI hidden when video disabled
- **Storage**: Indexed INI sections in linphonerc (shortcuts pattern)

## Future Enhancements

### Prompt Assistant — AI-powered system prompt generation
"Generate with AI" button next to system prompt TextArea. User types brief description, Gemini text API generates a full system prompt.

### Multi-provider support
Agent configs include `provider` field — extend beyond Gemini to OpenAI, etc.

---

## Log
- 2026-04-23: Initial plan created after codebase research
- 2026-04-23: Phase 1a implemented — flat single-agent settings
- 2026-04-23: Fixed AI Agent settings — moved to separate tab
- 2026-04-24: Design change — multi-agent + multi-scenario architecture (Phase 1b)
- 2026-04-24: Phase 2 complete — AICallController + GeminiLiveClient
- 2026-04-25: Phase 3 complete — arm-then-dial flow, CaptureFilePoller, CallCore cache, async teardown
- 2026-04-26: CallForge rebrand — purple theme, new SVGs, translations, streamlined login
- 2026-04-26: Fixed Gemini sporadic failures — 48kHz→16kHz downsampling
- 2026-04-26: Sample rate detection — conditional downsampling based on actual stream rate
- 2026-04-26: Renamed "AI Agent" → "AI Vendor" across entire app
- 2026-04-26: Added activeScenarioName property for in-call panel title
- 2026-04-26: Wired languageCode into Gemini speechConfig setup
- 2026-04-26: Feature-gated Video codecs and Hide FPS in Advanced Settings
- 2026-04-26: Removed irrelevant tray menu items (Mark All Read, Check for Update)
- 2026-04-26: Lavender tinted grey scale for reduced eye strain
- 2026-04-26: Space/milky way background replaces mountain waves on login screen
