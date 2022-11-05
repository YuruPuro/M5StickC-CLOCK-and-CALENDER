// M5StickC Plus Clock & Calendar
// (C) 2022 yurupuro

#include <M5StickCPlus.h>
#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>

#define IMU_ADDR 0x68   // 加速度センサー
#define BM8563ADDR 0x51 // RTC

#define BUTTON 37 // M BUTTON

// ----- NetWork Data ---
// ▼ ssid/passwordはご自宅のWi-FiのSSIDとPASSWORDを設定してください。
const char* ssid     = "****";
const char* password = "****";

// △▽△▽△▽ RTC用 △▽△▽△▽
int year = 2022 ;
int month = 1 ;
int day = 1 ;
int weekday = 0 ;
int hour = 0;
int minutes = 0;
int seconds = 0;
// -- 砂時計用
int offsetSec = 0;
// -- 表示更新用
int dispSeconds = 0 ;
int dispday = 0 ;
int dispMode = 1 ;
// -- カレンダー表示用
static char *wDayStr[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat" } ;
static char *monthStr[] = {"January","February","March","April","May","June","July","August","September","October","November","December" } ;
static int dOm[] = { 31,28,31,30,31,30,31,31,30,31,30,31 } ;

// 2進化10進数(BCD)を10進数に変換
byte BCDtoDec(byte value) {
  return ((value >> 4) * 10) + (value & 0x0F) ;
}

// ▲▼▲▼▲▼ I2C操作用 ▲▼▲▼▲▼
// 1バイト受信
uint8_t ReadByte(uint8_t i2cAddr , uint8_t addr) {
  Wire1.beginTransmission(i2cAddr);
  Wire1.write(addr);
  Wire1.endTransmission();
  Wire1.requestFrom(i2cAddr, 1);
  while (Wire1.available() < 1) ;
  uint8_t val = Wire1.read();
  return val;
}

// 1バイト送信
void WriteByte(uint8_t i2cAddr , uint8_t addr, uint8_t data) {
  Wire1.beginTransmission(i2cAddr);
  Wire1.write(addr);
  Wire1.write(data);
  Wire1.endTransmission();
}

// ▲▼▲▼▲▼ IMU ▲▼▲▼▲▼
// Read Accel Data
void IMUReadAccel(int16_t &accelX,int16_t &accelY,int16_t &accelZ) {
  // Read Address Set
  Wire1.beginTransmission(IMU_ADDR);
  Wire1.write(0x3B); // Accel register
  Wire1.endTransmission();
  
  // 3Word Read
  Wire1.requestFrom(IMU_ADDR, 6);
  while(Wire1.available()<6);
  accelX = Wire1.read()<<8|Wire1.read();
  accelY = Wire1.read()<<8|Wire1.read();
  accelZ = Wire1.read()<<8|Wire1.read();

  return ;
}

// ▲▼▲▼▲▼ NTP用 ▲▼▲▼▲▼
bool connectWiFi( ) {
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.reconnect( ) ;
    int i = 0 ;
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      i++ ;
      if (i > 20) {
        delay(30000);
        return  false ;
      }
    }
  }
  return true ;
}

