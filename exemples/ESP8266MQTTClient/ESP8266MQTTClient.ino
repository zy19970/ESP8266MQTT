/*
    项目名称：ESP8266MQTT库示例
    版本号：v1.0
    修改时间：2019.03.19
    使用开发板：WiFiduino（Arduino UNO+ESP8266）
    知识产权归 InTron™版权所有©保留权力。
*/

#include "ESP8266MQTT.h"

void onConnectionEstablished();

ESP8266MQTT client(
  "ssid",             // Wifi ssid
  "pass",             // Wifi password
  "192.168.1.101",    // MQTT broker ip
  1883,               // MQTT broker port
  "usr",              // MQTT username
  "mqttpass",         // MQTT password
  "test1",            // Client name
  onConnectionEstablished, // Connection established callback
  true,               // Enable web updater
  true                // Enable debug messages
);



void setup()
{
  Serial.begin(115200);
}



void onConnectionEstablished()
{
  // 订阅主题并且将该主题内收到的消息通过串口发送
  client.subscribe("mytopic/test", [](const String &payload) {
    Serial.println(payload);//此处可以编写一个函数来代替
  });

  // 向某个主题发送消息
  client.publish("mytopic/test", "This is a message");

}

void loop()
{
  client.loop();
}

