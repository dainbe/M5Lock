#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#include <M5Stack.h>
#include <ESP32Servo.h>

char *ssid = "<YOUR_SSID>";
char *password = "<YOUR_WIFI_PASSWORD>";

const char *endpoint = "<AWS_IOT_ENDPOINT>";
// Example: xxxxxxxxxxxxxx.iot.ap-northeast-1.amazonaws.com
const int port = 8883;
char *pubTopic = "$aws/things/<DEVICE_NAME>/shadow/update";
char *subTopic = "$aws/things/<DEVICE_NAME>/shadow/update/delta";

const char* rootCA = "-----BEGIN CERTIFICATE-----\n" \
"......" \
"-----END CERTIFICATE-----\n";

const char* certificate = "-----BEGIN CERTIFICATE-----\n" \
"......" \
"-----END CERTIFICATE-----\n";

const char* privateKey = "-----BEGIN RSA PRIVATE KEY-----\n" \
"......" \
"-----END RSA PRIVATE KEY-----\n";
long messageSentAt = 0;
char pubMessage[128];

int servo1Pin = 26;

// Published values for SG90 servos; adjust if needed
int minUs = 500;
int maxUs = 2400;

int pos = 0;      // position in degrees
bool lock_flag = true;  //true =  close
int sleeptime = 10; //second

Servo servo1; // create four servo objects
WiFiClientSecure httpsClient;
PubSubClient mqttClient(httpsClient);

void send_status() {
  sprintf(pubMessage, "{\"state\": {\"desired\":{\"lock_state\":%d}}}", lock_flag);
  Serial.print("Publishing message to topic ");
  Serial.println(pubTopic);
  Serial.println(pubMessage);
  mqttClient.publish(pubTopic, pubMessage);
  Serial.println("Published.");
}

void door_open() {
  M5.Lcd.wakeup();
  M5.Lcd.setBrightness(130);
  M5.Lcd.setTextFont(4);
  M5.Lcd.setTextSize(8);
  M5.Lcd.setCursor(80, 100, 1);
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextColor(GREEN, BLACK);
  M5.Lcd.println("open");
  servo1.attach(servo1Pin, minUs, maxUs);
  for (pos = 90; pos <= 180; pos += 1) {
    servo1.write(pos);
    delay(2);
  }
  delay(500);
  for (pos = 180; pos >= 90; pos -= 1) {
    servo1.write(pos);
    delay(2);
  }
  lock_flag = false;
  servo1.detach();

  M5.Lcd.setBrightness(20);

  send_status();
  delay(500);
}

void door_close() {
  M5.Lcd.setBrightness(130);
  M5.Lcd.setTextFont(4);
  M5.Lcd.setTextSize(10);
  M5.Lcd.setCursor(60, 100, 1);
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextColor(RED, BLACK);
  M5.Lcd.println("close");
  servo1.attach(servo1Pin, minUs, maxUs);
  for (pos = 90; pos >= 0; pos -= 1) {
    servo1.write(pos);
    delay(2);
  }
  delay(500);
  for (pos = 0; pos <= 90; pos += 1) {
    servo1.write(pos);
    delay(2);
  }
  lock_flag = true;
  servo1.detach();
  M5.Lcd.setBrightness(20);

  send_status();
  delay(500);
}

