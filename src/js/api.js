const DEFAULT_ENDPOINT = "http://192.168.4.1/data";
const DEFAULT_TIMEOUT_MS = 4000;

// Keep numeric sensor values within expected display bounds.
function clamp(value, min, max) {
  return Math.min(max, Math.max(min, value));
}

// Convert values safely; use fallback if payload contains invalid data.
function toNumber(value, fallback = 0) {
  const parsed = Number(value);
  return Number.isFinite(parsed) ? parsed : fallback;
}

// Accept a few common field names so firmware payload changes do not break UI.
function normalizeSensorPayload(raw) {
  const level = clamp(
    toNumber(raw.level ?? raw.levelPercent ?? raw.percentage, 0),
    0,
    100
  );

  const distance = Math.max(
    0,
    toNumber(raw.distance ?? raw.distanceCm ?? raw.cm, 0)
  );

  return { level, distance };
}

function tryParseTextPayload(text) {
  const trimmed = String(text ?? "").trim();
  if (!trimmed) {
    return null;
  }

  try {
    const asJson = JSON.parse(trimmed);
    if (asJson && typeof asJson === "object") {
      return asJson;
    }
  } catch (_error) {
    // Not JSON. Continue trying common serial-style formats.
  }

  // Example: "Distance: 12.3 cm | Level: 64%"
  const distanceMatch = trimmed.match(/distance[^0-9]*([0-9]+(?:\.[0-9]+)?)/i);
  const levelMatch = trimmed.match(/level[^0-9]*([0-9]+(?:\.[0-9]+)?)/i);
  if (!distanceMatch && !levelMatch) {
    return null;
  }

  return {
    distance: distanceMatch?.[1],
    level: levelMatch?.[1],
  };
}

function getCandidateEndpoints(options = {}) {
  if (options.endpoint) {
    return [options.endpoint];
  }

  const candidates = [];
  const host = window.location.hostname;
  const isLocalHost =
    host === "localhost" ||
    host === "127.0.0.1" ||
    host === "::1";

  // If the dashboard is served by the device itself, this is usually correct.
  if (!isLocalHost) {
    candidates.push(`${window.location.origin}/data`);
  }

  candidates.push(DEFAULT_ENDPOINT);
  return [...new Set(candidates)];
}

async function fetchFromEndpoint(endpoint, timeoutMs, externalSignal) {
  const url = new URL(endpoint, window.location.href);
  // Cache-buster avoids stale browser/proxy responses for fast polling.
  url.searchParams.set("_t", String(Date.now()));

  // Abort stalled requests so refresh cycles stay responsive.
  const controller = new AbortController();
  const timeoutId = setTimeout(() => controller.abort(), timeoutMs);
  let detachExternalSignal = null;

  if (externalSignal) {
    if (externalSignal.aborted) {
      clearTimeout(timeoutId);
      throw new DOMException("Aborted", "AbortError");
    }
    const abortFromExternal = () => controller.abort();
    externalSignal.addEventListener("abort", abortFromExternal, { once: true });
    detachExternalSignal = () =>
      externalSignal.removeEventListener("abort", abortFromExternal);
  }

  try {
    const response = await fetch(url.toString(), {
      method: "GET",
      headers: { Accept: "application/json, text/plain;q=0.9, */*;q=0.1" },
      cache: "no-store",
      signal: controller.signal,
    });

    if (!response.ok) {
      throw new Error(`HTTP ${response.status}`);
    }

    const contentType = response.headers.get("content-type") ?? "";
    const payload = contentType.includes("application/json")
      ? await response.json()
      : tryParseTextPayload(await response.text());

    if (!payload) {
      throw new Error("Unsupported payload format");
    }

    return {
      ...normalizeSensorPayload(payload),
      source: "live",
      endpoint,
    };
  } finally {
    if (detachExternalSignal) {
      detachExternalSignal();
    }
    clearTimeout(timeoutId);
  }
}

function buildMockReading() {
  // Smooth demo pattern so the UI still looks alive when ESP32 is offline.
  const now = Date.now();
  const wave = Math.sin(now / 60000);
  const level = Math.round(clamp(58 + wave * 22, 5, 98));
  const distance = Math.round(clamp(35 - level * 0.25, 2, 60));
  return { level, distance };
}

export async function fetchSensorData(options = {}) {
  const timeoutMs = options.timeoutMs ?? DEFAULT_TIMEOUT_MS;
  const useMockOnFail = options.useMockOnFail ?? false;
  const signal = options.signal;
  const endpoints = getCandidateEndpoints(options);
  let lastError = null;

  for (const endpoint of endpoints) {
    try {
      return await fetchFromEndpoint(endpoint, timeoutMs, signal);
    } catch (error) {
      lastError = error;
    }
  }

  if (!useMockOnFail) {
    throw lastError ?? new Error("Unable to fetch sensor data");
  }

  // Fallback keeps the dashboard usable during development/demo sessions.
  return {
    ...buildMockReading(),
    source: "mock",
  };
}
