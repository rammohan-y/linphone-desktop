# CallForge IPC Protocol v1

## Transport

- **Linux/macOS:** Unix domain socket at `/tmp/callforge.sock`
- **Windows:** Named pipe at `\\.\pipe\callforge`
- **Framing:** Each message is `<1-byte type><4-byte big-endian length><payload>`
  - Type `0x01` = JSON message (payload is UTF-8 JSON)
  - Type `0x02` = Binary audio chunk (payload is raw PCM s16le bytes)
- **JSON messages:** Objects with a `cmd` (host→daemon) or `event` (daemon→host) field
- **Binary messages:** Direction determined by who sends — host sends capture audio, daemon sends response audio

## Connection Lifecycle

1. Host launches daemon as child process (`QProcess`)
2. Daemon creates socket and listens
3. Host connects to socket
4. Host sends `hello` — daemon responds with `ready`
5. Normal message exchange
6. On host shutdown: host sends `shutdown` → daemon exits cleanly
7. If socket disconnects unexpectedly: daemon cleans up and exits after timeout

---

## Host → Daemon Commands

### Lifecycle

#### `hello`
Initial handshake after socket connect.
```json
{"cmd": "hello", "hostVersion": "6.2.0"}
```
Daemon responds with `ready` event.

#### `shutdown`
Host is exiting. Daemon should clean up and terminate.
```json
{"cmd": "shutdown"}
```

---

### AI Control

#### `arm`
Prepare AI for a call. Daemon connects to Gemini and waits.
```json
{"cmd": "arm", "scenarioIndex": 0}
```

#### `disarm`
Cancel armed state. Daemon disconnects from Gemini.
```json
{"cmd": "disarm"}
```

#### `start`
Start AI on an already-active call (mid-call start, no pre-arm).
```json
{"cmd": "start", "scenarioIndex": 0}
```

#### `stop`
Stop the active AI session.
```json
{"cmd": "stop"}
```

---

### Call Events (host notifies daemon)

#### `callStateChanged`
Host informs daemon of call state transitions.
```json
{"cmd": "callStateChanged", "state": 8, "stateName": "StreamsRunning"}
```

State values (from CallForgeHostContext::CallState):
| Value | Name |
|-------|------|
| 0 | Idle |
| 1 | IncomingReceived |
| 3 | OutgoingInit |
| 4 | OutgoingProgress |
| 5 | OutgoingRinging |
| 7 | Connected |
| 8 | StreamsRunning |
| 13 | Error |
| 14 | End |
| 19 | Released |

#### `captureStarted`
Host has started audio capture. Tells daemon the format so it can decode incoming binary audio chunks.
```json
{"cmd": "captureStarted", "sampleRate": 48000, "channels": 1, "sampleFormat": "s16le"}
```
After this, host sends binary audio frames (type `0x02`) continuously until `captureStopped`.

#### `captureStopped`
Host stopped audio capture (call ended or AI stopped).
```json
{"cmd": "captureStopped"}
```

#### `playbackFinished`
Host finished playing the current response audio to the remote caller.
```json
{"cmd": "playbackFinished"}
```

#### Binary: Capture Audio (host → daemon)
Sent as type `0x02` frames. Raw PCM s16le bytes at the sample rate specified in `captureStarted`.
Host reads from SDK capture file and forwards chunks over socket. ~20ms chunks (640 bytes at 16kHz, 1920 bytes at 48kHz).

---

### Agent CRUD

#### `getAgents`
```json
{"cmd": "getAgents"}
```
Daemon responds with `agents` event.

#### `addAgent`
```json
{"cmd": "addAgent", "agent": {"name": "Gemini Flash", "provider": "gemini", "apiKey": "...", "model": "gemini-2.0-flash-live-001", "voice": "Puck", "language": "en"}}
```
Daemon responds with `agents` event (updated list).

#### `updateAgent`
```json
{"cmd": "updateAgent", "index": 0, "agent": {"name": "Gemini Flash", "provider": "gemini", "apiKey": "...", "model": "gemini-2.0-flash-live-001", "voice": "Puck", "language": "en"}}
```
Daemon responds with `agents` event.

