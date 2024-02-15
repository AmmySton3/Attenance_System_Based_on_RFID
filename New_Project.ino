#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <SPI.h>
#include <MFRC522.h>
#include <SoftwareSerial.h>
#include <LiquidCrystal_I2C.h>
#include <ArduinoJson.h>
//#define WATCHDOG_TIMEOUT 10 // seconds

LiquidCrystal_I2C lcd(0x27,16,2);

//Variables
String tag;
constexpr uint8_t RST_PIN = D3;
constexpr uint8_t SS_PIN = D4; 

MFRC522 rfid(SS_PIN, RST_PIN); // Instance of the class
MFRC522::MIFARE_Key key;

int ledPin = D8;
int buzzer = D0;
int i = 0;

const char* ssid = "MSB";
const char* passphrase = "";
const char* serverName = "";

String accesstoken = "IamAuthorized";
String rfidNo = "RFID-24-001";
String rfidLocation = "MSB";
String st;
String content;
int statusCode;
const char* user = "Card not found";
 
 
//Function Declaration
bool testWifi(void);
void launchWeb(void);
void setupAP(void);
 
//--------Establishing Local server at port 80 whenever required
ESP8266WebServer server(80);
 
void setup()
{
   pinMode(buzzer, OUTPUT);
   pinMode(ledPin, OUTPUT);
  Serial.begin(9600); //Initializing if(DEBUG)Serial Monitor
  SPI.begin(); // Init SPI bus
  rfid.PCD_Init(); // Init MFRC522
 // ESP.wdtEnable(WDTO_8S); // enable 8-second watchdog timer3
  lcd.init();    //Initializing LCD
  lcd.backlight();
  lcd.clear();
  Serial.println();
  Serial.println("Disconnecting previously connected WiFi");
  WiFi.disconnect();
  EEPROM.begin(512); //Initializing EEPROM
  delay(10);
  Serial.println();
  Serial.println();
  Serial.println("Startup");
 
  // Read eeprom for ssid and password
  Serial.println("Reading EEPROM ssid");
 
  String esid;
  for (int i = 0; i < 32; ++i)
  {
    esid += char(EEPROM.read(i));
  }
  Serial.println();
  Serial.print("SSID: ");
  Serial.println(esid);
  Serial.println("Reading EEPROM pass");
 
  String epass = "";
  for (int i = 32; i < 96; ++i)
  {
    epass += char(EEPROM.read(i));
  }
  Serial.print("PASS: ");
  Serial.println(epass); 
 
  WiFi.begin(esid.c_str(), epass.c_str());
  powerOn();
  if (testWifi())
  {
    Serial.println("Succesfully Connected!!!");
    digitalWrite(ledPin, HIGH); // turn LED ON
    delay(2000);
    digitalWrite(ledPin, LOW); // turn LED ON
    return;
  }
  else
  {
    Serial.println("Turning the HotSpot On");
    launchWeb();
    setupAP();// Setup accesspoint or HotSpot
  }
 
  Serial.println();
  Serial.println("Waiting.");
  
  while ((WiFi.status() != WL_CONNECTED))
  {
    Serial.print(".");
    lcd.setCursor(0,0);
    lcd.print("Wait for wifi to");
    lcd.setCursor(0,1);
    lcd.print("be connected");
    delay(1000);
    lcd.clear();
    testWifi(); 
    //server.handleClient();
    resetNodeMCU();
  }
}

void loop() {
  lcd.setCursor(4,0);
  lcd.print("MSB");
  lcd.setCursor(0,1);
  lcd.print("Ston3 Tech Ltd");
 
  if ( ! rfid.PICC_IsNewCardPresent())
    return;
  if (rfid.PICC_ReadCardSerial()) {
    for (int i = rfid.uid.size - 1; i >= 0; i--) {
      tag.concat(String(rfid.uid.uidByte[i], HEX));
    }
    tone(buzzer, 5000, 100);
    Serial.println(tag);
      // Convert hexadecimal UID to decimal
  unsigned long decimalUID = hexToDec(tag);
  
    sendDataToDatabase(decimalUID);
    tag = "";
    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
  }
}  

