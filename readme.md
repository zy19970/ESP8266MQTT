# ESP8266MQTT库介绍文档
本ESP8266MQTT库是针对Arduino创建MQTT客户端更加便捷，由开源库PubSubClient（https://github.com/knolleary/pubsubclient） 二次开发而来，相对于PubSubClient重新封装了一些函数，方便使用。为方便大家更好的运用PubSubClient库，我也编写了PubSubClient的[中文文档](https://github.com/zy19970/Pubsubclient_API_document)，大家可以参考使用。
## 依赖库
    #include <ESP8266WiFi.h>
    #include <PubSubClient.h>
    #include <ESP8266WebServer.h>
    #include <ESP8266mDNS.h>
    #include <ESP8266HTTPUpdateServer.h>
在使用ESP8266MQTT库开发时，一定要确保上述引用的存在。
## 主要功能介绍
**void loop()**<br>
应定期调用此方法以允许客户端处理传入消息并维护其与服务器的连接。

**bool isConnected()**<br>
判断客户端是否处于连接状态，包括MQTT连接和WIFI连接。

**void publish(const String &topic, const String &payload, bool retain = false);**<br>
向topic推送payload内容的消息，并且设置retain的状态，默认是false。

**void subscribe(const String &topic, MessageReceivedCallback messageReceivedCallback);**<br>
订阅topic主题的消息，并且在接收到消息后运行回调函数。

**void unsubscribe(const String &topic);**<br>
取消订阅topic主题的消息。


