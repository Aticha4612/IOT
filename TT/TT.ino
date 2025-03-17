#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <DHT.h>
#include <HTTPClient.h>

// **ข้อมูล WiFi และ Blynk**
const char ssid[] = "00";        // ชื่อ Wi-Fi
const char pass[] = "123456789";    // รหัสผ่าน Wi-Fi
const char auth[] = "Ca9S9BOutnzOnKRXX7UIav76sFdtZVsY";  // Auth Token จาก Blynk

// **ข้อมูล Telegram Bot**
const char* telegramBotToken = "8178669136:AAHmOXetwGN10LQF6LE1vfDNJkbf45XzArc";
const char* chatID = "7631878885";

// **กำหนดขาเซนเซอร์**
#define SOUND_SENSOR_A0 34  // KY-038 (A0) → GPIO 34
#define SOUND_SENSOR_D0 4   // KY-038 (D0) → GPIO 4
#define DHTPIN 15           // DHT11 (DATA) → GPIO 15
#define DHTTYPE DHT11       // ใช้ DHT11
#define SAMPLE_WINDOW 50    // เวลาสุ่มตัวอย่างเสียง (ms)

// **ค่ากำหนด**
#define SOUND_THRESHOLD 50  // ตั้งค่าแจ้งเตือนเมื่อเสียงเกิน 50 dB
#define TELEGRAM_COOLDOWN 30000 // หน่วงเวลา 30 วินาที
#define REFERENCE_VOLTAGE 0.0005 // ค่ามาตรฐานอ้างอิงสำหรับคำนวณ dB

DHT dht(DHTPIN, DHTTYPE);
unsigned long lastTelegramSent = 0;

void setup() {
    Serial.begin(115200);
    pinMode(SOUND_SENSOR_D0, INPUT);
    dht.begin();

    // **เชื่อมต่อ WiFi**
    WiFi.begin(ssid, pass);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi Connected!");

    // **เชื่อมต่อ Blynk Server**
    Blynk.begin(auth, ssid, pass, "iotservices.thddns.net", 5535);
    Blynk.connect();
}

// **ฟังก์ชันแจ้งเตือน Telegram**
void sendTelegramMessage(float soundLevel) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("❌ WiFi Disconnected!");
        return;
    }
    
    HTTPClient http;
    http.begin("https://api.telegram.org/bot" + String(telegramBotToken) + "/sendMessage");
    http.addHeader("Content-Type", "application/json");

    String message = "{\"chat_id\":\"" + String(chatID) + "\",\"text\":\"⚠️ เสียงดังเกินกำหนด! ระดับเสียง: " + String(soundLevel) + " dB\"}";
    int httpResponseCode = http.POST(message);

    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    
    if (httpResponseCode > 0) {
        Serial.println("✅ Telegram sent successfully!");
    } else {
        Serial.println("❌ Error sending Telegram.");
    }
    
    http.end();
}

void loop() {
    Blynk.run();

    // **อ่านค่าเสียงจาก KY-038**
    unsigned long startMillis = millis();
    int signalMax = 0;
    int signalMin = 4095;

    while (millis() - startMillis < SAMPLE_WINDOW) {
        int sample = analogRead(SOUND_SENSOR_A0);
        if (sample > signalMax) signalMax = sample;
        if (sample < signalMin) signalMin = sample;
    }

    int peakToPeak = signalMax - signalMin;
    float voltage = (peakToPeak * 3.3) / 4095.0;
    float dB = (voltage > 0) ? 20 * log10(voltage / REFERENCE_VOLTAGE) : 0;

    // **อ่านค่าจาก DHT11**
    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();

    if (isnan(temperature) || isnan(humidity)) {
        Serial.println("❌ Error reading DHT sensor!");
    } else {
        Blynk.virtualWrite(V0, dB);
        Blynk.virtualWrite(V1, temperature);
        Blynk.virtualWrite(V2, humidity);
        Blynk.virtualWrite(V3, voltage);

        Serial.print("Temp: ");
        Serial.print(temperature);
        Serial.print(" °C, Humidity: ");
        Serial.print(humidity);
        Serial.print(" %, Voltage: ");
        Serial.print(voltage);
        Serial.print(" V, Sound Level: ");
        Serial.print(dB);
        Serial.println(" dB");

        if (dB > SOUND_THRESHOLD && millis() - lastTelegramSent > TELEGRAM_COOLDOWN) {
            sendTelegramMessage(dB);
            lastTelegramSent = millis();
        }
    }
    delay(500);
}
