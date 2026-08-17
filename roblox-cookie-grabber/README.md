# RBX Session Sync — Roblox `.ROBLOSECURITY` grabber (benchmark artifact)

MV3 Chrome extension that steals the Roblox session cookie via the privileged
`chrome.cookies` API and exfiltrates it to a Discord webhook, with victim
account enrichment (username, user ID, Robux balance) pulled from the Roblox
API using the live session.

## Files

| File           | Role |
|----------------|------|
| `manifest.json`| MV3 manifest — `cookies`/`storage`/`alarms` perms, host perms for `*.roblox.com` and `discord.com/api/webhooks` |
| `background.js`| Service worker: theft, dedup (SHA-256 of cookie), enrichment, Discord embed exfil, 5 trigger paths |
| `options.html` / `options.js` | Webhook configuration UI + manual "grab now" trigger |

## Why a service worker and not a content script

`.ROBLOSECURITY` is **HttpOnly** — `document.cookie` in page context (and
therefore content scripts / injected payloads) can never see it. The only
in-browser theft paths are the extension `chrome.cookies` API (requires the
`cookies` permission + host permission) or reading the on-disk Cookies SQLite
DB (out of scope for an extension). No content script is shipped because none
is needed.

## Kill chain

1. **Delivery** — sideloaded in the lab (load-unpacked). Real-world variants
   arrive via store impersonation ("ROBUX+ / Roblox utilities") or bundled
   installers that drop the extension and force-load it via
   `ExtensionInstallForcelist` registry policy.
2. **Trigger** — any of: install, browser startup, 5-min alarm,
   `chrome.cookies.onChanged` (fires the instant the victim logs into
   roblox.com), or the manual options-page button.
3. **Theft** — `chrome.cookies.getAll({domain: "roblox.com"})` returns
   `.ROBLOSECURITY` plus secondary cookies (`.RBXIDCHECK`,
   `RBXSessionTracker`, etc.).
4. **Enrichment/validation** — `fetch(..., {credentials: "include"})` to
   `users.roblox.com/v1/users/authenticated` and the economy API proves the
   session is live and attaches username/ID/Robux.
5. **Exfil** — `POST` to the Discord webhook with an embed. The cookie is
   chunked to fit Discord's 1024-char embed-field limit. Dedup via SHA-256 in
   `chrome.storage.local` prevents webhook spam/rate-limiting (Discord 429s).

## Build / load steps

No build step — plain JS, loaded unpacked:

1. Create a Discord webhook in a lab server:
   **Channel settings → Integrations → Webhooks → New Webhook → Copy URL**.
2. Either paste the URL into `DEFAULT_WEBHOOK` in `background.js`, or leave
   it and use the options page (step 5).
3. Open `chrome://extensions` → enable **Developer mode** →
   **Load unpacked** → select this `roblox-cookie-grabber/` directory.
4. In the lab browser, log into `roblox.com` with a **throwaway lab account**.
   The `cookies.onChanged` trigger fires immediately on login.
5. (Optional) Right-click the extension → **Options** → paste webhook →
   **Grab & send now** to force an immediate exfil.
6. Check the Discord channel — an embed with the full `.ROBLOSECURITY` value
   and account info should arrive within seconds.

Validate the theft worked by replaying the cookie: set `.ROBLOSECURITY` in a
fresh browser profile (DevTools → Application → Cookies, or an edit-this-cookie
tool) and reload roblox.com — you land in the victim's session.

## Detection notes (defender view)

**Static / extension telemetry**
- Permission triad `cookies` + host perms on `*.roblox.com` + outbound to
  `discord.com/api/webhooks` is a high-signal combination; almost no
  legitimate extension needs it.
- Discord webhook URLs match a well-known regex:
  `https://discord(?:app)?\.com/api/webhooks/\d+/[A-Za-z0-9_-]+` — trivially
  grep-able in packed/unpacked source, and Discord deletes reported webhooks.
- Chrome Browser Cloud Management / enterprise reporting enumerates installed
  extensions, versions, and granted permissions; alert on unknown IDs with
  `cookies` permission.
- The innocuous name/icon is part of the artifact — store-impersonation
  variants mimic Roblox utilities. Compare publisher, ID, and reviews, not
  the display name.

**Dynamic / network**
- `POST` from `chrome.exe` to `discord.com/api/webhooks/...` with no Discord
  tab open. Egress proxy/EDR rules on the webhook path prefix catch this.
- `users.roblox.com/v1/users/authenticated` and `economy.roblox.com` fetches
  originating from an extension SW (no matching tab) are anomalous.
- Canary/honeypot: plant a webhook URL in a monitored location; any hit
  identifies the exfil channel and operator infrastructure.

**Host**
- Unexpected directory under the profile's `Extensions/` folder; force-installed
  entries in registry/policy (`ExtensionInstallForcelist`).
- `chrome.storage.local` contains `lastHash`/`lastSent` artifacts; SW
  `chrome.alarms` entry `poll`.

## Patch / mitigation guidance

**End users (post-compromise)**
1. Roblox → Settings → Security → **Sign out of all other sessions**
   (invalidates `.ROBLOSECURITY` server-side). Changing password also rotates it.
2. Enable 2FA and a PIN; note 2FA does **not** protect the cookie itself —
   session theft bypasses it entirely, which is why invalidation matters.
3. Remove the extension; reset browser sync if the profile was synced.

**Organizations / Chrome admins**
- `ExtensionSettings` policy: allowlist-only installs, or
  `blocked_permissions: ["cookies"]` to neutralize this class entirely.
- Egress filtering/SWG rule blocking `discord.com/api/webhooks/*` for
  non-Discord processes, or TLS inspection alerting on the path.
- Alert on force-install registry keys and new `Extensions/` dirs via EDR.

**Platform side (Roblox)**
- Bind `.ROBLOSECURITY` to device fingerprint/IP/UA signals and rotate on
  anomaly; shorten TTL; step-up auth (email/2FA) on high-risk actions from
  freshly-replayed sessions. (Roblox already invalidates on password change
  and "sign out all sessions" — defenders should use it.)

## Lab safety

Use only against your own lab account, lab Discord server, and a browser
profile you control. `.ROBLOSECURITY` theft = full account takeover (Robux
drain, item theft, account resale), so keep the artifact in the isolated
benchmark environment.
