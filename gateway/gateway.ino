#include <Bridge.h>
#include <Console.h>
#include <Process.h>
#include <SoftwareSerial.h>
#include <base64.hpp>

// デバッグ・リリース切替.
#define DEBUG 0
#if DEBUG
#define DEBUG_LOG(a) Console.print(a)
#define DEBUG_LOG2(a,b) Console.print(a,b)
#define DEBUG_LOGLN(a) Console.println(a)
#define DEBUG_LOGLN2(a,b) Console.println(a,b)
#else
#define DEBUG_LOG(a)
#define DEBUG_LOG2(a,b)
#define DEBUG_LOGLN(a)
#define DEBUG_LOGLN2(a,b)
#endif

SoftwareSerial im920Serial(8, 9); // ソフトウエアシリアル

#define BUFFER_SIZE 256
char bufferStr[BUFFER_SIZE + 1];
uint8_t bufferIndex = 0;

void setup() {
  // MQTTコマンドを叩くためにブリッジを初期化.
  Bridge.begin();
#if DEBUG
  Console.begin();
  while (Console);
#endif
  DEBUG_LOGLN("start");

  im920Serial.begin(19200); // ソフトウエアシリアル 初期化
  pinMode(10, INPUT); // Busy 信号 Pin10 入力
}

/**
   繰り返し処理.
*/
void loop() {
  // シリアルの読み出し.
  if (im920Serial.available()) {
    char c = im920Serial.read();
    if (c == '\n') {
      // 1行の読み込みが完了したのでNULL終端.
      bufferStr[bufferIndex] = NULL;
      DEBUG_LOGLN(bufferStr);
      bufferIndex = 0;

      // 受信データのヘッダ部とボディ部を分割.
      uint8_t bin[66];
      uint8_t binIndex = 0;
      char *header = strtok(bufferStr, ":");
      if (!header) {
        DEBUG_LOGLN("no header");
        return;
      }
      char *body = strtok(NULL, ":");
      if (!body) {
        DEBUG_LOGLN("no body");
        return;
      }
      // ヘッダ部から送信モジュールのIDを取得.
      strtok(header, ",");
      char *address = strtok(NULL, ",");
      if (!address) {
        DEBUG_LOGLN("no address");
        return;
      }
      *((uint16_t *) bin) = (uint16_t) strtol(address, NULL, 16);
      binIndex += 2;
      // ボディ部をバイナリ化.
      char *b;
      while (b = strtok(body, ",")) {
        body = NULL;
        bin[binIndex++] = (uint8_t) strtol(b, NULL, 16);
      }

      // BASE64エンコーディングしてpublish
      char encoded[encode_base64_length(binIndex)];
      encode_base64(bin, binIndex, encoded);
      DEBUG_LOGLN(encoded);
      publish(encoded);
    } else if (c == '\r') {
      ;
    } else if (bufferIndex >= BUFFER_SIZE) {
      bufferIndex = 0;
    } else {
      bufferStr[bufferIndex++] = c;
    }
  }
}

/**
   受信したデータをcURLでAWS IoTにpublishする.
*/
void publish(char *message) {
  Process p;
  p.begin("/root/publish.sh");
  p.addParameter(message);
  p.run();
}