void getNTP( ) {
  connectWiFi() ;
  configTime(9 * 3600L, 0, "ntp.nict.jp", "time.google.com", "ntp.jst.mfeed.ad.jp");

  struct tm timeInfo;
  getLocalTime(&timeInfo) ;
  
  year = timeInfo.tm_year +1900 ; // 年
  month = timeInfo.tm_mon + 1 ;   // 月
  day = timeInfo.tm_mday ;        // 日
  weekday = timeInfo.tm_wday ;    // 曜日(0～6)
  hour = timeInfo.tm_hour ;        // 時
  minutes = timeInfo.tm_min ;     // 分
  seconds = timeInfo.tm_sec;      // 秒

  // -- RTC時刻設定
  // 2進化10進数(BCD)で指定します。(03以外)
  uint8_t hh = (hour/10) * 16 + (hour % 10) ;
  uint8_t mm = (minutes/10) * 16 + (minutes % 10) ;
  uint8_t ss = (seconds/10) * 16 + (seconds % 10) ;
  uint8_t yy1 = (1900 + year) % 100 ;
  yy1 = (year/10) * 16 + (year % 10) ;
  uint8_t yy2 = (year >= 2100) ? 0x10 : 0x00 ;
  uint8_t MM = (month/10) * 16 + (month % 10) ;
  uint8_t dd = (day/10) * 16 + (day % 10) ;

  WriteByte(BM8563ADDR , 0x00, 0x00);
  WriteByte(BM8563ADDR , 0x01, 0x00);
  WriteByte(BM8563ADDR , 0x08, yy1);
  WriteByte(BM8563ADDR , 0x07, MM & 0x0F | yy2);
  WriteByte(BM8563ADDR , 0x06, weekday & 0x07);
  WriteByte(BM8563ADDR , 0x05, dd & 0x3F);
  
  WriteByte(BM8563ADDR , 0x04, hh& 0x3F);
  WriteByte(BM8563ADDR , 0x03, mm & 0x7F);
  WriteByte(BM8563ADDR , 0x02, ss & 0x7F);
}

// ▲▼▲▼▲▼ 時計表示 ▲▼▲▼▲▼
// △▽△▽△▽　砂時計表示　△▽△▽△▽
void dispHourglass(bool flagInit) {
  if (flagInit) {
    // 初期表示
    M5.Lcd.setRotation(dispMode);
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.fillRoundRect(34,10,72,165,3,BLUE) ;

    int x1 = 38 ;
    int x2 = 100;
    M5.Lcd.fillRect(37,20,64,10,BLACK) ;
    for (int y=0;y<60;y++) {
      if (y > 30) {
        x1 ++ ;
        x2 -- ;
      }
      M5.Lcd.drawLine(x1, 30+y, x2, 30+y, ORANGE);
    }

    x1 = 68 ;
    x2 = 70;
    for (int y=0;y<70;y++) {
      if (y < 30) {
        x1 -- ;
        x2 ++ ;
      }
      M5.Lcd.drawLine(x1, 95+y, x2, 95+y, BLACK);
    }
    offsetSec = seconds ;
  }

  // 表示を更新するタイミングの為の変数更新：砂時計表示とは関係ない
  dispSeconds = seconds ;
  dispday = day ;

  // RTCの秒から砂時計表示の秒を算出
  if (offsetSec < 0) return ;
  int sec = (seconds - offsetSec + 60) % 60 ;
  int dispSec = 59 - sec ;

  // --- 砂時計表示
  // 上の表示を1Line減らす
  int  x1 = 38;
  int  x2 = 100;
  if (sec > 30) {
    x1 += sec - 30 ;
    x2 -= sec - 30 ;
  }
  M5.Lcd.drawLine(x1, 30+sec, x2, 30+sec, BLACK);

  // 下の表示を１ライン増やす▲が盛り上がる感じにするので全Line上書
  x1 = 68 ;
  x2 = 70 ;
  for (int i = 0 ; i < sec;i++) {
    int y = 105 + 60 - sec + i ;
    if (i < 30) {
      x1 = 68 - i ;
      x2 = 70 + i ;
    } else {
      x1 = 38;
      x2 = 100;
    }
    M5.Lcd.drawLine(x1, y, x2, y, ORANGE);
  }

  // 残り秒表示
  M5.Lcd.setTextFont(7);  // 7SEG - 48Pix
  M5.Lcd.setTextSize(1);  // Set the character size (1 (minimum) to 7 (maximum))
  M5.Lcd.setTextColor(GREENYELLOW);
  char strBuff[20] ;
  sprintf(strBuff,"%02d",dispSec) ; 
  M5.Lcd.fillRect(35,180,64,48,BLACK) ;
  M5.Lcd.drawString(strBuff, 35, 180);

  // ０秒になったらカウントを止める
  if (dispSec == 0) { offsetSec = -1 ; }
}

