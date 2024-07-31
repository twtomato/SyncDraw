#include <WiFi.h>
#include <esp_now.h>
#include <TFT_eSPI.h>
#include <TouchScreen.h>
#define MINPRESSURE 200
#define MAXPRESSURE 1000
#define messageID  "SyncDraw"
#define localColor  TFT_YELLOW
#define remoteColor TFT_WHITE

#define DRAW

const int rotation = 1; //  in rotation order - portrait, landscape, etc
const int coords[] = { 912, 3740, 524, 3727 }; // portrait - x1, x2, y1, y2 for ILI9341
//const int coords[] = { 3617, 374, 3769, 220 }; // portrait - x1, x2, y1, y2 for ILI9488
const int XP = 27, XM = 15, YP = 4, YM = 14; // D1 R32 touchscreen pins(6,A2,A1,7) ok
//const int XP = 3, XM = 7, YP = 1, YM = 14; // S3 UNO touchscreen pins(6,A2,A1,7) ok

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
TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);

boolean Touch_getXY(uint16_t *x, uint16_t *y) {
    TSPoint p = ts.getPoint();
    pinMode(YP, OUTPUT);      //restore shared pins
    pinMode(XM, OUTPUT);
    digitalWrite(YP, HIGH);   //because TFT control pins
    digitalWrite(XM, HIGH);
    bool pressed = (p.z > MINPRESSURE && p.z < MAXPRESSURE);
    if (pressed) {
      switch (rotation) {
        case 0: // portrait
          *x = map(p.x, coords[0], coords[1], 0, tft.width()); 
          *y = map(p.y, coords[2], coords[3], 0, tft.height());
        break;
        case 1: // landscape
          *x = map(p.y, coords[2], coords[3], 0, tft.width()); 
          *y = map(p.x, coords[1], coords[0], 0, tft.height());
        break;
        case 2: // portrait inverted
          *x = map(p.x, coords[1], coords[0], 0, tft.width()); 
          *y = map(p.y, coords[3], coords[2], 0, tft.height());
        break;
        case 3: // landscape inverted
          *x = map(p.y, coords[0], coords[1], 0, tft.width()); 
          *y = map(p.x, coords[3], coords[2], 0, tft.height());
        break;
      }     
      Serial.printf("p.x=%d, p.y=%d\n",p.x, p.y);  
    }
    return pressed;
}
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

  uint16_t pixel_x, pixel_y;    
  boolean pressed = Touch_getXY(&pixel_x, &pixel_y);     
  if(pressed) {
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

