import { fetchSensorData } from "./api.js";

const REFRESH_INTERVAL_MS = 1000;

// Cache DOM references once to avoid repeated lookups every refresh.
const levelEl = document.getElementById("level");
const distanceEl = document.getElementById("distance");
const statusChipEl = document.getElementById("statusChip");
const barFillEl = document.getElementById("barFill");
const updatedEl = document.getElementById("updated");
const refreshBtn = document.getElementById("refreshBtn");
const connectionLabelEl = document.getElementById("connectionLabel");
const statusDotEl = document.getElementById("statusDot");
const historyListEl = document.getElementById("historyList");
let pollTimerId = null;
let activeRequestController = null;
const recentReadings = [];

function getWaterState(level) {
  // UI thresholds for quick at-a-glance status.
  if (level < 30) return { label: "Low Level", className: "low" };
  if (level > 80) return { label: "High Level", className: "high" };
  return { label: "Normal", className: "normal" };
}

function setConnectionState(state, source = "live") {
  // Exactly one badge state class should be active at a time.
  statusDotEl.classList.remove("online", "offline", "mock");

  if (state === "online" && source === "mock") {
    statusDotEl.classList.add("mock");
    connectionLabelEl.textContent = "Demo Mode";
    return;
  }

  if (state === "online") {
    statusDotEl.classList.add("online");
    connectionLabelEl.textContent = "Connected";
    return;
  }

  statusDotEl.classList.add("offline");
  connectionLabelEl.textContent = "Offline";
}

function renderReading(reading) {
  // Keep display aligned with firmware values shown on OLED.
  const rawLevel = Number(reading.level);
  const level = Number.isFinite(rawLevel)
    ? Math.min(100, Math.max(0, Math.round(rawLevel)))
    : 0;
  const rawDistance = Number(reading.distance);
  const distance = Number.isFinite(rawDistance) ? Math.max(0, rawDistance) : 0;
  const waterState = getWaterState(level);
  const visibleFill = level === 0 ? 4 : level;

  levelEl.textContent = String(level);
  distanceEl.textContent = distance.toFixed(2);
  // Keep a tiny visible fill even at 0% so the bar never looks "missing".
  barFillEl.style.width = `${visibleFill}%`;

  statusChipEl.textContent = waterState.label;
  statusChipEl.classList.remove("low", "normal", "high");
  statusChipEl.classList.add(waterState.className);

  updatedEl.textContent = new Date().toLocaleTimeString();
  updateHistory(level);
}

function updateHistory(level) {
  const time = new Date().toLocaleTimeString();
  recentReadings.unshift({ level, time });
  if (recentReadings.length > 5) {
    recentReadings.length = 5;
  }

  historyListEl.innerHTML = recentReadings
    .map(
      (entry) => `
        <li class="historyItem">
          <span>${entry.level}% water level</span>
          <span class="metaText">${entry.time}</span>
        </li>
      `,
    )
    .join("");
}

async function refreshReading() {
  if (activeRequestController) {
    activeRequestController.abort();
  }

  activeRequestController = new AbortController();
  const requestId = ++refreshReading.requestCounter;

  try {
    const reading = await fetchSensorData({
      useMockOnFail: false,
      timeoutMs: 2500,
      signal: activeRequestController.signal,
    });
    // Ignore late responses so older data cannot overwrite fresher readings.
    if (requestId !== refreshReading.requestCounter) {
      return;
    }
    renderReading(reading);
    setConnectionState("online", reading.source);
  } catch (error) {
    if (requestId !== refreshReading.requestCounter) {
      return;
    }
    if (error?.name === "AbortError") {
      return;
    }
    setConnectionState("offline");
    statusChipEl.textContent = "No Data";
  } finally {
    if (requestId === refreshReading.requestCounter) {
      activeRequestController = null;
    }
  }
}
refreshReading.requestCounter = 0;

refreshBtn.addEventListener("click", () => {
  refreshReading();
});

function scheduleNextPoll() {
  clearTimeout(pollTimerId);
  pollTimerId = setTimeout(async () => {
    await refreshReading();
    scheduleNextPoll();
  }, REFRESH_INTERVAL_MS);
}

// Initial load + periodic polling for near-live updates.
refreshReading().finally(scheduleNextPoll);
