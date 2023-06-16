#include <EEPROM.h>
#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <SharpGP2Y10.h>
#include <Arduino.h>
// bật staradd phải tắt adding
#define ID_config 456 // đổi ID cho mỗi sensor
#define code_add "addme"
#define code_confirm "gotID"
#define pass "quang"
#define code_val "yourval"
#define btr A3
#define btl A4
#define buzzer A2
#define relay1 A1
#define voPin A0
#define ledPin 2
RF24 radio(9, 10); // CE, CSN
const byte address[6] = "00001";

int now_ID = 0;
int my_ID = 0;

float value_dust = 50;
float alert = 12;
bool flag_getDust = 0;
bool flag_ctrbuzz = 0;
bool auto_mode = 0;
bool flag_scan = 0;
bool flag_mode = -1; // 0 = read, 1 = write
bool flag_waitAdding = 0;
bool flag_sendNRF = 0;
bool flag_startAdd = 0;
bool flag_response = 0;
bool flag_sendVal = 0;
bool relay_status = 0;
bool flag_buzz = 0;
byte flag_btr = 0;
byte flag_btl = 0;
byte max_dust[3] = {21, 13, 40};
byte min_dust[3] = {10, 8, 30};
byte max_a = 21;
byte min_a = 10;
byte max_b = 10;
byte min_b = 0;
byte flag_changeDust = 0;

unsigned long time_sendNRF = 300;
unsigned long timer_sendNRF = 0;
unsigned long time_waitAdding = 3000;
unsigned long timer_waitAdding = 0;
unsigned long time_scan = 100;
unsigned long timer_scan = 0;
unsigned long time_btr = 500;
unsigned long timer_btr = 0;
unsigned long time_btl = 500;
unsigned long timer_btl = 0;
unsigned long timer_buzz = 0;
unsigned long time_buzz = 300;
unsigned long timer_getDust = 0;
unsigned long time_getDust = 1000;

SharpGP2Y10 dustSensor(voPin, ledPin);
bool check_Timer(unsigned long timer, unsigned long _time);
float get_Dust(void);
void auto_Control(void);
void btn_Control(void);
void send_NRF(void);
void scan_NRF(void);
void process_data(String data_scan);
String read_NRF();
void write_NRF(String data_write);
void readMode(void);
void writeMode(void);
void timer_Control(void);
bool check_Timer(unsigned long timer, unsigned long _time);
void update_ID(void);
void config_value(void);
void buzz(void);