void sendDataToDatabase(unsigned long decimalUID){
   if ((WiFi.status() == WL_CONNECTED))
  {
      WiFiClient client;
      HTTPClient http;
         
  // Your Domain name with URL path or IP address with path
    http.begin(client, serverName);
    http.addHeader("Content-Type", "application/json");
    
    // Prepare your HTTP POST request data
    String httpRequestData = "{\"accesstoken\":\"" + accesstoken + "\",\"card_number\":\"" + decimalUID + "\",\"rfid_number\":\"" + rfidNo + "\"}";
    Serial.print("httpRequestData: ");
    Serial.println(httpRequestData);
    
    // Send HTTP POST request
    int httpResponseCode = http.POST(httpRequestData);
    if (httpResponseCode > 0){
       if (httpResponseCode == HTTP_CODE_OK) {
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);
       String data = http.getString();
       Serial.println(data);
       DynamicJsonDocument jsonDoc(512);
       DeserializationError error = deserializeJson(jsonDoc, data);
       const char* messages = jsonDoc["message"];
       Serial.println(user);
       Serial.println(messages);
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print(messages);
        digitalWrite(ledPin, HIGH);
        tone(buzzer, 2000, 400);
        delay(10);
        digitalWrite(ledPin, LOW);
        lcd.clear(); 
      }else{
       Serial.print(httpResponseCode);
        lcd.clear();
        lcd.setCursor(2,0);
        lcd.print("Server Error");
        digitalWrite(ledPin, HIGH);
        tone(buzzer, 2000, 300);
        delay(30);
        digitalWrite(ledPin, LOW);
        lcd.clear();
     }
    }else{
      Serial.print("Error_code: ");
      Serial.println(httpResponseCode);
       lcd.clear();
       lcd.setCursor(2,0);
       lcd.print("Server Error");
       lcd.setCursor(2,1);
       lcd.print("No Response");
       tone(buzzer, 2000, 300);
       delay(30);
       lcd.clear();
    }
    // Free resources
    http.end();
  }
  else {
    Serial.println("WiFi Disconnected");
       lcd.clear();
       lcd.setCursor(2,0);
       lcd.print("Wifi Problem");
       lcd.setCursor(0,1);
       lcd.print("Wifi disconnectd");
       delay(30);
       lcd.clear();
       testWifi();
  } 
}

//Function that convert hexadecimal to decimal numbers
unsigned long hexToDec(String hexString) {
  unsigned long decValue = 0;
  int hexLength = hexString.length();

  for (int i = 0; i < hexLength; i++) {
    char hexChar = hexString.charAt(i);
    int hexDigit;

    if (hexChar >= '0' && hexChar <= '9') {
      hexDigit = hexChar - '0';
    } else if (hexChar >= 'A' && hexChar <= 'F') {
      hexDigit = hexChar - 'A' + 10;
    } else if (hexChar >= 'a' && hexChar <= 'f') {
      hexDigit = hexChar - 'a' + 10;
    } else {
      // Invalid hex character
      return 0;
    }
    decValue = (decValue * 16) + hexDigit;
  }
  return decValue;
}

//Reset Node MCU ESP8266
void resetNodeMCU() {
    ESP.restart();
} 

//Functions used for saving WiFi credentials and to connect to it which you do not need to change 
bool testWifi(void)
{
  int c = 0;
  Serial.println("Waiting for WiFi to connect");
  while ( c < 20 ) {
    if (WiFi.status() == WL_CONNECTED)
    {
      lcd.setCursor(0,0);
      lcd.print("Wifi Connected");
      delay(1000);
      lcd.clear();
      return true;
    }
    delay(500);
    Serial.print("*");
    c++;
  }
  Serial.println("");
  Serial.println("Connection timed out, opening AP or Hotspot");
  lcd.setCursor(0,0);
  lcd.print("Wifi disconnect");
  delay(1000);
  lcd.clear();
  return false;
}
 
