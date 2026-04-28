# CallForge Daemon Architecture — Process Isolation + Server-Driven UI

## Why

The current plugin (.so) loads into the GPL host process via QPluginLoader. It includes
mediastreamer2 GPL headers and shares address space with the host. Under strict GPL
interpretation this makes the plugin a derivative work, requiring source disclosure.

Moving proprietary logic to a separate daemon process eliminates the derivative-work
concern entirely. Two separate programs communicating over a socket are not a combined work.

Adding server-driven UI means the daemon owns both the logic AND the UI design — the host
is just a generic renderer. New features ship by updating only the daemon binary.

## Target Architecture

```
┌──────────────────────────────┐      Unix socket       ┌──────────────────────────────┐
│  CallForge Host (GPL)        │◄══ JSON messages ═════►│  callforge-ai-daemon         │
│                              │   /tmp/callforge.sock   │  (Proprietary)               │
│  AIBridge.cpp (~200 lines)   │                         │                              │
│  - QObject, Q_PROPERTYs     │                         │  AICallController            │
│  - socket client             │                         │  GeminiLiveClient            │
│  - forwards call events      │                         │  CaptureFilePoller           │
│  - executes audio commands   │                         │  Audio resampling            │
│                              │                         │  Agent/scenario config       │
│  GenericPanelRenderer.qml    │                         │  UI definition generator     │
│  - renders JSON → QML        │                         │                              │
│                              │                         │  Zero GPL headers            │
│  No AI logic                 │                         │  Optional: Rust, Go, or C++  │
└──────────────────────────────┘                         └──────────────────────────────┘
```

