//#define DEBUG_DATTEMP
//#define DEBUG_HEATING
//#define DEBUG_WARMFLOOR
#define TEMP_POL 18   // Минимальная температура теплого пола
#define normT 28      // Задание нормальной температуры воздуха поступающего в комнату для регуляции
#define kP 2          // Коэффициент пропорциональности

#include <DallasTemperature.h>
#include "DHT.h"
#include <EEPROM.h>
#include <OneWire.h>
#include <iarduino_RTC.h>  //Универсальная библиотека для RTC DS1302, DS1307, DS3231
/****************************Номера датчиков***********************************/
#define T_RAB 0		// Рабочая зона			Д1
#define T_SPAL 1	// Спальная зона 		Д2
#define T_POL 2		// Теплый пол 			Д4
#define T_PODVOZD 3 // Подаваемого воздуха 	Д5
#define T_ULICA 4 	// Уличный воздух 		Д6
#define T_RECUP 5 	// После рекуператора 	Д7
#define T_DHT 6		// Вытяжной воздух 		Д3
#define H_DHT 7
/****************************Контакты внешнего оборудования********************/
#define PIN_HEATING 26    // отопление
#define PIN_DHT 55        // датчик температуры и влажности
#define PIN_VENT_1 22     // реле 1, управляющее вентилятором
#define PIN_VENT_2 23     // реле 2, управляющее вентилятором
#define PIN_OFF_1 56      // выключатель вентиляции 1
#define PIN_OFF_2 57      // выключатель вентиляции 2
#define PIN_ZASL_OUT 4    // внешняя заслонка
#define PIN_ZASL_IN 5     // внутренняя заслонка (рекуператор)
#define PIN_POL 25        //контакт для теплого пола
iarduino_RTC time(RTC_DS1307); // Создание объекта для датчика температуры и влажности
DHT dht(PIN_DHT, DHT22); //Создание структуры датчика для температуры и влажности
OneWire oneWire(54);
DallasTemperature sensors(&oneWire);
DeviceAddress tempSensor[] = {
  {0x28, 0xFF, 0x6C, 0x37, 0xA1, 0x16, 0x04, 0xCC}, // 0 - rabZone
  {0x28, 0xFF, 0x9A, 0xCE, 0xA1, 0x16, 0x03, 0x97}, // 1 - spalZone
  {0x28, 0xFF, 0x66, 0x7E, 0xA1, 0x16, 0x05, 0x5C}, // 2 - teplPol
  {0x28, 0xFF, 0x11, 0x37, 0xA1, 0x16, 0x04, 0x4D}, // 3 - podVozd
  {0x28, 0xFF, 0x49, 0xD4, 0xA1, 0x16, 0x03, 0xA4}, // 4 - ulica
  {0x28, 0xFF, 0xAD, 0xCB, 0xA1, 0x16, 0x03, 0xC3}  // 5 - poslRecup
};
float infoTemp[8];
boolean floorState;
/****************************Уставки температуры для отопления*****************/
int ustTemp[][5][5] = {
  {
    {16, 25, 25, 26, 24},
    {20, 25, 25, 25, 25},
    {25, 25, 25, 25, 25},
    {28, 25, 25, 25, 25},
    {30, 25, 25, 25, 25}
  },
  {
    { -20, 27, 25, 25, 24},
    { -15, 26, 24, 24, 23},
    { -10, 26, 27, 26, 25},
    { -5, 25, 25, 26, 22},
    {0, 24, 24, 24, 22}
  }
};
struct Ventilation {  //структура для описания события
  long lastWork = 0; //последний запуск
  long timeWork = 10; //время работы 600 сек
  long timeNoWork = 30; //интервал 50 мин ( 3000 секунд )
  boolean stateWork = 0; //статус
  bool goodStart = 0;
  bool st = 0;
  bool oldSt = 0; //статусные переменные
  int zap = 0;
  int out = 0;  //Переменные для хранения положения заслонок
  int in  = 0;
  boolean freez = 0;
};
int delUprZasl[3];
long lastTime;
Ventilation vent ; //вентиляция
boolean season; // 0 summer, 1 - winter
/*************************Модуль вывода значений 1*******************************/
void printOut(String _text, int _value) {
  Serial.print(_text);
  Serial.print(":");
  Serial.print(_value);
  Serial.print("\t");
}
/*************************Модуль вывода значений 2*******************************/
void printOutLn(String _text, int _value) {
  Serial.print(_text);
  Serial.print(":");
  Serial.println(_value);
}
//**************************************************************************************************
// Обновление информации с датчиков
//**************************************************************************************************
void checkTemp() {
#ifdef DEBUG_DATTEMP
  Serial.println("***Temperature***");
#endif
  for (int i = 0; i < 6; i++)
  {
    sensors.requestTemperatures();
    infoTemp[i] = sensors.getTempC(tempSensor[i]);
#ifdef DEBUG_DATTEMP
    Serial.print("T");
    Serial.print(i);
    Serial.print(":");
    Serial.print(infoTemp[i]);
    Serial.print("\t");
#endif
  }
  infoTemp[T_DHT] = dht.readTemperature();
#ifdef DEBUG_DATTEMP
  Serial.print("T6:");
  Serial.print(infoTemp[T_DHT]);
  Serial.print("\t");
#endif
  infoTemp[H_DHT] = dht.readHumidity();
#ifdef DEBUG_DATTEMP
  Serial.print("H");
  Serial.print(":");
  Serial.print(infoTemp[H_DHT]);
  Serial.println("\t");
#endif
}
//**************************************************************************************************
// Проверка текущего сезона
//**************************************************************************************************
void checkMemSeason () {
  int curTemp = infoTemp[T_ULICA];
  int seasonFromMem = EEPROM.read(0x1);
  if ( curTemp < -5 && seasonFromMem != 1 )
    EEPROM.write(0x1,1);
  else if ( curTemp > 16 && seasonFromMem != 0 )
    EEPROM.write(0x1,0);
  season = EEPROM.read(0x1);
}
//**************************************************************************************************
// Алгоритм «Отопление»
//**************************************************************************************************
void heating() {
  int sumTemp = 0;
  int ind_1 = 0;
  int needT;
  sumTemp = infoTemp[T_RAB];
  sumTemp += infoTemp[T_SPAL];
  sumTemp += infoTemp[T_DHT];
  sumTemp /= 3;
  int ulTemp = infoTemp[T_ULICA];
  if ( season )
  {
    ind_1 = 4;
    while ( ulTemp <= ustTemp[season][ind_1][0] && ind_1 >= 0) ind_1--;
  }
  else
  {
    while ( ulTemp >= ustTemp[season][ind_1][0] && ind_1 < 5) ind_1++;
  }
  if ( time.Hours  >= 6 && time.Hours < 9) needT = ustTemp[season][ind_1][1];
  else if ( time.Hours  >= 9 && time.Hours < 18) needT = ustTemp[season][ind_1][2];
  else if ( time.Hours  >= 9 && time.Hours < 23) needT = ustTemp[season][ind_1][3];
  else needT = ustTemp[season][ind_1][4];
  digitalWrite(PIN_HEATING, needT < sumTemp ? 1 : 0);
#ifdef DEBUG_HEATING
  printOut("T_Out", ulTemp);
  printOut("Season", season);
  printOut("Ustan", ustTemp[season][ind_1][0]);
  printOut("T_In", sumTemp);
  printOut("T_Need", needT);
  printOutLn("Status", needT < sumTemp ? 1 : 0);
#endif
}
//**************************************************************************************************
// Алгоритм  «Подогрев пола»
//**************************************************************************************************
void warmfloor() {
  int tempPol = infoTemp[T_POL];
  boolean floorState;
  if (season && tempPol < TEMP_POL)     //если пол остыл ниже 18 градусов
   floorState = 0;
  else if (season && tempPol > TEMP_POL + 2) //Если пол нагрелся до 20
    floorState = 1;
  digitalWrite(PIN_POL, floorState);
  #ifdef DEBUG_WARMFLOOR
    printOut("Floor state",floorState);
    printOut("Season",season);
    printOutLn("Real temp",tempPol);
    #endif
}
//**************************************************************************************************
// Защита от обмораживания рекуператора
//**************************************************************************************************
void checkFreez() {
  if ( infoTemp[T_RECUP] <= 0 ) { vent.freez = 1;}
  if ( vent.freez == 1 )
  {
    if ( infoTemp[vent.freez] >= infoTemp[T_SPAL]-2 ) 
    {
      vent.freez = 0;
      polZasl(100, 0);
      speedVent(1);
    }
  }
}
/*************************Запуск вентилятора в указанном режиме**************/
void speedVent ( byte dSpeed) {
  if ( dSpeed == 0 )
  {
    digitalWrite(PIN_VENT_1, 1);
    digitalWrite(PIN_VENT_2, 1);
  }
  else if  ( dSpeed == 0 )
  {
    digitalWrite(PIN_VENT_1, 0);
    digitalWrite(PIN_VENT_2, 1);
  }
  else if  ( dSpeed == 2 )
  {
    digitalWrite(PIN_VENT_1, 1);
    digitalWrite(PIN_VENT_2, 0);
  }
}
/*************************Установка заслонок в указанное положение***********/
void polZasl (byte _In, byte _Out) {
  vent.in=constrain(_In,0,100);
  vent.out=constrain(_Out,0,100);
  analogWrite(PIN_ZASL_IN, map(vent.in, 0, 100, 0, 255));
  analogWrite(PIN_ZASL_OUT, map(vent.in, 0, 100, 0, 255));
}
//**************************************************************************************************
// Запуск проветривания
//**************************************************************************************************
void startVent() {
  Serial.print("Start is");
  season = 1;
  if ( season ) // Запуск зимой
  {
    polZasl(100, 0); //  Открытие клапана рециркуляции
    speedVent(1); // Запуск вентилятора
    delay(500); // Прогрев пластин рекуператора
    if ( infoTemp[T_RECUP] < infoTemp[T_SPAL] - 2 ) 
    {
      vent.goodStart = 0;
      Serial.println("BAD");
    }
    else 
    {
      polZasl(50, 100); // рециркуляция  - 50, приточный - 100
      delay(500); // Прогрев пластин рекуператора
      vent.goodStart = 1;
      Serial.println("GOOD");
    }
  }
  else // запуск летом
  {
    polZasl(100, 0); //  Открытие клапана рециркуляции
    speedVent(1); // Запуск вентилятора
    delay(1000); // Прогрев пластин рекуператора
    polZasl(0, 100); // рециркуляция  - 0, приточный - 100
    vent.goodStart = 1;
  }
}
//**************************************************************************************************
// Остановка проветривания
//**************************************************************************************************
void stopVent  () {
  polZasl(100, 0);
  delay(2000);
  speedVent(0);
  vent.goodStart = 0;
}