void launchWeb()
{
  Serial.println("");
  if (WiFi.status() == WL_CONNECTED)
    Serial.println("WiFi connected");
  Serial.print("Local IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("SoftAP IP: ");
  Serial.println(WiFi.softAPIP());
  createWebServer();
  // Start the server
  server.begin();
  Serial.println("Server started");
}
 
void setupAP(void)
{
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  int n = WiFi.scanNetworks();
  Serial.println("scan completed");
  if (n == 0)
    Serial.println("No WiFi Networks found");
  else
  {
    Serial.print(n);
    Serial.println(" Networks found");
    for (int i = 0; i < n; ++i)
    {
      // Print SSID and RSSI for each network found
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(WiFi.SSID(i));
      Serial.print(" (");
      Serial.print(WiFi.RSSI(i));
      Serial.print(")");
      Serial.println((WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "*");
      delay(10);
    }
  }
  Serial.println("");
  st = "<ol>";
  for (int i = 0; i < n; ++i)
  {
    // Print SSID and RSSI for each network found
    st += "<li>";
    st += WiFi.SSID(i);
    st += " (";
    st += WiFi.RSSI(i);
 
    st += ")";
    st += (WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "*";
    st += "</li>";
  }
  st += "</ol>";
  delay(100);
  WiFi.softAP("MSB", "");
  Serial.println("Initializing_Wifi_accesspoint");
  launchWeb();
  Serial.println("over");
}
 
void createWebServer()
{
 {
    server.on("/", []() {
 
      IPAddress ip = WiFi.softAPIP();
      String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
      content = "<!DOCTYPE HTML>\r\n<html>Device WiFi Connectivity Setup ";
      content += "<form action=\"/scan\" method=\"POST\"><input type=\"submit\" value=\"scan\"></form>";
      content += "<p>";
      content += st;
      content += "</p><form method='get' action='setting'><label>SSID: </label><input name='ssid' length=32><input name='pass' length=64><input type='submit'></form>";
      content += "</html>";
      server.send(200, "text/html", content);
    });
    server.on("/scan", []() {
      //setupAP();
      IPAddress ip = WiFi.softAPIP();
      String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
 
      content = "<!DOCTYPE HTML>\r\n<html>go back";
      server.send(200, "text/html", content);
    });
 
    server.on("/setting", []() {
      String qsid = server.arg("ssid");
      String qpass = server.arg("pass");
      if (qsid.length() > 0 && qpass.length() > 0) {
        Serial.println("clearing eeprom");
        for (int i = 0; i < 96; ++i) {
          EEPROM.write(i, 0);
        }
        Serial.println(qsid);
        Serial.println("");
        Serial.println(qpass);
        Serial.println("");
 
        Serial.println("writing eeprom ssid:");
        for (int i = 0; i < qsid.length(); ++i)
        {
          EEPROM.write(i, qsid[i]);
          Serial.print("Wrote: ");
          Serial.println(qsid[i]);
        }
        Serial.println("writing eeprom pass:");
        for (int i = 0; i < qpass.length(); ++i)
        {
          EEPROM.write(32 + i, qpass[i]);
          Serial.print("Wrote: ");
          Serial.println(qpass[i]);
        }
        EEPROM.commit();
 
        content = "{\"Success\":\"saved to eeprom... reset to boot into new wifi\"}";
        statusCode = 200;
        ESP.reset();
      } else {
        content = "{\"Error\":\"404 not found\"}";
        statusCode = 404;
        Serial.println("Sending 404");
      }
      server.sendHeader("Access-Control-Allow-Origin", "*");
      server.send(statusCode, "application/json", content);
 
    });
  } 
}
void powerOn(){
  lcd.setCursor(0,0);
  lcd.print("Please Wait");
  delay(500);
  lcd.setCursor(11,0);
  lcd.print(".");
  delay(1000);
  lcd.setCursor(12,0);
  lcd.print(".");
  delay(1000);
  lcd.setCursor(13,0);
  lcd.print(".");
  delay(1000);
  lcd.setCursor(14,0);
  lcd.print(".");
  delay(1000);
  lcd.setCursor(15,0);
  lcd.print(".");
  delay(1000);
  lcd.setCursor(16,0);
  lcd.print(".");
  delay(100);
  lcd.clear();
   }
