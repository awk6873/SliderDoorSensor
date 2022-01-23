// Высокочувствительный магнитный датчик открывания двери шкафа-купе для включения внутренней подсветки
// на HMC5883L (GY-271) и ATTiny85
// Дата последней модификации 2022.01.23
//
// Используется изменение проекции оси Z на ось Y магнетометра
// Для калибровки в положении "дверь закрыта" установить перемычку между setup_pin и GND,
// включить питание, подождать не менее 30 сек., выключить питание и снять перемычку
// Для калибровки порога срабатывания в DEV режиме подключить потенциометр на 10 кОм
// между Vcc и GND, средний контакт на setup_pin, раскомментарить строчку #define SETUP 1,
// перекомпилировать и загрузить прошивку в МК.
// Примерное значение порога срабатывания для условий:
// неодимовый магнит 15x15x2 мм на расстоянии 6 см от датчика - 45 градусов

// Распиновка
// ATTiny85:
//   SDA - 0 (pin 5), SCL - 2 (pin 7)
//   Регулировка чувствительности - A2 (pin 3)
//   Выход на ключ - 1 (pin 6)
// Arduino UNO (для отладки):
//   SDA - A4, SCL - A5
//   Регулировка чувствительности - A2
//   Выход на ключ - 13 

// #define DEBUG 1 
// #define SETUP 1
#include <EEPROM.h>

#include <Wire.h>  // I2C ATTiny Master


#define CALIBRATE_MEASURES 10  // 10 замеров на калибровку
#define LOOP_DELAY 200   // 5 циклов/сек.

#define MIN_DEGREE 10    // диапазон регулировки чувствительности 
#define MAX_DEGREE 150

#define HMC5883L 0x1E    // 0011110b, I2C 7bit address of HMC5883
#define setup_pin A2     // вход от потенциометра для калибровки чувствительности
#define switch_pin 1     // 13   // выход на ключ MOSFET

int heading_init;
int heading_threshold;

int measure_count = 0;

void setup(){
  
  pinMode(setup_pin, INPUT_PULLUP);
  pinMode(switch_pin, OUTPUT);

  // считываем калибровочное значение угла и порог срабатывания из EEPROM
  heading_init = EEPROM.read(0) | EEPROM.read(1) << 8;
  heading_threshold = EEPROM.read(2) | EEPROM.read(3) << 8;
  
  #ifdef DEBUG
  Serial.begin(9600);

  Serial.println();
  Serial.println("Heading init and threashold values from EEPROM:");
  Serial.print(heading_init);
  Serial.print("\t");
  Serial.print(heading_threshold);
  Serial.println();
  Serial.println();
  #endif

  // инициализация I2C
  Wire.begin();
  
  // инициализация HMC5883L
  Wire.beginTransmission(HMC5883L);    //open communication with HMC5883
  Wire.write(0x02);                    //select mode register
  Wire.write(0x00);                    //continuous measurement mode
  Wire.endTransmission();
}

void loop(){
  int x, y, z, x_init, y_init, z_init; //triple axis data
  int heading;

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

  if (analogRead(setup_pin) < 50 && ++measure_count < CALIBRATE_MEASURES) {
    // setup_pin подтянут к земле, т.е. включен режим калибровки,
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
  }     
  
  if (analogRead(setup_pin) < 50 && measure_count == CALIBRATE_MEASURES) {
    // калибровка завершена

    // получаем значение начального угла
    heading_init = (int) (atan2(z_init, y_init) * (float)180 / PI);

    #ifdef DEBUG
    Serial.println("Writing heading init value to EEPROM:");
    Serial.print(heading_init);
    Serial.println();
    Serial.println();
    #endif

    // запоминаем калибровочное значение в EEPROM
    EEPROM.write(0, heading_init);
    EEPROM.write(1, heading_init >> 8);
  }

  #ifdef SETUP
  // включен режим калибровки порога
  // получаем пороговое значение отклонения с потенциометра
  heading_threshold = map(analogRead(setup_pin), 0, 1023, MAX_DEGREE, MIN_DEGREE);
    #ifdef DEBUG
  Serial.println("Writing heading threshold value to EEPROM:");
  Serial.print(heading_threshold);
  Serial.println();
  Serial.println();
    #endif
  // запоминаем калибровочное значение в EEPROM
  EEPROM.write(2, heading_threshold);
  EEPROM.write(3, heading_threshold >> 8);
  #endif

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

  // получаем значение отклонения угла от начального
  heading = (int) (atan2(z, y) * (float)180 / PI) - heading_init;
  
  if (abs(heading) > heading_threshold)
    // отклонение больше установленного, включаем подсветку
    digitalWrite(switch_pin, HIGH);
  else 
    digitalWrite(switch_pin, LOW); 
    
}
