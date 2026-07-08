#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include "time.h"
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include <Wire.h>
#include <BH1750.h>

#define WIFI_SSID "Iruu"
#define WIFI_PASSWORD "19981998"
#define API_KEY "AIzaSyAFf-69-Xfyz4IuTTjgify9eRbTx0pkAuM"
#define USER_EMAIL "user@gmail.com"
#define USER_PASSWORD "User@123"
#define DATABASE_URL "https://greenhouse2024-3534b-default-rtdb.firebaseio.com"

// Define Firebase objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
FirebaseJson json;

String liveData = "/liveData";
String notification = "/notification";
String uid, Notify_Message, Prediction, modes, red, green, blue, light;
int pre_R, pre_G, pre_B;

unsigned long lastTime = 0;
unsigned long timerDelay = 10000;

unsigned long lastTime2 = 0;
unsigned long timerDelay2 = 60000;

#define B 4
#define G 16
#define R 17

#define led 13

int lux = 0;

// setting PWM properties
const int freq = 5000;
const int ledChannel1 = 0;
const int ledChannel2 = 1;
const int ledChannel3 = 2;
const int resolution = 8;

BH1750 lightMeter;

void initWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.println(WiFi.localIP());
  Serial.println();
}

void setup() {
  Serial.begin(9600);
  Wire.begin();
  lightMeter.begin();

  pinMode(led, OUTPUT);
  digitalWrite(led, LOW);

  ledcSetup(ledChannel1, freq, resolution);
  ledcSetup(ledChannel2, freq, resolution);
  ledcSetup(ledChannel3, freq, resolution);
  ledcAttachPin(R, ledChannel1);
  ledcAttachPin(G, ledChannel2);
  ledcAttachPin(B, ledChannel3);

  ledcWrite(ledChannel1, 0);
  ledcWrite(ledChannel2, 0);
  ledcWrite(ledChannel3, 0);

  initWiFi();

  config.api_key = API_KEY;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  config.database_url = DATABASE_URL;

  Firebase.reconnectWiFi(true);
  fbdo.setResponseSize(4096);
  config.token_status_callback = tokenStatusCallback;
  config.max_token_generation_retry = 5;

  Firebase.begin(&config, &auth);

  Serial.println("Getting User UID");
  while ((auth.token.uid) == "") {
    Serial.print('.');
    delay(1000);
  }
  uid = auth.token.uid.c_str();
  Serial.print("User UID: ");
  Serial.println(uid);
}

void loop() {
  if ((millis() - lastTime) > timerDelay) {
    luxRead();
    lastTime = millis();
  }

  if ((millis() - lastTime2) > timerDelay2) {
    Firebase.RTDB.setString(&fbdo, liveData + "/isNew", "true");
    lastTime2 = millis();
  }

  dbData();
}

void luxRead() {
  lux = lightMeter.readLightLevel();
  Serial.print("Light: ");
  Serial.print(lux);
  Serial.println(" lx");
  Firebase.RTDB.setString(&fbdo, liveData + "/lux", lux);
}

void dbData() {
  if (Firebase.RTDB.getString(&fbdo, liveData + "/mode")) {
    if (fbdo.dataType() == "string") {
      modes = fbdo.stringData();

      if (modes.equals("true")) {
        if (Firebase.RTDB.getString(&fbdo, liveData + "/Prediction")) {
          if (fbdo.dataType() == "string") {
            Prediction = fbdo.stringData();

            int firstComma = Prediction.indexOf(',');
            int secondComma = Prediction.lastIndexOf(',');

            pre_R = Prediction.substring(0, firstComma).toInt();
            pre_G = Prediction.substring(firstComma + 1, secondComma).toInt();
            pre_B = Prediction.substring(secondComma + 1).toInt();

            ledcWrite(ledChannel1, pre_R);
            ledcWrite(ledChannel2, pre_G);
            ledcWrite(ledChannel3, pre_B);

            Serial.printf("R: %d\nG: %d\nB: %d\n", pre_R, pre_G, pre_B);
          }
        } else {
          Serial.println(fbdo.errorReason());
        }
      } else if (modes.equals("false")) {
        fetchData(liveData + "/r", red);
        fetchData(liveData + "/g", green);
        fetchData(liveData + "/b", blue);
        fetchData(liveData + "/light", light);

        ledcWrite(ledChannel1, red.toInt());
        ledcWrite(ledChannel2, green.toInt());
        ledcWrite(ledChannel3, blue.toInt());

        // Fixed logic: control the light based on lux and light setting
        Serial.printf("lux: %d, light: %s\n", lux, light.c_str());
        if (light.equals("true") && lux < 10) {
          digitalWrite(led, HIGH);
          Notify_Message = "Turn on light";
          notify();
        } else {
          digitalWrite(led, LOW);
        }
      }
    }
  } else {
    Serial.println(fbdo.errorReason());
  }
}

void fetchData(const String& path, String& value) {
  if (Firebase.RTDB.getString(&fbdo, path)) {
    if (fbdo.dataType() == "string") {
      value = fbdo.stringData();
    }
  } else {
    Serial.println(fbdo.errorReason());
  }
}

void notify() {
  Firebase.RTDB.setString(&fbdo, notification + "/isNew", "true");
  delay(200);
  Firebase.RTDB.setString(&fbdo, notification + "/message", Notify_Message);
  delay(200);
}