// △▽△▽△▽　デジタル時計表示　△▽△▽△▽
void dispClock(bool flagInit) {
  if (flagInit ||( hour == 0 && minutes == 0 && seconds == 0)) {
    //　初期表示：全画面書き換え
    M5.Lcd.setRotation(1);
    M5.Lcd.fillScreen(BLACK);
  }

  // 年月日表示
  M5.Lcd.setTextFont(4);  // ASCII - 26Pix
  M5.Lcd.setTextSize(1);  // Set the character size (1 (minimum) to 7 (maximum))
  M5.Lcd.setTextColor(WHITE);
  char strBuff[20] ;
  sprintf(strBuff,"%04d/%02d/%02d - %s",year,month,day,wDayStr[weekday]) ; 
  M5.Lcd.drawString(strBuff, 15, 20);

  // 時刻表示
  M5.Lcd.setTextFont(7);  // 7SEG - 48Pix
  M5.Lcd.setTextSize(1);  // Set the character size (1 (minimum) to 7 (maximum))
  M5.Lcd.setTextColor(GREENYELLOW);
  sprintf(strBuff,"%02d:%02d:%02d",hour,minutes,seconds) ; 
  if (seconds == 0) {
    // 時刻表示全体を書き換え
    M5.Lcd.fillRect(15,55,220,48,BLACK) ;
  } else {
    // 秒部分だけ書き換え
    M5.Lcd.fillRect(166,55,64,48,BLACK) ;
  }
  M5.Lcd.drawString(strBuff, 15, 55);

  // 表示を更新するタイミングの為の変数更新
  dispSeconds = seconds ;
  dispday = day ;
}

// △▽△▽△▽　カレンダー表示　△▽△▽△▽
void dispCalendar(bool flagInit) {
  if (flagInit) {
    // 初期表示
    M5.Lcd.setRotation(3);
    M5.Lcd.fillScreen(BLACK);
  }

  // 年月表示
  M5.Lcd.setTextFont(4);  // ASCII - 26Pix
  M5.Lcd.setTextSize(1);  // Set the character size (1 (minimum) to 7 (maximum))
  M5.Lcd.setTextColor(CYAN);
  char strBuff[20] ;
  sprintf(strBuff,"%s %04d",monthStr[month-1],year) ; 
  M5.Lcd.drawString(strBuff, 30, 2);

  //　曜日表示
  M5.Lcd.setTextFont(1);  // ASCII - 8Pix
  M5.Lcd.setTextSize(1);  // Set the character size (1 (minimum) to 7 (maximum))
  for (int i = 0 ; i< 7 ; i++) {
    if (i == 0) {
      M5.Lcd.setTextColor(PINK);
    } else
    if (i == 6) {
      M5.Lcd.setTextColor(CYAN);
    } else {
     M5.Lcd.setTextColor(WHITE);
    }
    M5.Lcd.drawString(wDayStr[i], i*26+30, 29);
  }
  M5.Lcd.drawLine(28 , 38 , 202, 38, WHITE);

  // 1日が何曜日なのか求める
  // 曜日(ツェラーの公式)
  int yy = (month <= 2) ? year -1 : year ;
  int mm = (month <= 2) ? month + 12 : month ;
  int C = yy / 100 ;
  int Y = yy % 100 ;
  int wd = (5*C + Y + Y/4 + C/4 + (26*mm + 26)/10) % 7 ; 
  
  // 月の日数を求める
  int dnum = dOm[month-1] ;
  if (month == 2 && year % 4 == 0 && !(year % 100 == 0 && year % 400 != 0)) {  
    dnum ++ ;
  }

  // 表示
  M5.Lcd.setTextFont(2);  // ASCII - 16Pix
  M5.Lcd.setTextSize(1);  // Set the character size (1 (minimum) to 7 (maximum))
  M5.Lcd.setTextColor(WHITE);
  int li = 0 ;
  for (int i = 1; i <= dnum ; i++ ) {
    if (i == day) {
      // 今日に●を付ける
      M5.Lcd.fillRoundRect(wd*26+30, li*16+40, 16, 16, 3, GREEN);
      M5.Lcd.setTextColor(BLACK);
    } else {
      M5.Lcd.setTextColor(WHITE);
    }
    sprintf(strBuff,"%2d",i) ; 
    M5.Lcd.drawString(strBuff, wd*26+30, li*16+40);

    // 表示位置更新
    wd ++ ;
    if (wd > 6) {
      wd = 0 ;
      li ++ ;
    }
  }
}