#### `removeAgent`
```json
{"cmd": "removeAgent", "index": 0}
```
Daemon responds with `agents` event.

#### `testAgent`
Test connectivity to the AI vendor.
```json
{"cmd": "testAgent", "index": 0}
```
Daemon responds with `agentTestResult` event.

---

### Scenario CRUD

#### `getScenarios`
```json
{"cmd": "getScenarios"}
```
Daemon responds with `scenarios` event.

#### `addScenario`
```json
{"cmd": "addScenario", "scenario": {"name": "Test Audio Quality", "agentIndex": 0, "systemPrompt": "You are a test caller..."}}
```
Daemon responds with `scenarios` event.

#### `updateScenario`
```json
{"cmd": "updateScenario", "index": 0, "scenario": {"name": "Test Audio Quality", "agentIndex": 0, "systemPrompt": "You are a test caller..."}}
```
Daemon responds with `scenarios` event.

#### `removeScenario`
```json
{"cmd": "removeScenario", "index": 0}
```
Daemon responds with `scenarios` event.

---

### UI

#### `getUI`
Request current UI definition. Host sends this on connect and whenever it needs a full refresh.
```json
{"cmd": "getUI"}
```
Daemon responds with `ui` event.

#### `uiAction`
User performed an action on a UI component (button click, form submit, etc.).
```json
{"cmd": "uiAction", "action": "stop"}
```

```json
{"cmd": "uiAction", "action": "arm", "params": {"scenarioIndex": 0}}
```

```json
{"cmd": "uiAction", "action": "saveAgent", "params": {"index": 0, "agent": {"name": "...", "apiKey": "..."}}}
```

---

## Daemon → Host Events

### Lifecycle

#### `ready`
Response to `hello`. Daemon is initialized and ready.
```json
{"event": "ready", "daemonVersion": "1.0.0", "protocolVersion": 1}
```

---

### State Updates

#### `stateUpdate`
Sent whenever any AI state property changes. Only changed fields are included.
```json
{"event": "stateUpdate", "active": true}
```

```json
{"event": "stateUpdate", "armed": true, "geminiReady": false, "armedScenarioIndex": 0, "activeScenarioName": "Test Audio Quality", "status": "Connecting to Gemini..."}
```

Full set of possible fields:
| Field | Type | Description |
|-------|------|-------------|
| `active` | bool | AI session is running |
| `armed` | bool | AI is armed, waiting for call |
| `geminiReady` | bool | Gemini WebSocket connected |
| `armedScenarioIndex` | int | Currently armed scenario (-1 if none) |
| `status` | string | Human-readable status message |
| `activeScenarioName` | string | Name of active/armed scenario |

#### `transcriptAppend`
New transcript line(s) to append.
```json
{"event": "transcriptAppend", "text": "[00:05] Caller: Hello, please say something."}
```

---

### Audio Commands (daemon tells host what to do)

#### `startCapture`
Daemon wants host to begin capturing caller audio and streaming it over the socket.
```json
{"event": "startCapture"}
```
Host calls `CallHandle::startMixedRecordToFile(path)` internally, starts a poller to read
chunks from the capture file, and streams them as binary `0x02` frames to daemon.
Host sends `captureStarted` once capture is active.

#### `stopCapture`
```json
{"event": "stopCapture"}
```
Host stops capture and stops sending binary audio frames.

#### `responseAudioStart`
Daemon is about to send response audio chunks. Host should prepare to buffer and play.
```json
{"event": "responseAudioStart", "sampleRate": 16000, "channels": 1, "sampleFormat": "s16le"}
```

#### `responseAudioEnd`
Daemon finished sending response audio for this turn. Host writes buffered PCM as WAV
and plays it to the remote caller via `CallHandle::playFileToRemote()`.
```json
{"event": "responseAudioEnd"}
```
Host sends `playbackFinished` command when playback completes.

#### Binary: Response Audio (daemon → host)
Sent as type `0x02` frames between `responseAudioStart` and `responseAudioEnd`.
Raw PCM s16le bytes at the sample rate specified in `responseAudioStart`.
Daemon resamples Gemini's 24kHz output to 16kHz before sending.

