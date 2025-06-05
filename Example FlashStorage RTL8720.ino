#include "WiFi.h"
#include "WiFiServer.h"
#include "WiFiClient.h"
#include "FlashStorage_RTL8720.h"
typedef struct {
  char apssid[100];
  char appass[100];
  bool hidden;
} WiFiConfig;
const int CONFIG_SIGNATURE = 0xABCDEF12;
IPAddress local_ip(192, 168, 4, 1);
char *apssid = "CHOMTV";
char *appass = "@@@@2222";
int current_channel = 13;
WiFiServer server(80);

String urlDecode(String input) {
  input.replace("+", " ");
  input.replace("%20", " ");
  input.replace("%21", "!");
  input.replace("%22", "\"");
  input.replace("%23", "#");
  input.replace("%24", "$");
  input.replace("%25", "%");
  input.replace("%26", "&");
  input.replace("%27", "'");
  input.replace("%28", "(");
  input.replace("%29", ")");
  input.replace("%2A", "*");
  input.replace("%2B", "+");
  input.replace("%2C", ",");
  input.replace("%2F", "/");
  input.replace("%3A", ":");
  input.replace("%3B", ";");
  input.replace("%3C", "<");
  input.replace("%3D", "=");
  input.replace("%3E", ">");
  input.replace("%3F", "?");
  input.replace("%40", "@");
  input.replace("%5B", "[");
  input.replace("%5D", "]");
  return input;
}

#define RESET_BUTTON_PIN  PA12
#define RESET_HOLD_TIME  10000

void clearWiFiConfig() {
  Serial.println("Xóa dữ liệu WiFi trong flash...");
  WiFiConfig emptyConfig = {"", "", false};
  FlashStorage.put(0, 0xFFFFFFFF);
  FlashStorage.put(sizeof(CONFIG_SIGNATURE), emptyConfig);
  Serial.println("Xóa thành công, khởi động lại...");
  delay(2000);
  NVIC_SystemReset();
}

void checkResetButton() {
  static unsigned long buttonPressTime = 0;
  if (digitalRead(RESET_BUTTON_PIN) == LOW) {
    if (buttonPressTime == 0) {
      buttonPressTime = millis();
    } else if (millis() - buttonPressTime >= RESET_HOLD_TIME) {
      clearWiFiConfig();
    }
  } else {
    buttonPressTime = 0;
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);

  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);
  int signature;
  WiFiConfig storedConfig;
  FlashStorage.get(0, signature);

  if (signature == CONFIG_SIGNATURE) {
    FlashStorage.get(sizeof(signature), storedConfig);
    Serial.println("Dữ liệu đã tồn tại trong Flash:");
    Serial.println(storedConfig.apssid);
    Serial.println(storedConfig.appass);
  } else {
    Serial.println("Không có dữ liệu, sử dụng mặc định.");
    strncpy(storedConfig.apssid, apssid, sizeof(storedConfig.apssid) - 1);
    storedConfig.apssid[sizeof(storedConfig.apssid) - 1] = '\0';
    strncpy(storedConfig.appass, appass, sizeof(storedConfig.appass) - 1);
    storedConfig.appass[sizeof(storedConfig.appass) - 1] = '\0';
    storedConfig.hidden = false;
	FlashStorage.put(0, CONFIG_SIGNATURE);
    FlashStorage.put(sizeof(signature), storedConfig);
  }

  WiFi.config(local_ip);
  WiFi.apbegin(storedConfig.apssid, storedConfig.appass, (char *)String(current_channel).c_str(), storedConfig.hidden);
  server.begin();
  digitalWrite(LED_R, HIGH);
}
void loop() {
  checkResetButton();
  WiFiClient client = server.available();
  if (client) {
    String request = client.readStringUntil('\r');  
    client.flush();
    if (request.indexOf("GET /?apssid=") != -1) {
      String query = request.substring(request.indexOf("?") + 1);        
      String newapssid = getValue(query, "apssid=", "&");
      String newappass = getValue(query, "appass=", query.indexOf("&hidden=") != -1 ? "&" : " ");     
      bool hidden = query.indexOf("hidden=") != -1;   
      newapssid = urlDecode(newapssid); 
      newappass = urlDecode(newappass);
      WiFiConfig newConfig;
      newapssid.trim();
      newappass.trim();
      newapssid.toCharArray(newConfig.apssid, sizeof(newConfig.apssid));
      newappass.toCharArray(newConfig.appass, sizeof(newConfig.appass));
      newConfig.hidden = hidden;
      FlashStorage.put(0, CONFIG_SIGNATURE);  
      FlashStorage.put(sizeof(CONFIG_SIGNATURE), newConfig);  

      delay(2000); 
      sendHomePage(client);
    } else {
      sendHomePage(client);  
    }
  }
}
String makeResponse(int code, String content_type) {
  String response = "HTTP/1.1 " + String(code) + " OK\n";
  response += "Content-Type: " + content_type + "\n";
  response += "Connection: close\n\n";
  return response;
}
void sendHomePage(WiFiClient& client) {
  WiFiConfig storedConfig;
  FlashStorage.get(sizeof(CONFIG_SIGNATURE), storedConfig); 
  String currentapssid = (strlen(storedConfig.apssid) > 0) ? String(storedConfig.apssid) : String(apssid);
  String currentappass = (strlen(storedConfig.appass) > 0) ? String(storedConfig.appass) : String(appass);
  String hideapssid = storedConfig.hidden ? "checked" : "";
  String response = makeResponse(200, "text/html") + R"(
    <!DOCTYPE HTML>
    <html>
    <head>
      <meta charset="UTF-8" name="viewport" content="width=device-width, initial-scale=1.0">
    </head>
    <body>
      <h2>Cấu hình WiFi</h2>
      <form action="/" method="GET">
        <label>Tên WiFi:</label></br>
        <input type='text' name='apssid' value=')" + currentapssid + R"('></br>
        <label>Mật khẩu WiFi:</label></br>
        <input type='text' name='appass' value=')" + currentappass + R"('></br>
        <label>Ẩn mạng:</label>
        <input style='width: 20px;' type='checkbox' name='hidden' )" + hideapssid + R"(>
		<input class='btn_save' type='submit' value='Lưu'>
      </form>
    </body>
    </html>
  )";

  client.write(response.c_str());
}
String getValue(String data, String key, String endChar) {
  int startIndex = data.indexOf(key) + key.length();
  int endIndex = data.indexOf(endChar, startIndex);
  return data.substring(startIndex, endIndex);
}