void setup() {
  Serial.begin(115200);
  M5.begin(true, false, true);
  M5.Lcd.clear(BLACK);
  M5.Lcd.setTextFont(4);
  M5.Lcd.setTextSize(3);
  M5.Lcd.setCursor(50, 80, 1);
  M5.Lcd.setTextColor(WHITE, BLACK);

  servo1.setPeriodHertz(50);      // Standard 50hz servo

  // Start WiFi
  Serial.println("Connecting to ");
  Serial.print(ssid);

  M5.Lcd.println("Connecting to ");
  M5.Lcd.setCursor(45, 110, 1);
  M5.Lcd.print(ssid);

  WiFi.begin(ssid, password);
  M5.Lcd.progressBar(40, 150, 240, 30, 0);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected.");

  M5.Lcd.fillRect(0, 0, 360, 140, 0);
  M5.Lcd.setCursor(80, 100, 1);
  M5.Lcd.println("Connected.");

  M5.Lcd.progressBar(40, 150, 240, 30, 30);

  delay(500);

  // Configure MQTT Client
  httpsClient.setCACert(rootCA);
  httpsClient.setCertificate(certificate);
  httpsClient.setPrivateKey(privateKey);
  mqttClient.setServer(endpoint, port);
  mqttClient.setCallback(mqttCallback);

  M5.Lcd.fillRect(0, 0, 360, 140, 0);
  M5.Lcd.setCursor(50, 80, 1);
  M5.Lcd.println("Connecting to ");
  M5.Lcd.setCursor(100, 110, 1);
  M5.Lcd.print("AWS IoT");

  M5.Lcd.progressBar(40, 150, 240, 30, 60);

  while (!mqttClient.connected()) {
    connectAWSIoT();
    delay(500);
  }
  M5.Lcd.fillRect(0, 0, 360, 140, 0);
  M5.Lcd.setCursor(80, 100, 1);
  M5.Lcd.println("Connected.");

  M5.Lcd.progressBar(40, 150, 240, 30, 90);
  delay(500);

  M5.Lcd.fillRect(0, 0, 360, 140, 0);
  M5.Lcd.setCursor(100, 100, 1);
  M5.Lcd.println("Finish!");

  M5.Lcd.progressBar(40, 150, 240, 30, 100);
  delay(500);

  door_close(); //初期化
}

void connectAWSIoT() {
  String clientId = "ESP32 - ";
  clientId += String(random(0xffff), HEX);
  // Attempt to connect
  if (mqttClient.connect(clientId.c_str())) {
    Serial.println("Connected.");
    int qos = 0;
    mqttClient.subscribe(subTopic, qos);
    Serial.println("Subscribed.");
  } else {
    Serial.print("Failed. Error state = ");
    Serial.println(mqttClient.state());
    // Wait 5 seconds before retrying
  }

}

void mqttCallback (char* topic, byte* payload, unsigned int length) {
  StaticJsonDocument<200> doc;
  char json[128];
  bool lock_state;

  Serial.print("Received. topic = ");
  Serial.println(topic);
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    json[i] = (char)payload[i];
  }
  Serial.print("\n");

  DeserializationError error = deserializeJson(doc, json);

  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.c_str());
    return;
  } else {
    lock_state = doc["state"]["lock_state"];
    Serial.print("lock_state = ");
    Serial.println(lock_state);
  }

  if (lock_state == true && lock_flag == false) { // 閉める命令で鍵が開いているとき
    Serial.println("close");
    door_close();

  } else if (lock_state == false && lock_flag == true) { // 開ける命令で閉まっているとき
    Serial.println("open");
    door_open();
  }
}



void mqttLoop() {
  if (!mqttClient.connected()) {
    connectAWSIoT();
    delay(500);
  } else {
    mqttClient.loop();
  }

  if (M5.BtnA.wasReleased() || M5.BtnB.wasReleased() || M5.BtnC.wasReleased()) {
    if (lock_flag) {
      door_open();
    }
    else {
      door_close();
    }
  }
  long now = millis();
  if (now - messageSentAt > 600000) {
    messageSentAt = now;
    send_status();
  }
}

void loop() {
  M5.update();
  //mqttLoop();
  if (!mqttClient.connected()) {
    connectAWSIoT();
    delay(500);
  } else {
    mqttClient.loop();
  }

  if (M5.BtnA.wasReleased() || M5.BtnB.wasReleased() || M5.BtnC.wasReleased()) {
    if (lock_flag) {
      door_open();
    }
    else {
      door_close();
    }
  }
  long now = millis();
  if (now - messageSentAt > 600000) {
    messageSentAt = now;
    send_status();
  }
}
