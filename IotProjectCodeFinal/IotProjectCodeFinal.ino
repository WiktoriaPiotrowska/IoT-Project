#include <Wire.h> //The ESP32 communicates with the I2C devics such as OLED display
#include <Adafruit_GFX.h> //Graphic Lib for OLED display
#include <Adafruit_SSD1306.h> //For my OLED display size
#include <WiFi.h> //connects with hotspot
#include <WebServer.h> //web server for the dashboad

#define TRIG_PIN 5
#define ECHO_PIN 18
#define BUZZER_PIN 13

//OLED screen setup
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire);

//Web server runs on the normal HTTP port
WebServer server(80);

//Main dashboard page stored on the ESP32
const char INDEX_HTML[] PROGMEM = R"__INDEXHTML__(<!doctype html> 
<html lang="en">
<head>
  <!-- Basic webpage setup.
       UTF-8 allows normal characters to display correctly.
       The viewport tag makes the dashboard fit properly on a phone screen. -->
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">

  <!-- Browser tab title. -->
  <title>Water Level Monitor</title>

  <!-- The CSS file is served by the ESP32 using the /styles/style.css route. -->
  <link rel="stylesheet" href="/styles/style.css">
</head>
<body>
  <!-- Main dashboard wrapper.
       Everything visible on the webpage is inside this container. -->
  <div class="dashboard">

    <!-- Header section.
         Shows the project name and the live connection status.
         The JavaScript updates the badge depending on whether /data responds. -->
    <header class="header">
      <div>
        <h1>Water Level Monitor</h1>
        <p>ESP32 • Live Sensor</p>
      </div>

      <!-- Connection badge.
           The dot and label change between Connected and Offline. -->
      <div class="connectionBadge">
        <span class="statusDot" id="statusDot"></span>
        <span id="connectionLabel">Connecting...</span>
      </div>
    </header> 

    <!-- Live reading card.
         This is the main display area for the current water level. -->
    <main class="card">

      <!-- Main reading row.
           The large percentage, status chip and measured distance are displayed here. -->
      <div class="readingRow">

        <!-- Large water level percentage.
             JavaScript updates the span with id="level" using the JSON data from /data. -->
        <div class="levelDisplay">
          <span id="level">--</span><span class="unit">%</span>
        </div>

        <!-- Sensor details section.
             The status chip shows Low Level, Normal or High Level.
             The distance value shows the raw ultrasonic distance in centimetres. -->
        <div class="sensorDetails">
          <div class="statusChip" id="statusChip">--</div>
          <div class="metaText">Distance: <span id="distance">--</span> cm</div>
        </div>
      </div>

      <!-- Visual water level bar.
           The width of barFill is changed by JavaScript to match the live percentage. -->
      <div class="levelBar">
        <div class="levelBarFill" id="barFill" style="width:42%"></div>
      </div> 

      <!-- Refresh button and last updated time.
           The page refreshes automatically, but this button lets the user request a reading manually. -->
      <div class="actionsRow">
        <button class="btn" id="refreshBtn" type="button">Refresh</button>
        <div class="metaText">Last updated: <span id="updated">--:--:--</span></div>
      </div>
       
      <!-- Data source controls.
           By default this points to the ESP32 /data route.
           It is useful for testing because the user can change the endpoint if needed. -->
      <div class="sourceRow">
        <label class="controlLabel" for="endpointInput">Data Source</label>
        <input
          id="endpointInput"
          class="endpointInput"
          type="text"
          placeholder="127.0.0.1:8765/data or 192.168.4.1/data"
          spellcheck="false"
          autocomplete="off"
        >
        <button class="btn" id="applyEndpointBtn" type="button">Set Source</button>
        <div class="metaText">Active: <span id="activeEndpoint">--</span></div>
      </div>
    </main>
      
    <!-- Alert section.
         The user can choose their own low and high water level limits.
         When a limit is reached, the banner changes and the event is stored in the alert history. -->
    <section class="card alertsCard">
      <div class="sectionHeader">
        <h2>Alerts</h2>
        <span class="metaText" id="alertStateText">No active alert</span>
      </div>

      <!-- Alert banner.
           JavaScript changes this between normal, low alert and high alert. -->
      <div class="alertBanner alertNormal" id="alertBanner">System Normal</div>
        
      <!-- Alert controls.
           Default low threshold is 25%.
           Default high threshold is 85%.
           The values are saved in localStorage so they stay after refreshing the page. -->
      <div class="alertControls">
        <label class="controlLabel" for="lowThresholdInput">Low %</label>
        <input id="lowThresholdInput" class="thresholdInput" type="number" min="0" max="100" step="1" value="25">

        <label class="controlLabel" for="highThresholdInput">High %</label>
        <input id="highThresholdInput" class="thresholdInput" type="number" min="0" max="100" step="1" value="85">

        <button class="btn" id="applyAlertBtn" type="button">Apply</button>
        <button class="btn" id="notifyBtn" type="button">Enable Notifications</button>
      </div>
       
      <!-- Alert history.
           Stores the latest alert messages so the user can see when a warning happened. -->
      <ul class="historyList" id="alertHistoryList">
        <li class="historyItem">
          <span>No alerts yet.</span>
          <span class="metaText">--</span>
        </li>
      </ul>
    </section>
    
    <!-- Recent readings section.
         Stores the latest 5 readings at 10 minute intervals.
         This gives a simple history without filling the page with readings every second. -->
    <section class="card historyCard">
      <div class="sectionHeader">
        <h2>Recent Readings</h2>
        <span class="metaText">Latest 5 (10 min interval)</span>
      </div>

      <ul class="historyList" id="historyList">
        <li class="historyItem">
          <span>Waiting for data...</span>
          <span class="metaText">--</span>
        </li>
      </ul>
    </section>
  </div>

  <script>
    //These variables connect the JavaScript to the HTML elements on the page
    //The IDs are used so the dashboard can update live without reloading the browser
    const levelEl = document.getElementById("level");
    const distanceEl = document.getElementById("distance");
    const statusChipEl = document.getElementById("statusChip");
    const barFillEl = document.getElementById("barFill");
    const updatedEl = document.getElementById("updated");
    const refreshBtn = document.getElementById("refreshBtn");
    const connectionLabelEl = document.getElementById("connectionLabel");
    const statusDotEl = document.getElementById("statusDot");
    const historyListEl = document.getElementById("historyList");
    const endpointInputEl = document.getElementById("endpointInput");
    const applyEndpointBtnEl = document.getElementById("applyEndpointBtn");
    const activeEndpointEl = document.getElementById("activeEndpoint");
    const lowThresholdInputEl = document.getElementById("lowThresholdInput");
    const highThresholdInputEl = document.getElementById("highThresholdInput");
    const applyAlertBtnEl = document.getElementById("applyAlertBtn");
    const notifyBtnEl = document.getElementById("notifyBtn");
    const alertStateTextEl = document.getElementById("alertStateText");
    const alertBannerEl = document.getElementById("alertBanner");
    const alertHistoryListEl = document.getElementById("alertHistoryList");

    //Timing values for the webpage
    //The live reading updates every second
    //The recent readings section only saves one reading every 10 minutes
    const REFRESH_INTERVAL_MS = 1000;
    const HISTORY_INTERVAL_MS = 10 * 60 * 1000;

    //The default data source is the /data route on the same ESP32
    //The dashboard loads from the ESP32 and then asks /data for live JSON readings
    let selectedEndpoint = `${window.location.origin}/data`;

    // Used to stop older slower requests from overwriting newer readings.
    let latestRequestId = 0;

    //Stores recent readings shown in the Recent Readings section
    const recentReadings = [];
    let lastHistorySampleAt = 0;

    //Alert settings are saved in the browser
    //This means the chosen low and high limits stay saved after refreshing the page
    const ALERT_LOW_STORAGE_KEY = "water-monitor-alert-low";
    const ALERT_HIGH_STORAGE_KEY = "water-monitor-alert-high";
    const alertHistory = [];
    let currentAlertState = "normal";
    let notificationsEnabled = false;
    let notificationMode = "off";
    let audioContext = null;
    let lastRenderedLevel = null;

    function fixEndpoint(value) {
      //Cleans up the data source entered by the user
      //The user can type a full URL, an IP address, or just /data
      const trimmed = String(value ?? "").trim();
      if (!trimmed) return null;
      if (/^https?:\/\//i.test(trimmed)) return trimmed;
      if (trimmed.startsWith("/")) return `${window.location.origin}${trimmed}`;
      return `http://${trimmed.replace(/^\/+/, "")}`;
    }

    function getWaterState(level) {
      //Converts the water percentage into a simple dashboard status
      //This controls the status chip beside the reading
      if (level < 30) return { label: "Low Level", className: "low" };
      if (level > 80) return { label: "High Level", className: "high" };
      return { label: "Normal", className: "normal" };
    }

    function setConnectionState(state) {
      //Updates the connection badge at the top of the page
      //If /data responds, the dashboard shows Connected
      //If /data fails, the dashboard shows Offline
      statusDotEl.classList.remove("online", "offline");

      if (state === "online") {
        statusDotEl.classList.add("online");
        connectionLabelEl.textContent = "Connected";
      } else {
        statusDotEl.classList.add("offline");
        connectionLabelEl.textContent = "Offline";
      }
    }

    function updateEndpointText() {
      //Keeps the data source input and the active endpoint label matching
      endpointInputEl.value = selectedEndpoint;
      activeEndpointEl.textContent = selectedEndpoint;
    }

    function clampInt(value, min, max, fallback) {
      //Converts a value into an integer and keeps it inside a safe range
      //This prevents invalid percentages such as -10 or 200
      const parsed = Number.parseInt(String(value ?? ""), 10);
      if (!Number.isFinite(parsed)) return fallback;
      return Math.min(max, Math.max(min, parsed));
    }

    function getThresholds() {
      //Reads saved alert limits from localStorage
      //If no saved values exist, the default input values are used
      const lowDefault = clampInt(lowThresholdInputEl.value, 0, 100, 25);
      const highDefault = clampInt(highThresholdInputEl.value, 0, 100, 85);

      try {
        const savedLow = window.localStorage.getItem(ALERT_LOW_STORAGE_KEY);
        const savedHigh = window.localStorage.getItem(ALERT_HIGH_STORAGE_KEY);

        return {
          low: clampInt(savedLow ?? lowDefault, 0, 100, 25),
          high: clampInt(savedHigh ?? highDefault, 0, 100, 85),
        };
      } catch (_error) {
        return { low: lowDefault, high: highDefault };
      }
    }

    function saveThresholds(low, high) {
      //Saves the alert limits and prevents invalid settings
      //The low threshold must always be lower than the high threshold
      let fixedLow = clampInt(low, 0, 100, 25);
      let fixedHigh = clampInt(high, 0, 100, 85);

      if (fixedLow >= fixedHigh) {
        if (fixedLow >= 100) {
          fixedLow = 99;
          fixedHigh = 100;
        } else {
          fixedHigh = fixedLow + 1;
        }
      }

      try {
        window.localStorage.setItem(ALERT_LOW_STORAGE_KEY, String(fixedLow));
        window.localStorage.setItem(ALERT_HIGH_STORAGE_KEY, String(fixedHigh));
      } catch (_error) {
        //If the browser blocks localStorage, the dashboard still works
        //The alert values just will not be saved after refresh
      }

      lowThresholdInputEl.value = String(fixedLow);
      highThresholdInputEl.value = String(fixedHigh);

      return { low: fixedLow, high: fixedHigh };
    }

    function isLocalPage(hostname) {
      //Checks if the page is running on a local address
      //This matters because browser notifications are restricted on normal HTTP pages
      return hostname === "localhost" || hostname === "127.0.0.1" || hostname === "::1";
    }

    function notificationsAvailable() {
      //Checks whether proper browser notifications can be used
      //Notifications normally require HTTPS, localhost, or a secure browser context
      if (typeof window.Notification === "undefined") return false;
      if (window.isSecureContext) return true;
      return isLocalPage(window.location.hostname);
    }

    function getAudioContext() {
      // Creates an audio context for the fallback beep alert.
      // This is used if browser notifications are not available.
      if (audioContext) return audioContext;

      const AudioContextImpl = window.AudioContext || window.webkitAudioContext;
      if (!AudioContextImpl) return null;

      try {
        audioContext = new AudioContextImpl();
        return audioContext;
      } catch (_error) {
        return null;
      }
    }

    function playBeep() {
      //Plays a short beep using the browser WebAudio API
      //This acts as a fallback alert method if notifications cannot be used
      const ctx = getAudioContext();
      if (!ctx) return;

      const osc = ctx.createOscillator();
      const gain = ctx.createGain();

      osc.type = "sine";
      osc.frequency.value = 880;
      gain.gain.value = 0.0001;

      osc.connect(gain);
      gain.connect(ctx.destination);

      const now = ctx.currentTime;
      gain.gain.setValueAtTime(0.0001, now);
      gain.gain.exponentialRampToValueAtTime(0.08, now + 0.02);
      gain.gain.exponentialRampToValueAtTime(0.0001, now + 0.22);

      osc.start(now);
      osc.stop(now + 0.25);
    }

    function getAlertState(level, thresholds) {
      //Compares the current water level with the saved alert limits
      //It returns low, high or normal
      if (level <= thresholds.low) return "low";
      if (level >= thresholds.high) return "high";
      return "normal";
    }

    function showAlertState(state) {
      //Updates the alert banner on the dashboard
      //The class changes the colour and the text explains the current state
      alertBannerEl.classList.remove("alertNormal", "alertLow", "alertHigh");

      if (state === "low") {
        alertBannerEl.classList.add("alertLow");
        alertBannerEl.textContent = "Alert: Water level too low";
        alertStateTextEl.textContent = "Low water level";
        return;
      }

      if (state === "high") {
        alertBannerEl.classList.add("alertHigh");
        alertBannerEl.textContent = "Alert: Water level too high";
        alertStateTextEl.textContent = "High water level";
        return;
      }

      alertBannerEl.classList.add("alertNormal");
      alertBannerEl.textContent = "System Normal";
      alertStateTextEl.textContent = "No active alert";
    }

    function showAlertHistory() {
      //Displays the latest alert messages under the alert banner
      //If no alerts have happened, it shows a default message
      if (alertHistory.length === 0) {
        alertHistoryListEl.innerHTML = `
          <li class="historyItem">
            <span>No alerts yet.</span>
            <span class="metaText">--</span>
          </li>
        `;
        return;
      }

      alertHistoryListEl.innerHTML = alertHistory
        .map(
          (item) => `
            <li class="historyItem">
              <span>${item.message}</span>
              <span class="metaText">${item.time}</span>
            </li>
          `,
        )
        .join("");
    }

    function addAlertHistory(entry) {
      //Adds a new alert message to the top of the history list
      //Only the latest five alert messages are kept
      if (entry) {
        alertHistory.unshift(entry);
      }

      if (alertHistory.length > 5) {
        alertHistory.length = 5;
      }

      showAlertHistory();
    }

    function sendAlert(message) {
      //Sends an alert to the user
      //It tries browser notifications first
      //If that is not possible, it falls back to vibration and a beep
      if (!notificationsEnabled) return;

      if (notificationMode === "system" && notificationsAvailable()) {
        if (window.Notification.permission !== "granted") return;

        try {
          new window.Notification("Water Level Monitor", { body: message });
          return;
        } catch (_error) {
          //If notifications fail, the fallback alert methods below are used
        }
      }

      try {
        window.navigator?.vibrate?.([200, 120, 200]);
      } catch (_error) {
        //Some browsers do not allow vibration
      }

      playBeep();
    }

    function checkAlerts(level, timestampMs) {
      //Checks whether the water level has crossed the alert limits
      //It only adds a new alert when the state changes
      //This stops the same warning being added every second
      const thresholds = getThresholds();
      const newState = getAlertState(level, thresholds);

      showAlertState(newState);

      if (newState === currentAlertState) return;

      const time = new Date(timestampMs).toLocaleTimeString();

      if (newState === "low") {
        addAlertHistory({ message: `Water level too low (≤ ${thresholds.low}%)`, time });
        sendAlert(`Water level too low (≤ ${thresholds.low}%)`);
      } else if (newState === "high") {
        addAlertHistory({ message: `Water level too high (≥ ${thresholds.high}%)`, time });
        sendAlert(`Water level too high (≥ ${thresholds.high}%)`);
      } else if (currentAlertState !== "normal") {
        addAlertHistory({ message: "Alert cleared", time });
      }

      currentAlertState = newState;
    }

    function showReading(payload) {
      //Takes the JSON response from the ESP32 and updates the dashboard
      //The values are cleaned first so impossible values are not displayed
      const rawLevel = Number(payload?.level);
      const level = Number.isFinite(rawLevel) ? Math.min(100, Math.max(0, Math.round(rawLevel))) : 0;

      const rawDistance = Number(payload?.distance);
      const distance = Number.isFinite(rawDistance) ? Math.max(0, rawDistance) : 0;

      const state = getWaterState(level);

      //Keeps the progress bar slightly visible even when the level is 0%
      const visibleFill = level === 0 ? 4 : level;

      lastRenderedLevel = level;

      //Updates the main percentage, the distance text and the progress bar
      levelEl.textContent = String(level);
      distanceEl.textContent = distance.toFixed(2);
      barFillEl.style.width = `${visibleFill}%`;

      //Updates the coloured status chip
      statusChipEl.textContent = state.label;
      statusChipEl.classList.remove("low", "normal", "high");
      statusChipEl.classList.add(state.className);

      //Updates the time, recent readings and alert system
      const nowMs = Date.now();
      updatedEl.textContent = new Date(nowMs).toLocaleTimeString();

      updateHistory(level, nowMs);
      checkAlerts(level, nowMs);
    }

    function updateHistory(level, timestampMs) {
      //The main dashboard updates every second
      //The recent readings list only saves a reading every 10 minutes
      const shouldSave =
        recentReadings.length === 0 ||
        timestampMs - lastHistorySampleAt >= HISTORY_INTERVAL_MS;

      if (!shouldSave) return;

      lastHistorySampleAt = timestampMs;

      const time = new Date(timestampMs).toLocaleTimeString();
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

    async function refresh() {
      //Main live update function, It requests the newest JSON reading from the ESP32 /data route and then updates the dashboard with that data
      const requestId = ++latestRequestId;

      try {
        const url = new URL(selectedEndpoint, window.location.href);

        //Adds a timestamp to the request UR
        //This helps stop the browser from reusing an old cached response
        url.searchParams.set("_t", String(Date.now()));

        //fetch() asks the ESP32 for the latest data
        //cache: "no-store" tells the browser not to use saved data
        const res = await fetch(url.toString(), { cache: "no-store" });

        if (!res.ok) throw new Error(`HTTP ${res.status}`);

        const data = await res.json();

        //If an older request finishes after a newer request, ignore it
        //This stops old readings from overwriting newer readings
        if (requestId !== latestRequestId) return;

        showReading(data);
        setConnectionState("online");
      } catch (_error) {
        //If the ESP32 does not respond, the dashboard changes to Offline, This makes it clear that the problem is connection related
        if (requestId !== latestRequestId) return;

        setConnectionState("offline");
        statusChipEl.textContent = "No Data";
        connectionLabelEl.textContent = "Offline";
      }
    }

    //Manual refresh button, This lets the user request a new reading immediately
    refreshBtn.addEventListener("click", () => refresh());

    applyEndpointBtnEl.addEventListener("click", () => {
      //Allows a different data source to be tested if needed,This is useful for debugging if the ESP32 IP address changes
      const fixedEndpoint = fixEndpoint(endpointInputEl.value);

      selectedEndpoint = fixedEndpoint ?? `${window.location.origin}/data`;

      updateEndpointText();
      refresh();
    });

    endpointInputEl.addEventListener("keydown", (event) => {
      //Pressing Enter in the input box does the same thing as clicking Set Source
      if (event.key === "Enter") {
        event.preventDefault();
        applyEndpointBtnEl.click();
      }
    });

    applyAlertBtnEl.addEventListener("click", () => {
      //Saves the alert settings and checks the current reading again
      //This means the alert banner updates straight away after changing the limits
      const low = clampInt(lowThresholdInputEl.value, 0, 100, 25);
      const high = clampInt(highThresholdInputEl.value, 0, 100, 85);

      saveThresholds(low, high);

      if (typeof lastRenderedLevel === "number") {
        checkAlerts(lastRenderedLevel, Date.now());
      } else {
        refresh();
      }
    });

    notifyBtnEl.addEventListener("click", async () => {
      //Enables alert notifications.
      //It uses browser notifications if possible.
      //If not, it uses sound and vibration as a fallback.
      notificationsEnabled = true;
      getAudioContext();

      if (notificationsAvailable()) {
        try {
          const permission =
            window.Notification.permission === "granted"
              ? "granted"
              : await window.Notification.requestPermission();

          if (permission === "granted") {
            notificationMode = "system";
            notifyBtnEl.textContent = "Notifications Enabled";
            addAlertHistory({ message: "Notifications enabled", time: new Date().toLocaleTimeString() });
            return;
          }
        } catch (_error) {
          //If notification permission fails, the fallback alert mode is used below
        }
      }

      notificationMode = "fallback";
      notifyBtnEl.textContent = "Alerts Enabled";
      addAlertHistory({ message: "Alerts enabled (sound/vibration)", time: new Date().toLocaleTimeString() });
    });

    //Starts the dashboard when the page first loads
    //It fills in the endpoint text, loads saved thresholds and shows the normal alert state
    updateEndpointText();

    const savedThresholds = getThresholds();
    saveThresholds(savedThresholds.low, savedThresholds.high);

    showAlertState("normal");
    addAlertHistory(null);

    //First refresh runs as soon as the page open, setInterval repeats every second to keep the dashboard live
    refresh();
    setInterval(refresh, REFRESH_INTERVAL_MS);
  </script>
</body>
</html>
)__INDEXHTML__";

// CSS for the dashboard page
const char STYLE_CSS[] PROGMEM = R"__STYLECSS__(* { box-sizing: border-box; }

/* Main page layout */
body {
  margin: 0;
  font-family: system-ui;
  background: linear-gradient(160deg, #0b1220, #071629);
  color: white;
  min-height: 100vh;
  display: flex;
  justify-content: center;
  padding: 20px;
}

/* Keeps the dashboard from becoming too wide */
.dashboard {
  width: 100%;
  max-width: 900px;
}

/* Header card, uses flex so the title and connection badge sit on oppsite sides */
.header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 18px 22px;
  border-radius: 18px;
  background: rgba(255, 255, 255, .06);
  border: 1px solid rgba(255, 255, 255, .12);
  backdrop-filter: blur(12px);
}

h1 {
  margin: 0;
  font-size: 20px;
}

h2 {
  margin: 0;
  font-size: 16px;
  font-weight: 650;
}

p {
  margin: 4px 0 0;
  font-size: 12px;
  opacity: .7;
}

/* Connection badge beside the title, dot changes colour based on if the ESP32 is online or offline */
.connectionBadge {
  display: flex;
  align-items: center;
  gap: 10px;
  padding: 8px 14px;
  border-radius: 999px;
  background: rgba(255, 255, 255, .08);
  font-size: 12px;
}

.statusDot {
  width: 10px;
  height: 10px;
  border-radius: 50%;
  background: #22c55e;
  box-shadow: 0 0 0 6px rgba(34, 197, 94, .2);
}

.statusDot.online {
  background: #22c55e;
  box-shadow: 0 0 0 6px rgba(34, 197, 94, .2);
}

.statusDot.offline {
  background: #ef4444;
  box-shadow: 0 0 0 6px rgba(239, 68, 68, .18);
}

.statusDot.mock {
  background: #f59e0b;
  box-shadow: 0 0 0 6px rgba(245, 158, 11, .18);
}

/* Shared card style, used for the live reading, alerts and recent readings sections */
.card {
  margin-top: 14px;
  padding: 18px 22px;
  border-radius: 18px;
  background: rgba(255, 255, 255, .06);
  border: 1px solid rgba(255, 255, 255, .12);
  backdrop-filter: blur(12px);
}

/* Row with percentage and distance, wrap helps layout work on smaller screens*/
.readingRow {
  display: flex;
  justify-content: space-between;
  align-items: flex-end;
  gap: 14px;
  flex-wrap: wrap;
}
/* Large % display*/
.levelDisplay {
  font-size: 56px;
  font-weight: 750;
  letter-spacing: -1px;
  line-height: 1;
}

.unit {
  font-size: 16px;
  opacity: .7;
  margin-left: 6px;
  font-weight: 650;
}
/* Distance and status chip area on dashboard*/
.sensorDetails {
  display: flex;
  flex-direction: column;
  align-items: flex-end;
  gap: 6px;
}
/* status chip base style*/
.statusChip {
  padding: 8px 12px;
  border-radius: 999px;
  background: rgba(255, 255, 255, .08);
  border: 1px solid rgba(255, 255, 255, .10);
  font-size: 12px;
}
/* status chip colour for high, low, normal*/
.statusChip.low {
  background: rgba(239, 68, 68, .2);
  border-color: rgba(239, 68, 68, .4);
}

.statusChip.normal {
  background: rgba(34, 197, 94, .2);
  border-color: rgba(34, 197, 94, .4);
}

.statusChip.high {
  background: rgba(245, 158, 11, .2);
  border-color: rgba(245, 158, 11, .4);
}

.metaText {
  font-size: 12px;
  opacity: .7;
}

/* Water level progress bar, the inside fill width is changed by JavaScrpit based on the water level percentage */
.levelBar {
  margin-top: 14px;
  height: 18px;
  border-radius: 999px;
  overflow: hidden;
  background: rgba(255, 255, 255, .12);
  border: 1px solid rgba(255, 255, 255, .2);
}

.levelBarFill {
  height: 100%;
  min-width: 8px;
  background: linear-gradient(90deg, rgba(59, 130, 246, .9), rgba(34, 197, 94, .9));
  transition: width .35s ease;
  box-shadow: inset 0 0 12px rgba(255, 255, 255, .25);
}

/* Refresh and data source rows, these controls allow the user to manually change the data endpoint */
.actionsRow {
  margin-top: 14px;
  display: flex;
  justify-content: space-between;
  align-items: center;
  gap: 12px;
  flex-wrap: wrap;
}

.sourceRow {
  margin-top: 12px;
  display: flex;
  align-items: center;
  gap: 8px;
  flex-wrap: wrap;
}

.endpointInput {
  min-width: 220px;
  max-width: 100%;
  flex: 1 1 260px;
  border-radius: 10px;
  border: 1px solid rgba(255, 255, 255, .2);
  background: rgba(255, 255, 255, .08);
  color: white;
  padding: 9px 10px;
}

.endpointInput::placeholder {
  color: rgba(255, 255, 255, .55);
}
/* Button style used across the dashboard */
.btn {
  appearance: none;
  border: 0;
  cursor: pointer;
  padding: 10px 12px;
  border-radius: 12px;
  background: rgba(59, 130, 246, .22);
  border: 1px solid rgba(59, 130, 246, .35);
  color: #fff;
  font-weight: 650;
}

.btn:disabled {
  opacity: .6;
  cursor: progress;
}

.btn:active {
  transform: translateY(1px);
}

.historyCard {
  margin-top: 14px;
}

.alertsCard {
  margin-top: 14px;
}
/* Alert banner, JavaScript changes the class depending on whether the system is normal*/
.alertBanner {
  margin-top: 12px;
  padding: 10px 12px;
  border-radius: 12px;
  font-size: 13px;
  font-weight: 650;
  border: 1px solid transparent;
}

.alertNormal {
  background: rgba(34, 197, 94, .16);
  border-color: rgba(34, 197, 94, .4);
}

.alertLow {
  background: rgba(239, 68, 68, .2);
  border-color: rgba(239, 68, 68, .45);
}

.alertHigh {
  background: rgba(245, 158, 11, .2);
  border-color: rgba(245, 158, 11, .45);
}
 /* Alerts threshold input area */
.alertControls {
  margin-top: 12px;
  display: flex;
  flex-wrap: wrap;
  gap: 8px;
  align-items: center;
}

.controlLabel {
  font-size: 12px;
  opacity: .8;
}

.thresholdInput {
  width: 72px;
  border-radius: 10px;
  border: 1px solid rgba(255, 255, 255, .2);
  background: rgba(255, 255, 255, .08);
  color: white;
  padding: 9px 10px;
}

.sectionHeader {
  display: flex;
  justify-content: space-between;
  align-items: center;
  gap: 10px;
}
/* style for alert history and recent reading */
.historyList {
  margin: 12px 0 0;
  padding: 0;
  list-style: none;
  display: grid;
  gap: 8px;
}

.historyItem {
  display: flex;
  justify-content: space-between;
  align-items: center;
  gap: 8px;
  padding: 10px 12px;
  border-radius: 12px;
  background: rgba(255, 255, 255, .08);
  border: 1px solid rgba(255, 255, 255, .12);
  font-size: 13px;
}
)__STYLECSS__";

