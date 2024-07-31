#include <WiFi.h>
#include <esp_now.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#define messageID  "SyncDraw"
#define localColor  TFT_YELLOW
#define remoteColor TFT_WHITE
#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33

#define DRAW

const int coords[] = { 467, 3879, 262, 3736 }; // CYD portrait - left, right, top, bottom
const int rotation = 3; //  in rotation order - portrait, landscape, etc

TFT_eSPI tft = TFT_eSPI();

esp_now_peer_info_t peerInfo;
volatile bool newDataReceived = false;
uint8_t peerAddress[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
 
typedef struct message {
  String  ID = messageID;
  uint16_t xPen;
  uint16_t yPen;
  bool clearSCR = false;
}  message;
 
message dataToSend;
message receivedData;

#ifdef DRAW
SPIClass tsSpi = SPIClass(VSPI);
XPT2046_Touchscreen ts(XPT2046_CS, XPT2046_IRQ);
#endif

void onSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("Message Sent: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}

void onReceive(const uint8_t *mac_addr, const uint8_t *incomingData, int len) {
  memcpy(&receivedData, incomingData, sizeof(receivedData));
  if(receivedData.ID == messageID) newDataReceived = true;  
}

void setup() {
  Serial.begin(115200);
  tft.begin();
  tft.setRotation(rotation);  
  tft.fillScreen(TFT_BLACK);
  tft.drawRect(0, 0, tft.width(), tft.height(), TFT_YELLOW);

  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // CYD
  tsSpi.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS); /* Start second SPI bus for touchscreen */
  ts.begin(tsSpi); /* Touchscreen init */
  ts.setRotation(rotation); /* Inverted landscape orientation to match screen */  
  // CYD

  esp_now_register_send_cb(onSent);

  memcpy(peerInfo.peer_addr, peerAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }

  esp_now_register_recv_cb(onReceive);  
}

void loop() {

#ifdef DRAW
  // display touched point with colored dot
  if (ts.tirqTouched() && ts.touched()) {
    TS_Point p = ts.getPoint(); 
    Serial.printf("x = %d, y = %d\n", p.x, p.y);
    uint16_t pixel_x = map(p.x, coords[0], coords[1], 0, tft.width());
    uint16_t pixel_y = map(p.y, coords[2], coords[3], 0, tft.height());

    tft.fillCircle(pixel_x, pixel_y, 2, localColor);   
    
    dataToSend.xPen = pixel_x;
    dataToSend.yPen = pixel_y;       
    if((pixel_x + 20) > tft.width() && (pixel_y + 20) > tft.height()) {
      tft.fillScreen(TFT_BLACK);
      tft.drawRect(0, 0, tft.width(), tft.height(), TFT_YELLOW);
      dataToSend.clearSCR = true;
    }
    else dataToSend.clearSCR = false;
    esp_now_send(peerAddress, (uint8_t *)&dataToSend, sizeof(dataToSend));      
    
  }
#endif  

  if (newDataReceived) {
    tft.fillCircle(receivedData.xPen, receivedData.yPen, 2, remoteColor);
    Serial.printf("Received x = %d, y = %d\n", receivedData.xPen, receivedData.yPen);
    if(receivedData.clearSCR) {
      tft.fillScreen(TFT_BLACK);
      tft.drawRect(0, 0, tft.width(), tft.height(), TFT_YELLOW);
    }  
    newDataReceived = false;  
  }   
}

