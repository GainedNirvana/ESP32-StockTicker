#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include <WiFiClientSecure.h>
#include "time.h"

const char* wifi_ssid = "SSID";         // replace with your Wi-Fi Network Name
const char* wifi_password = "PASSWORD";      //replace with your Wi-Fi Password
const char* apikey = "KEY";     // replace with your api key
const char* stocklabel = "AAPL";            //ticker label here
const long updateinterval = 60000;     

const char* apiHost = "finnhub.io";
String apiPath = "/api/v1/quote?symbol=" + String(stocklabel) + "&token=" + String(apikey);

TFT_eSPI tft = TFT_eSPI();
const int center_x = 120;
const int center_y = 120;

long lastUpdate = 0;
float currentPrice = 0.0;
float previousPrice = 0.0; 
float priceChange = 0.0;

const char* ntpServer = "pool.ntp.org";
const long  gmtoffset_sec = -28800; // -8 hours for PST
const int   daylightsavingoffset_sec = 3600; // 1 hour for daylights saving
String currentime = "00:00:00"; // To hold the current local time string
long lastClockUpdate = 0; 

void initWiFi();
bool fetchStockPrice();
void updateDisplay();

void initTime() {
  configTime(gmtoffset_sec, daylightsavingoffset_sec, ntpServer);
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
    tft.drawString("Program Started...", center_x, center_y - 30);

   
    initWiFi();
    initTime();

    tft.drawString("Syncing Time...", center_x, center_y);
    configTime(gmtoffset_sec, daylightsavingoffset_sec, ntpServer); 
    Serial.println("Local time synced");

    tft.drawString("Fetching Data...", center_x, center_y + 30);
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

    if (millis() - lastUpdate > updateinterval) {
      
        Serial.println("Attempting to fetch new stock price...");
        if (fetchStockPrice()) {
            updateDisplay(); 
        }
        lastUpdate = millis(); 
    }
    
    delay(10); 
}

void initWiFi() {
    WiFi.begin(wifi_ssid, wifi_password);
    Serial.print("Connecting to WiFi...");

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nConnected to WiFi!");
    } else {
        Serial.println("\nFailed to connect to WiFi");
        tft.fillScreen(TFT_RED);
        tft.drawString("Wi-Fi Error!", center_x, center_y);
    }
}

bool fetchStockPrice() {
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;

    String url = "https://" + String(apiHost) + apiPath; 

    Serial.println("API URL Check:");
    Serial.println(url);

    http.begin(client, url);
    int httpCode = http.GET();
    
    if (httpCode > 0) {
        if (httpCode == HTTP_CODE_OK) {
            String payload = http.getString();
            Serial.print("API Response Received");
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
                Serial.printf("Data fetched. Current Price: $%.2f, Change: $%.2f\n", currentPrice, priceChange);
                http.end();
                return true; 

            } else {
                Serial.println("JSON structure error: Check API key validity.");
            }
        }
    } else {
        Serial.printf("HTTP GET failed, error: %s\n", http.errorToString(httpCode).c_str());
    }

    http.end();
    return false;
}

void updateClock() {

    const int clock_y_position = 30;

    if (millis() - lastClockUpdate >= 1000) {
        struct tm timeinfo;
        if (getLocalTime(&timeinfo)) {
            char timeStr[9]; 
            strftime(timeStr, 9, "%H:%M:%S", &timeinfo);
            
            if (currentime != String(timeStr)) {
                
                tft.fillRect(center_x - 50, clock_y_position - 10, 100, 20, TFT_BLACK); 
                
                tft.setTextFont(2); 
                tft.setTextColor(TFT_MAGENTA, TFT_BLACK);
                tft.setTextDatum(MC_DATUM); 
            
                tft.drawString(timeStr, center_x, clock_y_position);
                
                currentime = String(timeStr);
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
    tft.drawString(currentime, center_x, 30); 
    tft.setTextFont(6); 
    tft.setTextColor(TFT_CYAN);
    tft.drawString(stocklabel, center_x, 75); 

    tft.setTextFont(7); 
    tft.setTextColor(priceColor);
    
    String priceStr = "$" + String(currentPrice, 2); 
    tft.drawString(priceStr, center_x, 135); 

    tft.setTextFont(4);
    String changeStr = changeIcon + String(priceChange, 2); 
    tft.drawString(changeStr, center_x, 175); 

    tft.setTextFont(1); 
    tft.setTextColor(TFT_DARKGREY);
    tft.drawString("Refreshed: " + refreshTime, center_x, 205); 
}