//WiFi details for the hotspot
const char* WIFI_SSID = "WiktoriasiPhone";
const char* WIFI_PASSWORD = "Wiktoria06";

//Timing values
//millis() is used in the main loop instead of delays so the server can respond while sensor and OLED are updating
const unsigned long WIFI_CONNECT_TIMEOUT_MS = 20000;
const unsigned long SERIAL_LOG_INTERVAL_MS = 1000; //
const unsigned long URL_PRINT_INTERVAL_MS = 5000;
const unsigned long SENSOR_READ_INTERVAL_MS = 250;

//Fallback WiFi network made by the ESP32 if the hotspot fails
// ESP32 own network
const char* FALLBACK_AP_SSID = "WaterLevelMonitor";
const char* FALLBACK_AP_PASSWORD = "12345678";

//Tank calibration values
float tankHeight = 16.0; //height of the tank in cm
float minDistance = 7.0; //distance when the tank is full
float maxDistance = minDistance + tankHeight; //distance when the tank is empty

//Latest sensor values, these are shared by the OLED,buzzer,serial monitor and /data web route
float latestDistance = 0.0;
int latestLevel = 0;

//Timing variables used inside loop()
//These allow actions to happen without blocking the web server
unsigned long lastSensorReadMs = 0; //
unsigned long lastSerialLogMs = 0; //
unsigned long lastUrlPrintMs = 0; //

