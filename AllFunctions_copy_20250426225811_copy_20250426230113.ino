#define TINY_GSM_MODEM_SIM7000
#define TINY_GSM_RX_BUFFER 1024
#define SerialAT Serial1

// Enable debug AT commands
#define DUMP_AT_COMMANDS
#define TINY_GSM_TEST_GPRS    true
#define TINY_GSM_TEST_GPS     false
#define TINY_GSM_POWERDOWN    true

#include <TinyGsmClient.h>
#include <SPI.h>
#include <SD.h>
#include <Ticker.h>
#include <ArduinoHttpClient.h>

// SIM card settings
#define GSM_PIN ""  // Leave empty if no PIN needed

// TNM Malawi APN settings
const char apn[] = "internet.tnm.mw";
const char gprsUser[] = "tnm";
const char gprsPass[] = "tnm";

// Firebase settings - Corrected structure
const char* FIREBASE_HOST = "https://faults-2a6ef-default-rtdb.firebaseio.com/electricityStatus.json?auth=YOUR_1RUKKBU2nlDeFOJMsAd4HZudtGoGC8aragbRIsNg";
const char* FIREBASE_AUTH = "1RUKKBU2nlDeFOJMsAd4HZudtGoGC8aragbRIsNg";
const int FIREBASE_PORT = 443;  // Using HTTP port (change to 443 if using HTTPS)

#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, Serial);
TinyGsm modem(debugger);
#else
TinyGsm modem(SerialAT);
#endif

// Deep sleep settings
#define uS_TO_S_FACTOR 1000000ULL
#define TIME_TO_SLEEP  60  // Sleep for 60 seconds

// Pin definitions (unchanged)
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

void setup() {
    Serial.begin(115200);
    delay(10);

    // Initialize LED and power pins
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);

    // Power on the modem
    pinMode(PWR_PIN, OUTPUT);
    digitalWrite(PWR_PIN, HIGH);
    delay(1000);
    digitalWrite(PWR_PIN, LOW);

    // Initialize SD card (optional)
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

void powerDownModem() {
    Serial.println("Powering down modem...");
    modem.sendAT("+CPOWD=1");
    if (modem.waitResponse(10000L) != 1) {
        Serial.println("First power-down attempt failed, retrying...");
        delay(1000);
        modem.sendAT("+CPOWD=1");
        modem.waitResponse(10000L);
    }
    modem.poweroff();
    Serial.println("Modem powered off");
}

void loop() {
    Serial.println("Initializing modem...");
    if (!modem.init()) {
        Serial.println("Failed to init modem");
        delay(10000);
        return;
    }

    // Configure modem GPIO
    modem.sendAT("+CGPIO=0,48,1,0");
    modem.waitResponse(10000L);

#if TINY_GSM_TEST_GPRS
    // Unlock SIM if needed
    if (GSM_PIN && modem.getSimStatus() != 3) {
        modem.simUnlock(GSM_PIN);
    }

    // Configure network mode
    modem.sendAT("+CFUN=0");
    modem.waitResponse(10000L);
    delay(200);

    modem.setNetworkMode(2);  // Auto mode
    delay(200);
    modem.setPreferredMode(3);  // LTE only
    delay(200);
    modem.sendAT("+CFUN=1");
    modem.waitResponse(10000L);
    delay(200);

    Serial.println("Waiting for network...");
    if (!modem.waitForNetwork(180000L)) {  // 3-minute timeout
        Serial.println("Network connection failed");
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

        // Generate random electricity status
        bool electricityPresent = random(0, 2);
        String status = electricityPresent ? "present" : "absent";
        Serial.println("Electricity status: " + status);

        // Create Firebase client with timeout
        TinyGsmClient client(modem);
        client.setTimeout(20000);  // 20-second timeout
        HttpClient http(client, FIREBASE_HOST, FIREBASE_PORT);

        // Prepare Firebase request - CORRECTED PATH STRUCTURE
        String path = "/electricityStatus.json?auth=" + String(FIREBASE_AUTH);
        String jsonPayload = "{\"electricity\":\"" + status + "\"}";

        Serial.println("Sending to Firebase...");
        Serial.println("Full URL: http://" + String(FIREBASE_HOST) + path);
        Serial.println("Payload: " + jsonPayload);

        // Send HTTP PUT request with retries
        int statusCode = -1;
        String response;
        
        for (int attempt = 0; attempt < 3; attempt++) {
            http.put(path, "application/json", jsonPayload);
            statusCode = http.responseStatusCode();
            response = http.responseBody();
            
            if (statusCode == 200) break;
            
            Serial.print("Attempt ");
            Serial.print(attempt + 1);
            Serial.println(" failed, retrying...");
            delay(2000);
        }

        Serial.println("Firebase Status: " + String(statusCode));
        Serial.println("Response: " + response);

        if (statusCode != 200) {
            Serial.println("Failed to send data to Firebase!");
        }
    } else {
        Serial.println("GPRS not connected");
    }

    // Clean disconnect
    modem.sendAT("+COPS=2");  // Deregister from network
    modem.waitResponse(10000L);
    delay(1000);
    
    modem.gprsDisconnect();
    if (!modem.isGprsConnected()) {
        Serial.println("GPRS disconnected");
    }
#endif

#if TINY_GSM_POWERDOWN
    powerDownModem();
#endif

    // Enter deep sleep
    Serial.println("Entering deep sleep...");
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
    delay(200);
    Serial.flush();
    esp_deep_sleep_start();
}