// Высокочувствительный магнитный датчик открывания двери шкафа-купе для включения внутренней подсветки
// на HMC5883L (GY-271) и ATTiny85
// Дата последней модификации 2021.12.05

// Для калибровки положения двери "закрыта" подстроечник регулировки перевести в мин.положение
// и включить питание

// Распиновка
// ATTiny85:
//   SDA - 0 (pin 5), SCL - 2 (pin 7)
//   Регулировка чувствительности - A2 (pin 3)
//   Выход на ключ - 1 (pin 6)
// Arduino UNO (для отладки):
//   SDA - A5, SCL - A4
//   Регулировка чувствительности - A0
//   Выход на ключ - 13 

#define DEBUG 1 
#include <EEPROM.h>

#include <Wire.h>  // I2C ATTiny Master

#define CALIBRATE_MEASURES 10  // 1 сек. на калибровку
#define LOOP_DELAY 100   // 10 циклов/сек.

#define MIN_DEGREE 10    // диапазон регулировки чувствительности 
#define MAX_DEGREE 150

#define HMC5883L 0x1E    // 0011110b, I2C 7bit address of HMC5883
#define setup_pin  A0    // вход от потенциометра для установки чувствительности
#define switch_pin  13   // выход на ключ MOSFET

int x_init, y_init, z_init;

int measure_count = 0;

int heading_init = 0;

void setup(){
  pinMode(setup_pin, INPUT);
  pinMode(switch_pin, OUTPUT);

  // считываем калибровочные значения по осям из EEPROM
  x_init = EEPROM.read(0) | EEPROM.read(1)<<8;
  y_init = EEPROM.read(2) | EEPROM.read(3)<<8;
  z_init = EEPROM.read(4) | EEPROM.read(5)<<8;
  
  #ifdef DEBUG
  Serial.begin(9600);

  Serial.println();
  Serial.println("Axes init values from EEPROM");
  Serial.print(x_init);
  Serial.print("\t");
  Serial.print(y_init);
  Serial.print("\t");
  Serial.print(z_init);
  Serial.println();
  Serial.println();
  #endif

  // получаем значение начального угла
  heading_init = (int) (atan2(z_init, y_init) * 180 / PI);

  // инициализация I2C
  Wire.begin();
  
  // Put the HMC5883L IC into the correct operating mode
  Wire.beginTransmission(HMC5883L);    //open communication with HMC5883
  Wire.write(0x02);                    //select mode register
  Wire.write(0x00);                    //continuous measurement mode
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

  if (analogRead(setup_pin) < 20 && ++measure_count < CALIBRATE_MEASURES) {
    // подстроечник в минимальном положении, т.е. включен режим калибровки
    // и еще не выполнено нужное кол-во замеров,
    // выполняем калибровку  

    if (measure_count == 1) {
      // для первого замера
      #ifdef DEBUG
      Serial.println("Calibrating sensor");
      #endif
    
      // инициализируем значения 
      x_init = x, y_init = y, z_init = z;
    }

    // для последующих замеров усредняем значения
    x_init = (x_init + x) / 2;
    y_init = (y_init + y) / 2;
    z_init = (z_init + z) / 2;
    
    #ifdef DEBUG
    Serial.print(x_init);
    Serial.print('\t');
    Serial.print(y_init);
    Serial.print('\t');
    Serial.print(z_init);   
    Serial.println();
    #endif

    return;
  }     
  
  if (analogRead(setup_pin) < 20 && measure_count == CALIBRATE_MEASURES) {
    // калибровка завершена

    // получаем значение начального угла
    heading_init = (int) (atan2(z_init, y_init) * 180 / PI);

    #ifdef DEBUG
    Serial.println("Writing axes init values to EEPROM");
    Serial.print(x_init);
    Serial.print('\t');
    Serial.print(y_init);
    Serial.print('\t');
    Serial.print(z_init);
    Serial.print('\t');
    Serial.print(heading_init);
    Serial.println();
    Serial.println();
    #endif

    // запоминаем калибровочные значения в EEPROM
    EEPROM.write(0, x_init);
    EEPROM.write(1, x_init>>8);
    EEPROM.write(2, y_init);
    EEPROM.write(3, y_init>>8);
    EEPROM.write(4, z_init);
    EEPROM.write(5, z_init>>8);  
  }
  
  // получаем значение отклонения угла от начального
  int heading = (int) (atan2(z, y) * 180 / PI) - heading_init;
  
  // получаем пороговое значение отклонения с потенциометра
  int heading_threshold = map(analogRead(setup_pin), 0, 1023, MAX_DEGREE, MIN_DEGREE);

  #ifdef DEBUG  
  Serial.print(x);
  Serial.print("\t");
  Serial.print(y);
  Serial.print("\t");
  Serial.print(z);
  Serial.print("\t");
  Serial.print(heading);
  Serial.print("\t");
  Serial.print(heading_threshold);
  Serial.println();
  #endif 
  
  if (abs(heading) > heading_threshold)
    // отклонение больше установленного, включаем подсветку
    digitalWrite(switch_pin, HIGH);
  else 
    digitalWrite(switch_pin, LOW);
    
}