//True when the ESP32 has made its own fallback WiFi network
bool usingFallbackAp = false;

//declared functions
void readAndRenderSensor();
bool connectToHotspot();
void startFallbackAccessPoint();
void printAccessUrls();

void setCorsHeaders() {
  //Lets the webpage read /data without browser blocking issues
  server.sendHeader("Access-Control-Allow-Origin", "*"); //
  server.sendHeader("Access-Control-Allow-Methods", "GET, OPTIONS"); //
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type"); //  
  server.sendHeader("Access-Control-Allow-Private-Network", "true"); //
}

float readDistanceOnce() {
  //Sends 1 microseconf trigger pulse to the HC-SR04, the sensor than waits for the echo to return from the water surface
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(5);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  //Reads the echo time and turns it into cm
  long duration = pulseIn(ECHO_PIN, HIGH, 30000); //measure how long the ECHO pin stays high for
  float distance = duration * 0.0343 / 2; //converts time in CM than divided by 2 because the sound travels to the water and back

  return distance;
}

float getDistance() {
  //Takes five readings and uses the middle value to reduce random jumps this gives a more stable reading
  float samples[5];
  int validCount = 0;

  for (int i = 0; i < 5; i++) {
    float distance = readDistanceOnce();

    //Only keep readings inside a sensible range for the sensor and rejects clearly wrong values
    if (distance >= 2.0 && distance <= 400.0) {
      samples[validCount++] = distance;
    }

    delay(10);
  }

  //Return -1 if the sensor did not get any good readings
  if (validCount == 0) {
    return -1.0;
  }

  //Simple sort because there are only a few readings
  for (int i = 1; i < validCount; i++) {
    float key = samples[i];
    int j = i - 1;

    while (j >= 0 && samples[j] > key) {
      samples[j + 1] = samples[j];
      j--;
    }

    samples[j + 1] = key;
  }

  //Middle value is used as the final distance
  return samples[validCount / 2];
}


