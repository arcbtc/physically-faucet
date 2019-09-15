

/**
 *  Physically faucet wiring ESP32 NodeMCU 32s
 *
 *  Epaper PIN MAP: [VCC - 3.3V, GND - GND, CS - GPIO5, Reset - GPIO16, 
 *                   AO (DC) - GPI17, SDA (MOSI) - GPIO23, SCK - GPIO18, LED - GPIO4]
 *                   
 *  Keypad Matrix PIN MAP: [GPIO12 - GPIO35]
 *
 *  LED PIN MAP: [POS (long leg) - GPIO15, NEG (short leg) - GND]
 *
 */

#include <WiFiClientSecure.h>
#include <ArduinoJson.h> 
#include "qrcode.h"
#include <SPI.h>
#include "Ucglib.h"

Ucglib_ST7735_18x128x160_HWSPI ucg(/*cd=*/ 17, /*cs=*/ 5, /*reset=*/ 16);


String giftinvoice;
String giftid;
String giftlnurl;
String amount = "100";
bool spent = false;
String giftstatus = "unpaid";

//enter your wifi details
char wifiSSID[] = "YOUR-WIFI";
char wifiPASS[] = "YOUR-WIFI-PASS";


const char* gifthost = "api.lightning.gifts";
const int httpsPort = 443;

const char* lndhost = "YOUR NODE URL";
String adminmacaroon = "YOUR-ADMIN-MACAROON";
const int lndport = 3010; //usually 8010

void setup() {
  
  pinMode(4, OUTPUT);
  digitalWrite(4, HIGH);
  delay(1000);
  ucg.begin(UCG_FONT_MODE_TRANSPARENT);
  ucg.setFont(ucg_font_ncenR14_hr);
  ucg.clearScreen();
  
  Serial.begin(115200);
         
  WiFi.begin(wifiSSID, wifiPASS);   
  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("connecting");
    delay(2000);
  }

  nodecheck();
  ucg.setFont(ucg_font_helvB10_hr);
  ucg.setPrintPos(0,20);
  
}

void loop() {
  
ucg.print("Generating gift!");
create_gift();
makepayment();

while(giftstatus == "unpaid"){
  
  checkgiftstatus();
  delay(500);
  Serial.println("checkyournode");
}

showAddress(giftlnurl);

while(!spent){
  checkgift();
  delay(1000);
}


ucg.setColor(0, 0, 0);
ucg.drawBox(0, 0, 200, 200);
ucg.setColor(255, 255, 255);
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
  Serial.println(line);

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
WiFiClientSecure client;
  if (!client.connect(lndhost, lndport)){
    }
  else {
       client.print(String("GET ")+ "https://" + lndhost +":"+ String(lndport) +"/v1/getinfo HTTP/1.1\r\n" +
                 "Host: "  + lndhost +":"+ String(lndport) +"\r\n" +
                  "User-Agent: ESP32\r\n" +
                 "Content-Type: application/json\r\n" +
                 "Connection: close\r\n\r\n");
                 
     String line = client.readStringUntil('\n');
    Serial.println(line);
    while (client.connected()) {
      String line = client.readStringUntil('\n');
      if (line == "\r") {
        break;
      }
    } 
    String content = client.readStringUntil('\n');
    Serial.println(content);
    client.stop();
}
}

void makepayment(){
  String memo = "Memo-";
  
  WiFiClientSecure client;
  if (!client.connect(lndhost, lndport)){
    }
  else { 
  String topost = "{\"payment_request\": \""+ giftinvoice +"\"}";
  Serial.println(topost);
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
    Serial.println(content);

    client.stop();
    
 
}
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
  Serial.println(line);

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

  const size_t capacity = 2*JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(8) + 290;
  DynamicJsonDocument doc(capacity);

  deserializeJson(doc, line);
  spent = doc["spent"]; 
  
  Serial.println(spent);
}




////QR CODE


void showAddress(String XXX){

  String addr = XXX;
  
  // auto detect best qr code size
  int qrSize = 10;
  int sizes[17] = { 14, 26, 42, 62, 84, 106, 122, 152, 180, 213, 251, 287, 331, 362, 412, 480, 504 };
  int len = addr.length();
  for(int i=0; i<17; i++){
    if(sizes[i] > len){
      qrSize = i+1;
      break;
    }
  }
  
  // Create the QR code
  QRCode qrcode;
  uint8_t qrcodeData[qrcode_getBufferSize(qrSize)];
  qrcode_initText(&qrcode, qrcodeData, qrSize, 1, addr.c_str());

  ucg.setPrintPos(10,10);
  float scale = 2;
  int padding = 15;
  ucg.setColor(255, 255, 255);
  ucg.drawBox(0, 0, 200, 200);
  ucg.setColor(0, 0, 0);
    for (uint8_t y = 0; y < qrcode.size; y++) {
    for (uint8_t x = 0; x < qrcode.size; x++) {
            if(qrcode_getModule(&qrcode, x, y)){
              
             ucg.drawBox(padding+scale*x, 10 + padding+scale*y, scale, scale);
            }
            else{
               
            }
        }
    }
}
