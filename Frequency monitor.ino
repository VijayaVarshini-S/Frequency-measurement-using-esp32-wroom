#include <WiFi.h>
#include <WebServer.h>
#include "driver/pcnt.h"
#include <time.h>

const char* WIFI_SSID = "varsvars";
const char* WIFI_PASSWORD = "varsvars";

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 19800;
const int daylightOffset_sec = 0;

WebServer server(80);


String csvData =
"Date,Time,Freq1_Hz,Freq2_Hz,Freq3_Hz,Freq4_Hz\n";
unsigned long lastCSVLog = 0;
const unsigned long CSV_LOG_INTERVAL = 1800000UL; // 30 minutes
unsigned long sampleNumber = 0;

// 4 input channels
static const uint8_t SIG_PIN[4] = {32, 33, 34, 35};
static const pcnt_unit_t PCNT_UNIT[4] = {
    PCNT_UNIT_0, PCNT_UNIT_1, PCNT_UNIT_2, PCNT_UNIT_3
};
// 1 second gate = best accuracy balance
static const uint32_t GATE_TIME_US = 1000000UL;

// PCNT limit (overflow handling)
static const int16_t PCNT_H_LIM = 30000;

volatile int32_t overflowCount[4] = {0};
float freqHz[4] = {0};
uint32_t lastGateStart = 0;
static void IRAM_ATTR pcntOverflowISR(void* arg) {
    int ch = (int)(intptr_t)arg;
    uint32_t status = 0;

    pcnt_get_event_status(PCNT_UNIT[ch], &status);

    if (status & PCNT_EVT_H_LIM) {
        overflowCount[ch]++;
    }
}
void setupPCNT(int ch) {
    pcnt_config_t cfg = {};

    cfg.pulse_gpio_num = SIG_PIN[ch];
    cfg.ctrl_gpio_num  = PCNT_PIN_NOT_USED;
    cfg.channel        = PCNT_CHANNEL_0;
    cfg.unit           = PCNT_UNIT[ch];

    cfg.pos_mode       = PCNT_COUNT_INC;
    cfg.neg_mode       = PCNT_COUNT_DIS;

    cfg.lctrl_mode     = PCNT_MODE_KEEP;
    cfg.hctrl_mode     = PCNT_MODE_KEEP;

    cfg.counter_h_lim  = PCNT_H_LIM;
    cfg.counter_l_lim  = 0;

    pcnt_unit_config(&cfg);

    pcnt_set_filter_value(PCNT_UNIT[ch], 100); // noise filter (~1µs)
    pcnt_filter_enable(PCNT_UNIT[ch]);

    pcnt_event_enable(PCNT_UNIT[ch], PCNT_EVT_H_LIM);
    pcnt_isr_handler_add(PCNT_UNIT[ch], pcntOverflowISR, (void*)(intptr_t)ch);

    pcnt_counter_pause(PCNT_UNIT[ch]);
    pcnt_counter_clear(PCNT_UNIT[ch]);
    pcnt_counter_resume(PCNT_UNIT[ch]);
}
void readFrequency() {
    uint32_t now = micros();
    uint32_t elapsed = now - lastGateStart;
    lastGateStart = now;

    for (int i = 0; i < 4; i++) {

        int16_t count = 0;

        pcnt_counter_pause(PCNT_UNIT[i]);
        pcnt_get_counter_value(PCNT_UNIT[i], &count);

        int32_t total;

        noInterrupts();
        total = (int32_t)overflowCount[i] * PCNT_H_LIM + count;
        overflowCount[i] = 0;
        interrupts();

        pcnt_counter_clear(PCNT_UNIT[i]);
        pcnt_counter_resume(PCNT_UNIT[i]);

        if (elapsed > 0) {
            freqHz[i] = (float)total * 1000000.0f / (float)elapsed;
        } else {
            freqHz[i] = 0;
        }
    }
}
void handleData() {
    String json = "{";

    json += "\"freq1\":" + String(freqHz[0], 4);
    json += ",\"freq2\":" + String(freqHz[1], 4);
    json += ",\"freq3\":" + String(freqHz[2], 4);
    json += ",\"freq4\":" + String(freqHz[3], 4);

    json += "}";

    server.send(200, "application/json", json);
}
void handleRoot() {

String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>ESP32 Frequency Meter</title>

<style>
body{
    margin:0;
    font-family: monospace;
    background:#0b0f14;
    color:#e6edf3;
}

h1{
    text-align:center;
    color:#4fc3f7;
    margin:15px;
    font-size:20px;
}

.sub{
    text-align:center;
    color:#7aa2f7;
    font-size:12px;
    margin-bottom:10px;
}

.grid{
    display:grid;
    grid-template-columns:repeat(auto-fit,minmax(180px,1fr));
    gap:12px;
    padding:15px;
}

.card{
    background:#131920;
    border:1px solid #263241;
    border-radius:8px;
    padding:15px;
    text-align:center;
}

.card.ok{ border-color:#4fc3f7; }
.card.bad{ border-color:#ff5252; }

.label{
    font-size:12px;
    color:#8aa0b2;
    margin-bottom:8px;
}

.freq{
    font-size:26px;
    font-weight:bold;
    color:#69f0ae;
}

.unit{
    font-size:12px;
    color:#607d8b;
}

.status{
    text-align:center;
    font-size:12px;
    margin-top:10px;
    color:#607d8b;
}

.dot{
    display:inline-block;
    width:8px;
    height:8px;
    border-radius:50%;
    background:#69f0ae;
    margin-right:5px;
}

.dot.red{ background:#ff5252; }
</style>
</head>

<body>

<h1>⚡ ESP32 Frequency Meter</h1>
<div class="sub">PCNT Hardware Counter</div>

<div class="grid">

<div class="card" id="c1">
<div class="label">CH1 (GPIO 32)</div>
<div class="freq" id="f1">---</div>
<div class="unit">Hz</div>
</div>

<div class="card" id="c2">
<div class="label">CH2 (GPIO 33)</div>
<div class="freq" id="f2">---</div>
<div class="unit">Hz</div>
</div>

<div class="card" id="c3">
<div class="label">CH3 (GPIO 34)</div>
<div class="freq" id="f3">---</div>
<div class="unit">Hz</div>
</div>

<div class="card" id="c4">
<div class="label">CH4 (GPIO 35)</div>
<div class="freq" id="f4">---</div>
<div class="unit">Hz</div>
</div>

</div>

<div class="status">
<span class="dot" id="dot"></span>
<span id="status">Connecting...</span>
</div>

<script>

function update(d){

    let ok = true;

    for(let i=1;i<=4;i++){
        let f = d["freq"+i];
        let el = document.getElementById("f"+i);
        let card = document.getElementById("c"+i);

        if(f <= 0){
            el.innerHTML = "0.000";
            card.className = "card bad";
            ok = false;
        } else {
            el.innerHTML = f.toFixed(3);
            card.className = "card ok";
        }
    }

    let dot = document.getElementById("dot");
    let status = document.getElementById("status");

    if(ok){
        dot.className = "dot";
        status.innerHTML = "Signal OK";
    } else {
        dot.className = "dot red";
        status.innerHTML = "No Signal / Weak Input";
    }
}

function poll(){
    fetch('/data')
    .then(r => r.json())
    .then(update)
    .catch(e => {
        document.getElementById("status").innerHTML = "Disconnected";
        document.getElementById("dot").className = "dot red";
    });
}

setInterval(poll, 500);
poll();

</script>
<div style="text-align:center;margin:20px;">
<button onclick="window.location='/downloadcsv'"
style="
padding:12px 20px;
font-size:16px;
background:#4fc3f7;
border:none;
border-radius:6px;
cursor:pointer;">
Download CSV
</button>
</div>
</body>
</html>
)rawliteral";

server.send(200, "text/html", html);
}
void logToCSV()
{
    struct tm timeinfo;

    if(!getLocalTime(&timeinfo))
    {
        return;
    }

    char dateStr[16];
    char timeStr[16];

    strftime(dateStr,sizeof(dateStr),"%Y-%m-%d",&timeinfo);
    strftime(timeStr,sizeof(timeStr),"%H:%M:%S",&timeinfo);

    csvData += String(dateStr);
    csvData += ",";
    csvData += String(timeStr);
    csvData += ",";
    csvData += String(freqHz[0],4);
    csvData += ",";
    csvData += String(freqHz[1],4);
    csvData += ",";
    csvData += String(freqHz[2],4);
    csvData += ",";
    csvData += String(freqHz[3],4);
    csvData += "\n";
}
void handleCSV()
{
    server.sendHeader(
        "Content-Disposition",
        "attachment; filename=frequency_log.csv"
    );

    server.send(
        200,
        "text/csv",
        csvData
    );
}
void setup() {
    Serial.begin(115200);

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(300);
        Serial.print(".");
    }

    Serial.println("\nWiFi Connected");
    Serial.println(WiFi.localIP());
    configTime(
    gmtOffset_sec,
    daylightOffset_sec,
    ntpServer
    );

    struct tm timeinfo;

    while(!getLocalTime(&timeinfo))
    {
      delay(500);
      Serial.print(".");
    }

    Serial.println("\nTime synchronized");

    pcnt_isr_service_install(0);

    for (int i = 0; i < 4; i++) {
        setupPCNT(i);
    }
    server.on("/", handleRoot);
    server.on("/data", handleData);
    server.on("/downloadcsv", handleCSV);
    server.begin();

    lastGateStart = micros();
}
void loop() {
    server.handleClient();

    static uint32_t lastSample = 0;

    // fixed 1-second sampling window
    if (micros() - lastSample >= GATE_TIME_US) {
        lastSample = micros();
        readFrequency();

        if(millis() - lastCSVLog >= CSV_LOG_INTERVAL)
        {
           lastCSVLog = millis();
           logToCSV();
        }

        Serial.printf(
           "F1=%.2f Hz | F2=%.2f Hz | F3=%.2f Hz | F4=%.2f Hz\n",
            freqHz[0], freqHz[1], freqHz[2], freqHz[3]
        );
    }
}


