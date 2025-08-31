#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>  
#include <Adafruit_ST7735.h>

#define TFT_CS 5
#define TFT_RST 39
#define TFT_MOSI 37
#define TFT_SCLK 35
#define TFT_DC 33

int busComing = 0;

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);
const char* ssid = "";
const char* password = "";

const char* apiKey = "";

const char* ntpServer = "pool.ntp.org";

void setup() {
  Serial.begin(115200);
  delay(1000);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");

  configTime(0, 0, ntpServer);
  setenv("TZ", "EST5EDT,M3.2.0,M11.1.0", 1);
  tzset();  

  Serial.print("Waiting for time sync");
  while (time(nullptr) < 100000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" done!");
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);
  tft.fillScreen(ST77XX_BLACK);
  Serial.println("tft set");
  
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    String url = "https://external.transitapp.com/v3/public/stop_departures?global_stop_id=GMTVT:15513";
    Serial.println("Sending GET request to:");
    Serial.println(url);

    http.begin(url);
    http.addHeader("apiKey", apiKey);

    int httpCode = http.GET();

    if (httpCode > 0) {
      Serial.printf("HTTP GET code: %d\n", httpCode);
      String payload = http.getString();

      const size_t capacity = 60 * 1024;
      DynamicJsonDocument doc(capacity);
      DeserializationError error = deserializeJson(doc, payload);

      if (error) {
        Serial.print("JSON deserialization failed: ");
        Serial.println(error.c_str());
        return;
      }

      long nextDeparture = 0;
      JsonArray route_departures = doc["route_departures"];
      if (route_departures.size() > 0) {
        JsonArray itineraries = route_departures[0]["itineraries"];
        if (itineraries.size() > 0) {
          JsonArray schedule_items = itineraries[0]["schedule_items"];
          for (JsonObject item : schedule_items) {
            if (!item["is_cancelled"]) {
              nextDeparture = item["departure_time"];
              break;
            }
          }
        }
      }

      if (nextDeparture > 0) {
        time_t now = time(nullptr);
        long diffSeconds = nextDeparture - now;
        int diffMinutes = diffSeconds / 60;
        busComing = diffMinutes;

        if (diffMinutes >= 0) {
          // Convert next departure to EST/EDT
          time_t departureTime = nextDeparture;
          struct tm * timeinfo = localtime(&departureTime);
          char timeString[16];
          strftime(timeString, sizeof(timeString), "%I:%M %p", timeinfo);

          if (timeString[0] == '0') {
            memmove(timeString, timeString + 1, strlen(timeString));
          }


          Serial.print("Next bus in: ");
          Serial.print(diffMinutes);
          Serial.println(" minutes");

          Serial.print("Next bus time (local/EST): ");
          Serial.println(timeString);
        } else {
          Serial.println("Next bus time already passed.");
        }
      } else {
        Serial.println("No upcoming departures found.");
      }

    } else {
      Serial.printf("GET request failed, error: %s\n", http.errorToString(httpCode).c_str());
    }

    http.end();
    tft.setTextSize(2);
    tft.setCursor(0,0);
    tft.write("Next bus: ");
    tft.write(busComing.c_str());
    Serial.println(busComing);

    delay(15000); 
  }
}