Audio exchange via socket streaming (no shared filesystem):
- Capture: Host reads SDK capture file → streams PCM chunks over socket → daemon forwards to Gemini
- Response: Daemon receives Gemini audio → streams PCM chunks over socket → host buffers, writes WAV, plays via SDK
- Only the host touches /tmp (for SDK's file-based capture/playback APIs). Daemon is a pure network service.

---

## Phases

### Phase 1 — IPC Protocol Definition

Define the JSON message schema for host↔daemon communication. No code yet, just the
contract both sides will implement against.

**Deliverable:** `protocol.md` document in this plans folder.

**Message categories:**

Host → Daemon (commands):
- Lifecycle: `connect`, `disconnect`, `getUI`
- AI control: `arm`, `disarm`, `start`, `stop`
- Call events: `callStateChanged`, `captureReady`, `playbackFinished`
- Config CRUD: `getAgents`, `addAgent`, `removeAgent`, `updateAgent`,
  `getScenarios`, `addScenario`, `removeScenario`, `updateScenario`
- Testing: `testAgent`

Daemon → Host (events):
- State: `stateUpdate` (active, armed, geminiReady, status, activeScenarioName)
- Transcript: `transcriptAppend`
- Audio commands: `startCapture`, `stopCapture`, `playAudio`, `setMicMute`
- UI definitions: `ui` (panels, banners, settings, actions)
- Config responses: `agents`, `scenarios`
- Test results: `agentTestResult`

**Framing:** Length-prefixed JSON. Each message is `<4-byte big-endian length><JSON bytes>`.

**Test:** Review document, confirm it covers all current AICallController functionality.

---

### Phase 2 — Daemon Skeleton

Standalone C++ binary (no Qt, no GPL headers) with:
- Unix socket listener on `/tmp/callforge.sock`
- JSON message parsing (using nlohmann/json or similar)
- Message dispatch loop
- Stub handlers that log received commands and send hardcoded responses
- CMakeLists.txt with cross-platform socket abstraction

**Files created:**
```
callforge-daemon/
├── CMakeLists.txt
├── src/
│   ├── main.cpp              — socket listen + accept + event loop
│   ├── IpcServer.cpp/hpp     — socket I/O, message framing
│   └── MessageHandler.cpp/hpp — dispatch + stub responses
└── protocol/
    └── messages.hpp          — message type constants
```

**Test:** Start daemon, connect with `socat`, send `{"cmd":"getUI"}`,
verify daemon responds with stub JSON.

---

### Phase 3 — AIBridge Host Component

Replace the current plugin loader path with a thin bridge that connects to the daemon
over the socket. The bridge exposes the same Q_PROPERTYs the QML currently binds to.

**Files created/modified:**
- New: `Linphone/core/bridge/AIBridge.cpp/hpp`
  - QObject with: active, armed, geminiReady, transcript, status, activeScenarioName
  - Q_INVOKABLE: armAICall, disarmAICall, startAICall, stopAICall
  - Q_INVOKABLE: CRUD for agents/scenarios
  - Socket client: connects to `/tmp/callforge.sock`
  - Receives daemon events → updates properties → emits signals
  - Receives QML calls → sends commands to daemon
- Modified: `App.cpp` — register AIBridge as `AICallControllerCpp` context property
  (replaces current plugin-provided registration)
- Modified: `App.cpp` — launch daemon via `QProcess` on startup

**Test:** Host starts, launches daemon, connects. Arm a scenario from QML →
AIBridge sends `arm` command → daemon stub responds with state update →
QML banner appears. All using stubs, no real Gemini yet.

---

### Phase 4 — Move Business Logic to Daemon

Move the actual AI logic from the plugin .so into the daemon:
- `AICallController` (minus Qt MOC — plain C++ class)
- `GeminiLiveClient` (WebSocket to Gemini — uses standalone WebSocket lib, not Qt)
- `CaptureFilePoller` (file I/O, no Qt dependency needed — use std::thread + std::filesystem)
- Audio resampling functions (pure math, already GPL-free)
- Agent/scenario config (JSON file instead of linphonerc)

**Key change:** Remove all `#include <mediastreamer2/*>` from daemon code.
Audio capture is now host-driven:
1. AIBridge sends `startCapture` to host → host calls CallHandle::startMixedRecordToFile()
2. Host sends `captureReady` with file path + sample rate to daemon
3. Daemon polls the file directly (CaptureFilePoller)

Audio playback:
1. Daemon writes response WAV to /tmp
2. Daemon sends `playAudio` with path to host
3. AIBridge calls CallHandle::playFileToRemote()
4. Host sends `playbackFinished` when done

**Files created/modified:**
- Daemon: `src/AIController.cpp/hpp` — business logic (no Qt)
- Daemon: `src/GeminiClient.cpp/hpp` — WebSocket client (standalone lib)
- Daemon: `src/CapturePoller.cpp/hpp` — file poller (std::thread)
- Daemon: `src/AudioUtils.cpp/hpp` — resampling
- Daemon: `src/Config.cpp/hpp` — JSON file config for agents/scenarios
- Host: `AIBridge.cpp` — handle audio commands from daemon

**Test:** Full end-to-end: arm scenario → dial number → AI conversation works →
transcript appears → stop. Same as today but over socket.

---

### Phase 5 — Server-Driven UI: Generic Renderer

Replace the hardcoded QML panels with a generic renderer in the host that
takes JSON UI definitions from the daemon and renders them dynamically.

**Component types the renderer supports:**
- `statusBanner` — colored bar with text (green/orange/grey)
- `transcript` — read-only scrollable text area
- `button` — action button (sends action ID back to daemon)
- `text` — static label
- `textInput` — editable field (sends value on change)
- `secretInput` — password field
- `dropdown` — select from options
- `repeaterList` — list of cards with actions
- `formDialog` — popup form with fields + save/cancel

**Example daemon → host message:**
```json
{
  "event": "ui",
  "callPanel": {
    "title": "AI Scenario - Test Audio Quality",
    "components": [
      {"type": "statusBanner", "text": "listening...", "variant": "success"},
      {"type": "transcript", "text": "[00:05] Caller: Hello", "id": "transcript1"},
      {"type": "button", "text": "Stop AI", "action": "stop", "style": "danger"}
    ]
  },
  "banner": {
    "visible": true,
    "text": "AI Armed: Test Audio Quality — dial a number",
    "variant": "warning",
    "actions": [{"text": "Cancel", "action": "disarm"}]
  },
  "callPageActions": [{
    "title": "AI Agent",
    "scenarios": [
      {"name": "Test Audio Quality", "action": "arm", "index": 0},
      {"name": "Verify IVR Menu", "action": "arm", "index": 1}
    ]
  }]
}
```

**Files created/modified:**
- New: `Linphone/view/Plugin/GenericPanelRenderer.qml` — dynamic component loader
- New: `Linphone/view/Plugin/GenericBanner.qml` — generic armed banner
- New: `Linphone/view/Plugin/GenericCallAction.qml` — generic scenario picker
- Modified: `AIBridge.cpp/hpp` — expose UI JSON as properties
- Modified: `CallsWindow.qml` — load GenericPanelRenderer instead of plugin .qrc panels
- Modified: `CallPage.qml` — load GenericBanner/GenericCallAction

**Test:** Daemon sends different UI definitions → host renders them correctly.
Change daemon UI JSON → host updates without rebuild.

---

### Phase 6 — Server-Driven Settings UI

Extend the generic renderer to handle settings tabs. Daemon defines the settings
forms (AI Vendors, AI Scenarios) as JSON.

**Example:**
```json
{
  "event": "settingsUI",
  "tabs": [
    {
      "title": "AI Vendors",
      "components": [
        {"type": "repeaterList", "dataKey": "agents", "card": {
          "title": "{name}", "subtitle": "{provider} — {model}",
          "actions": ["edit", "delete", "test"]
        }},
        {"type": "button", "text": "+ Add Vendor", "action": "addAgentDialog"}
      ]
    },
    {
      "title": "AI Scenarios",
      "components": [
        {"type": "repeaterList", "dataKey": "scenarios", "card": {
          "title": "{name}", "subtitle": "{agentName}",
          "actions": ["edit", "delete"]
        }},
        {"type": "button", "text": "+ Add Scenario", "action": "addScenarioDialog"}
      ]
    }
  ]
}
```

**Files created/modified:**
- New: `Linphone/view/Plugin/GenericSettingsTab.qml`
- Modified: `SettingsPage.qml` — inject daemon-defined tabs
- Modified: `AIBridge.cpp` — relay settings CRUD commands

**Test:** Open Settings → AI Vendors tab renders from daemon JSON → add/edit/delete
agents works → restart app → config persists (daemon stores in its own JSON file).

---

### Phase 7 — Cross-Platform Daemon Build

Set up build system to produce daemon binaries for Linux, macOS, and Windows.

**Socket abstraction:**
- Linux/macOS: Unix domain socket
- Windows: Named pipe (`\\.\pipe\callforge`)

**Build options:**
- CMake cross-compilation targets
- Or rewrite daemon in Rust (cargo build --target) for simpler cross-platform story

**Files created/modified:**
- Daemon: `src/Platform.cpp/hpp` — socket abstraction (Unix socket vs named pipe)
- Daemon: `CMakeLists.txt` — cross-platform targets
- Host: `AIBridge.cpp` — platform-specific socket path

**Deliverable:** Three binaries:
- `callforge-ai-daemon` (Linux ELF)
- `callforge-ai-daemon` (macOS Mach-O)
- `callforge-ai-daemon.exe` (Windows PE)

**Test:** Build and run on each platform. Host connects to daemon on each OS.

---

### Phase 8 — Cleanup & Cutover

Remove old plugin infrastructure:
- Delete `callforge-plugin/` .so build (or archive it)
- Remove `QPluginLoader` code from host
- Remove `CallForgePluginInterface.hpp`, `CallForgeHostContextImpl`, `CallForgePluginLoader`
- Remove plugin-specific QML handling from CallsWindow.qml and CallPage.qml
- Clean up host CMakeLists.txt

**Test:** Full end-to-end regression — arm, call, transcript, settings, stop.
Verify no GPL headers referenced by daemon. Verify host builds clean without plugin code.

---

## Phase Dependencies

```
Phase 1 (protocol)
  └──► Phase 2 (daemon skeleton)
  └──► Phase 3 (AIBridge host)
         └──► Phase 4 (move business logic) — first fully working end-to-end
                └──► Phase 5 (server-driven call UI)
                └──► Phase 6 (server-driven settings UI)
                       └──► Phase 7 (cross-platform)
                              └──► Phase 8 (cleanup)
```

Phases 2 and 3 can be done in parallel after Phase 1.
Phases 5 and 6 can be done in parallel after Phase 4.

## Language Decision (Daemon)

| Option | Pros | Cons |
|--------|------|------|
| C++ (no Qt) | Reuse existing code with minimal rewrite | Cross-platform sockets need abstraction, WebSocket lib needed |
| Rust | Single binary, memory safety, great WebSocket libs (tungstenite), easy cross-compile | Full rewrite of business logic |
| Go | Single binary, easy cross-compile, good WebSocket support | Full rewrite, larger binary size |

**Recommendation:** Start with C++ (no Qt) to reuse existing GeminiLiveClient and
CaptureFilePoller with minimal changes. Replace Qt WebSocket with a standalone lib
(e.g., libwebsockets or ixwebsocket). Optionally migrate to Rust later.

## Open Questions

1. Should the host auto-start the daemon, or should it run as a system service?
2. Should the daemon support multiple simultaneous host connections?
3. Authentication between host and daemon — needed or not (single-user machine)?
4. Should the daemon config file be JSON, TOML, or INI?
