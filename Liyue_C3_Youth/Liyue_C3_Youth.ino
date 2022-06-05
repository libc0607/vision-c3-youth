/*
 * Liyue C3 Youth V1
 * Original: https://oshwhub.com/Myzhazha/li-yue-shen-zhi-yan-gua-jian
 * Mod by @libc0607:　https://github.com/libc0607/vision-c3-youth
 * 
 * How-to: 
 * a. Make sure PCB is well welded
 * b. Install dependencies: 
 *   arduino-esp32 @2.0.3
 *   Arduino_GFX
 *   JPEGDEC
 * c. Select "ESP32C3 Dev Module", 921600, CDC enabled, 160MHz, 40MHz, DIO, 4MB/1MB APP/3MB FATFS, then "Upload"
 * d. connect to PC USB, hold the button until u see "Hard resetting via RTS pin..."
 * e. Create a Wi-Fi Hotspot, ssid="Celestia", pwd="mimitomo"
 * f. Power up the PCB, hold the button until the screen turns GREY; 
 * g. Once it's connected, screen turns BLUE with <IP_ADDRESS> printed
 * h. Format filesystem (u only need to do it once): enter http://<IP_ADDRESS>/formatfs?format=114514 in ur browser
 * i. Webserver bring-up, Use cURL to upload data/index.htm, data/edit.htm (do it once): 
 *    curl -X POST -F "file=@index.htm" http://<IP_ADDRESS>/edit
 *    curl -X POST -F "file=@edit.htm" http://<IP_ADDRESS>/edit
 * j. Open http://<IP_ADDRESS>/ in your browser
 * k. upload .mjpeg video
 * l. when finish uploading, click the button to poweroff (screen GREEN for 300ms)
 * m. press the key for 1.5~5s, boot to video mode
*/


// 一些你可能会想要配置的内容
#define TFT_BRIGHTNESS_DEFAULT  0.04     // float, 0(dark)~1(fullon) 
#define TFT_BRIGHTNESS_VIDEO    1.0     // float, 0(dark)~1(fullon) 
#define KEY_MIN_INTERVAL_MS     800     // 按键最小间隔时间，防止误触和抖动，毫秒
#define KEY_FORCE_POWERDOWN_MS  3000    // 长按超过3000ms后松开，触发关机
#define KEY_BOOTUP_DELAY_MS     1200    // 断电状态下，长按超过这个时间加上大约几百毫秒后，认为开机
#define KEY_BOOTUP_WIFI_MS      5000    // 开机按键时长超过这个值则进入wifi模式，否则正常启动
#define ROOT_DIR                "/"     // 在这个文件夹下寻找视频文件
#define VIDEO_EXT_NAME          ".mjpeg"  // 寻找上面这个文件夹下后缀为.mjpeg的文件尝试播放

#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <SPI.h>
#include <FS.h>
#include <FFat.h>
#include <Arduino_GFX_Library.h> 
#include "MjpegClass.h"


#define PIN_BOOT_KEY    0
#define PIN_LDO_LATCH   1
#define PIN_SPI_MISO    2
#define PIN_BAT_ADC     3   // unused
#define PIN_LCD_CS      4   
#define PIN_TF_CS       5   // unused
#define PIN_SPI_CLK     6
#define PIN_SPI_MOSI    7
#define PIN_LCD_DC      8
#define PIN_TFT_BL      9
#define PIN_LCD_RST     10

#define MJPEG_BUFFER_SIZE (240 * 240 * 2 / 8)
static MjpegClass mjpeg;
uint8_t *mjpeg_buf;
#define SPI_FREQ 40000000
#define LEDC_BITS 10
#define LEDC_LEVELS 1024
#define FILESYSTEM FFat
#define DBG_OUTPUT_PORT Serial

Arduino_DataBus *bus = new Arduino_ESP32SPI(PIN_LCD_DC, PIN_LCD_CS, PIN_SPI_CLK, PIN_SPI_MOSI, PIN_SPI_MISO);
Arduino_ST7789 *gfx = new Arduino_ST7789(bus, PIN_LCD_RST, 0, true, 240, 240);

uint64_t key_down_ts = 0; 
uint64_t key_up_ts = 0;
bool key_event = false;
bool bootup = false;
bool bootmode_wifi = false;

const char* wifi_ssid = "Celestia";
const char* wifi_pwd = "mimitomo";
const char* mdns_host = "vision-youth";
WebServer server(80);
File fsUploadFile;

void returnOK() {
  server.send(200, "text/plain", "");
}

void returnFail(String msg) {
  server.send(500, "text/plain", msg + "\r\n");
}

