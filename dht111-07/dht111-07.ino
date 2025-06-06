#include <ESP8266WiFi.h>
#include <FirebaseESP8266.h>
#include <DHT.h>
#include <time.h>
#include <ArduinoOTA.h>

// Wi-Fi credentials
#define WIFI_SSID "WEWEXNET"
#define WIFI_PASSWORD "internetgh4"

// Firebase credentials
#define FIREBASE_HOST "dht111-362e6-default-rtdb.firebaseio.com"
#define FIREBASE_AUTH "PMD24FStkDchq5YiMMD5SiTgY56aU9PHAIkNHBm3"

// DHT sensor configuration
#define DHTPIN D2
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// Firebase objects
FirebaseData firebaseData;
FirebaseAuth auth;
FirebaseConfig config;

// Unique Sensor ID (e.g., Sensor01, Sensor02...)
String sensorID = "Sensor07";

// Time tracking
int lastLoggedHour = -1;

void setup() {
  Serial.begin(9600);
  dht.begin();

  // Connect to Wi-Fi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\nConnected!");

  // OTA setup
  ArduinoOTA.setHostname(sensorID.c_str());  // Optional: use sensor ID as hostname
  ArduinoOTA.onStart([]() {
    Serial.println("OTA Update Starting...");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nOTA Update Complete!");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("OTA Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();

  // Firebase setup
  config.host = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // Upload MAC address to Firebase
  String mac = WiFi.macAddress();
  String macPath = "/" + sensorID + "/macaddress";
  if (Firebase.setString(firebaseData, macPath, mac)) {
    Serial.println("MAC address uploaded: " + mac);
  } else {
    Serial.println("MAC upload failed: " + firebaseData.errorReason());
  }

  // NTP time sync
  configTime(7 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  Serial.print("Waiting for NTP sync");
  time_t now = time(nullptr);
  while (now < 1000000000) {
    Serial.print(".");
    delay(1000);
    now = time(nullptr);
  }
  Serial.println("\nTime synchronized!");
}

void loop() {
  ArduinoOTA.handle();  // Handle OTA updates

  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();

  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  // Get formatted date and hour
  String year, month, day, hour;
  getFormattedDateTime(year, month, day, hour);
  int currentHour = hour.substring(0, 2).toInt();

  // Log data hourly
  if (currentHour != lastLoggedHour) {
    lastLoggedHour = currentHour;

    String logPath = "/" + sensorID + "/logs/" + year + "/" + month + "/" + day + "/" + hour + ":00";
    FirebaseJson json;
    json.add("temperature", temperature);
    json.add("humidity", humidity);

    if (Firebase.setJSON(firebaseData, logPath, json)) {
      Serial.println("Logged data at " + hour);
    } else {
      Serial.println("Log error: " + firebaseData.errorReason());
    }

    cleanOldLogs(year, month);
  }

  // Upload real-time data every 3 seconds
  FirebaseJson realtimeJson;
  realtimeJson.add("temperature", temperature);
  realtimeJson.add("humidity", humidity);

  String realtimePath = "/" + sensorID + "/realtime";
  if (Firebase.setJSON(firebaseData, realtimePath, realtimeJson)) {
    Serial.println("Real-time data updated");
  } else {
    Serial.println("Realtime error: " + firebaseData.errorReason());
  }

  delay(3000);  // Wait before next update
}

String getFormattedDateTime(String &year, String &month, String &day, String &hour) {
  time_t now = time(nullptr);
  struct tm *timeinfo = localtime(&now);

  char y[5], m[3], d[3], h[3];
  strftime(y, sizeof(y), "%Y", timeinfo);
  strftime(m, sizeof(m), "%m", timeinfo);
  strftime(d, sizeof(d), "%d", timeinfo);
  strftime(h, sizeof(h), "%H", timeinfo);

  year = String(y);
  month = String(m);
  day = String(d);
  hour = String(h) + ":00";

  return year + "/" + month + "/" + day + "/" + hour;
}

void cleanOldLogs(String currentYear, String currentMonth) {
  int yearInt = currentYear.toInt();
  int monthInt = currentMonth.toInt();
  int oldestYear = yearInt;
  int oldestMonth = monthInt - 5;

  if (oldestMonth < 1) {
    oldestMonth += 12;
    oldestYear--;
  }

  String oldestPath = "/" + sensorID + "/logs/" + String(oldestYear) + "/" + (oldestMonth < 10 ? "0" : "") + String(oldestMonth);
  Firebase.deleteNode(firebaseData, oldestPath);
  Serial.print("Deleted old logs: ");
  Serial.println(oldestPath);
}