#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <R4HttpClient.h>
#include <ArduinoJson.h>
#include "ArduinoGraphics.h"
#include "Arduino_LED_Matrix.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET    -1

WiFiSSLClient client;
R4HttpClient http;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

ArduinoLEDMatrix matrix;

const long blocksPerHalving = 210000;
const float initialReward = 50.0;
const float maxSupply = 21000000.0;

const char* ssid = "EndTheFed";  // Replace with your WiFi network name
const char* password = "NoW1r3s4Me";  // Replace with your WiFi password

long SATOSHIS_IN_ONE_BTC = 100000000;

void setup() {
  Serial.begin(9600);
  while (!Serial);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  Serial.print("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi!");
  displayCenteredText("Connected to WiFi", 1, 0, 0);
  matrix.begin();
  drawBitcoinB();
}

void loop() {
  checkWiFiConnection();
  Serial.println("Getting block height...");
  display.clearDisplay();
  String blockHeightStr = getBitcoinBlockHeight();

  if (blockHeightStr == "-1") {
    display.print("Error fetching block height");
  } else {
    String blockLabel = "-Block Height-"; 
    displayCenteredText(blockLabel, 1, 0, -12);
    displayCenteredText(formatWithCommas(blockHeightStr), 2, 0, 0);
  }
  display.display();
  delay(5000);  // Wait before fetching prices

  float percentMined = 0;
  if(blockHeightStr != "-1"){
    display.clearDisplay();
    displayCenteredText("-Percent Mined-", 1, 0, -12);
    percentMined = calculatePercentageMined(blockHeightStr.toInt()); 
    displayCenteredText(String(percentMined) + "%", 2, 0, 0);
  }

  Serial.println("Getting Bitcoin price...");
  display.clearDisplay();
  String priceUSD = getBitcoinPrice("USD");

  if (priceUSD == "-1") {
    display.print("Error fetching price");
  } else {
    displayCenteredText("-USD per BTC-", 1, 0, -12);
    displayCenteredText("$"+formatWithCommas(priceUSD), 2, 0, 0);
    delay(5000); 

    int satsPerDollar = SATOSHIS_IN_ONE_BTC / priceUSD.toInt();
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(15, 9);
    display.print("MSK");
    display.setCursor(10, 15);
    display.print("-----");
    display.setCursor(12, 15);
    display.print("----");
    display.setCursor(10, 21);
    display.print("UTC+3");
    // Display the Satoshis per Dollar on the right side with larger text
    display.setTextSize(2); // Large text for the value
    displayCenteredText(moscowTime(satsPerDollar), 2, 12, 0);
    display.display();
    delay(5000);  // Wait 10 seconds before updating again
  }

  if(blockHeightStr != "-1" && priceUSD != "-1"){
      String marketCap = calculateMarketCap(priceUSD.toFloat(), percentMined); 
      display.clearDisplay();
      displayCenteredText("-Market Cap-", 1, 0, -12);
      displayCenteredText(String(marketCap), 2, 0, 0);
  }
}

String moscowTime(int satsPerDollar) {
  int hours = satsPerDollar / 100;
  int minutes = satsPerDollar % 100;

  // Format the hours and minutes with leading zeros if needed
  String formattedTime = "";
  if (hours < 10) formattedTime += "0";  // Add leading zero for hours
  formattedTime += String(hours) + ":";

  if (minutes < 10) formattedTime += "0";  // Add leading zero for minutes
  formattedTime += String(minutes);

  return formattedTime;
}

String getBitcoinBlockHeight() {
  if (WiFi.status() == WL_CONNECTED) {
    http.begin(client, "https://mempool.space/api/blocks", 443);
    http.setTimeout(3000);
    int httpResponseCode = http.GET();
    if (httpResponseCode == HTTP_CODE_OK) {
      String response = http.getBody();
      http.close();
      Serial.println("Block data retrieved");

      DynamicJsonDocument doc(4096);
      DeserializationError error = deserializeJson(doc, response);

      //TODO there is an issue with the length need to fix. 
      if (!error || error) {
        int highestHeight = 0;
        for (JsonObject block : doc.as<JsonArray>()) {
          int height = block["height"];
          if (height > highestHeight) {
            highestHeight = height;
          }
        }
        return String(highestHeight);
      } else {
        Serial.print("JSON deserialization failed: ");
        Serial.println(error.f_str());
      }
    } else {
      Serial.print("HTTP request failed, error code: ");
      Serial.println(httpResponseCode);
    }
    http.close();
  } else {
    Serial.println("WiFi not connected");
  }
  return "-1";
}

float calculatePercentageMined(long currentBlockHeight) {
    // Constants
    float totalMined = 0;
    
    // Calculate total mined based on halving events
    int halvingCount = currentBlockHeight / blocksPerHalving;
    for (int i = 0; i < halvingCount; i++) {
        totalMined += blocksPerHalving * (initialReward / pow(2, i));
    }
    
    // Add remaining blocks in the current cycle if any
    int remainingBlocks = currentBlockHeight % blocksPerHalving;
    if (remainingBlocks > 0) {
        totalMined += remainingBlocks * (initialReward / pow(2, halvingCount));
    }

    // Calculate percentage mined
    float percentageMined = (totalMined / maxSupply) * 100.0;
    return round(percentageMined * 100) / 100;  // Round to nearest hundredth
}


