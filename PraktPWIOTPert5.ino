#include <WiFi.h>
#include <WiFiManager.h>
#include <Firebase_ESP_Client.h>
#include <ESP32Servo.h>
#include <Adafruit_NeoPixel.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <TimeLib.h>  // Konversi waktu UNIX ke format tanggal
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>

#define TRIG_PIN 8
#define ECHO_PIN 9
#define IR_SENSOR_PIN 34
#define SERVO_PIN 12
#define LED_PIN 38
#define NUM_LEDS 1

// Firebase credentials
#define API_KEY "AIzaSyBf8qpsjsQj9xdx0q0dzi_JEsQkXnxgqBY"
#define DATABASE_URL "https://monitoring-jarak-38889-default-rtdb.asia-southeast1.firebasedatabase.app/"

// Objek untuk Firebase dan NTP
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 25200, 60000); // GMT+7 (WIB)

unsigned long sendDataPrevMillis = 0;
bool signupOK = false;

Servo myServo;
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

void setup() {
  Serial.begin(115200);
  
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(IR_SENSOR_PIN, INPUT);
  myServo.attach(SERVO_PIN);
  strip.begin();
  strip.show();

  // Hubungkan ke WiFi
  WiFiManager wifiManager;
  wifiManager.autoConnect("ESP32_AP");
  Serial.println("Connected to WiFi");

  // Mulai NTP Client untuk waktu real-time
  timeClient.begin();

  // Konfigurasi Firebase
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("signUp OK");
    signupOK = true;
  } else {
    Serial.printf("%s\n", config.signer.signupError.message.c_str());
  }
  
  config.token_status_callback = tokenStatusCallback;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

float readUltrasonic() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  return pulseIn(ECHO_PIN, HIGH) * 0.034 / 2;
}

void loop() {
  timeClient.update(); // Update waktu NTP
  
  if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 5000 || sendDataPrevMillis == 0)) {
    sendDataPrevMillis = millis();
    
    float distance = readUltrasonic();
    int irValue = digitalRead(IR_SENSOR_PIN);
    time_t rawTime = timeClient.getEpochTime();

    // Format timestamp: YYYY-MM-DD HH:MM:SS
    String formattedDate = String(year(rawTime)) + "-" + 
                           String(month(rawTime)) + "-" + 
                           String(day(rawTime));
    String formattedTime = String(hour(rawTime)) + ":" + 
                           String(minute(rawTime)) + ":" + 
                           String(second(rawTime));
    
    String timestamp = formattedDate + " " + formattedTime;
    String path = "/sensor_readings/" + timestamp;

    Firebase.RTDB.setFloat(&fbdo, path + "/ultrasonic", distance);
    Firebase.RTDB.setInt(&fbdo, path + "/ir_sensor", irValue);
    Firebase.RTDB.setString(&fbdo, path + "/timestamp", timestamp);

    Serial.println("Data added at: " + path);
  }

  if (Firebase.RTDB.getInt(&fbdo, "/actuator/servo")) {
    int servoAngle = fbdo.intData(); 
    myServo.write(servoAngle);
    Serial.println("Servo moved to: " + String(servoAngle));
  }
  
  if (Firebase.RTDB.getInt(&fbdo, "/actuator/ws2812b/r")) {
    int red = fbdo.intData();
    if (Firebase.RTDB.getInt(&fbdo, "/actuator/ws2812b/g")) {
      int green = fbdo.intData();
      if (Firebase.RTDB.getInt(&fbdo, "/actuator/ws2812b/b")) {
        int blue = fbdo.intData();
        
        strip.setPixelColor(0, strip.Color(red, green, blue));
        strip.show();
        
        Serial.println("LED Color: R=" + String(red) + " G=" + String(green) + " B=" + String(blue));
      }
    }
  }
}