int getWaterLevelPercent(float distance) {
  // if reading failed , keep the last good reading
  if (distance < 0) return latestLevel;

  //Smaller distance means the water is closer to the sensor
  if (distance <= minDistance) return 100; // full show 100
  if (distance >= maxDistance) return 0; // empty show 0

  //Convert the distance into a percentage
  float level = 100 - ((distance - minDistance) / tankHeight) * 100;

//the results can never go lower than 0 or higher than 100
  if (level < 0) level = 0; 
  if (level > 100) level = 100;

//Rounds to the nearest whole number
  return (int)(level + 0.5f);
}

void handleRoot() {
  //Sends the dashboard page
  //Root handles the. main webpage
  setCorsHeaders();
  server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
}

void handleStyleCss() {
  //Sends the CSS used by the dashboard
  server.send_P(200, "text/css; charset=utf-8", STYLE_CSS);
}

void handleFavicon() {
  //browser asks for an icon, sending an 204 response, stops from cluterring the serial monitor with icons
  server.send(204, "image/x-icon", "");
}

void handleOptions() {
  //Handles browser requests
  setCorsHeaders();
  server.send(204, "text/plain", "");
}

void handleData() {
  //Take a new reading before sending data to the webpage, main API route. The JavaScript dashboard calls for /data everysecond.
  //This function takes a new reading and builts a JSON response and sends it back to the browser
  readAndRenderSensor();

//Builds the JSON response manually
  String payload = "{"; //
  payload += "\"distance\":"; //
  payload += String(latestDistance, 2); //
  payload += ",\"level\":"; //
  payload += String(latestLevel); //
  payload += "}"; //

  setCorsHeaders(); //sends JSON back to the webpage
  server.send(200, "application/json", payload); //The brower reads the information and updates the % distance, progress bar
  //status chip, alerts and recent readings
}

