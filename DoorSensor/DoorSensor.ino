// Высокочувствительный магнитный датчик открывания двери шкафа-купе для включения внутренней подсветки
// на HMC5883L (GY-271) и ATTiny85

#define DEBUG 1 
#include <EEPROM.h>

#include <Wire.h>  // I2C ATTiny Master

#define CALIBRATE_MEASURES 300  // 30 сек. на калибровку
#define LOOP_DELAY 100  // 10 циклов/сек.

#define MIN_DEGREE 10   // диапазон регулировки чувствительности
#define MAX_DEGREE 150

#define HMC5883L 0x1E    // 0011110b, I2C 7bit address of HMC5883
#define set_pin  A0      // вход установки чувствительности
#define out_pin  13      // выход на ключ 

int y_offset, z_offset;
int y_min = 0, y_max = 0, z_min = 0, z_max = 0;

int measure_count = 0;

float init_heading = 0;

void setup(){
  pinMode(set_pin, INPUT);
  pinMode(out_pin, OUTPUT);

  // считываем значения смещений из EEPROM
  y_offset = EEPROM.read(0) | EEPROM.read(1)<<8;
  z_offset = EEPROM.read(2) | EEPROM.read(3)<<8;
  
  #ifdef DEBUG
  Serial.begin(9600);
  Serial.println(y_offset);
  Serial.println(z_offset);
  #endif

  Wire.begin();
  
  // Put the HMC5883L IC into the correct operating mode
  Wire.beginTransmission(HMC5883L);   //open communication with HMC5883
  Wire.write(0x02);                    //select mode register
  Wire.write(0x00);                   //continuous measurement mode
  Wire.endTransmission();

}

void loop(){
  int x, y, z; //triple axis data
  
  // Tell the HMC5883 where to begin reading data
  Wire.beginTransmission(HMC5883L);
  Wire.write(0x03); //select register 3, X MSB register
  Wire.endTransmission();
   
  // Read data from each axis, 2 registers per axis
  Wire.requestFrom(HMC5883L, 6);
  if (6<=Wire.available()) {
    x = Wire.read()<<8; //X msb
    x |= Wire.read(); //X lsb
    z = Wire.read()<<8; //Z msb
    z |= Wire.read(); //Z lsb
    y = Wire.read()<<8; //Y msb
    y |= Wire.read(); //Y lsb
  }

  if (analogRead(set_pin) < 5 && measure_count++ < CALIBRATE_MEASURES) {
    // подстроечник в 0, запоминаем мин/макс. для калибровки компаса
    y_min = min(y, y_min);
    y_max = max(y, y_max);
    z_min = min(z, z_min);
    z_max = max(z, z_max);
    #ifdef DEBUG
    Serial.print(y_min);
    Serial.print(' ');
    Serial.print(y_max);
    Serial.print(' ');
    Serial.print(z_min);
    Serial.print(' ');    
    Serial.println(z_max);
    #endif
  }     
  
  if (measure_count == CALIBRATE_MEASURES) {
    // калибровка завершена, получаем смещения
    y_offset = y_min + (y_max - y_min) / 2;
    z_offset = z_min + (z_max - z_min) / 2;

    // запоминаем в EEPROM
    EEPROM.write(0, y_offset);
    EEPROM.write(1, y_offset>>8);
    EEPROM.write(2, z_offset);
    EEPROM.write(3, z_offset>>8);  
  }
  
  // применяем смещения
  y -= y_offset;
  z -= z_offset;
  
  // получаем значение угла
  float heading = atan2(z, y) * 180 / PI;

  if (init_heading == 0)
    // запоминаем начальное положение
    init_heading = heading;

  // получаем отклонение
  heading -= init_heading;
  
  if (abs(heading) > map(analogRead(set_pin), 0, 1023, MIN_DEGREE, MAX_DEGREE))
    // отклонение больше установленного, включаем подсветку
    digitalWrite(out_pin, HIGH);
  else 
    digitalWrite(out_pin, LOW);
    
  #ifdef DEBUG  
  Serial.println(heading);
  #endif

  delay(LOOP_DELAY);  
    
}