#### `setMicMute`
```json
{"event": "setMicMute", "muted": true}
```
Host calls `CallHandle::setMicrophoneMuted(muted)`.

---

### Config Responses

#### `agents`
Full agent list. Sent in response to any agent CRUD command or `getAgents`.
```json
{"event": "agents", "agents": [
  {"name": "Gemini Flash", "provider": "gemini", "apiKey": "AIza...", "model": "gemini-2.0-flash-live-001", "voice": "Puck", "language": "en"},
  {"name": "Gemini Pro", "provider": "gemini", "apiKey": "AIza...", "model": "gemini-2.5-pro-live", "voice": "Aoede", "language": "en"}
]}
```

#### `scenarios`
Full scenario list.
```json
{"event": "scenarios", "scenarios": [
  {"name": "Test Audio Quality", "agentIndex": 0, "systemPrompt": "You are a test caller..."},
  {"name": "Verify IVR Menu", "agentIndex": 1, "systemPrompt": "Navigate the IVR..."}
], "agentNames": ["Gemini Flash", "Gemini Pro"]}
```

#### `agentTestResult`
```json
{"event": "agentTestResult", "success": true, "message": "Connected to Gemini successfully"}
```

```json
{"event": "agentTestResult", "success": false, "message": "Invalid API key"}
```

---

### Server-Driven UI

#### `ui`
Full UI definition. Sent on connect, and whenever UI needs to change (state transitions, config changes).
```json
{
  "event": "ui",

  "callPanel": {
    "title": "AI Scenario - Test Audio Quality",
    "components": [
      {
        "type": "statusBanner",
        "text": "Test Audio Quality — listening...",
        "variant": "success"
      },
      {
        "type": "transcript",
        "id": "mainTranscript",
        "text": "[00:05] Caller: Hello\n[00:05] AI: Hi there",
        "scrollable": true
      },
      {
        "type": "button",
        "text": "Stop AI",
        "action": "stop",
        "style": "danger"
      }
    ]
  },

  "banner": {
    "visible": true,
    "text": "AI Armed: Test Audio Quality — dial a number",
    "variant": "warning",
    "actions": [
      {"text": "Cancel", "action": "disarm"}
    ]
  },

  "callPageActions": [
    {
      "title": "AI Agent",
      "icon": "robot",
      "items": [
        {"text": "Test Audio Quality", "action": "arm", "params": {"scenarioIndex": 0}},
        {"text": "Verify IVR Menu", "action": "arm", "params": {"scenarioIndex": 1}}
      ]
    }
  ],

  "settingsTabs": [
    {
      "title": "AI Vendors",
      "components": [
        {
          "type": "repeaterList",
          "dataKey": "agents",
          "items": [
            {"name": "Gemini Flash", "provider": "gemini", "model": "gemini-2.0-flash-live-001"}
          ],
          "card": {
            "title": "{name}",
            "subtitle": "{provider} — {model}",
            "actions": [
              {"text": "Edit", "action": "editAgentDialog", "params": {"index": "$index"}},
              {"text": "Test", "action": "testAgent", "params": {"index": "$index"}},
              {"text": "Delete", "action": "removeAgent", "params": {"index": "$index"}, "style": "danger", "confirm": "Delete this vendor?"}
            ]
          }
        },
        {
          "type": "button",
          "text": "+ Add Vendor",
          "action": "addAgentDialog",
          "style": "secondary"
        }
      ],
      "dialogs": {
        "addAgentDialog": {
          "title": "Add AI Vendor",
          "fields": [
            {"key": "name", "label": "Name", "type": "text", "required": true},
            {"key": "provider", "label": "Provider", "type": "dropdown", "options": ["gemini"], "default": "gemini"},
            {"key": "apiKey", "label": "API Key", "type": "secret", "required": true},
            {"key": "model", "label": "Model", "type": "text", "default": "gemini-2.0-flash-live-001"},
            {"key": "voice", "label": "Voice", "type": "text", "default": "Puck"},
            {"key": "language", "label": "Language", "type": "text", "default": "en"}
          ],
          "submitAction": "addAgent",
          "submitText": "Add"
        },
        "editAgentDialog": {
          "title": "Edit AI Vendor",
          "fields": [
            {"key": "name", "label": "Name", "type": "text", "required": true},
            {"key": "provider", "label": "Provider", "type": "dropdown", "options": ["gemini"], "default": "gemini"},
            {"key": "apiKey", "label": "API Key", "type": "secret", "required": true},
            {"key": "model", "label": "Model", "type": "text"},
            {"key": "voice", "label": "Voice", "type": "text"},
            {"key": "language", "label": "Language", "type": "text"}
          ],
          "submitAction": "updateAgent",
          "submitText": "Save"
        }
      }
    },
    {
      "title": "AI Scenarios",
      "components": [
        {
          "type": "repeaterList",
          "dataKey": "scenarios",
          "items": [
            {"name": "Test Audio Quality", "agentName": "Gemini Flash"}
          ],
          "card": {
            "title": "{name}",
            "subtitle": "{agentName}",
            "actions": [
              {"text": "Edit", "action": "editScenarioDialog", "params": {"index": "$index"}},
              {"text": "Delete", "action": "removeScenario", "params": {"index": "$index"}, "style": "danger", "confirm": "Delete this scenario?"}
            ]
          }
        },
        {
          "type": "button",
          "text": "+ Add Scenario",
          "action": "addScenarioDialog",
          "style": "secondary"
        }
      ],
      "dialogs": {
        "addScenarioDialog": {
          "title": "Add AI Scenario",
          "fields": [
            {"key": "name", "label": "Scenario Name", "type": "text", "required": true},
            {"key": "agentIndex", "label": "AI Vendor", "type": "dropdown", "optionsFrom": "agentNames"},
            {"key": "systemPrompt", "label": "System Prompt", "type": "textarea", "required": true}
          ],
          "submitAction": "addScenario",
          "submitText": "Add"
        },
        "editScenarioDialog": {
          "title": "Edit AI Scenario",
          "fields": [
            {"key": "name", "label": "Scenario Name", "type": "text", "required": true},
            {"key": "agentIndex", "label": "AI Vendor", "type": "dropdown", "optionsFrom": "agentNames"},
            {"key": "systemPrompt", "label": "System Prompt", "type": "textarea", "required": true}
          ],
          "submitAction": "updateScenario",
          "submitText": "Save"
        }
      }
    }
  ]
}
```

