#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include <WiFiClientSecure.h>
#include "time.h"

// --- Configuration ---
const char* WIFI_SSID = "SSID";         // <-- **REPLACE THIS** with your Wi-Fi Network Name
const char* WIFI_PASS = "PASSWORD";      // <-- **REPLACE THIS** with your Wi-Fi Password
const char* API_KEY = "KEY";     // **<-- YOUR API KEY IS HERE**
const char* STOCK_SYMBOL = "AAPL";            //PUT YOUR STOCK LABEL HERE
const long UPDATE_INTERVAL_MS = 60000;      // Update every 60 seconds (60000 ms)

// API URL 
const char* apiHost = "finnhub.io";
String apiPath = "/api/v1/quote?symbol=" + String(STOCK_SYMBOL) + "&token=" + String(API_KEY);

TFT_eSPI tft = TFT_eSPI();
const int CENTER_X = 120;
const int CENTER_Y = 120;

long lastUpdate = 0;
float currentPrice = 0.0;
float previousPrice = 0.0; // To track changes

float priceOpen = 0.0;
float priceHigh = 0.0;
float priceLow = 0.0;
float priceChange = 0.0;
String latestTime = "N/A"; 
String latestDate = "N/A"; 

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = -28800; // -8 hours for Pacific Standard Time (PST)
const int   daylightOffset_sec = 3600; // 1 hour for Daylight Saving
String currentTimeStr = "00:00:00"; // To hold the current local time string
long lastClockUpdate = 0; 

void initWiFi();
bool fetchStockPrice();
void updateDisplay();

void initTime() {
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.println("Time configured via NTP.");
}

void setup() {
    Serial.begin(115200);

    
    tft.init();
    tft.setRotation(0); 
    tft.fillScreen(TFT_BLACK);
    tft.setTextFont(4); 
    tft.setTextColor(TFT_WHITE);
    tft.setTextDatum(MC_DATUM); 
    tft.drawString("Program Started...", CENTER_X, CENTER_Y - 30);

   
    initWiFi();
    initTime();

    tft.drawString("Syncing Time...", CENTER_X, CENTER_Y);
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer); 
    Serial.println("Local time synced via NTP.");

    tft.drawString("Fetching Data...", CENTER_X, CENTER_Y + 30);
    if (fetchStockPrice()) {
        previousPrice = currentPrice;
    }
}

void loop() {
   
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Wi-Fi lost. Reconnecting...");
        initWiFi(); 
        delay(5000); 
        return;
    }

    updateClock(); 

    if (millis() - lastUpdate > UPDATE_INTERVAL_MS) {
        Serial.println("--- Attempting to fetch new stock price... ---");
        
        if (fetchStockPrice()) {
            updateDisplay(); 
        }
        
        lastUpdate = millis(); 
    }
    
    delay(10); 
}

void initWiFi() {
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.print("Connecting to WiFi ");

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nConnected to WiFi!");
    } else {
        Serial.println("\nFailed to connect to WiFi. Check SSID/Pass.");
        tft.fillScreen(TFT_RED);
        tft.drawString("Wi-Fi Error!", CENTER_X, CENTER_Y);
    }
}

bool fetchStockPrice() {
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;

    String url = "https://" + String(apiHost) + apiPath; 

    Serial.println("--- API URL Check: ---");
    Serial.println(url);

    http.begin(client, url);
    int httpCode = http.GET();
    
    if (httpCode > 0) {
        if (httpCode == HTTP_CODE_OK) {
            String payload = http.getString();
            Serial.print("API Response Received (Partial): ");
            Serial.println(payload.substring(0, 100) + "...");

            DynamicJsonDocument doc(4096);
            DeserializationError error = deserializeJson(doc, payload);

            if (error) {
                Serial.print("JSON parsing failed: ");
                Serial.println(error.c_str());
                http.end();
                return false;
            }

            if (doc.containsKey("c") && doc.containsKey("d")) {
                
                previousPrice = currentPrice;
    
                currentPrice = doc["c"].as<float>();
                float priceChange = doc["d"].as<float>(); 
                
                Serial.printf("Finnhub data fetched. Current Price: $%.2f, Change: $%.2f\n", currentPrice, priceChange);
                http.end();
                return true; 

            } else {
                Serial.println("JSON structure error: Missing 'c' or 'd' key. Check API key validity.");
            }
        }
    } else {
        Serial.printf("HTTP GET failed, error: %s\n", http.errorToString(httpCode).c_str());
    }

    http.end();
    return false;
}

void updateClock() {

    const int CLOCK_Y_POS = 30;
    

    if (millis() - lastClockUpdate >= 1000) {
        struct tm timeinfo;
        if (getLocalTime(&timeinfo)) {
            char timeStr[9]; 
            strftime(timeStr, 9, "%H:%M:%S", &timeinfo);
            
            if (currentTimeStr != String(timeStr)) {
                
                tft.fillRect(CENTER_X - 50, CLOCK_Y_POS - 10, 100, 20, TFT_BLACK); 
                
                tft.setTextFont(2); 
                tft.setTextColor(TFT_MAGENTA, TFT_BLACK);
                tft.setTextDatum(MC_DATUM); 
            
                tft.drawString(timeStr, CENTER_X, CLOCK_Y_POS);
                
                currentTimeStr = String(timeStr);
            }
        }
        lastClockUpdate = millis();
    }
}

void updateDisplay() {
    struct tm timeinfo;
    char refreshTimeStr[12];  
    String refreshTime;
    
    if (getLocalTime(&timeinfo)) {
        strftime(refreshTimeStr, 12, "%H:%M:%S", &timeinfo);
        refreshTime = String(refreshTimeStr);
    } else {
        refreshTime = "Time N/A";
    }

    uint32_t priceColor = TFT_WHITE;
    String changeIcon = "";
    
    if (priceChange > 0.01) { 
        priceColor = TFT_GREEN; 
        changeIcon = "▲ ";
    } else if (priceChange < -0.01) { 
        priceColor = TFT_RED;    
        changeIcon = "▼ ";
    } else {
        priceColor = TFT_YELLOW; 
    }

    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);

    tft.setTextFont(2); 
    tft.setTextColor(TFT_MAGENTA);
    tft.drawString(currentTimeStr, CENTER_X, 30); 

    tft.setTextFont(6); 
    tft.setTextColor(TFT_CYAN);
    tft.drawString(STOCK_SYMBOL, CENTER_X, 75); 

    tft.setTextFont(7); 
    tft.setTextColor(priceColor);
    
    String priceStr = "$" + String(currentPrice, 2); 
    tft.drawString(priceStr, CENTER_X, 135); 

    tft.setTextFont(4);
    String changeStr = changeIcon + String(priceChange, 2); 
    tft.drawString(changeStr, CENTER_X, 175); 

    tft.setTextFont(1); 
    tft.setTextColor(TFT_DARKGREY);
    tft.drawString("Refreshed: " + refreshTime, CENTER_X, 205); 
}