bool loadFromSdCard(String path) {
  //bool is_gzipped = false;
  
  String dataType = "text/plain";
  if (path.endsWith("/")) {
    path += "index.htm";
  }

  if (path.endsWith(".src")) {
    path = path.substring(0, path.lastIndexOf("."));
  } else if (path.endsWith(".htm")) {
    dataType = "text/html";
  } else if (path.endsWith(".css")) {
    dataType = "text/css";
  } else if (path.endsWith(".js")) {
    dataType = "application/javascript";
  } else if (path.endsWith(".png")) {
    dataType = "image/png";
  } else if (path.endsWith(".gif")) {
    dataType = "image/gif";
  } else if (path.endsWith(".jpg")) {
    dataType = "image/jpeg";
  } else if (path.endsWith(".ico")) {
    dataType = "image/x-icon";
  } else if (path.endsWith(".xml")) {
    dataType = "text/xml";
  } else if (path.endsWith(".pdf")) {
    dataType = "application/pdf";
  } else if (path.endsWith(".zip")) {
    dataType = "application/zip";
  }

  File dataFile = FILESYSTEM.open(path.c_str());
  if (dataFile.isDirectory()) {
    path += "/index.htm";
    dataType = "text/html";
    dataFile = FILESYSTEM.open(path.c_str());
  }

  if (!dataFile) {
    return false;
  }

  if (server.hasArg("download")) {
    dataType = "application/octet-stream";
  }
  
  if (server.streamFile(dataFile, dataType) != dataFile.size()) {
    Serial.println("Sent less data than expected!");
  }

  dataFile.close();
  return true;
}

void handleFileUpload() {
  if (server.uri() != "/edit") {
    return;
  }
  HTTPUpload& upload = server.upload();
  String filename = upload.filename;
  if (!filename.startsWith("/")) 
    filename = "/" + filename;
  if (upload.status == UPLOAD_FILE_START) {
    gfx->fillScreen(ORANGE);
    if (FILESYSTEM.exists(filename)) {
      FILESYSTEM.remove(filename);
    }
    fsUploadFile = FILESYSTEM.open(filename, FILE_WRITE);
    Serial.print("Upload: START, filename: "); Serial.println(filename);
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (fsUploadFile) {
      fsUploadFile.write(upload.buf, upload.currentSize);
    }
    //Serial.print("Upload: WRITE, Bytes: "); Serial.println(upload.currentSize);
  } else if (upload.status == UPLOAD_FILE_END) {
    if (fsUploadFile) {
      fsUploadFile.close();
    }
    gfx->fillScreen(BLUE);
    Serial.print("Upload: END, Size: "); Serial.println(upload.totalSize);
  }
}

void deleteRecursive(String path) {
  File file = FILESYSTEM.open((char *)path.c_str());
  if (!file.isDirectory()) {
    file.close();
    FILESYSTEM.remove((char *)path.c_str());
    return;
  }

  file.rewindDirectory();
  while (true) {
    File entry = file.openNextFile();
    if (!entry) {
      break;
    }
    String entryPath = path + "/" + entry.name();
    if (entry.isDirectory()) {
      entry.close();
      deleteRecursive(entryPath);
    } else {
      entry.close();
      FILESYSTEM.remove((char *)entryPath.c_str());
    }
    yield();
  }

  FILESYSTEM.rmdir((char *)path.c_str());
  file.close();
}

void handleDelete() {
  if (server.args() == 0) {
    return returnFail("BAD ARGS");
  }
  String path = server.arg(0);
  if (path == "/" || !FILESYSTEM.exists((char *)path.c_str())) {
    returnFail("BAD PATH");
    return;
  }
  deleteRecursive(path);
  returnOK();
}

void handleCreate() {
  if (server.args() == 0) {
    return returnFail("BAD ARGS");
  }
  String path = server.arg(0);
  if (path == "/" || FILESYSTEM.exists((char *)path.c_str())) {
    returnFail("BAD PATH");
    return;
  }

  if (path.indexOf('.') > 0) {
    File file = FILESYSTEM.open((char *)path.c_str(), FILE_WRITE);
    if (file) {
      file.write(0);
      file.close();
    }
  } else {
    FILESYSTEM.mkdir((char *)path.c_str());
  }
  returnOK();
}

void formatFS() {
  if (!server.hasArg("format")) {
    return returnFail("BAD ARGS");
  }
  String path = server.arg("format");
  if (path != "114514" ) {
    return returnFail("BAD ARGS");
  }
  gfx->fillScreen(RED);
  Serial.println(F("114, 514, fs.format() started"));
  FILESYSTEM.format();                      
  gfx->fillScreen(BLUE);
  Serial.println(F("1919810!"));
  server.send(200, "text/plain", "1919810!");
}

