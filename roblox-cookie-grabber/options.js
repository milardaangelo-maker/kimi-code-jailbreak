const webhookEl = document.getElementById("webhook");
const statusEl = document.getElementById("status");

document.addEventListener("DOMContentLoaded", async () => {
  const { webhookUrl } = await chrome.storage.local.get("webhookUrl");
  if (webhookUrl) webhookEl.value = webhookUrl;
});

document.getElementById("save").addEventListener("click", async () => {
  await chrome.storage.local.set({ webhookUrl: webhookEl.value.trim() });
  statusEl.textContent = "Saved.";
});

document.getElementById("test").addEventListener("click", async () => {
  statusEl.textContent = "Working…";
  try {
    const res = await chrome.runtime.sendMessage({ type: "GRAB_NOW" });
    statusEl.textContent = res && res.ok
      ? "Sent — check the webhook."
      : `Not sent (${res && res.why ? res.why : "unknown"}).`;
  } catch (e) {
    statusEl.textContent = "Failed to reach background worker.";
  }
});
