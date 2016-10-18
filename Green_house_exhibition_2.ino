/*
  Пример работы со всем набором датчиков
  Выводы 4, 10, 11, 12, 13 нельзя использовать из-за Ethernet платы
  Выводы 0 и 1 нежелательно использовать, так как это порт отладки
  Created by Rostislav Varzar
*/

#include <DHT.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Ethernet.h>
#include <ArduinoJson.h>
#include <Servo.h>

// Датчики DS18B20
#define DS18B20_1 8
#define DS18B20_2 9
OneWire oneWire1(DS18B20_1);
OneWire oneWire2(DS18B20_2);
DallasTemperature ds_sensor1(&oneWire1);
DallasTemperature ds_sensor2(&oneWire2);

// Датчики DHT11
#define DHTPIN 2
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// Датчики влажности почвы и освещенности
#define MOISTURE_1 A0
#define MOISTURE_2 A1
#define LIGHT A2

// Выходы реле
#define RELAY1 4
#define RELAY2 6
#define RELAY3 7
#define RELAY4 A3

// Выход ШИМ
#define PWM_LED 3

// Сервомоторы
#define SERVO1_PWM 5
Servo servo_1;

// Настройки сетевого адаптера
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
// Local IP if DHCP fails
IPAddress ip(192, 168, 1, 250);
IPAddress dnsServerIP(192, 168, 1, 1);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
EthernetClient client;

// Частота опроса датчика DHT11
#define DHT11_UPDATE_TIME 3000
// Частота опроса датчика DS18B20 №1
#define DS1_UPDATE_TIME 1000
// Частота опроса датчика DS18B20 №2
#define DS2_UPDATE_TIME 1000
// Частота опроса аналоговых датчиков
#define ANALOG_UPDATE_TIME 1000
// Частота вывода данных на сервер IoT
#define IOT_UPDATE_TIME 1000

// Таймер обновления опроса датчика DHT11
long timer_dht11 = 0;
// Таймер обновления опроса датчика DS18B20 №1
long timer_ds1 = 0;
// Таймер обновления опроса датчика DS18B20 №1
long timer_ds2 = 0;
// Таймер обновления опроса аналоговых датчиков
long timer_analog = 0;
// Таймер обновления вывода на сервер IoT
long timer_iot = 0;

// Состояние помпы
int pump_state = 0;
// Состояние освещения
int light_state = 0;
// Состояние крышки
int window_state = 0;

// Параметры IoT сервера
char iot_server[] = "cttit5402.cloud.thingworx.com";
IPAddress iot_address(52, 87, 101, 142);
char appKey[] = "92b0be1d-e8c3-4b40-86a7-d58865d23684";
char thingName[] = "MGBot_Exhibition_Greenhouse";
char serviceName[] = "MGBot_Exhibition_SetParams";

// Параметры сенсоров для IoT сервера
#define sensorCount 7                                     // How many values you will be pushing to ThingWorx
char* sensorNames[] = {"dht11_temp", "dht11_hum", "sensor_light", "ds18b20_temp1", "ds18b20_temp2", "soil_moisture1", "soil_moisture2"};
float sensorValues[sensorCount];
// Номера датчиков
#define dht11_temp     0
#define dht11_hum      1
#define sensor_light   2
#define ds18b20_temp1  3
#define ds18b20_temp2  4
#define soil_moisture1 5
#define soil_moisture2 6

// Максимальное время ожидания ответа от сервера
#define IOT_TIMEOUT1 5000
#define IOT_TIMEOUT2 100

// Таймер ожидания прихода символов с сервера
long timer_iot_timeout = 0;

// Размер приемного буффера
#define BUFF_LENGTH 64

// Приемный буфер
char buff[BUFF_LENGTH] = "";

void setup()
{
  // Инициализация последовательного порта
  Serial.begin(115200);
  delay(1000);
  // Инициализация датчиков температуры DS18B20
  ds_sensor1.begin();
  ds_sensor2.begin();
  // Инициализация датчика DHT11
  dht.begin();
  // Инициализация выходов реле
  pinMode(RELAY1, OUTPUT);
  pinMode(RELAY2, OUTPUT);
  pinMode(RELAY3, OUTPUT);
  pinMode(RELAY4, OUTPUT);
  // Инициализация выхода на ШИМ
  pinMode(PWM_LED, OUTPUT);
  // Инициализация портов для управления сервомоторами
  servo_1.attach(SERVO1_PWM);
  // Инициализация сетевой платы
  if (Ethernet.begin(mac) == 0)
  {
    Serial.println("Failed to configure Ethernet using DHCP");
    Ethernet.begin(mac, ip, dnsServerIP, gateway, subnet);
  }
  Serial.print("LocalIP: ");
  Serial.println(Ethernet.localIP());
  Serial.print("SubnetMask: ");
  Serial.println(Ethernet.subnetMask());
  Serial.print("GatewayIP: ");
  Serial.println(Ethernet.gatewayIP());
  Serial.print("dnsServerIP: ");
  Serial.println(Ethernet.dnsServerIP());
  Serial.println("");
  // Однократный опрос датчиков
  readSensorDHT11();
  readSensorDS1();
  readSensorDS2();
  readSensorAnalog();
  // Установка сервомотора в крайнее положение
  servo_1.write(90);
}