void readAndRenderSensor() {
  //Read the distance and update the stored values
  float measuredDistance = getDistance();

  if (measuredDistance >= 0) {
    latestDistance = measuredDistance;
    latestLevel = getWaterLevelPercent(latestDistance);
  }

  unsigned long nowMs = millis(); //

  //Print readings per second on the serial monitor
  if (nowMs - lastSerialLogMs >= SERIAL_LOG_INTERVAL_MS) {
    lastSerialLogMs = nowMs;
    Serial.print("Distance: ");
    Serial.print(latestDistance);
    Serial.print(" cm | Level: ");
    Serial.print(latestLevel);
    Serial.println("%");
  }

  //Buzzer warning for low water level
  if (latestLevel <= 20) {
    tone(BUZZER_PIN, 1500);
  } else {
    noTone(BUZZER_PIN);
  }

  //Update the OLED screen
  display.clearDisplay();

  display.setCursor(0, 0);
  display.setTextSize(1);
  display.println("Water Level Monitor");

  display.setTextSize(2);
  display.setCursor(0, 20);
  display.print(latestLevel);
  display.println("%");

  display.setTextSize(1);
  display.setCursor(0, 50);
  display.print("Dist: ");
  display.print(latestDistance);
  display.println(" cm");

  display.display();
}