void getStatus() {
  // A human-readable system status API
  String stat = "";
  // card type
  stat += "FFat: ";
  stat += String(int(FILESYSTEM.usedBytes() / (1024)));
  stat += "kB Used /  ";
  stat += String(int(FILESYSTEM.totalBytes() / (1024)));
  stat += "kB Total";
  
  stat += ", Firmware ";
  // firmware info
  stat += __DATE__ ;
  stat += " ";
  stat += __TIME__ ; 
  stat += "\r\n";
  server.send(200, "text/plain", stat);
}

void printDirectory() {
  if (!server.hasArg("dir")) {
    return returnFail("BAD ARGS");
  }
  String path = server.arg("dir");
  if (path != "/" && !FILESYSTEM.exists((char *)path.c_str())) {
    return returnFail("BAD PATH");
  }
  File dir = FILESYSTEM.open((char *)path.c_str());
  path = String();
  if (!dir.isDirectory()) {
    dir.close();
    return returnFail("NOT DIR");
  }
  dir.rewindDirectory();
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/json", "");
  WiFiClient client = server.client();

  server.sendContent("[");
  for (int cnt = 0; true; ++cnt) {
    File entry = dir.openNextFile();
    if (!entry) {
      break;
    }

    String output;
    if (cnt > 0) {
      output = ',';
    }

    output += "{\"type\":\"";
    output += (entry.isDirectory()) ? "dir" : "file";
    output += "\",\"name\":\"";
    output += entry.name();
    output += "\",\"size\":\"";
    output += entry.size();
    output += "\"";
    output += "}";
    server.sendContent(output);
    entry.close();
  }
  server.sendContent("]");
  dir.close();
}

void handleNotFound() {
  if (loadFromSdCard(server.uri())) {
    return;
  }
  String message = "Filesystem Not Detected\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " NAME:" + server.argName(i) + "\n VALUE:" + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
  Serial.print(message);
}



static int drawMCU(JPEGDRAW *pDraw) {
  gfx->draw16bitBeRGBBitmap(pDraw->x, pDraw->y, pDraw->pPixels, pDraw->iWidth, pDraw->iHeight);
  return 1;
}

void error_loop() {
  while(1) { 
    delay(50);
  }
}

void ARDUINO_ISR_ATTR boot_key_isr() {
  uint64_t ts;
  ts = millis();
  if ( (digitalRead(PIN_BOOT_KEY) == HIGH) 
      && (ts - key_down_ts > KEY_MIN_INTERVAL_MS) ) {
    key_down_ts = ts; 
    key_event = true;
  } else if ( (digitalRead(PIN_BOOT_KEY) == LOW) 
      && (ts - key_up_ts > KEY_MIN_INTERVAL_MS) ) {
    key_up_ts = ts; 
    
    // immediately poweroff
    // this should be processed in ISR because the main loop may stuck
    if ((key_up_ts - key_down_ts > KEY_FORCE_POWERDOWN_MS) && bootup && (key_down_ts != 0)) {
      pinMode(PIN_LDO_LATCH, INPUT);
    }
  }
}


