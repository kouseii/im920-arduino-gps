#include <SoftwareSerial.h>

// デバッグ・リリース切替.
#define DEBUG 0
#if DEBUG
#define DEBUG_LOG(a) Serial.print(a)
#define DEBUG_LOG2(a,b) Serial.print(a,b)
#define DEBUG_LOGLN(a) Serial.println(a)
#define DEBUG_LOGLN2(a,b) Serial.println(a,b)
#else
#define DEBUG_LOG(a)
#define DEBUG_LOG2(a,b)
#define DEBUG_LOGLN(a)
#define DEBUG_LOGLN2(a,b)
#endif

SoftwareSerial gps(3, 2);
SoftwareSerial im920(8, 9);

// GPSデータ解析用.
#define GPS_LINE_BUFFER_SIZE 256
#define COMMA ","
char gpsLineBuffer[GPS_LINE_BUFFER_SIZE];
int gpsLineBufferIndex = 0;
int gpsLineBufferChecksumIndex = 0;

// 送信データの構造.
struct __attribute__ ((packed)) SendData {
  uint32_t date;
  uint32_t tm;
  float lat;
  float lon;
  uint16_t sped;
};
SendData sendData;

/**
   セットアップ処理.
*/
void setup() {
#if DEBUG
  Serial.begin(9600);
#endif
  DEBUG_LOGLN("start");

  gps.begin(9600);
  im920.begin(19200);

  // 送信データのデータ構造を初期化.
  memset(&sendData, NULL, sizeof(sendData));
}

/**
   繰り返し処理.
*/
void loop() {
  // GPSモジュールから行読み込み.
  int len = readGpsLine();
  if (len <= 0) {
    return;
  }

  // 緯度経度以外を除外.
  if (strncmp("$GPRMC", gpsLineBuffer, 6)) {
    return;
  }
  DEBUG_LOGLN(gpsLineBuffer);

  // 緯度経度情報を分解.
  strtok(gpsLineBuffer, COMMA);  // $GPRMC
  char *strTime = strtok(NULL, COMMA);  // UTC時刻
  char *state = strtok(NULL, COMMA);  // ステータス
  if (!state || state[0] != 'A') {
    return;
  }
  char *strLat = strtok(NULL, COMMA); // 緯度(dddmm.mmmm)
  strtok(NULL, COMMA);  // 北緯か南緯か
  char *strLon = strtok(NULL, COMMA);  // 経度(dddmm.mmmm)
  strtok(NULL, COMMA);  // 東経か西経か
  char *sped = strtok(NULL, COMMA); // 移動速度(knot)
  strtok(NULL, COMMA); // 移動真方位(角度)
  char *date = strtok(NULL, COMMA); // 協定世界時(UTC）での日付
  if (!date) {
    return;
  }

  // データを詰める.
  sendData.date = strtol(date, NULL, 10);
  sendData.tm = strtol(strTime, NULL, 10);
  sendData.lat = atof(strLat);
  sendData.lon = atof(strLon);
  sendData.sped = (uint16_t) (atof(sped) * 10);

  // 送信.
  for (uint8_t i = 0; i < 3; i++) {
    if (send()) {
      break;
    }
  }

  // これ以上はデータを蓄えることは不可能なので送信の可否に関わらず送信データを初期化.
  memset(&sendData, NULL, sizeof(sendData));
}

/**
   GPSモジュールから1行読み込む.
*/
int readGpsLine() {
  if (!gps.isListening()) {
    gps.listen();
  }

  while (1) {
    // バッファーオーバー対策.
    if (gpsLineBufferIndex >= GPS_LINE_BUFFER_SIZE) {
      gpsLineBufferIndex = 0;
      gpsLineBufferChecksumIndex = 0;
      return -1;
    }

    char c = gps.read();
    if (c < 0) {
      // シリアルバッファにデータなし.
      return 0;
    } else if (c == '\r') {
      // 1行読み込んだのでNULL終端.
      gpsLineBuffer[gpsLineBufferIndex] = 0;
    } else if (c == '\n') {
      // 1行の完了処理.
      if (gpsLineBufferIndex < 4 || gpsLineBufferChecksumIndex < 2) {
        gpsLineBufferIndex = 0;
        gpsLineBufferChecksumIndex = 0;
        return -1;
      }

      // チェックサムを確認.
      int checksum = 0;
      for (int i = 1, c = gpsLineBufferChecksumIndex - 1; i < c; i++) {
        checksum ^= gpsLineBuffer[i];
      }
      checksum &= 0xFF;
      if (hexstr2int(gpsLineBuffer[gpsLineBufferChecksumIndex]) * 16
          + hexstr2int(gpsLineBuffer[gpsLineBufferChecksumIndex + 1]) != checksum) {
        gpsLineBufferIndex = 0;
        gpsLineBufferChecksumIndex = 0;
        DEBUG_LOGLN("GPS checksum error");
        return -1;
      }

      // 状態をリセットして長さを返す.
      int len = gpsLineBufferIndex;
      gpsLineBufferIndex = 0;
      gpsLineBufferChecksumIndex = 0;
      return len;
    } else {
      // 改行以外は1行バッファに詰め込む.
      gpsLineBuffer[gpsLineBufferIndex++] = c;

      // チェックサムの場所まで来たらその場所を記録.
      if (c == '*') {
        gpsLineBufferChecksumIndex = gpsLineBufferIndex;
      }
    }
  }
}

/**
 * 16進文字を整数に変換する.
 */
int hexstr2int(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  return 10 + (c - 'A');
}

/**
 * 無線送信.
 */
bool send() {
  // IM920の応答を確認するためにリッスン.
  if (!im920.isListening()) {
    im920.listen();
  }

  // 送信.
  im920.print("TXDA ");
  uint8_t *bin = (uint8_t *)&sendData;
  for (uint8_t j = 0; j < sizeof(sendData); j++, bin++) {
    uint8_t v = *bin;
    if (v < 0x10) {
      im920.print("0");
    }
    im920.print(v, HEX);
  }
  im920.print("\r\n");

  // 応答を確認.
  bool result = false;
  while (true) {
    char c = im920.read();
    if (c >= 0) {
      DEBUG_LOG(c);
    }
    if (c == '\n') {
      return result;
    } else if (c == 'O') {
      result = true;
    }
  }
  return result;
}