// ▲▼▲▼▲▼ 表示制御 ▲▼▲▼▲▼
void display(bool init) {
  switch(dispMode) {
  case 3:
    dispCalendar(init) ;
    break ;
  
  case 2:
  case 4:
    dispHourglass(init) ;
    break ;
  
  case 1: 
  default:
    dispClock(init) ;
  }
}

// △▽△▽△▽ 表示制御 △▽△▽△▽
void setDispMode( ) {
  // --- IMU ---
  float gForceX, gForceY, gForceZ;
  int16_t accelX, accelY, accelZ;
  IMUReadAccel(accelX,accelY,accelZ) ; 
  gForceX = accelX / 16384.0; // 32768 ÷ 2 
  gForceY = accelY / 16384.0; 
  gForceZ = accelZ / 16384.0;
  
  // - calculate the angle
  double roll  = atan2(gForceY, gForceZ) * RAD_TO_DEG;
  double pitch = atan(-gForceX / sqrt(gForceY * gForceY + gForceZ * gForceZ)) * RAD_TO_DEG;
  roll += (roll > 0) ? -180.0 : 180.0 ;

  // 傾きから表示する時計の種類を設定
  if (pitch > 45) {
    dispMode = 3 ;
  } else
  if (80 < roll && roll < 100) {
    dispMode = 2 ;
  } else
  if (-100 < roll && roll < -80) {
    dispMode = 4 ;
  } else {
    dispMode = 1 ;
  }
}

// ▲▼▲▼▲▼ Setup ▲▼▲▼▲▼
void setup() {
  M5.begin();
  M5.IMU.Init();

  // --- Netwaorkに接続
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  // ----- 起動時 初期読み取り -----
  getNTP( ) ;

  // ----- IMU -----
  WriteByte(IMU_ADDR,0x6B,0) ;  // PowerMode Initial
  WriteByte(IMU_ADDR,0x1B,0) ;  // gyro ±250deg/s 
  WriteByte(IMU_ADDR,0x1C,0) ;  // accel ±2g 
  setDispMode( ) ;
  
  // ----- BUTTON -----
  pinMode(BUTTON, INPUT);

  // ----- 初期表示 -----
  M5.Axp.ScreenBreath(12);
  display(true) ;
}

// ▲▼▲▼▲▼ Loop ▲▼▲▼▲▼
void loop() {
  // --- RTC ---
  hour    = BCDtoDec(ReadByte(BM8563ADDR,0x04) & 0x3F) ;
  minutes = BCDtoDec(ReadByte(BM8563ADDR,0x03) & 0x7F) ;
  seconds = BCDtoDec(ReadByte(BM8563ADDR,0x02) & 0x7F) ;

  // --- DIAPLAY
  if (dispSeconds != seconds) {
    display(false) ;
  }

  // --- IMU ---
  int preDispMode = dispMode ;
  setDispMode( ) ;

  // 表示更新
  if (preDispMode != dispMode ) {
    display(true) ;
  }

  // 必要があればボタンでNTPから時刻を再取得
  if (digitalRead(BUTTON) == LOW ) {
    getNTP( ) ;
    display(true) ;
  }

  delay(200) ;
}