/*************************Регуляция проветривания****************************/
void workVent() {   //регулирование подаваемого воздуха в П режиме
  //начальное положение заслонок в зависимости от времени года
  vent.out = 255;
  vent.in = 135 * season;
  int error = infoTemp[T_PODVOZD] - normT; // считаем отклонение
  if (abs(error) > 2) //Проверка на гистерезис
  {
    vent.out = vent.out + error * kP;
    vent.in = vent.in - error * kP;
    constrain(vent.out, 0, 255);
    constrain(vent.in, 0, 255);
    analogWrite(PIN_ZASL_OUT, vent.out);
    analogWrite(PIN_ZASL_IN, vent.in);
  }
  printOut("error: ", error);
  printOut("sVent.out:", vent.out);
  printOut("sVent.in:", vent.in);
}
//**************************************************************************************************
// Регулировка проветривания согласно таблице из ТЗ
//**************************************************************************************************
void tadblUprVent()
{	
	int needTemp = infoTemp[T_PODVOZD];
	if ( needTemp < 5) 
	{
		delUprZasl[1] = 2;
		delUprZasl[2] = -2;
		if ( needTemp < 0)  delUprZasl[0] = 5;
		else if ( needTemp < 5)  delUprZasl[0] = 7;
	}
	else if ( needTemp < 14)
	{
		delUprZasl[1] = 2;
		delUprZasl[2] = vent.zasl[0]==100?-2:0;
		if ( needTemp < 8) delUprZasl[0] = 10;
		else if ( needTemp < 10) delUprZasl[0] = 20;
		else delUprZasl[0] = 30;
	}
	else if ( needTemp > 18) 
	{
		delUprZasl[1]=-2;
		delUprZasl[2]=vent.zasl[0]==0?2:0;
		if ( needTemp < 22) delUprZasl[0] = 30;
		else if ( needTemp < 25) delUprZasl[0] = 20;
		else if ( needTemp < 28) delUprZasl[0] = 10;
		else delUprZasl[0] = 5;
	}
	// проверка времени
    long timeT =  time.seconds + time.minutes * 60 + (long)time.Hours*60*60;
	if ( timeT + delUprZasl[0] >= 86400 ) timeT+=86400;
	if ( lastTime + delUprZasl[0] > timeT ) 
	{
		polZasl(vent.zasl[0]+delUprZasl[1],vent.zasl[1]+delUprZasl[2]);
		lastTime = currTime();
	}
}
//**************************************************************************************************
// Алгоритм  «Проветривание»
//**************************************************************************************************
void ventilation() {
  // Читаем состояние внешних контактов
  int rezVent = !(digitalRead(PIN_OFF_1) || digitalRead(PIN_OFF_2)) + !digitalRead(PIN_OFF_1);
  /*
  Serial.print("Vent status: ");
  Serial.print(rezVent);
  Serial.print("\tTime: ");
  Serial.print(time.Hours);
  Serial.print("h");
  Serial.print(time.minutes);
  Serial.print("m");
  Serial.print(time.seconds);
  Serial.print("s\t");
  */
    
  if ( rezVent == 1 )
  {
    long timeT =  time.seconds + time.minutes * 60 + (long)time.Hours*60*60;
  Serial.print("All Sec:");
  Serial.print(timeT);
  Serial.print("\t");
  Serial.print("lastWork:");
  Serial.print(vent.lastWork);
  Serial.print("\t");
    if ( (vent.lastWork + vent.timeWork) >= 86400 || vent.lastWork + vent.timeNoWork > 86400 ) timeT += 86400;// выключение или выключение выпадает на след день
    
    //Если OFF и текущее время больше времени последнего запуска + интервал запуска
    if ( !vent.stateWork && timeT > vent.lastWork + vent.timeNoWork )
    {
      Serial.println("Start");
      startVent();
      if ( vent.goodStart )
      { 
        vent.stateWork = 1; 
        vent.lastWork = timeT;
      }
    }
    //Если ON и время больше, чем время запуска + время работы.
    else if ( vent.stateWork && timeT > vent.lastWork + vent.timeWork ) //&& vent.goodStart)
    {
      Serial.println("Stop");
      vent.stateWork = 0; 
      vent.lastWork = timeT;
      stopVent();
    }
    else if ( vent.stateWork )
    {
      Serial.println("Work");
      //workVent();      //Вентиляция работает, регулируем
    }
    else
      Serial.println("Off"); //Вентиляция выключена
      
  }
  else // работа в режиме принудительного проветривания  или выключение
  {
    Serial.println("Work by switch"); //Вентиляция выключена
    speedVent(rezVent);
  }
}
//**************************************************************************************************
// Инициализация модулей
//**************************************************************************************************
void setup() {
  Serial.begin(9600);   //активируем связь с ПК для отладки
  dht.begin();      // начало работы с датчиком температуры и влажности
  time.begin();     //начало работы с часами
  pinMode (PIN_POL , OUTPUT);
  pinMode (PIN_OFF_1, INPUT_PULLUP);
  pinMode (PIN_OFF_2, INPUT_PULLUP);
  pinMode (PIN_ZASL_OUT, OUTPUT);
  pinMode (PIN_ZASL_IN, OUTPUT);
  pinMode (PIN_VENT_1, OUTPUT);
  pinMode (PIN_VENT_2, OUTPUT);
  speedVent(0);
  polZasl(0,0);
}
//**************************************************************************************************
// Бесконечный цикл
//**************************************************************************************************
void loop() {
  time.gettime();
  checkTemp();      // обновление датчиков
  checkMemSeason();   // проверка сезонов
  checkFreez();
  if ( !vent.freez )  ventilation();      // Вентиляция
  heating();   // Отопление
  warmfloor();      // Теплый пол
}
