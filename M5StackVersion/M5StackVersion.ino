//As using admin macaroon, limited exposure of funds is reccomended - this project used Zap desktop wallet as it has neutrino LND https://docs.zaphq.io/docs-desktop-neutrino-connect
//In terminal, with Zap running run *ssh -R SOME-NAME.serveo.net:3010:localhost:8180 serveo.net* (replace SOME-NAME, see line 25)
#include <WiFiClientSecure.h>
#include <ArduinoJson.h> 
#include <M5Stack.h> 
#include "Physfau.c"

String giftinvoice;
String giftid;
String giftlnurl;
String amount = "100";
bool spent = false;
String giftstatus = "unpaid";
unsigned long tickertime;
unsigned long tickertimeupdate;
unsigned long giftbreak = 600000;  
unsigned long currenttime = 600000;

//enter your wifi details
char wifiSSID[] = "YOUR-WIFI";
char wifiPASS[] = "YOUR-WIFI-PASS";

const char* gifthost = "api.lightning.gifts";
const int httpsPort = 443;

const char* lndhost = "SOME-NAME.serveo.net"; //in terminal run ssh -R SOME-NAME.serveo.net:3010:localhost:8180 serveo.net
String adminmacaroon = "YOUR-LND-ADMIN-MACAROON";
const int lndport = 3010;


void setup() {
  
  M5.begin();
  Wire.begin();
  Serial.begin(115200);

         
  WiFi.begin(wifiSSID, wifiPASS);   
  while (WiFi.status() != WL_CONNECTED) {
     M5.Lcd.fillScreen(BLACK);
     M5.Lcd.setCursor(45, 80);
     M5.Lcd.setTextSize(2);
     M5.Lcd.setTextColor(TFT_WHITE);
     M5.Lcd.println("CONNECTING TO WIFI");
    delay(2000);
  }

}


void loop() {
    nodecheck();
  M5.Lcd.drawBitmap(0, 0, 320, 240, (uint8_t *)Physfau_map);
  bool checkah = false;
  while(checkah == false){
   M5.update();
   if (M5.BtnA.wasReleased()) {
    checkah = true;
   }
   else if (M5.BtnB.wasReleased()) {
    checkah = true;
   }
   else if (M5.BtnC.wasReleased()) {
    checkah = true;
   }
  }
  M5.Lcd.fillScreen(BLACK);
     M5.Lcd.setCursor(110, 80);
     M5.Lcd.setTextSize(2);
     M5.Lcd.setTextColor(TFT_WHITE);
     M5.Lcd.println("PROCESSING");
  nodecheck();
  M5.Lcd.fillScreen(BLACK);
     M5.Lcd.setCursor(65, 80);
     M5.Lcd.setTextSize(2);
     M5.Lcd.setTextColor(TFT_WHITE);
     M5.Lcd.println("GENERATING GIFT!");
  create_gift();
  makepayment();

  while(giftstatus == "unpaid"){
    checkgiftstatus();
    delay(500);
  }
  page_qrdisplay(giftlnurl);

  while(!spent){
    checkgift();
    delay(500);
  }

  for (int i = 5; i >= 1; i--){
     M5.Lcd.fillScreen(BLACK);
     M5.Lcd.setCursor(45, 80);
     M5.Lcd.setTextSize(2);
     M5.Lcd.setTextColor(TFT_RED);
     M5.Lcd.println("More sats in "+ String(i) +" min");
     delay(60000);
  }
spent = false;
}


void create_gift(){

  WiFiClientSecure client;
  
  if (!client.connect(gifthost, httpsPort)) {
    return;
  }
  
  String topost = "{  \"amount\" : " + amount +"}";
  String url = "/create";
  client.print(String("POST ") + url + " HTTP/1.1\r\n" +
                "Host: " + gifthost + "\r\n" +
                "User-Agent: ESP32\r\n" +
                "Content-Type: application/json\r\n" +
                "Connection: close\r\n" +
                "Content-Length: " + topost.length() + "\r\n" +
                "\r\n" + 
                topost + "\n");

  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") {
      break;
    }
  }
  
  String line = client.readStringUntil('\n');
  const size_t capacity = JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(6) + 620;
  DynamicJsonDocument doc(capacity);
  deserializeJson(doc, line);
  const char* order_id = doc["orderId"]; 
  const char* statuss = doc["status"]; // "unpaid"
  const char* lightning_invoice_payreq = doc["lightningInvoice"]["payreq"]; 
  const char* lnurl = doc["lnurl"];
  giftinvoice = lightning_invoice_payreq;
  giftstatus = statuss;
  giftid = order_id;
  giftlnurl = lnurl;
}


void nodecheck(){
  bool checker = false;
  while(!checker){
  WiFiClientSecure client;

  if (!client.connect(lndhost, lndport)){

    M5.Lcd.fillScreen(BLACK);
     M5.Lcd.setCursor(65, 80);
     M5.Lcd.setTextSize(2);
     M5.Lcd.setTextColor(TFT_RED);
     M5.Lcd.println("NO NODE DETECTED");
     delay(1000);
  }
  else{
    checker = true;
  }
  }
  
}


void makepayment(){
  String memo = "Memo-";
  WiFiClientSecure client;
  if (!client.connect(lndhost, lndport)){
    return;
  }
  String topost = "{\"payment_request\": \""+ giftinvoice +"\"}";
  client.print(String("POST ")+ "https://" + lndhost +":"+ String(lndport) +"/v1/channels/transactions HTTP/1.1\r\n" +
               "Host: "  + lndhost +":"+ String(lndport) +"\r\n" +
               "User-Agent: ESP322\r\n" +
               "Grpc-Metadata-macaroon:" + adminmacaroon + "\r\n" +
               "Content-Type: application/json\r\n" +
               "Connection: close\r\n" +
               "Content-Length: " + topost.length() + "\r\n" +
               "\r\n" + 
               topost + "\n");

  String line = client.readStringUntil('\n');
  Serial.println(line);
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") {  
      break;
    }
  }  
  String content = client.readStringUntil('\n');
  client.stop(); 
}


void checkgiftstatus(){
  WiFiClientSecure client;
  if (!client.connect(gifthost, httpsPort)) {
    return;
  }
  String url = "/status/" + giftid;
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + gifthost + "\r\n" +
               "User-Agent: ESP32\r\n" +
               "Content-Type: application/json\r\n" +
               "Connection: close\r\n\r\n");

  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") {
      break;
    }
  }
  String line = client.readStringUntil('\n');
  const size_t capacity = JSON_OBJECT_SIZE(1) + 40;
  DynamicJsonDocument doc(capacity);
  deserializeJson(doc, line);
  const char* giftstatuss = doc["status"]; 
  giftstatus = giftstatuss;
  
}

void checkgift(){
  WiFiClientSecure client;
  if (!client.connect(gifthost, httpsPort)) {
    return;
  }
  String url = "/gift/" + giftid;
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + gifthost + "\r\n" +
               "User-Agent: ESP32\r\n" +
               "Content-Type: application/json\r\n" +
               "Connection: close\r\n\r\n");

  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") {
      break;
    }
  }
  String line = client.readStringUntil('\n');
  Serial.println(line);
  const size_t capacity = 3*JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(4) + JSON_OBJECT_SIZE(9) + 1260;
  DynamicJsonDocument doc(capacity);  
  deserializeJson(doc, line);
  spent = doc["spent"]; 
}


void page_qrdisplay(String xxx)
{  

  M5.Lcd.fillScreen(BLACK); 
  M5.Lcd.qrcode(giftlnurl,45,0,240,10);
  delay(100);

}