void setup()
{
  uint64_t i, boot_ts, rel_ts;
  uint32_t file_cnt;
  File vFile;

  boot_ts = millis();
  Serial.begin(115200);
  Serial.println("");
  pinMode(PIN_LDO_LATCH, OUTPUT);
  pinMode(PIN_BOOT_KEY, INPUT);
  bootup = false;
  gfx->begin(SPI_FREQ);
  gfx->fillScreen(BLACK);

  
  delay(KEY_BOOTUP_DELAY_MS);
  digitalWrite(PIN_LDO_LATCH, HIGH);
  ledcAttachPin(PIN_TFT_BL, 1); 
  ledcSetup(1, 12000, LEDC_BITS); 
  ledcWrite(1, LEDC_LEVELS-(uint32_t)((TFT_BRIGHTNESS_DEFAULT)*((float)LEDC_LEVELS)));
  
  gfx->fillScreen(DARKGREY);
  gfx->setCursor(16, 80);
  gfx->println(F("Long press to Wi-Fi mode..."));
  gfx->setCursor(16, 120);
  bootmode_wifi = false;
  while(digitalRead(PIN_BOOT_KEY) == HIGH) { 
    rel_ts = millis();
    delay(5);
    if ( (millis() - boot_ts > KEY_BOOTUP_WIFI_MS) 
          && (rel_ts - boot_ts < KEY_BOOTUP_WIFI_MS) ) {
      bootmode_wifi = true;
      gfx->setCursor(16, 100);
      gfx->println(F("Boot to Wi-Fi mode."));
      break;
    }
  }  

  Serial.println(F("BOOT"));
  attachInterrupt(PIN_BOOT_KEY, boot_key_isr, CHANGE); 
  
  if (!FILESYSTEM.begin(true)) {
    Serial.println(F("ERROR: FS mount failed"));
    gfx->println(F(" EERROR: FS mount failed"));
    error_loop();
  }

  if (bootmode_wifi) {
    // wifi upload mode
    DBG_OUTPUT_PORT.printf("Wi-Fi mode, Connecting to %s\n", wifi_ssid);
    gfx->fillScreen(LIGHTGREY);
    WiFi.mode(WIFI_STA);
    WiFi.begin(wifi_ssid, wifi_pwd);
    while (WiFi.status() != WL_CONNECTED ) {
      delay(250);
      DBG_OUTPUT_PORT.print(".");
    }
    gfx->fillScreen(BLUE);
    DBG_OUTPUT_PORT.println("");
    DBG_OUTPUT_PORT.print("Connected! IP address: ");
    DBG_OUTPUT_PORT.println(WiFi.localIP());
    gfx->setCursor(16, 100);
    gfx->println(WiFi.localIP());
    MDNS.begin(mdns_host);
    MDNS.addService("http", "tcp", 80);
    DBG_OUTPUT_PORT.print("Open http://"); DBG_OUTPUT_PORT.print(mdns_host);
    DBG_OUTPUT_PORT.println(".local/edit to see the file browser");
    
    server.on("/status", HTTP_GET, getStatus);
    server.on("/formatfs", HTTP_GET, formatFS);
    server.on("/list", HTTP_GET, printDirectory);
    server.on("/edit", HTTP_DELETE, handleDelete);
    server.on("/edit", HTTP_PUT, handleCreate);
    server.on("/edit", HTTP_POST, []() {
      returnOK(); 
    }, handleFileUpload);
    server.onNotFound(handleNotFound);
    server.begin();
    DBG_OUTPUT_PORT.println("HTTP server started");

    bootup = true;
    while (1) {
      server.handleClient();
      delay(2);
      if (key_event) {         
        key_event = false;
        gfx->fillScreen(GREEN);
        delay(300);
        digitalWrite(PIN_LDO_LATCH, LOW);   // poweroff
      } 
    }
  } else {
    // video mode
    Serial.println(F("Video mode"));
    WiFi.mode(WIFI_OFF);
    ledcWrite(1, LEDC_LEVELS-(uint32_t)((TFT_BRIGHTNESS_VIDEO)*((float)LEDC_LEVELS)));
    mjpeg_buf = (uint8_t *)malloc(MJPEG_BUFFER_SIZE);
    if (!mjpeg_buf) {
      Serial.println(F("mjpeg_buf malloc failed"));
      error_loop();
    }
    File root_dir = FILESYSTEM.open(ROOT_DIR);
    if (!root_dir) {
      Serial.println(F("ERROR: Failed to open " ROOT_DIR));
      gfx->println(F(" EERROR: open " ROOT_DIR " failed"));
      error_loop();
    }
    file_cnt = 0;
    bootup = true;
    while (1) {
      //gfx->fillScreen(BLACK);
      vFile.close();
      vFile = root_dir.openNextFile();
      delay(1); 
      if (!vFile) {
        if (file_cnt != 0) {
          Serial.println(F("Rewind DIR"));
          root_dir.rewindDirectory();
          file_cnt = 0;
          continue;
        } else {
          Serial.println(F("ERROR: No video found"));
          gfx->println(F(" EERROR: No video found"));
          error_loop();
        }
      }
      String file_name = String(vFile.name());
      if ((!vFile.isDirectory()) && file_name.endsWith(VIDEO_EXT_NAME)) {
        Serial.print("Opened video file: ");Serial.println(file_name);
        file_cnt++;
        mjpeg.setup(&vFile, MJPEG_BUFFER_SIZE, drawMCU, false, true, true);  // c3 has core0 only 
        while (1) {
          mjpeg.readMjpegBuf();
          mjpeg.drawJpg();
          delay(1);   // quick & dirty wdt feeder
          if (key_event) {         
            key_event = false;
            break;
          } 
          if (!vFile.available()) {
            vFile.seek(0);
            mjpeg.updateFilePointer(&vFile);
            Serial.println(F("MJPEG video rewind"));
            delay(1);
          }
        }
      } else {
        Serial.print("Not valid: ");Serial.println(file_name);
        delay(1);
        continue;
      }
    }
    vFile.close();
    root_dir.close();
  }

  // cleanup
  ledcDetachPin(PIN_TFT_BL);
  delay(50);
  gfx->displayOff();
  digitalWrite(PIN_LDO_LATCH, LOW);
}

void loop()
{
}
