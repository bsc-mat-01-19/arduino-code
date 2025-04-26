#define TINY_GSM_MODEM_SIM7000
#define TINY_GSM_RX_BUFFER 1024
#define SerialAT Serial1

#include <TinyGsmClient.h>
#include <ArduinoHttpClient.h>

const char apn[]      = "internet.tnm.mw";
const char gprsUser[] = "tnm";
const char gprsPass[] = "tnm";

// ThingSpeak
const char* server = "api.thingspeak.com";
const int port = 80;
const char* apiKey = "5M1CR2RBEUGIVE1T";

// Phone Number to send SMS to
const char* phoneNumber = "+265883366358";

#define UART_BAUD 115200
#define MODEM_RST 4
#define MODEM_PWR 23
#define MODEM_TX  27
#define MODEM_RX  26

#define LED_PIN 12

#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, Serial);
TinyGsm modem(debugger);
TinyGsmClient client(modem);
HttpClient http(client, server, port);

void setup() {
  Serial.begin(115200);
  delay(10);

  pinMode(MODEM_PWR, OUTPUT);
  digitalWrite(MODEM_PWR, HIGH);
  delay(1000);
  digitalWrite(MODEM_PWR, LOW);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Serial.println("Booting...");

  SerialAT.begin(UART_BAUD, SERIAL_8N1, MODEM_RX, MODEM_TX);
  delay(3000);

  Serial.println("Initializing modem...");
  modem.restart();

  int simStatus = modem.getSimStatus();
  Serial.print("SIM Status: ");
  Serial.println(simStatus);
  if (simStatus != 1) {
    Serial.println("❌ SIM not ready!");
    return;
  } else {
    Serial.println("✅ SIM ready");
  }

  Serial.println("Connecting to network...");
  if (!modem.waitForNetwork()) {
    Serial.println("❌ Network fail");
    return;
  }

  Serial.println("Network connected");

  if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
    Serial.println("❌ GPRS connection failed");
    return;
  }

  Serial.println("GPRS connected");

  // Simulate electricity detection (0 or 1)
  bool electricityPresent = random(0, 2);
  int value = electricityPresent ? 1 : 0;

  // Define the pole number
  String poleNumber = "24423";

  String statusMessage = (value == 1)
    ? "fault has been fixed"
    : "On pole number " + poleNumber + " there is fault";

  Serial.println(statusMessage);

  // Send SMS
  sendSMS(phoneNumber, statusMessage);

  // Build URL with both value and poleNumber
  String url = "/update?api_key=" + String(apiKey) +
               "&field1=" + String(value) +
               "&field2=" + poleNumber;

  Serial.println("Sending to ThingSpeak...");
  Serial.println("URL: " + url);

  http.get(url);

  int statusCode = http.responseStatusCode();
  String response = http.responseBody();

  Serial.print("Status code: ");
  Serial.println(statusCode);
  Serial.print("Response: ");
  Serial.println(response);

  http.stop();

  modem.gprsDisconnect();
  Serial.println("GPRS disconnected");

  digitalWrite(LED_PIN, HIGH); // Done indicator
}

void sendSMS(const char* phoneNumber, String message) {
  Serial.println("Sending SMS...");

  modem.sendAT("+CMGF=1");
  delay(1000);

  modem.sendAT("+CMGS=\"" + String(phoneNumber) + "\"");
  delay(1000);

  modem.sendAT(message);
  delay(1000);

  modem.sendAT((char)26); // Ctrl+Z
  delay(3000);

  Serial.println("SMS Sent");
}

void loop() {
  // Nothing in loop
}