// Высокочувствительный магнитный датчик открывания двери шкафа-купе для включения внутренней подсветки
// на HMC5883L (GY-271) и ATTiny85

// Распиновка
// A0 - установка чувствительности
// Arduino UNO I2C - A4 (SDA), A5 (SCL) 

#define DEBUG 1 
#include <EEPROM.h>

#include <Wire.h>  // I2C ATTiny Master

#define CALIBRATE_MEASURES 10  // 1 сек. на калибровку
#define LOOP_DELAY 100  // 10 циклов/сек.

#define MIN_DEGREE 10   // диапазон регулировки чувствительности 
#define MAX_DEGREE 150

#define HMC5883L 0x1E    // 0011110b, I2C 7bit address of HMC5883
#define set_pin  A0      // вход от потенциометра для установки чувствительности
#define out_pin  13      // выход на ключ MOSFET

int x_offset, y_offset, z_offset;

int measure_count = 0;

float init_heading = 0;

void setup(){
  pinMode(set_pin, INPUT);
  pinMode(out_pin, OUTPUT);

  // считываем значения смещений из EEPROM
  x_offset = EEPROM.read(0) | EEPROM.read(1)<<8;
  y_offset = EEPROM.read(2) | EEPROM.read(3)<<8;
  z_offset = EEPROM.read(4) | EEPROM.read(5)<<8;
  
  #ifdef DEBUG
  Serial.begin(9600);

  Serial.println();
  Serial.println("Reading offsets from EEPROM");
  Serial.print(x_offset);
  Serial.print("\t");
  Serial.print(y_offset);
  Serial.print("\t");
  Serial.println(z_offset);
  Serial.println();
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

  delay(LOOP_DELAY);
  
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

  if (analogRead(set_pin) < 5 && ++measure_count < CALIBRATE_MEASURES) {
    // подстроечник в минимальном положении, т.е. включен режим калибровки
    // определяем средние смещения по осям для калибровки  

    if (measure_count == 1) {
      // для первого замера
      #ifdef DEBUG
      Serial.println("Calibrating sensor");
      #endif
    
      // инициализируем значения смещений
      x_offset = x, y_offset = y, z_offset = z;
    }

    // для последующих замеров усредняем значения
    x_offset = (x_offset + x) / 2;
    y_offset = (y_offset + y) / 2;
    z_offset = (z_offset + z) / 2;
    
    #ifdef DEBUG
    Serial.print(x_offset);
    Serial.print('\t');
    Serial.print(y_offset);
    Serial.print('\t');
    Serial.print(z_offset);   
    Serial.println();
    #endif

    return;
  }     
  
  if (measure_count++ == CALIBRATE_MEASURES) {
    // калибровка завершена

    #ifdef DEBUG
    Serial.println("Writing offsets to EEPROM");
    Serial.print(x_offset);
    Serial.print('\t');
    Serial.print(y_offset);
    Serial.print('\t');
    Serial.println(z_offset);
    Serial.println();
    #endif

    // запоминаем смещения в EEPROM
    EEPROM.write(0, x_offset);
    EEPROM.write(1, x_offset>>8);
    EEPROM.write(2, y_offset);
    EEPROM.write(3, y_offset>>8);
    EEPROM.write(4, z_offset);
    EEPROM.write(5, z_offset>>8);  

    // сбрасываем начальное положение
    init_heading = 0;
  }
  
  // применяем смещения к текущим замерам
  x -= x_offset;
  y -= y_offset;
  z -= z_offset;
  
  // получаем значение угла
  float heading = atan2(z, y) * 180 / PI;

  if (init_heading == 0)
    // запоминаем начальное положение
    init_heading = heading;

  // получаем отклонение
  heading -= init_heading;

  #ifdef DEBUG  
  Serial.println(heading);
  Serial.print(x);
  Serial.print("\t");
  Serial.print(y);
  Serial.print("\t");
  Serial.print(z);
  Serial.println();
  #endif 
  
  if (abs(heading) > map(analogRead(set_pin), 0, 1023, MAX_DEGREE, MIN_DEGREE))
    // отклонение больше установленного, включаем подсветку
    digitalWrite(out_pin, HIGH);
  else 
    digitalWrite(out_pin, LOW);
    
}