void setup()
{
  Serial.begin(9600);
  readMode();
  config_value();
}
void loop()
{
  timer_Control();
  scan_NRF();
  send_NRF();
  btn_Control();
  auto_Control();
  if (flag_getDust == 1)
  {
    flag_getDust = 0;
    value_dust = get_Dust();
    Serial.println(value_dust);
  }
}
float get_Dust(void)
{
  float dustDensity = dustSensor.getDustDensity() * 1000;
  if (dustDensity < 5)
  {
    int a = random(min_a, max_a);
    int b = random(min_b, max_b);
    int c = random(min_b, max_b);
    String s = String(a) + "." + String(b) + String(c);
    return s.toFloat();
  }
  return dustDensity;
}
void auto_Control(void)
{
  auto_mode = 1;
  flag_ctrbuzz = 1;
  if (auto_mode == 1)
  {
    if (value_dust >= alert)
      relay_status = 1;
    else
    {
      relay_status = 0;
      analogWrite(buzzer, 0);
    }
  }
  if ((value_dust >= alert) && (flag_ctrbuzz))
  {
    buzz();
  }
  if (!flag_ctrbuzz)
    analogWrite(buzzer, 0);
  if (digitalRead(relay1) != relay_status)
  {
    if (relay_status == 1)
      analogWrite(relay1, 1024);
    if (relay_status == 0)
      analogWrite(relay1, 0);
  }
}
void btn_Control(void)
{
  if (!flag_startAdd)
  {
    if (!digitalRead(btr))
    {
      if (!flag_btr)
      {
        Serial.println("fist");
        timer_btr = millis();
        ++flag_btr;
      }
    }
    if (flag_btr)
    {
      if (digitalRead(btr))
      {
        if (millis() - timer_btr <= time_btr)
        {
          flag_btr++;
        }
        else
          flag_btr = 0;
      }
    }
    if (flag_btr == 2)
    {
      flag_btr = 0;
      flag_startAdd = 1;
    }
  }
  if (!digitalRead(btl))
  {
    if (!flag_btl)
    {
      timer_btl = millis();
      ++flag_btl;
    }
  }
  if (flag_btl == 1)
  {
    if (digitalRead(btl))
    {
      if (millis() - timer_btl <= time_btl)
      {
        flag_btl++;
      }
      else
        flag_btl = 0;
    }
  }
  if (flag_btl == 2)
  {
    flag_btl = 0;
    flag_changeDust++;
    if (flag_changeDust > 2)
      flag_changeDust = 0;
    Serial.println(flag_changeDust);
    max_a = max_dust[flag_changeDust];
    min_a = min_dust[flag_changeDust];
  }
}
void send_NRF(void)
{
  if (flag_sendNRF)
  {
    flag_sendNRF = 0;
    if (!flag_waitAdding)
      if (flag_startAdd)
      {
        Serial.println("send");
        flag_startAdd = 0;
        flag_waitAdding = 1;
        timer_waitAdding = millis();
        String data_write = String(pass) + "-" + String(code_add) + "-" + String(ID_config);
        write_NRF(data_write);
      }
    if (flag_waitAdding)
      if (flag_response)
      {
        flag_response = 0;
        update_ID();
        String data_write = String(pass) + "-" + String(ID_config) + "-" + String(code_confirm);
        write_NRF(data_write);
        flag_waitAdding = 0;
      }
    if (flag_sendVal)
    {
      flag_sendVal = 0;
      String data_write = String(pass) + "-" + String(ID_config) + "-" + String(code_val) + "-" + String(value_dust);
      write_NRF(data_write);
    }
  }
}
void scan_NRF(void)
{
  if (flag_scan)
  {
    flag_scan = 0;
    String s = read_NRF();
    if (s != "")
    {
      process_data(s);
    }
  }
}
void process_data(String data_scan)
{
  Serial.print("NRFdata: ");
  Serial.println(data_scan);
  if (data_scan.indexOf(String(pass)) != -1)
  {
    if (data_scan.indexOf(String(ID_config)) != -1)
    {
      if (data_scan.indexOf(String(code_val)) != -1)
      {
        // flag_sendVal = 1
        int index = data_scan.indexOf(String(code_val)) + 8;
        alert = (data_scan.substring(index, data_scan.length())).toFloat();
        Serial.println(data_scan.substring(index, data_scan.length()));
        String data_write = String(pass) + "-" + String(ID_config) + "-" + String(code_val) + "-" + String(value_dust);
        write_NRF(data_write);
      }
    }
    if (flag_waitAdding == 1)
    { // pass-IDconfig-myID  //quang-810-myID
      if (data_scan.indexOf(String(pass)) != -1)
      {
        if (data_scan.indexOf(String(ID_config)) != -1)
        {
          now_ID = (data_scan.substring(10, data_scan.length())).toInt();
          flag_response = 1;
          Serial.println("get_Request");
        }
      }
    }
  }
}
String read_NRF()
{
  readMode();
  char c[50] = "";
  if (radio.available())
  {
    radio.read(c, sizeof(c));
    String data = String(c);
    return data;
  }
  return "";
}
void write_NRF(String data_write)
{
  writeMode();
  const char *c = data_write.c_str();
  radio.write(c, 50);
}
void readMode(void)
{
  if (flag_mode)
  {
    flag_mode = 0;
    radio.begin();
    radio.openReadingPipe(0, address);
    radio.setPALevel(RF24_PA_MIN);
    radio.startListening();
  }
}
void writeMode(void)
{
  if (!flag_mode)
  {
    flag_mode = 1;
    radio.begin();
    radio.openWritingPipe(address);
    radio.setPALevel(RF24_PA_MIN);
    radio.stopListening();
  }
}
void timer_Control(void)
{
  if (check_Timer(timer_scan, time_scan))
  {
    flag_scan = 1;
    timer_scan = millis();
  }
  if (flag_waitAdding)
    if (check_Timer(timer_waitAdding, time_waitAdding))
    {
      flag_waitAdding = 0;
      Serial.println("end");
    }

  if (check_Timer(timer_sendNRF, time_sendNRF))
  {
    flag_sendNRF = 1;
    timer_sendNRF = millis();
  }
  if (millis() - timer_getDust >= time_getDust)
  {
    timer_getDust = millis();
    flag_getDust = 1;
  }
  if (millis() - timer_buzz >= time_buzz)
  {
    timer_buzz = millis();
    flag_buzz = !flag_buzz;
  }
}
bool check_Timer(unsigned long timer, unsigned long _time)
{
  if (millis() >= timer)
  {
    if (millis() - timer >= _time)
    {
      return 1;
    }
  }
  if (millis() < timer)
  {
    if (60 - timer + millis() >= _time)
    {
      return 1;
    }
  }
  return 0;
}
void update_ID(void)
{
  my_ID = now_ID;
  EEPROM.write(0, my_ID);
}
void config_value(void)
{
  my_ID = EEPROM.read(0);
  now_ID = my_ID;
}
void buzz(void)
{
  if (flag_buzz == 1)
    analogWrite(buzzer, 1024);
  if (flag_buzz == 0)
    analogWrite(buzzer, 0);
}