bool connectToHotspot() {
  //Try to connect to the phone hotspot first and puts ESP32 in station mode
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to hotspot");
  unsigned long startedAt = millis();

//tries to connect until connection or timeout
  while (WiFi.status() != WL_CONNECTED && millis() - startedAt < WIFI_CONNECT_TIMEOUT_MS) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    IPAddress ip = WiFi.localIP();

      //Once connected the ESP32 recevies an IP address from Hotspot
    Serial.print("Connected to hotspot. ESP32 IP: ");
    Serial.println(ip);

    Serial.print("Phone Dashboard URL: http://");
    Serial.print(ip);
    Serial.println("/");

    Serial.print("Phone API URL: http://");
    Serial.print(ip);
    Serial.println("/data");

    return true;
  }

//If connection failed the fallbacl access will be used
  Serial.println("Hotspot connection failed.");
  Serial.println("ESP32 is 2.4GHz only. On iPhone hotspot.");
  return false;
}

void startFallbackAccessPoint() {
  //If the phone hotspot fails, the ESP32 makes its own WiFi network, the user can directly connect
  WiFi.mode(WIFI_AP);
  WiFi.softAP(FALLBACK_AP_SSID, FALLBACK_AP_PASSWORD);
  usingFallbackAp = true;

  Serial.print("Fallback AP IP: ");
  Serial.println(WiFi.softAPIP());

  Serial.print("Fallback Dashboard URL: http://");
  Serial.print(WiFi.softAPIP());
  Serial.println("/");

  Serial.print("Fallback API URL: http://");
  Serial.print(WiFi.softAPIP());
  Serial.println("/data");
}