void loop()
{
  // Опрос датчика DHT11
  if (millis() > timer_dht11 + DHT11_UPDATE_TIME)
  {
    // Опрос датчиков
    readSensorDHT11();
    // Сброс таймера
    timer_dht11 = millis();
  }
  // Опрос датчика DS18B20 №1
  if (millis() > timer_ds1 + DS1_UPDATE_TIME)
  {
    // Опрос датчиков
    readSensorDS1();
    // Сброс таймера
    timer_ds1 = millis();
  }
  // Опрос датчика DS18B20 №2
  if (millis() > timer_ds2 + DS2_UPDATE_TIME)
  {
    // Опрос датчиков
    readSensorDS2();
    // Сброс таймера
    timer_ds2 = millis();
  }
  // Опрос аналоговых датчиков
  if (millis() > timer_analog + ANALOG_UPDATE_TIME)
  {
    // Опрос датчиков
    readSensorAnalog();
    // Сброс таймера
    timer_analog = millis();
  }
  // Вывод данных на сервер IoT
  if (millis() > timer_iot + IOT_UPDATE_TIME)
  {
    // Вывод данных на сервер IoT
    sendDataIot();
    // Сброс таймера
    timer_iot = millis();
  }
}

// Чтение датчика DHT11
void readSensorDHT11()
{
  // DHT11
  sensorValues[dht11_hum] = dht.readHumidity();
  sensorValues[dht11_temp] = dht.readTemperature();
}

// Чтение датчика DS18B20 №1
void readSensorDS1()
{
  // DS18B20
  ds_sensor1.requestTemperatures();
  sensorValues[ds18b20_temp1] = ds_sensor1.getTempCByIndex(0);
}

// Чтение датчика DS18B20 №2
void readSensorDS2()
{
  // DS18B20
  ds_sensor2.requestTemperatures();
  sensorValues[ds18b20_temp2] = ds_sensor2.getTempCByIndex(0);
}

// Чтение аналоговых датчиков
void readSensorAnalog()
{
  // Аналоговые датчики
  sensorValues[sensor_light] = (1023.0 - analogRead(LIGHT)) / 1023.0 * 100.0;
  sensorValues[soil_moisture1] = analogRead(MOISTURE_1) / 1023.0 * 100.0;
  sensorValues[soil_moisture2] = analogRead(MOISTURE_2) / 1023.0 * 100.0;
}

// Подключение к серверу IoT ThingWorx
void sendDataIot()
{
  // Подключение к серверу
  Serial.println("Connecting to IoT server...");
  if (client.connect(iot_address, 80))
  {
    // Проверка установления соединения
    if (client.connected())
    {
      // Отправка заголовка сетевого пакета
      Serial.println("Sending data to IoT server...\n");
      Serial.print("POST /Thingworx/Things/");
      client.print("POST /Thingworx/Things/");
      Serial.print(thingName);
      client.print(thingName);
      Serial.print("/Services/");
      client.print("/Services/");
      Serial.print(serviceName);
      client.print(serviceName);
      Serial.print("?appKey=");
      client.print("?appKey=");
      Serial.print(appKey);
      client.print(appKey);
      Serial.print("&method=post&x-thingworx-session=true");
      client.print("&method=post&x-thingworx-session=true");
      // Отправка данных с датчиков
      for (int idx = 0; idx < sensorCount; idx ++)
      {
        Serial.print("&");
        client.print("&");
        Serial.print(sensorNames[idx]);
        client.print(sensorNames[idx]);
        Serial.print("=");
        client.print("=");
        Serial.print(sensorValues[idx]);
        client.print(sensorValues[idx]);
      }
      // Закрываем пакет
      Serial.println(" HTTP/1.1");
      client.println(" HTTP/1.1");
      Serial.println("Accept: application/json");
      client.println("Accept: application/json");
      Serial.print("Host: ");
      client.print("Host: ");
      Serial.println(iot_server);
      client.println(iot_server);
      Serial.println("Content-Type: application/json");
      client.println("Content-Type: application/json");
      Serial.println();
      client.println();

      // Ждем ответа от сервера
      timer_iot_timeout = millis();
      while ((client.available() == 0) && (millis() < timer_iot_timeout + IOT_TIMEOUT1));

      // Выводим ответ о сервера, и, если медленное соединение, ждем выход по таймауту
      int iii = 0;
      bool currentLineIsBlank = true;
      bool flagJSON = false;
      timer_iot_timeout = millis();
      while ((millis() < timer_iot_timeout + IOT_TIMEOUT2) && (client.connected()))
      {
        while (client.available() > 0)
        {
          char symb = client.read();
          Serial.print(symb);
          if (symb == '{')
          {
            flagJSON = true;
          }
          else if (symb == '}')
          {
            flagJSON = false;
          }
          if (flagJSON == true)
          {
            buff[iii] = symb;
            iii ++;
          }
          timer_iot_timeout = millis();
        }
      }
      buff[iii] = '}';
      buff[iii + 1] = '\0';
      Serial.println(buff);
      // Закрываем соединение
      client.stop();

      // Расшифровываем параметры
      StaticJsonBuffer<BUFF_LENGTH> jsonBuffer;
      JsonObject& json_array = jsonBuffer.parseObject(buff);
      pump_state = json_array["pump_state"];
      window_state = json_array["window_state"];
      light_state = json_array["light_state"];
      Serial.println("Pump state:   " + String(pump_state));
      Serial.println("Light state:  " + String(light_state));
      Serial.println("Window state: " + String(window_state));
      Serial.println();

      // Делаем управление устройствами
      controlDevices();

      Serial.println("Packet successfully sent!");
      Serial.println();
    }
  }
}

// Управление исполнительными устройствами
void controlDevices()
{
  // Форточка
  if (window_state)
  {
    servo_1.write(120);
  }
  else
  {
    servo_1.write(90);
  }
  // Освещение
  analogWrite(PWM_LED, light_state * 2.55);
  // Помпа
  digitalWrite(RELAY1, pump_state);
}

