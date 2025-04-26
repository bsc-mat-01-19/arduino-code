#define TINY_GSM_MODEM_SIM7000
#define TINY_GSM_RX_BUFFER 1024
#define SerialAT Serial1

#define DUMP_AT_COMMANDS
#define TINY_GSM_TEST_GPRS    true
#define TINY_GSM_TEST_GPS     false
#define TINY_GSM_POWERDOWN    true

#include <TinyGsmClient.h>
#include <SPI.h>
#include <SD.h>
#include <Ticker.h>
#include <ArduinoHttpClient.h>

#define GSM_PIN ""  // Leave empty if SIM card doesn't require a PIN

const char apn[] = "internet.tnm.mw";  // TNM Malawi APN
const char gprsUser[] = "tnm";
const char gprsPass[] = "tnm";

// Firebase settings
const char* FIREBASE_HOST = "faults-2a6ef-default-rtdb.firebaseio.com";
const char* FIREBASE_AUTH = "1RUKKBU2nlDeFOJMsAd4HZudtGoGC8aragbRIsNg";
const char* FIREBASE_IP = "34.120.160.131";  // Firebase IP
const int FIREBASE_PORT = 443;

#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, Serial);
TinyGsm modem(debugger);
#else
TinyGsm modem(SerialAT);
#endif

#define uS_TO_S_FACTOR 1000000ULL
#define TIME_TO_SLEEP  60  // Sleep time in seconds

#define UART_BAUD   115200
#define PIN_DTR     25
#define PIN_TX      27
#define PIN_RX      26
#define PWR_PIN     4

#define SD_MISO     2
#define SD_MOSI     15
#define SD_SCLK     14
#define SD_CS       13
#define LED_PIN     12

void setup()
{
    Serial.begin(115200);
    delay(10);

    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);

    pinMode(PWR_PIN, OUTPUT);
    digitalWrite(PWR_PIN, HIGH);
    delay(1000);
    digitalWrite(PWR_PIN, LOW);

    // Initialize SD Card (optional)
    SPI.begin(SD_SCLK, SD_MISO, SD_MOSI, SD_CS);
    if (!SD.begin(SD_CS)) {
        Serial.println("SDCard MOUNT FAIL");
    } else {
        uint32_t cardSize = SD.cardSize() / (1024 * 1024);
        Serial.println("SDCard Size: " + String(cardSize) + "MB");
    }

    delay(1000);
    SerialAT.begin(UART_BAUD, SERIAL_8N1, PIN_RX, PIN_TX);

    Serial.println("Initializing modem...");
    if (!modem.restart()) {
        Serial.println("Failed to restart modem");
    }
}

void loop()
{
    Serial.println("Initializing modem...");
    if (!modem.init()) {
        Serial.println("Failed to init modem");
    }

    modem.sendAT("+CGPIO=0,48,1,0");
    modem.waitResponse(10000L);

#if TINY_GSM_TEST_GPRS
    if (GSM_PIN && modem.getSimStatus() != 3) {
        modem.simUnlock(GSM_PIN);
    }

    modem.sendAT("+CFUN=0");
    modem.waitResponse(10000L);
    delay(200);

    modem.setNetworkMode(2);  // Auto
    delay(200);
    modem.setPreferredMode(3);  // LTE only
    delay(200);
    modem.sendAT("+CFUN=1");
    modem.waitResponse(10000L);
    delay(200);

    Serial.println("Waiting for network...");
    if (!modem.waitForNetwork()) {
        delay(10000);
        return;
    }

    if (modem.isNetworkConnected()) {
        Serial.println("Network connected");
    }

    Serial.println("Connecting to GPRS...");
    if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
        Serial.println("GPRS connect failed");
        delay(10000);
        return;
    }

    if (modem.isGprsConnected()) {
        Serial.println("GPRS connected");

        // Simulated electricity status (replace with sensor logic)
        bool electricityPresent = random(0, 2);
        String status = electricityPresent ? "present" : "absent";
        Serial.println("Electricity status: " + status);

        // Send to Firebase
        TinyGsmClient client(modem);
        HttpClient http(client, FIREBASE_IP, FIREBASE_PORT);  // IP-based for SIM7000

        String path = "/electricityStatus.json?auth=" + String(FIREBASE_AUTH);
        String jsonPayload = "{\"electricity\":\"" + status + "\"}";

        http.beginRequest();
        http.put(path);  // Use PUT to write directly
        http.sendHeader("Host", FIREBASE_HOST);
        http.sendHeader("Content-Type", "application/json");
        http.sendHeader("Content-Length", jsonPayload.length());
        http.beginBody();
        http.print(jsonPayload);
        http.endRequest();

        int statusCode = http.responseStatusCode();
        String response = http.responseBody();

        Serial.println("Firebase Status: " + String(statusCode));
        Serial.println("Response: " + response);
    } else {
        Serial.println("GPRS not connected");
    }

    modem.gprsDisconnect();
    if (!modem.isGprsConnected()) {
        Serial.println("GPRS disconnected");
    }
#endif

#if TINY_GSM_POWERDOWN
    modem.sendAT("+CPOWD=1");
    modem.waitResponse(10000L);
    modem.poweroff();
    Serial.println("Poweroff.");
#endif

    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
    delay(200);
    esp_deep_sleep_start();
}