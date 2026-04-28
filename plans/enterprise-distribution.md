This is extremely important -- the Linphone SDK is AGPLv3 and the desktop app is GPLv3. This has major implications for the commercial licensing model. Now I have all the information I need to produce the comprehensive plan.

---

# CallForge Enterprise Distribution and Licensing Plan

## Table of Contents

1. [Critical Legal Prerequisite: Open Source License Compliance](#1-critical-legal-prerequisite)
2. [Architecture Overview](#2-architecture-overview)
3. [Licensing Model Options with Pricing Tiers](#3-licensing-model-options)
4. [License Validation Flow (Online + Offline)](#4-license-validation-flow)
5. [Website Technology Decisions](#5-website-technology-decisions)
6. [Backend API Design (Node.js + MySQL)](#6-backend-api-design)
7. [Admin Console Features](#7-admin-console-features)
8. [Desktop App Integration (C++ License Check)](#8-desktop-app-integration)
9. [Security Hardening](#9-security-hardening)
10. [Additional Enterprise Features](#10-additional-enterprise-features)
11. [Implementation Phases](#11-implementation-phases)

---

## 1. Critical Legal Prerequisite: Open Source License Compliance

**This must be addressed before any commercial licensing work begins.**

The codebase analysis reveals:
- `/home/rammohanyadavalli/Downloads/linphone-desktop/LICENSE.txt` -- **GPLv3** (the desktop app shell)
- `/home/rammohanyadavalli/Downloads/linphone-desktop/external/linphone-sdk/LICENSE.txt` -- **AGPLv3** (the Linphone SDK)
- `/home/rammohanyadavalli/Downloads/linphone-desktop/external/linphone-sdk/liblinphone/LICENSE.txt` -- **AGPLv3** (liblinphone itself)

AGPLv3 is the most restrictive copyleft license. It requires that anyone who interacts with the software over a network must be able to receive the complete source code. This means:

**Option A: Acquire a Commercial License from Belledonne Communications.** Belledonne Communications (the company behind Linphone) offers dual licensing. Their commercial license removes the AGPL/GPL obligations and allows proprietary distribution. This is the path used by companies like Ooma and Obi that embed Linphone. Cost is negotiable -- typically in the range of 10K-50K EUR/year depending on usage.

**Option B: Comply with AGPLv3/GPLv3.** You can charge for the software, but you MUST provide complete source code to anyone who receives the binary, including your modifications, the license validation code, the AI integration code, everything. Users could strip the license check and redistribute. This makes DRM-style protection legally unenforceable against someone who exercises their GPL rights.

**Option C: Clean-room separation.** Build the licensing/AI/CallForge-specific logic as a separate proprietary service that communicates with the GPL'd desktop app via a well-defined API boundary. The desktop app remains GPL and is freely distributable. The proprietary value is in the backend service (scenario execution, AI vendor management, transcript analysis, reporting). This is the "open core" model used by GitLab, Elastic, and others.

**Recommended approach: Option C (open core) for immediate progress, with Option A pursued in parallel.** The rest of this plan assumes Option C: the desktop app binary is freely available (with source), but it is useless without a valid CallForge account because all AI orchestration, scenario storage, transcript processing, and reporting happen server-side. The license check in the desktop app controls access to server-side resources, not to the binary itself.

---

## 2. Architecture Overview

```
                    INTERNET
                       |
              +--------+--------+
              |   Cloudflare    |  <-- DDoS protection, WAF, CDN
              |   (free tier)   |
              +--------+--------+
                       |
              +--------+--------+
              |     Nginx       |  <-- Reverse proxy, TLS termination,
              |  (port 443)     |      rate limiting, static assets
              +--+-----+----+--+
                 |     |    |
        +--------+  +--+--+ +--------+
        |           |      |          |
   WordPress    Node.js  Node.js   Static
   (port 8080)  API      Admin     React
   Marketing    (3001)   (3002)    Admin
   Site                            Build
        |           |
        |     +-----+------+
        |     |   MySQL 8   |
        |     | (port 3306) |
        |     +-------------+
        |
   WordPress DB
   (separate MySQL
    database)

   Desktop App (C++/Qt6)
        |
        +--- HTTPS to api.callforge.io (license validation, usage reporting)
        +--- WebSocket to Gemini/AI vendors (existing, unchanged)
        +--- SIP calls via LibLinphone (existing, unchanged)
```

### Component Responsibilities

**Marketing Website (WordPress at marketing.callforge.io)**
- Product pages, pricing, testimonials, case studies
- Blog for SEO and thought leadership
- Contact/demo request forms
- Download portal (gated behind registration, served by Node.js API)

**Backend API (Node.js at api.callforge.io)**
- User registration and authentication (JWT-based)
- License key generation, validation, and revocation
- Download token generation (signed, time-limited URLs)
- Usage telemetry ingestion (test count, duration, AI vendor usage)
- Webhook endpoints for payment processors (Stripe)
- Enterprise SSO integration (SAML/OIDC)

**Admin Console (React SPA at admin.callforge.io)**
- Dashboard: active users, license utilization, revenue metrics
- User management: create, suspend, modify licenses
- Enterprise management: organization hierarchy, seat allocation
- Usage analytics: per-user test counts, AI vendor breakdown, call duration
- Audit log viewer
- License key management

**Desktop App (CallForge binary)**
- At startup: validate cached license or phone home to API
- Reports usage telemetry (test count per session) on each test completion
- Receives feature flags from API (which AI vendors enabled, test limits)
- All existing functionality (SIP calling, Gemini AI integration, scenario management) remains in the app
- Source code remains GPL-available; the commercial value is in the account/API access

---

## 3. Licensing Model Options with Pricing Tiers

### Research: How Comparable Tools Price

| Tool | Model | Pricing Range |
|------|-------|---------------|
| Postman | Free tier + per-seat | Free / $14/user/mo / $49/user/mo / Enterprise custom |
| k6 Cloud | Per-virtual-user-hour | $0.06/VUH, plans from $0 to enterprise |
| Sauce Labs | Per-concurrent-session | $39/mo starter to $249/mo enterprise |
| LoadRunner (Micro Focus) | Per-virtual-user license | $2,000-15,000+ per VU pack |
| Cypress Cloud | Per-test-result recorded | Free 500/mo, $75/mo for 25K, $200/mo for 100K |
| Mabl | Per-test-run | Starting ~$500/mo, enterprise custom |
| Testim | Per-seat + test runs | Starting ~$450/mo |

### Recommended Tiers for CallForge

**Tier 1: Starter (Solo/Small Team)**
- Price: $49/month or $490/year (2 months free)
- 1 user seat
- 100 AI test calls per month
- 2 AI vendor configurations
- 5 scenarios
- Community support (GitHub issues)
- 7-day offline grace period

**Tier 2: Professional (Growing Teams)**
- Price: $149/month or $1,490/year per seat (minimum 3 seats)
- Unlimited AI test calls
- Unlimited vendors and scenarios
- Transcript export (CSV, JSON)
- Priority email support
- 14-day offline grace period
- Basic reporting dashboard

**Tier 3: Enterprise (Custom)**
- Price: Starting at $499/month per seat, volume discounts
- Everything in Professional
- SSO (SAML 2.0 / OIDC)
- Audit logging
- Custom AI vendor integration support
- Dedicated account manager
- 30-day offline grace period
- On-premise deployment option (license server)
- SLA guarantee

**Trial: 14-Day Free Trial**
- Full Professional features
- 50 AI test calls
- No credit card required
- Converts to Starter or locks out after expiry

### License Key Format

Use a structured key that encodes metadata:

```
CF-{TIER}-{ORGID}-{RANDOM}-{CHECK}
Example: CF-PRO-00042-8a7b3c9d-e4f5
```

The key itself is a lookup identifier only. All validation happens server-side. The desktop app never decodes the key locally; it sends it to the API and receives a signed JWT license token.

---

## 4. License Validation Flow (Online + Offline)

### Online Validation (Primary)

```
Desktop App Start
    |
    v
Read cached license token from:
    ~/.config/callforge/license.dat  (encrypted)
    |
    v
Is token present and not expired?
    |-- No --> Prompt for login (email + password) or license key
    |           |
    |           v
    |       POST api.callforge.io/v1/auth/login
    |       Body: { email, password, machineId, appVersion }
    |           |
    |           v
    |       Receive: { accessToken (JWT, 15min), refreshToken (JWT, 30d), licenseToken (JWT, grace period) }
    |       Cache licenseToken to ~/.config/callforge/license.dat (encrypted with machine-specific key)
    |           |
    |           v
    |       App proceeds to main UI
    |
    |-- Yes (token present) -->
    |       Is internet reachable?
    |       |-- Yes --> POST api.callforge.io/v1/license/validate
    |       |           Body: { licenseToken, machineId, appVersion }
    |       |           Response: { valid: true, newLicenseToken, features, limits }
    |       |           Update cached token
    |       |           App proceeds
    |       |
    |       |-- No (offline) --> Verify licenseToken signature locally using embedded public key
    |                            Is token within grace period?
    |                            |-- Yes --> App proceeds in offline mode (features per cached token)
    |                            |-- No --> Show "License expired. Connect to internet to revalidate."
    |                                       Block app functionality
```

### License Token (JWT) Structure

```json
{
  "sub": "user-uuid",
  "org": "org-uuid",
  "tier": "professional",
  "seats": 5,
  "machineId": "sha256-of-hardware-identifiers",
  "features": {
    "maxVendors": -1,
    "maxScenarios": -1,
    "maxTestsPerMonth": -1,
    "transcriptExport": true,
    "sso": false
  },
  "iat": 1745000000,
  "exp": 1746209600,
  "grace": 1747419200,
  "iss": "api.callforge.io",
  "jti": "unique-token-id"
}
```

- `exp`: Normal expiry (requires online revalidation after this)
- `grace`: Hard deadline for offline use (14 days after `exp` for Professional)
- The JWT is signed with RS256 (RSA 2048-bit). The public key is embedded in the desktop app binary.

### Machine ID Generation

Generate a stable, non-PII machine fingerprint:
- Linux: SHA-256 of (`/etc/machine-id` + first MAC address + username)
- Windows: SHA-256 of (MachineGuid from registry + first MAC + username)
- macOS: SHA-256 of (IOPlatformSerialNumber + first MAC + username)

This prevents a single license from being used on unlimited machines simultaneously. The server tracks active `machineId` values per license and enforces seat limits.

### Grace Period Logic

```
Time -->
|-------- exp --------|-------- grace --------|
     Normal online         Offline allowed          Locked out
     revalidation          (degraded features)
     every 24h
```

During the grace period, the app shows a persistent banner: "License revalidation required. Connect to the internet within X days." Usage telemetry is queued locally and uploaded when connectivity resumes.

---

## 5. Website Technology Decisions

### Recommendation: WordPress for Marketing + Custom Node.js API + React Admin

**WordPress for the Marketing Site -- why this is the right choice for a solo developer:**

1. Time-to-market: Hundreds of enterprise SaaS themes exist (Flavor, SaaSland, Developer). A polished site in 2-3 days, not 2-3 weeks.
2. SEO plugins (Yoast, RankMath) handle meta tags, sitemaps, schema markup out of the box.
3. Blog is native -- critical for SEO and enterprise credibility.
4. Form plugins (Gravity Forms, WPForms) handle demo requests and lead capture.
5. WooCommerce can serve as the self-service purchase flow if needed, or you can use Stripe Checkout links.
6. Security is achievable with proper hardening (see Section 9).

**Why NOT a fully custom marketing site:** As a solo developer, every hour spent on a custom landing page is an hour not spent on the product. WordPress themes for SaaS marketing pages are nearly indistinguishable from custom builds when properly configured.

**Why NOT WordPress for the backend/admin:** WordPress's PHP-based architecture is wrong for API services, real-time telemetry, and JWT-based auth. Keep the API in Node.js.

### Architecture Pattern: WordPress + Node.js Hybrid

This is a well-established pattern (used by companies like Automattic itself for WordPress.com, and by many SaaS companies):

```
                         callforge.io (Nginx)
                              |
              +---------------+---------------+
              |               |               |
        /blog/*          /api/v1/*      /admin/*
        /pricing         /auth/*        (React SPA)
        /features        /license/*
        /download        /telemetry/*
              |               |
         WordPress       Node.js Express
         (PHP-FPM)       (PM2 managed)
              |               |
         wp_database     callforge_database
         (MySQL)         (MySQL - same server,
                          different database)
```

Nginx routes by URL path:
- `callforge.io/` and all marketing routes go to WordPress
- `api.callforge.io/` goes to Node.js API
- `admin.callforge.io/` serves static React build, API calls go to Node.js

---

## 6. Backend API Design (Node.js + MySQL)

### Technology Stack

- **Runtime:** Node.js 20 LTS
- **Framework:** Express 4.x (mature, battle-tested)
- **ORM:** Knex.js (query builder) -- NOT a full ORM, gives control over queries while preventing SQL injection
- **Authentication:** jsonwebtoken + bcrypt + passport.js
- **Validation:** Joi or Zod for request schema validation
- **Process Manager:** PM2 (clustering, auto-restart, log rotation)
- **Email:** Nodemailer with SMTP (SendGrid/Mailgun for transactional emails)
- **Payment:** Stripe SDK (subscriptions, invoices, webhooks)

### API Endpoints

```
Authentication
  POST   /v1/auth/register          -- Create account (email, password, name, company)
  POST   /v1/auth/login             -- Login (returns access + refresh + license tokens)
  POST   /v1/auth/refresh           -- Refresh access token
  POST   /v1/auth/logout            -- Invalidate refresh token
  POST   /v1/auth/forgot-password   -- Send reset email
  POST   /v1/auth/reset-password    -- Reset with token
  POST   /v1/auth/verify-email      -- Email verification

License Management
  POST   /v1/license/validate       -- Validate license token (called by desktop app)
  POST   /v1/license/activate       -- Activate license on a new machine
  POST   /v1/license/deactivate     -- Remove machine from license
  GET    /v1/license/status         -- Get current license details

Downloads
  GET    /v1/download/latest        -- Get download URL (authenticated, generates signed URL)
  GET    /v1/download/:token        -- Download binary via signed token (time-limited)

Telemetry
  POST   /v1/telemetry/usage        -- Report test execution (called by desktop app per-test)
  POST   /v1/telemetry/batch        -- Batch upload queued offline telemetry

Subscriptions (webhook)
  POST   /v1/webhooks/stripe        -- Stripe payment events

Admin (protected by admin role + IP whitelist)
  GET    /v1/admin/users            -- List users with pagination/search
  GET    /v1/admin/users/:id        -- User detail
  PATCH  /v1/admin/users/:id        -- Modify user/license
  GET    /v1/admin/organizations    -- List organizations
  GET    /v1/admin/analytics/usage  -- Usage analytics
  GET    /v1/admin/analytics/revenue -- Revenue analytics
  GET    /v1/admin/audit-log        -- Audit log
```

### MySQL Schema (Core Tables)

```sql
-- Organizations (enterprises)
CREATE TABLE organizations (
    id              CHAR(36) PRIMARY KEY,           -- UUIDv4
    name            VARCHAR(255) NOT NULL,
    domain          VARCHAR(255),                    -- For SSO domain matching
    tier            ENUM('trial','starter','professional','enterprise') NOT NULL DEFAULT 'trial',
    max_seats       INT NOT NULL DEFAULT 1,
    stripe_customer_id VARCHAR(255),
    created_at      TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at      TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    INDEX idx_domain (domain),
    INDEX idx_stripe (stripe_customer_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Users
CREATE TABLE users (
    id              CHAR(36) PRIMARY KEY,
    org_id          CHAR(36) NOT NULL,
    email           VARCHAR(255) NOT NULL UNIQUE,
    password_hash   VARCHAR(255) NOT NULL,           -- bcrypt, 12 rounds
    name            VARCHAR(255) NOT NULL,
    role            ENUM('owner','admin','member') NOT NULL DEFAULT 'member',
    email_verified  BOOLEAN DEFAULT FALSE,
    mfa_secret      VARCHAR(255),                    -- TOTP secret (encrypted at app layer)
    mfa_enabled     BOOLEAN DEFAULT FALSE,
    failed_login_attempts INT DEFAULT 0,
    locked_until    TIMESTAMP NULL,
    last_login_at   TIMESTAMP NULL,
    created_at      TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at      TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    FOREIGN KEY (org_id) REFERENCES organizations(id),
    INDEX idx_email (email),
    INDEX idx_org (org_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- License Keys
CREATE TABLE licenses (
    id              CHAR(36) PRIMARY KEY,
    org_id          CHAR(36) NOT NULL,
    license_key     VARCHAR(64) NOT NULL UNIQUE,     -- CF-PRO-00042-8a7b3c9d-e4f5
    tier            ENUM('trial','starter','professional','enterprise') NOT NULL,
    max_machines    INT NOT NULL DEFAULT 1,
    is_active       BOOLEAN DEFAULT TRUE,
    expires_at      TIMESTAMP NOT NULL,
    grace_days      INT NOT NULL DEFAULT 7,
    created_at      TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (org_id) REFERENCES organizations(id),
    INDEX idx_key (license_key),
    INDEX idx_org (org_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Machine activations (tracks which machines are using a license)
CREATE TABLE machine_activations (
    id              CHAR(36) PRIMARY KEY,
    license_id      CHAR(36) NOT NULL,
    user_id         CHAR(36) NOT NULL,
    machine_id      VARCHAR(64) NOT NULL,            -- SHA-256 fingerprint
    machine_name    VARCHAR(255),                     -- User-friendly OS hostname
    app_version     VARCHAR(20),
    last_seen_at    TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    activated_at    TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (license_id) REFERENCES licenses(id),
    FOREIGN KEY (user_id) REFERENCES users(id),
    UNIQUE KEY uk_license_machine (license_id, machine_id),
    INDEX idx_user (user_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Usage telemetry
CREATE TABLE usage_events (
    id              BIGINT AUTO_INCREMENT PRIMARY KEY,
    user_id         CHAR(36) NOT NULL,
    org_id          CHAR(36) NOT NULL,
    machine_id      VARCHAR(64) NOT NULL,
    event_type      ENUM('test_start','test_complete','test_failed','app_start','app_stop') NOT NULL,
    ai_vendor       VARCHAR(100),                     -- 'gemini', 'openai', etc.
    scenario_name   VARCHAR(255),
    call_duration_ms INT,
    metadata        JSON,                             -- Flexible extra data
    recorded_at     TIMESTAMP NOT NULL,               -- When it happened (client time)
    created_at      TIMESTAMP DEFAULT CURRENT_TIMESTAMP, -- When server received it
    FOREIGN KEY (user_id) REFERENCES users(id),
    FOREIGN KEY (org_id) REFERENCES organizations(id),
    INDEX idx_org_date (org_id, recorded_at),
    INDEX idx_user_date (user_id, recorded_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Refresh tokens (for invalidation)
CREATE TABLE refresh_tokens (
    id              CHAR(36) PRIMARY KEY,
    user_id         CHAR(36) NOT NULL,
    token_hash      VARCHAR(64) NOT NULL,             -- SHA-256 of the token
    expires_at      TIMESTAMP NOT NULL,
    revoked         BOOLEAN DEFAULT FALSE,
    created_at      TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (user_id) REFERENCES users(id),
    INDEX idx_user (user_id),
    INDEX idx_hash (token_hash)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Audit log
CREATE TABLE audit_log (
    id              BIGINT AUTO_INCREMENT PRIMARY KEY,
    actor_id        CHAR(36),                         -- User who performed action (NULL for system)
    action          VARCHAR(100) NOT NULL,             -- 'user.login', 'license.activate', etc.
    target_type     VARCHAR(50),                       -- 'user', 'license', 'organization'
    target_id       CHAR(36),
    ip_address      VARCHAR(45),                       -- IPv4 or IPv6
    user_agent      VARCHAR(500),
    details         JSON,
    created_at      TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    INDEX idx_actor (actor_id, created_at),
    INDEX idx_target (target_type, target_id, created_at),
    INDEX idx_action (action, created_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Download tokens (signed, time-limited)
CREATE TABLE download_tokens (
    id              CHAR(36) PRIMARY KEY,
    user_id         CHAR(36) NOT NULL,
    token_hash      VARCHAR(64) NOT NULL,
    platform        ENUM('linux','windows','macos') NOT NULL,
    expires_at      TIMESTAMP NOT NULL,
    downloaded      BOOLEAN DEFAULT FALSE,
    created_at      TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (user_id) REFERENCES users(id),
    INDEX idx_hash (token_hash)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
```

---

## 7. Admin Console Features

### Technology

- **Frontend:** React 18 + TypeScript + Vite + TailwindCSS + Recharts (for charts)
- **Auth:** JWT with MFA requirement for all admin users
- **Build:** Static SPA served by Nginx at `admin.callforge.io`

### Dashboard Views

**Overview Dashboard:**
- Active users (daily/weekly/monthly)
- Tests executed (line chart, 30-day trend)
- Revenue (MRR, churn rate)
- Trial-to-paid conversion rate
- Active licenses vs. total licenses

**User Management:**
- Searchable/filterable user table
- Per-user detail: login history, machine activations, test count, license status
- Actions: suspend, reactivate, extend trial, change tier, reset password, revoke all sessions

**Organization Management:**
- Enterprise accounts with member lists
- Seat utilization (used vs. allocated)
- Billing history (Stripe integration)

**Usage Analytics:**
- Tests per user/org over time
- AI vendor usage breakdown (Gemini vs. future vendors)
- Average call duration
- Scenario popularity
- Peak usage hours

**Audit Log:**
- Filterable by actor, action, date range
- Covers: logins, license activations, admin actions, payment events

**License Management:**
- Generate new license keys
- View all active licenses with machine activations
- Revoke/deactivate licenses
- Bulk operations

---

## 8. Desktop App Integration (C++ License Check)

### Where It Fits in the Existing Architecture

Based on the codebase analysis, the license check should be integrated at the `App::initCore()` level in `/home/rammohanyadavalli/Downloads/linphone-desktop/Linphone/core/App.cpp`. The app already has a dual-threaded architecture (UI thread + SDK thread via `CoreModel`), and the license check should run on the UI thread before `initCore()` completes.

### New Files to Create

```
Linphone/core/license/LicenseManager.hpp     -- License validation logic
Linphone/core/license/LicenseManager.cpp
Linphone/core/license/MachineId.hpp           -- Machine fingerprint generation
Linphone/core/license/MachineId.cpp
Linphone/model/license/LicenseApi.hpp         -- HTTP client for license API
Linphone/model/license/LicenseApi.cpp
Linphone/view/Page/Layout/License/LicenseActivationPage.qml  -- Login/activation UI
Linphone/view/Page/Layout/License/LicenseExpiredPage.qml      -- Expiry notification
```

### Integration Point

In `main.cpp` (line 94 area), after `App` is created but before `initCore()`:

```cpp
// Pseudocode for the license check insertion point
auto app = QSharedPointer<App>::create(argc, argv);

// NEW: License validation before anything else
LicenseManager licenseManager;
LicenseManager::ValidationResult result = licenseManager.validate();

if (result.status == LicenseManager::Status::NeedsActivation) {
    // Show activation/login QML page instead of main app
    // This replaces the initCore() path
    app->showLicenseActivation();
} else if (result.status == LicenseManager::Status::Expired) {
    app->showLicenseExpired(result.expiryDate);
} else {
    // Valid license (online-confirmed or offline-within-grace)
    app->setLicenseFeatures(result.features);
    app->initCore();
    app->setSelf(app);
}
```

### What to Cache Locally

File: `~/.config/callforge/license.dat` (the existing config directory per `callforge.conf`)

Contents (encrypted with AES-256-GCM, key derived from machine ID + embedded salt):
```json
{
  "licenseToken": "<JWT signed by server with RS256>",
  "refreshToken": "<opaque token>",
  "lastValidated": "2026-04-26T10:00:00Z",
  "cachedFeatures": {
    "tier": "professional",
    "maxVendors": -1,
    "maxScenarios": -1,
    "maxTestsPerMonth": -1,
    "transcriptExport": true
  },
  "usageQueue": [
    {"event": "test_complete", "ts": "2026-04-26T10:05:00Z", "vendor": "gemini", "duration": 45000}
  ]
}
```

### Offline Validation Logic

```cpp
bool LicenseManager::validateOffline() {
    // 1. Read license.dat, decrypt with machine-specific key
    // 2. Extract JWT licenseToken
    // 3. Verify JWT signature using embedded RSA public key (NO private key in app)
    // 4. Check 'exp' field -- if not expired, fully valid
    // 5. If 'exp' passed but 'grace' not passed, allow degraded mode
    // 6. If 'grace' passed, deny access
    // 7. Check machineId in JWT matches current machine
    // The RSA public key is embedded at compile time in a const array
}
```

### Usage Metering Integration

Hook into the existing `AICallController` at `/home/rammohanyadavalli/Downloads/linphone-desktop/Linphone/core/ai/AICallController.cpp`. After `onGeminiTurnComplete()` or when the AI call ends, emit a usage event:

```cpp
void AICallController::onCallEnded() {
    // Existing cleanup...
    
    // NEW: Report usage
    UsageEvent event;
    event.type = UsageEvent::TestComplete;
    event.vendor = "gemini";
    event.scenarioName = mActiveScenarioName;
    event.durationMs = QDateTime::currentMSecsSinceEpoch() - mCallStartTime;
    LicenseManager::instance()->reportUsage(event);
    // If online: POST immediately to /v1/telemetry/usage
    // If offline: append to usageQueue in license.dat
}
```

### Feature Gating

The existing `SettingsCore` already has feature-gating patterns (e.g., `disableChatFeature`, `disableMeetingsFeature` visible in the header). The license tier features should integrate with this same pattern:

```cpp
// In SettingsCore, add license-derived feature gates:
Q_PROPERTY(int maxAiVendors READ getMaxAiVendors NOTIFY licenseChanged)
Q_PROPERTY(int maxAiScenarios READ getMaxAiScenarios NOTIFY licenseChanged)
Q_PROPERTY(bool transcriptExportEnabled READ getTranscriptExportEnabled NOTIFY licenseChanged)
Q_PROPERTY(QString licenseTier READ getLicenseTier NOTIFY licenseChanged)
Q_PROPERTY(int remainingTests READ getRemainingTests NOTIFY licenseChanged)
```

---

## 9. Security Hardening

### 9.1 Nginx Configuration

```nginx
# /etc/nginx/nginx.conf

worker_processes auto;
worker_rlimit_nofile 65535;

events {
    worker_connections 4096;
    multi_accept on;
    use epoll;
}

http {
    # --- Basic hardening ---
    server_tokens off;                          # Hide nginx version
    more_clear_headers Server;                  # Remove Server header entirely (requires headers-more module)
    
    # --- Request size limits ---
    client_max_body_size 10m;                   # Max upload (for binary downloads, adjust per-location)
    client_body_buffer_size 16k;
    client_header_buffer_size 1k;
    large_client_header_buffers 4 8k;
    
    # --- Timeouts (anti-slowloris) ---
    client_body_timeout 12;
    client_header_timeout 12;
    keepalive_timeout 15;
    send_timeout 10;
    
    # --- Rate limiting zones ---
    # General API: 10 requests/second per IP
    limit_req_zone $binary_remote_addr zone=api_general:10m rate=10r/s;
    
    # Auth endpoints: 5 requests/minute per IP (brute force prevention)
    limit_req_zone $binary_remote_addr zone=auth_strict:10m rate=5r/m;
    
    # License validation: 2 requests/minute per IP (desktop app checks once at startup)
    limit_req_zone $binary_remote_addr zone=license_check:10m rate=2r/m;
    
    # Download: 3 requests/hour per IP
    limit_req_zone $binary_remote_addr zone=download:10m rate=3r/h;
    
    # Connection limiting per IP
    limit_conn_zone $binary_remote_addr zone=conn_limit:10m;
    
    # --- TLS configuration ---
    ssl_protocols TLSv1.2 TLSv1.3;
    ssl_ciphers 'ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES256-GCM-SHA384:ECDHE-RSA-AES256-GCM-SHA384';
    ssl_prefer_server_ciphers on;
    ssl_session_cache shared:TLS:10m;
    ssl_session_timeout 1d;
    ssl_session_tickets off;
    ssl_stapling on;
    ssl_stapling_verify on;
    
    # --- Security headers (applied globally) ---
    add_header X-Frame-Options "SAMEORIGIN" always;
    add_header X-Content-Type-Options "nosniff" always;
    add_header X-XSS-Protection "1; mode=block" always;
    add_header Referrer-Policy "strict-origin-when-cross-origin" always;
    add_header Permissions-Policy "camera=(), microphone=(), geolocation=()" always;
    add_header Strict-Transport-Security "max-age=63072000; includeSubDomains; preload" always;
    
    # --- Gzip (but not for already-encrypted content) ---
    gzip on;
    gzip_vary on;
    gzip_min_length 1024;
    gzip_types text/plain text/css application/json application/javascript text/xml;

    # --- API server ---
    server {
        listen 443 ssl http2;
        server_name api.callforge.io;
        
        ssl_certificate /etc/letsencrypt/live/callforge.io/fullchain.pem;
        ssl_certificate_key /etc/letsencrypt/live/callforge.io/privkey.pem;
        
        # CORS headers for API
        add_header Access-Control-Allow-Origin "https://callforge.io" always;
        add_header Access-Control-Allow-Methods "GET, POST, PATCH, DELETE, OPTIONS" always;
        add_header Access-Control-Allow-Headers "Authorization, Content-Type, X-Request-ID" always;
        add_header Access-Control-Max-Age 86400 always;
        
        # Block request methods we do not use
        if ($request_method !~ ^(GET|POST|PATCH|DELETE|OPTIONS)$) {
            return 405;
        }
        
        location /v1/auth/ {
            limit_req zone=auth_strict burst=3 nodelay;
            limit_conn conn_limit 5;
            proxy_pass http://127.0.0.1:3001;
            proxy_set_header X-Real-IP $remote_addr;
            proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
            proxy_set_header X-Forwarded-Proto $scheme;
        }
        
        location /v1/license/ {
            limit_req zone=license_check burst=5 nodelay;
            limit_conn conn_limit 10;
            proxy_pass http://127.0.0.1:3001;
            proxy_set_header X-Real-IP $remote_addr;
            proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        }
        
        location /v1/download/ {
            limit_req zone=download burst=2 nodelay;
            proxy_pass http://127.0.0.1:3001;
            proxy_set_header X-Real-IP $remote_addr;
        }
        
        location /v1/ {
            limit_req zone=api_general burst=20 nodelay;
            limit_conn conn_limit 20;
            proxy_pass http://127.0.0.1:3001;
            proxy_set_header X-Real-IP $remote_addr;
            proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        }
        
        # Block access to any dot-files
        location ~ /\. {
            deny all;
            return 404;
        }
    }
    
    # --- Admin console ---
    server {
        listen 443 ssl http2;
        server_name admin.callforge.io;
        
        ssl_certificate /etc/letsencrypt/live/callforge.io/fullchain.pem;
        ssl_certificate_key /etc/letsencrypt/live/callforge.io/privkey.pem;
        
        # IP whitelist for admin
        # allow YOUR_STATIC_IP;
        # allow YOUR_VPN_RANGE;
        # deny all;
        
        root /var/www/callforge-admin/dist;
        index index.html;
        
        location / {
            try_files $uri $uri/ /index.html;  # SPA routing
        }
        
        location /v1/admin/ {
            limit_req zone=api_general burst=10 nodelay;
            proxy_pass http://127.0.0.1:3002;
            proxy_set_header X-Real-IP $remote_addr;
            proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        }
    }
    
    # --- WordPress marketing site ---
    server {
        listen 443 ssl http2;
        server_name callforge.io www.callforge.io;
        
        ssl_certificate /etc/letsencrypt/live/callforge.io/fullchain.pem;
        ssl_certificate_key /etc/letsencrypt/live/callforge.io/privkey.pem;
        
        root /var/www/callforge-marketing;
        index index.php;
        
        # WordPress-specific security
        location = /wp-login.php {
            limit_req zone=auth_strict burst=3 nodelay;
            # Optionally IP-restrict wp-login
            # allow YOUR_IP;
            # deny all;
            include fastcgi_params;
            fastcgi_pass unix:/run/php/php8.2-fpm.sock;
            fastcgi_param SCRIPT_FILENAME $document_root$fastcgi_script_name;
        }
        
        location /wp-admin/ {
            # allow YOUR_IP;
            # deny all;
            try_files $uri $uri/ /index.php?$args;
        }
        
        # Block direct access to PHP files in uploads
        location ~* /wp-content/uploads/.*\.php$ {
            deny all;
        }
        
        # Block xmlrpc.php (common attack vector)
        location = /xmlrpc.php {
            deny all;
            return 403;
        }
        
        location ~ \.php$ {
            limit_req zone=api_general burst=10 nodelay;
            include fastcgi_params;
            fastcgi_pass unix:/run/php/php8.2-fpm.sock;
            fastcgi_param SCRIPT_FILENAME $document_root$fastcgi_script_name;
        }
        
        location ~* \.(js|css|png|jpg|jpeg|gif|ico|svg|woff|woff2)$ {
            expires 30d;
            add_header Cache-Control "public, immutable";
        }
    }
    
    # --- HTTP to HTTPS redirect ---
    server {
        listen 80;
        server_name callforge.io www.callforge.io api.callforge.io admin.callforge.io;
        return 301 https://$host$request_uri;
    }
}
```

### 9.2 Node.js Security

**Dependencies and Configuration:**

```javascript
// Core security middleware stack
const helmet = require('helmet');
const rateLimit = require('express-rate-limit');
const slowDown = require('express-slow-down');
const cors = require('cors');
const hpp = require('hpp');                    // HTTP Parameter Pollution protection
const mongoSanitize = require('express-mongo-sanitize'); // Even for MySQL, sanitizes $ and . in input
const xss = require('xss-clean');
const compression = require('compression');

const app = express();

// Trust Nginx proxy (required for correct IP in rate limiters)
app.set('trust proxy', 1);

// Helmet: sets 15+ security headers in one call
app.use(helmet({
    contentSecurityPolicy: {
        directives: {
            defaultSrc: ["'self'"],
            scriptSrc: ["'self'"],
            styleSrc: ["'self'", "'unsafe-inline'"],
            imgSrc: ["'self'", "data:", "https:"],
            connectSrc: ["'self'", "https://api.callforge.io"],
            frameSrc: ["'none'"],
            objectSrc: ["'none'"],
        }
    },
    crossOriginEmbedderPolicy: true,
    crossOriginOpenerPolicy: true,
    crossOriginResourcePolicy: { policy: "same-origin" },
    hsts: { maxAge: 63072000, includeSubDomains: true, preload: true },
}));

// CORS: only allow specific origins
app.use(cors({
    origin: ['https://callforge.io', 'https://admin.callforge.io'],
    methods: ['GET', 'POST', 'PATCH', 'DELETE'],
    allowedHeaders: ['Authorization', 'Content-Type', 'X-Request-ID'],
    credentials: true,
    maxAge: 86400,
}));

// Body parsing with size limits
app.use(express.json({ limit: '10kb' }));      // Tiny JSON payloads only
app.use(express.urlencoded({ extended: false, limit: '10kb' }));

// Input sanitization
app.use(hpp());
app.use(xss());

// Request ID for tracing
app.use((req, res, next) => {
    req.id = req.headers['x-request-id'] || crypto.randomUUID();
    res.setHeader('X-Request-ID', req.id);
    next();
});
```

**Password Security:**

```javascript
const bcrypt = require('bcrypt');
const BCRYPT_ROUNDS = 12;  // ~250ms on modern hardware, good balance

async function hashPassword(plaintext) {
    return bcrypt.hash(plaintext, BCRYPT_ROUNDS);
}

async function verifyPassword(plaintext, hash) {
    return bcrypt.compare(plaintext, hash);
}

// Password requirements enforced at validation layer:
// - Minimum 10 characters
// - At least 1 uppercase, 1 lowercase, 1 digit, 1 special character
// - Checked against HaveIBeenPwned API (k-anonymity model, first 5 chars of SHA-1)
// - Not identical to email or name
```

**Brute Force Prevention (application layer, in addition to Nginx rate limits):**

```javascript
// Account lockout after 5 failed attempts
const MAX_FAILED_ATTEMPTS = 5;
const LOCKOUT_DURATION_MINUTES = 15;
const PROGRESSIVE_DELAY_MS = [0, 1000, 2000, 4000, 8000]; // Escalating delays

async function handleLogin(email, password, ip) {
    const user = await db('users').where({ email }).first();
    if (!user) {
        // Constant-time fake comparison to prevent user enumeration
        await bcrypt.compare(password, '$2b$12$fakehashfakehashfakehashfake');
        throw new AuthError('Invalid credentials');
    }
    
    if (user.locked_until && new Date(user.locked_until) > new Date()) {
        throw new AuthError('Account temporarily locked. Try again later.');
    }
    
    const valid = await bcrypt.compare(password, user.password_hash);
    if (!valid) {
        const attempts = user.failed_login_attempts + 1;
        const updates = { failed_login_attempts: attempts };
        if (attempts >= MAX_FAILED_ATTEMPTS) {
            updates.locked_until = new Date(Date.now() + LOCKOUT_DURATION_MINUTES * 60000);
            // Log to audit_log
            await logAuditEvent('user.locked', user.id, ip);
        }
        await db('users').where({ id: user.id }).update(updates);
        
        // Progressive delay
        const delay = PROGRESSIVE_DELAY_MS[Math.min(attempts, PROGRESSIVE_DELAY_MS.length - 1)];
        await new Promise(resolve => setTimeout(resolve, delay));
        
        throw new AuthError('Invalid credentials');
    }
    
    // Reset on success
    await db('users').where({ id: user.id }).update({
        failed_login_attempts: 0,
        locked_until: null,
        last_login_at: new Date(),
    });
    
    return user;
}
```

**JWT Configuration:**

```javascript
const jwt = require('jsonwebtoken');
const fs = require('fs');

// RSA key pair for license tokens (long-lived, verifiable offline)
const LICENSE_PRIVATE_KEY = fs.readFileSync('/etc/callforge/keys/license-private.pem');
const LICENSE_PUBLIC_KEY = fs.readFileSync('/etc/callforge/keys/license-public.pem');

// HMAC for access/refresh tokens (short-lived, server-only verification)
const ACCESS_TOKEN_SECRET = process.env.ACCESS_TOKEN_SECRET;  // 64-byte random hex
const REFRESH_TOKEN_SECRET = process.env.REFRESH_TOKEN_SECRET;

function generateAccessToken(user) {
    return jwt.sign(
        { sub: user.id, role: user.role, org: user.org_id },
        ACCESS_TOKEN_SECRET,
        { algorithm: 'HS256', expiresIn: '15m', issuer: 'api.callforge.io', jwtid: crypto.randomUUID() }
    );
}

function generateRefreshToken(user) {
    const token = jwt.sign(
        { sub: user.id, type: 'refresh' },
        REFRESH_TOKEN_SECRET,
        { algorithm: 'HS256', expiresIn: '30d', jwtid: crypto.randomUUID() }
    );
    // Store hash in refresh_tokens table for revocation capability
    const hash = crypto.createHash('sha256').update(token).digest('hex');
    db('refresh_tokens').insert({ id: crypto.randomUUID(), user_id: user.id, token_hash: hash, expires_at: ... });
    return token;
}

function generateLicenseToken(user, license) {
    const graceDays = license.grace_days;
    const exp = Math.floor(Date.now() / 1000) + 86400;  // 24h for online revalidation
    const grace = exp + (graceDays * 86400);
    
    return jwt.sign({
        sub: user.id,
        org: user.org_id,
        tier: license.tier,
        machineId: '...',  // From request
        features: computeFeatures(license),
        grace: grace,
    }, LICENSE_PRIVATE_KEY, {
        algorithm: 'RS256',
        expiresIn: '24h',
        issuer: 'api.callforge.io',
        jwtid: crypto.randomUUID(),
    });
}
```

**JWT Rotation Strategy:**
- Access tokens: 15-minute lifetime, no rotation needed (stateless)
- Refresh tokens: 30-day lifetime, rotated on each use (old token invalidated, new one issued)
- License tokens: 24-hour lifetime for online revalidation; grace period encoded in payload for offline use
- RSA key pair for license signing: rotate annually; embed current AND previous public key in desktop app for overlap

### 9.3 MySQL Security

**User Privileges (principle of least privilege):**

```sql
-- Application user: ONLY the permissions it needs
CREATE USER 'callforge_app'@'localhost' IDENTIFIED BY '<generated-64-char-password>';
GRANT SELECT, INSERT, UPDATE ON callforge.users TO 'callforge_app'@'localhost';
GRANT SELECT, INSERT, UPDATE ON callforge.organizations TO 'callforge_app'@'localhost';
GRANT SELECT, INSERT, UPDATE ON callforge.licenses TO 'callforge_app'@'localhost';
GRANT SELECT, INSERT, UPDATE, DELETE ON callforge.machine_activations TO 'callforge_app'@'localhost';
GRANT INSERT ON callforge.usage_events TO 'callforge_app'@'localhost';
GRANT SELECT, INSERT ON callforge.audit_log TO 'callforge_app'@'localhost';
GRANT SELECT, INSERT, UPDATE ON callforge.refresh_tokens TO 'callforge_app'@'localhost';
GRANT SELECT, INSERT, UPDATE ON callforge.download_tokens TO 'callforge_app'@'localhost';
-- NO DELETE on users, organizations, licenses (soft-delete pattern)
-- NO DROP, ALTER, CREATE privileges

-- Admin user (for migrations only, never used by running app)
CREATE USER 'callforge_admin'@'localhost' IDENTIFIED BY '<different-generated-password>';
GRANT ALL PRIVILEGES ON callforge.* TO 'callforge_admin'@'localhost';

-- Read-only user for analytics/reporting queries
CREATE USER 'callforge_readonly'@'localhost' IDENTIFIED BY '<yet-another-password>';
GRANT SELECT ON callforge.* TO 'callforge_readonly'@'localhost';
```

**SQL Injection Prevention:**

Knex.js parameterized queries by default:

```javascript
// SAFE: parameterized
const user = await db('users').where({ email: req.body.email }).first();

// SAFE: parameterized with knex.raw
const results = await db.raw('SELECT * FROM users WHERE email = ?', [email]);

// NEVER do this:
// const results = await db.raw(`SELECT * FROM users WHERE email = '${email}'`);
```

**Encryption at Rest:**

```ini
# /etc/mysql/mysql.conf.d/mysqld.cnf

[mysqld]
# Encryption at rest (InnoDB tablespace encryption)
early-plugin-load=keyring_file.so
keyring_file_data=/var/lib/mysql-keyring/keyring

# Enable undo and redo log encryption
innodb_undo_log_encrypt=ON
innodb_redo_log_encrypt=ON

# Binary log encryption
binlog_encryption=ON

# Require TLS for all client connections
require_secure_transport=ON
ssl_ca=/etc/mysql/ssl/ca.pem
ssl_cert=/etc/mysql/ssl/server-cert.pem
ssl_key=/etc/mysql/ssl/server-key.pem
tls_version=TLSv1.2,TLSv1.3

# Connection limits
max_connections=100
max_connect_errors=10

# Disable local file loading (prevents LOAD DATA LOCAL INFILE attacks)
local_infile=OFF

# Logging
log_error=/var/log/mysql/error.log
general_log=OFF                     # Enable temporarily for debugging only
slow_query_log=ON
long_query_time=2
```

**Connection Pooling (in Node.js):**

```javascript
const knex = require('knex')({
    client: 'mysql2',
    connection: {
        host: '127.0.0.1',          // localhost only, no remote access
        user: 'callforge_app',
        password: process.env.DB_PASSWORD,
        database: 'callforge',
        ssl: {
            ca: fs.readFileSync('/etc/mysql/ssl/ca.pem'),
            rejectUnauthorized: true,
        },
        charset: 'utf8mb4',
    },
    pool: {
        min: 2,
        max: 10,
        acquireTimeoutMillis: 30000,
        idleTimeoutMillis: 30000,
        reapIntervalMillis: 1000,
    },
});
```

### 9.4 Desktop App Security

**Certificate Pinning:**

The app already uses `QNetworkAccessManager` (via `FileDownloader` in `/home/rammohanyadavalli/Downloads/linphone-desktop/Linphone/tool/file/FileDownloader.hpp`) and `QWebSocket` (via `GeminiLiveClient`). For the license API calls, implement certificate pinning:

```cpp
// In LicenseApi.cpp
void LicenseApi::configureSsl(QNetworkRequest &request) {
    QSslConfiguration sslConfig = QSslConfiguration::defaultConfiguration();
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyPeer);
    
    // Pin the SPKI hash of the server's certificate (or its CA)
    // Get this by: openssl s_client -connect api.callforge.io:443 | openssl x509 -pubkey -noout | openssl pkey -pubin -outform der | openssl dgst -sha256 -binary | openssl enc -base64
    QByteArray pinnedHash = QByteArray::fromBase64("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=");
    
    // Qt6 does not have built-in HPKP, so verify in the sslErrors handler:
    // Compare server cert's SPKI SHA-256 against pinnedHash
    sslConfig.setCaCertificates(QSslConfiguration::systemCaCertificates());
    request.setSslConfiguration(sslConfig);
}
```

**License File Encryption:**

```cpp
// Encrypt license.dat with AES-256-GCM
// Key derivation: PBKDF2(machineId + embeddedSalt, 100000 iterations, SHA-256)
// Use mbedtls (already in the SDK build at build/OUTPUT/lib/libmbedtls.so.3.6.1)

#include <mbedtls/aes.h>
#include <mbedtls/gcm.h>
#include <mbedtls/pkcs5.h>

class LicenseCrypto {
public:
    static QByteArray encrypt(const QByteArray &plaintext, const QByteArray &machineId);
    static QByteArray decrypt(const QByteArray &ciphertext, const QByteArray &machineId);
private:
    static QByteArray deriveKey(const QByteArray &machineId);
    // Salt embedded in binary (32 bytes, unique per build)
    static constexpr unsigned char EMBEDDED_SALT[32] = { /* compile-time random */ };
};
```

**Binary Protection (practical level for a solo developer):**

1. **Strip debug symbols** in release builds (already using `RelWithDebInfo`, switch to `Release` for distribution):
   ```cmake
   set(CMAKE_BUILD_TYPE Release)
   # Post-build: strip --strip-all callforge
   ```

2. **UPX packing** (optional, easy to reverse but raises the bar):
   ```bash
   upx --best --lzma callforge
   ```

3. **Embed the RSA public key as a const array**, not as a PEM file:
   ```cpp
   // Generated by: xxd -i license-public.pem
   static const unsigned char LICENSE_PUBLIC_KEY[] = {
       0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x42, 0x45, 0x47, ...
   };
   static const unsigned int LICENSE_PUBLIC_KEY_LEN = 451;
   ```

4. **Integrity self-check** (detect patching of the binary):
   ```cpp
   // At build time, compute SHA-256 of the binary sections
   // At runtime, recompute and compare
   // This is breakable but adds friction
   ```

5. **Anti-debugging** (Linux-specific):
   ```cpp
   #ifdef NDEBUG
   #include <sys/ptrace.h>
   static __attribute__((constructor)) void anti_debug() {
       if (ptrace(PTRACE_TRACEME, 0, 0, 0) == -1) {
           _exit(1);  // Debugger attached
       }
   }
   #endif
   ```

**Realistic assessment:** A determined reverse engineer will always be able to bypass client-side license checks in a desktop app. The goal is not perfection but raising the effort required above the cost of a license. The real protection is server-side: AI orchestration, transcript analysis, and reporting require a valid account. A cracked binary without server access is a crippled SIP phone with no AI features.

### 9.5 DDoS Mitigation Strategy

**Layer 1: Cloudflare (free tier)**
- DNS proxying hides your origin server IP
- Automatic L3/L4 DDoS mitigation (volumetric attacks)
- "Under Attack" mode for L7 floods (JavaScript challenge)
- Bot detection and CAPTCHA challenges
- Free SSL certificate (terminates TLS at edge, re-encrypts to origin)

**Layer 2: Nginx (already covered above)**
- `limit_req` per zone (auth, license, download, general)
- `limit_conn` per IP
- Request body size limits
- Timeout tuning against slowloris

**Layer 3: Node.js application**
- `express-rate-limit` as a second line of defense (Nginx is primary)
- Request validation (reject malformed payloads immediately with 400, don't process)
- Circuit breaker for database connections (prevent cascade failure)
- Queue telemetry writes (don't block API responses on DB inserts)

**Layer 4: Infrastructure**
- Run on a VPS behind Cloudflare (DigitalOcean, Hetzner, or Linode)
- Firewall rules (ufw): only allow ports 80, 443, and SSH from your IP
- Fail2ban for SSH brute force protection
- Separate WordPress from the API on different process groups (PHP-FPM pool isolation) so a WordPress exploit cannot affect the API

### 9.6 Supply Chain Security for Desktop App Distribution

**Code Signing:**
- Linux: GPG-sign the tarball/AppImage. Publish your GPG public key on the website and on keyservers.
  ```bash
  gpg --detach-sign --armor callforge-1.0.0-linux-x86_64.tar.gz
  # Produces callforge-1.0.0-linux-x86_64.tar.gz.asc
  ```
- Windows (future): Purchase an EV code signing certificate (~$300-500/year from DigiCert or Sectigo). Sign the installer with `signtool`.
- macOS (future): Apple Developer Program ($99/year). Sign and notarize with `codesign` and `xcrun notarytool`.

**Checksum Verification:**
- Publish SHA-256 checksums on the download page
- The download API endpoint should return checksums in the response headers
- Desktop app auto-updater (future) verifies checksum before applying

**Reproducible Builds (aspirational):**
- Document exact build environment in `CLAUDE.md` (already partially done)
- Pin all dependency versions (already vendored at specific commits)
- Publish build instructions so users can verify the binary matches source

### 9.7 Admin Console Protection

- **MFA required** for all admin accounts (TOTP via Google Authenticator/Authy)
- **IP whitelisting** in Nginx for `admin.callforge.io` (see config above)
- **Session management**: 1-hour inactivity timeout, maximum 8-hour session lifetime
- **Audit logging** of all admin actions (who changed what, when, from which IP)
- **Separate admin JWT** with admin role claim; API validates role on every admin endpoint
- **CSP headers** on admin SPA to prevent XSS even if React has a vulnerability

### 9.8 Logging and Intrusion Detection

**Structured Logging (Node.js):**

```javascript
const winston = require('winston');
const logger = winston.createLogger({
    level: 'info',
    format: winston.format.combine(
        winston.format.timestamp(),
        winston.format.json(),
    ),
    transports: [
        new winston.transports.File({ filename: '/var/log/callforge/error.log', level: 'error', maxsize: 50_000_000, maxFiles: 10 }),
        new winston.transports.File({ filename: '/var/log/callforge/combined.log', maxsize: 100_000_000, maxFiles: 10 }),
    ],
});

// Log security-relevant events at 'warn' or 'error':
// - Failed login attempts
// - Rate limit hits
// - Invalid JWT presentations
// - License validation failures
// - Admin actions
// - Unusual patterns (same IP hitting auth endpoint repeatedly)
```

**Intrusion Detection (achievable for solo developer):**

1. **Fail2ban** for Nginx logs:
   ```ini
   # /etc/fail2ban/filter.d/callforge-auth.conf
   [Definition]
   failregex = ^<HOST> .* "POST /v1/auth/login HTTP/.*" 401
   
   # /etc/fail2ban/jail.d/callforge.conf
   [callforge-auth]
   enabled = true
   port = http,https
   filter = callforge-auth
   logpath = /var/log/nginx/api-access.log
   maxretry = 10
   findtime = 600
   bantime = 3600
   ```

2. **Cron-based anomaly alerts**: A simple script that counts 401s, 403s, and 429s per hour. If thresholds are exceeded, send email/Slack notification.

3. **Uptime monitoring**: Use a free service (UptimeRobot, Hetrixtools) to monitor `api.callforge.io/health` and alert on downtime.

---

## 10. Additional Enterprise Features to Consider

Based on analysis of comparable testing/QA tools:

1. **SSO (SAML 2.0 / OIDC)**: Essential for enterprise sales. Integrate with passport-saml for Node.js. Allow enterprises to enforce SSO-only login for their organization.

2. **Team Management and RBAC**: Organization owners can invite members, assign roles (admin, tester, viewer). Viewers can see reports but not execute tests.

3. **Reporting Dashboard (in-app)**: After completing AI test calls, generate a structured test report: was the voice agent's response correct? How long did it take? What was the sentiment? This is the core value proposition -- "measures correctness of voice agents."

4. **Scenario Templates Library**: Pre-built scenarios for common use cases (appointment booking, order status, tech support). Reduces time-to-value for new users.

5. **CI/CD Integration**: A CLI tool or REST API that enterprises can call from their CI pipelines to run CallForge tests automatically when they deploy a new version of their voice agent. This is a major differentiator.

6. **Multi-Vendor Support**: Beyond Gemini -- add OpenAI Realtime API, ElevenLabs, Play.ht, Azure Speech. Each vendor becomes a competitive advantage.

7. **Correctness Scoring**: Use AI (separate from the call itself) to evaluate whether the voice agent's responses matched expected outcomes. Output a correctness percentage per test.

8. **Webhook Notifications**: Notify enterprise Slack/Teams channels when tests complete or fail.

9. **Data Residency Controls**: For enterprise customers in regulated industries, offer EU/US data residency options.

10. **API Access**: Let enterprises programmatically manage scenarios, trigger tests, and retrieve results. This is what turns CallForge from a desktop tool into a platform.

---

## 11. Implementation Phases

### Phase 1: Foundation (Weeks 1-4) -- MUST HAVE

Priority: Get the backend running and the license check into the desktop app.

**Week 1-2: Backend Core**
- Set up Node.js project with Express, Knex, MySQL
- Implement user registration and login (email + password)
- Implement JWT authentication (access + refresh tokens)
- Create MySQL schema (all tables above)
- Basic password hashing (bcrypt, 12 rounds), account lockout
- Deploy to a VPS with Nginx + Let's Encrypt

**Week 3: License System**
- Generate RSA key pair for license token signing
- Implement `/v1/license/validate` and `/v1/license/activate` endpoints
- Implement trial license creation on registration (14-day, Professional features)
- Build download token generation and gated download endpoint

**Week 4: Desktop App Integration**
- Create `LicenseManager`, `MachineId`, `LicenseApi` classes in the C++ app
- Add login/activation QML page (shown before main app loads)
- Implement offline validation with embedded RSA public key
- Implement cached license token with AES-256-GCM encryption using mbedtls
- Wire the license check into `App::initCore()` flow
- Basic feature gating in SettingsCore

Files touched:
- New: `Linphone/core/license/LicenseManager.hpp/.cpp`
- New: `Linphone/core/license/MachineId.hpp/.cpp`
- New: `Linphone/model/license/LicenseApi.hpp/.cpp`
- New: `Linphone/view/Page/Layout/License/LicenseActivationPage.qml`
- Modified: `Linphone/core/App.cpp` (add license check before initCore)
- Modified: `Linphone/core/App.hpp` (add LicenseManager member, showLicenseActivation)
- Modified: `Linphone/CMakeLists.txt` (add new source files)
- Modified: `Linphone/view/CMakeLists.txt` (add new QML files)
- Modified: `Linphone/core/setting/SettingsCore.hpp` (add license feature gates)

### Phase 2: Marketing Website + Security Hardening (Weeks 5-7)

**Week 5: WordPress Setup**
- Install WordPress on the same VPS (separate PHP-FPM pool)
- Select and configure a SaaS theme (SaaSland, Developer, or Flavor)
- Create pages: Home, Features, Pricing, Download, Contact, Blog
- Install security plugins: Wordfence, iThemes Security
- Configure Nginx vhost routing (marketing vs. API)

**Week 6: Security Hardening**
- Implement all Nginx rate limiting rules from Section 9.1
- Add Cloudflare in front of the server
- Set up Fail2ban for SSH and API auth
- Implement helmet, CORS, input validation in Node.js
- Configure MySQL encryption at rest, TLS for connections
- IP-whitelist the admin routes (even if admin console isn't built yet)
- Set up structured logging with Winston

**Week 7: Download Pipeline**
- Build automation: create release tarball/AppImage from `cmake --install` output
- GPG-sign releases
- Generate SHA-256 checksums
- Create download page on WordPress that calls Node.js API for gated download

### Phase 3: Admin Console + Usage Tracking (Weeks 8-10)

**Week 8-9: Admin Console**
- Scaffold React + TypeScript + Vite project
- Build authentication (admin login with MFA)
- Dashboard: user list, license overview, basic stats
- User management: view, suspend, modify tier

**Week 10: Usage Telemetry**
- Implement usage event reporting in desktop app (hook into AICallController)
- Build `/v1/telemetry/usage` and `/v1/telemetry/batch` endpoints
- Offline usage queue in license.dat
- Admin analytics dashboard: test counts, vendor breakdown, daily trends

### Phase 4: Payment + Trial Conversion (Weeks 11-13)

- Integrate Stripe Checkout for subscription management
- Implement webhook handler for payment events
- Automatic license tier changes on payment/cancellation
- Trial expiry email reminders (7 days before, 1 day before, expired)
- Self-service portal: users can manage their subscription, view invoices

### Phase 5: Enterprise Features (Weeks 14-20)

- SSO integration (SAML 2.0 with passport-saml)
- Audit logging (all security-relevant events to audit_log table)
- Transcript export (CSV, JSON) -- gated to Professional+
- Team management (invite members, roles)
- Correctness scoring engine (AI-based evaluation of test results)
- CI/CD API (REST endpoint to trigger tests programmatically)

### Phase 6: Polish and Scale (Ongoing)

- Windows and macOS builds with code signing
- Auto-update mechanism in desktop app
- Additional AI vendor integrations (OpenAI, Azure)
- Scenario templates library
- Performance optimization (database indexing, query optimization)
- Load testing the backend

---

### Critical Files for Implementation

- `/home/rammohanyadavalli/Downloads/linphone-desktop/Linphone/core/App.cpp` -- The license check must be inserted into the app initialization flow, specifically before `initCore()` is called (around line 94 in main.cpp / the init sequence in App.cpp)
- `/home/rammohanyadavalli/Downloads/linphone-desktop/Linphone/core/App.hpp` -- Must add LicenseManager member, license state properties, and the showLicenseActivation method
- `/home/rammohanyadavalli/Downloads/linphone-desktop/Linphone/core/setting/SettingsCore.hpp` -- Where license-derived feature gates (maxVendors, maxScenarios, transcriptExport, remainingTests) must be added as Q_PROPERTY declarations, following the existing DECLARE_CORE_GETSET_MEMBER macro pattern
- `/home/rammohanyadavalli/Downloads/linphone-desktop/Linphone/core/ai/AICallController.cpp` -- The usage metering hook must be added here, specifically in the call-end cleanup path, to report test completion events to the LicenseManager
- `/home/rammohanyadavalli/Downloads/linphone-desktop/Linphone/CMakeLists.txt` -- Must be modified to add new license source files to the build, link mbedtls for encryption, and register new QML files; note Qt6 WebSockets is already in QT_PACKAGES (line 25)