---

## UI Component Types

| Type | Description | Properties |
|------|-------------|------------|
| `statusBanner` | Colored banner with text | `text`, `variant` (success/warning/info/danger) |
| `transcript` | Read-only scrollable text area | `id`, `text`, `scrollable` |
| `button` | Action button | `text`, `action`, `style` (primary/secondary/danger), `params` |
| `text` | Static label | `text`, `size` (h1/h2/body/caption), `color` |
| `textInput` | Editable text field | `key`, `label`, `value`, `placeholder` |
| `secret` | Password/key field | `key`, `label`, `value` |
| `textarea` | Multi-line text input | `key`, `label`, `value`, `rows` |
| `dropdown` | Select from options | `key`, `label`, `options`, `optionsFrom`, `value` |
| `repeaterList` | List of cards with actions | `dataKey`, `items`, `card` |

## UI Update Strategy

- On state change (arm/disarm/start/stop): daemon sends full `ui` event with updated components
- On transcript append: daemon sends `transcriptAppend` event (incremental, not full UI refresh)
- On config change: daemon sends `ui` event with updated `settingsTabs`
- Host can request full refresh anytime with `getUI` command

---

## Error Handling

Daemon sends errors as state updates:
```json
{"event": "stateUpdate", "status": "Error: Gemini connection failed", "active": false, "armed": false}
```

Protocol-level errors (malformed JSON, unknown command):
```json
{"event": "error", "message": "Unknown command: xyz"}
```

---

## Versioning

- `protocolVersion` in `ready` event allows future incompatible changes
- Host checks version and warns user if daemon needs updating
- New fields can be added to existing messages without version bump (additive changes)