String getBitcoinPrice(const char* currency) {
  if (WiFi.status() == WL_CONNECTED) {
    http.begin(client, "https://mempool.space/api/v1/prices", 443);
    http.setTimeout(3000);
    int httpResponseCode = http.GET();
    if (httpResponseCode == HTTP_CODE_OK) {
      String response = http.getBody();
      http.close();
      Serial.println("Price data retrieved");

      DynamicJsonDocument doc(1024);
      DeserializationError error = deserializeJson(doc, response);

      if (!error) {
        if (doc.containsKey(currency)) {
          int price = doc[currency];
          return String(price);
        } else {
          Serial.print("Currency not found: ");
          Serial.println(currency);
        }
      } else {
        Serial.print("JSON deserialization failed: ");
        Serial.println(error.f_str());
      }
    } else {
      Serial.print("HTTP request failed, error code: ");
      Serial.println(httpResponseCode);
    }
    http.close();
  } else {
    Serial.println("WiFi not connected");
  }
  return "-1";
}

void checkWiFiConnection() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected. Attempting to reconnect...");
    display.clearDisplay();
    display.setCursor(0, 0);
    display.setTextSize(1);
    display.print("Reconnecting WiFi...");
    display.display();

    WiFi.disconnect();  // Optional: Ensure we start fresh
    WiFi.begin(ssid, password);

    unsigned long startAttemptTime = millis();

    // Keep trying to reconnect for 10 seconds
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
      delay(500); // Small delay between each retry
      Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nReconnected to WiFi!");
      display.clearDisplay();
      display.setCursor(0, 0);
      display.print("Reconnected to WiFi!");
      display.display();
      delay(2000);  // Display success message briefly
    } else {
      Serial.println("\nFailed to reconnect to WiFi.");
      display.clearDisplay();
      display.setCursor(0, 0);
      display.print("WiFi reconnect failed.");
      display.display();
      delay(2000);  // Display failure message briefly
    }
  }
}

void displayCenteredText(const String &text, int textSize, int16_t xOffset, int16_t yOffset) {
  display.setTextSize(textSize);
  // Calculate X position to center text
  int16_t xCenter = (SCREEN_WIDTH - text.length() * 6 * textSize) / 2;
  int16_t yCenter = ((SCREEN_HEIGHT - 4 * textSize) / 2) + yOffset;

  display.setCursor(xCenter + xOffset, yCenter);
  display.print(text);
  display.display();
}

String formatWithCommas(String numberStr) {
  int len = numberStr.length();

  // Start from the end of the string and insert commas every 3 digits
  for (int i = len - 3; i > 0; i -= 3) {
    numberStr = numberStr.substring(0, i) + "," + numberStr.substring(i);
  }
  return numberStr;
}

void drawBitcoinB() {
  matrix.clear();
  uint32_t bitcoin[][4] = {
    {
      0x901f808,
      0x80f00880,
      0x881f8090,
      66
    },
    {
      0x903fc18,
      0xc1fc1fc1,
      0x8c3fc090,
      66
    },
    {
        0x3184a444,
        0x44042081,
        0x100a0040
    },
    {
        0x318ff830,
        0xc3f830c3,
        0xcff8318,
        66
    }
  };

  matrix.stroke(0xFFFFFFFF);
  matrix.textScrollSpeed(50);

  const char text[] = "  Buy Bitcoin  ";
  matrix.textFont(Font_5x7);
  matrix.beginText(0, 1, 0xFFFFFF);
  matrix.println(text);
  matrix.endText(SCROLL_LEFT);
  matrix.endDraw();
  
  delay(1000); 
  matrix.loadFrame(bitcoin[0]);
  
  delay(1000); 
  matrix.loadFrame(bitcoin[1]);
    
  delay(1000); 
  matrix.loadFrame(bitcoin[2]);
  
  delay(1000); 
  matrix.loadFrame(bitcoin[0]);
}


String calculateMarketCap(float priceUSD, float percentMined) {
    float marketCap = (priceUSD * maxSupply) * (percentMined/100);
    // Format the market cap
    String marketCapStr;
    if (marketCap >= 1e12) {
        marketCap = round(marketCap / 1e10) / 100.0;  // Round to nearest hundredth
        marketCapStr = String(marketCap, 2) + "T";    // Trillions
    } else if (marketCap >= 1e9) {
        marketCap = round(marketCap / 1e7) / 100.0;   // Round to nearest hundredth
        marketCapStr = String(marketCap, 2) + "B";    // Billions
    } else {
        marketCapStr = String(marketCap, 0);          // Exact value if less than billion
    }

    return "$" + marketCapStr;
}