void printAccessUrls() {
  //Reprints the dashboard URL so it is easy to find in Serial Monitor
  if (WiFi.status() == WL_CONNECTED) {
    IPAddress ip = WiFi.localIP();
    Serial.print("Dashboard: http://");
    Serial.print(ip);
    Serial.println("/");
    return;
  }

  if (usingFallbackAp) {
    IPAddress ip = WiFi.softAPIP();
    Serial.print("Dashboard (AP): http://");
    Serial.print(ip);
    Serial.println("/");
    return;
  }

  Serial.println("WiFi not connected yet.");
}

void setup() {
  //starts the serial monitor for debugging and setup messages
  Serial.begin(115200);
  delay(1200);

  Serial.println();
  Serial.println("Booting Water Level Monitor...");

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  //Start the OLED display
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { //start of OLED display at I2C address 0x3C
    Serial.println("OLED not found");
    for (;;);
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  //Connect to hotspot or use fallback AP mode
  if (!connectToHotspot()) {
    startFallbackAccessPoint();
  }

  lastUrlPrintMs = millis(); //prints the URL
  printAccessUrls();

  //Web server routes
  server.on("/", handleRoot); //register all web server routes
  server.on("/", HTTP_OPTIONS, handleOptions); //sends main basboard page
  server.on("/styles/style.css", handleStyleCss); // sends styling
  server.on("/favicon.ico", handleFavicon); //avoids favicon erros
  server.on("/data", handleData); // sends live data from livr sensor readings as JSON
  server.on("/data", HTTP_OPTIONS, handleOptions); //options route for brower compatibility
  server.begin(); //starts webserver

  //First reading at startup
  readAndRenderSensor(); //takes 1st reading so the OLED and webpage can have data
  lastSensorReadMs = millis();
}

void loop() {
  //Keeps the web server running, without this the /data would not update
  server.handleClient();

  unsigned long nowMs = millis(); //

  //Print the dashboard URL every few seconds, millis() is used so the ESP32 can handle requests while timing checks arw happeneing
  if (nowMs - lastUrlPrintMs >= URL_PRINT_INTERVAL_MS) {
    lastUrlPrintMs = nowMs;
    printAccessUrls();
  }

  //Update sensor and OLED on a timed interval
  if (nowMs - lastSensorReadMs >= SENSOR_READ_INTERVAL_MS) {
    lastSensorReadMs = nowMs;
    readAndRenderSensor();
  }
}
