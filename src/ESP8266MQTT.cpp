#include "ESP8266MQTT.h"


// =============== Constructor / destructor ===================

ESP8266MQTT::ESP8266MQTT(const char wifiSsid[], const char* wifiPassword, const char* mqttServerIp,
  const short mqttServerPort, const char* mqttUsername, const char* mqttPassword,
  const char* mqttClientName, ConnectionEstablishedCallback connectionEstablishedCallback,
  const bool enableWebUpdater, const bool enableSerialLogs)
  : mWifiSsid(wifiSsid), mWifiPassword(wifiPassword), mMqttServerIp(mqttServerIp),
    mMqttServerPort(mqttServerPort), mMqttUsername(mqttUsername), mMqttPassword(mqttPassword),
    mMqttClientName(mqttClientName), mConnectionEstablishedCallback(connectionEstablishedCallback),
    mEnableWebUpdater(enableWebUpdater), mEnableSerialLogs(enableSerialLogs)
{
  mWifiConnected = false;
  mMqttConnected = false;
  mLastWifiConnectionAttemptMillis = 0;
  mLastWifiConnectionSuccessMillis = 0;
  mLastMqttConnectionMillis = 0;
    
  mTopicSubscriptionListSize = 0;

  // 网页更新程序
  if(mEnableWebUpdater)
  {
    mHttpServer = new ESP8266WebServer(80);
    mHttpUpdater = new ESP8266HTTPUpdateServer();
  }

  // 创建MQTT客户端
  mWifiClient = new WiFiClient();
  mMqttClient = new PubSubClient(mMqttServerIp, mMqttServerPort, *mWifiClient);
  mMqttClient->setCallback([this](char* topic, byte* payload, unsigned int length) {this->mqttMessageReceivedCallback(topic, payload, length);});
}

ESP8266MQTT::~ESP8266MQTT() {}


// =============== Public functions =================

void ESP8266MQTT::loop()
{
  long currentMillis = millis();
  
  if (WiFi.status() == WL_CONNECTED)
  {
    if(!mWifiConnected)
    {
      if(mEnableSerialLogs)
        Serial.printf("\nWifi connected, ip : %s \n", WiFi.localIP().toString().c_str());

      mLastWifiConnectionSuccessMillis = millis();
      
      // 配置Web更新程序
      if (mEnableWebUpdater)
      {
        MDNS.begin(mMqttClientName);
        mHttpUpdater->setup(mHttpServer, "/", mMqttUsername, mMqttPassword);
        mHttpServer->begin();
        MDNS.addService("http", "tcp", 80);
      
        if(mEnableSerialLogs)
          Serial.printf("Web updater ready, Open http://%s.local in your browser and login with username '%s' and password '%s'\n", mMqttClientName, mMqttUsername, mMqttPassword);
      }
  
      mWifiConnected = true;
    }
    
    // 处理MQTT
    if (mMqttClient->connected())
    {    
      mMqttClient->loop();
    }
    else
    {
      if(mMqttConnected)
      {
        if(mEnableSerialLogs)
          Serial.println("失去了MQTT连接.");
        
        mTopicSubscriptionListSize = 0;
        
        mMqttConnected = false;
      }
      
      if(currentMillis - mLastMqttConnectionMillis > CONNECTION_RETRY_DELAY || mLastMqttConnectionMillis == 0)
        connectToMqttBroker();
    }
      
    // 处理Web更新程序
    if(mEnableWebUpdater)
      mHttpServer->handleClient();
  }
  else
  {
    if(mWifiConnected) 
    {
      if(mEnableSerialLogs)
        Serial.println("失去WIFI连接.");
      
      mWifiConnected = false;
      WiFi.disconnect();
    }
    
    // 如果自上次连接丢失后没有重连，我们将重试连接到wifi
    if(mLastWifiConnectionAttemptMillis == 0 || mLastWifiConnectionSuccessMillis > mLastWifiConnectionAttemptMillis)
      connectToWifi();
  }
  
  if(mDelayedExecutionListSize > 0)
  {
    long currentMillis = millis();
    
    for(byte i = 0 ; i < mDelayedExecutionListSize ; i++)
    {
      if(mDelayedExecutionList[i].targetMillis <= currentMillis)
      {
        (*mDelayedExecutionList[i].callback)();
        for(int j = i ; j < mDelayedExecutionListSize-1 ; j++)
          mDelayedExecutionList[j] = mDelayedExecutionList[j + 1];
        mDelayedExecutionListSize--;
        i--;
      }
    }
  }
}

bool ESP8266MQTT::isConnected() const
{
  return mWifiConnected && mMqttConnected;
}

void ESP8266MQTT::publish(const String &topic, const String &payload, bool retain)
{
  mMqttClient->publish(topic.c_str(), payload.c_str(), retain);

  if (mEnableSerialLogs)
    Serial.printf("MQTT - 【发送消息】 [%s] %s \n", topic.c_str(), payload.c_str());
}

void ESP8266MQTT::subscribe(const String &topic, MessageReceivedCallback messageReceivedCallback)
{
  mMqttClient->subscribe(topic.c_str());

  if (mEnableSerialLogs)
    Serial.printf("MQTT - 【订阅】 %s \n", topic.c_str());

  if (mTopicSubscriptionListSize < MAX_TOPIC_SUBSCRIPTION_LIST_SIZE)
  {
    mTopicSubscriptionList[mTopicSubscriptionListSize] = { topic, messageReceivedCallback };
    mTopicSubscriptionListSize++;
  }
  else if (mEnableSerialLogs)
    Serial.println("【错误】 【订阅】 达到最大回调大小.");
}

void ESP8266MQTT::unsubscribe(const String &topic)
{
  bool found = false;

  for (int i = 0; i < mTopicSubscriptionListSize; i++)
  {
    if (!found)
    {
      if (mTopicSubscriptionList[i].topic.equals(topic))
      {
        found = true;
        mMqttClient->unsubscribe(topic.c_str());
        if (mEnableSerialLogs)
          Serial.printf("MQTT - 【取消订阅】 %s \n", topic.c_str());
      }
    }

    if (found)
    {
      if ((i + 1) < MAX_TOPIC_SUBSCRIPTION_LIST_SIZE)
        mTopicSubscriptionList[i] = mTopicSubscriptionList[i + 1];
    }
  }

  if (found)
    mTopicSubscriptionListSize--;
  else if (mEnableSerialLogs)
    Serial.println("【错误】 【订阅】 订阅主题未找到.");
}

void ESP8266MQTT::executeDelayed(const long delay, DelayedExecutionCallback callback)
{
  if(mDelayedExecutionListSize < MAX_DELAYED_EXECUTION_LIST_SIZE)
  {
    DelayedExecutionRecord delayedExecutionRecord;
    delayedExecutionRecord.targetMillis = millis() + delay;
    delayedExecutionRecord.callback = callback;
    
    mDelayedExecutionList[mDelayedExecutionListSize] = delayedExecutionRecord;
    mDelayedExecutionListSize++;
  }
  else if(mEnableSerialLogs)
    Serial.printf("\n【错误】 达到最大延迟执行列表大小");
}


// ================== Private functions ====================-

void ESP8266MQTT::connectToWifi()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(mWifiSsid, mWifiPassword);

  if(mEnableSerialLogs)
    Serial.printf("\nConnecting to %s ", mWifiSsid);
  
  mLastWifiConnectionAttemptMillis = millis();
}

void ESP8266MQTT::connectToMqttBroker()
{
  if(mEnableSerialLogs)
    Serial.printf("\n正在连接MQTT服务器 %s ", mMqttServerIp);

  if (mMqttClient->connect(mMqttClientName, mMqttUsername, mMqttPassword))
  {
    mMqttConnected = true;
    
    if(mEnableSerialLogs) 
      Serial.print("成功！ \n");

    (*mConnectionEstablishedCallback)();
  }
  else if (mEnableSerialLogs)
  {
    Serial.print("无法连接, ");

    switch (mMqttClient->state())
    {
      case -4:
        Serial.print("连接超时");
        break;
      case -3:
        Serial.print("失去连接");
        break;
      case -2:
        Serial.print("连接失败");
        break;
      case -1:
        Serial.print("断开连接");
        break;
      case 1:
        Serial.print("连接协议错误");
        break;
      case 2:
        Serial.print("客户端ID错误");
        break;
      case 3:
        Serial.print("连接不可用");
        break;
      case 4:
        Serial.print("连接证书错误");
        break;
      case 5:
        Serial.print("连接未经授权");
        break;
    }
  }
  
  mLastMqttConnectionMillis = millis();
}

void ESP8266MQTT::mqttMessageReceivedCallback(char* topic, byte* payload, unsigned int length)
{
  // 将接收到的消息转换为String
  char buffer[MAX_MQTT_PAYLOAD_SIZE];

  if (length >= MAX_MQTT_PAYLOAD_SIZE)
    length = MAX_MQTT_PAYLOAD_SIZE - 1;

  strncpy(buffer, (char*)payload, length);
  buffer[length] = '\0';

  String payloadStr = buffer;

  if(mEnableSerialLogs)
    Serial.printf("MQTT - 【接收消息】 【%s】 %s \n", topic, payloadStr.c_str());

  // Send the message to subscribers
  for (int i = 0 ; i < mTopicSubscriptionListSize ; i++)
  {
    if (mTopicSubscriptionList[i].topic.equals(topic))
      (*mTopicSubscriptionList[i].callback)(payloadStr); // 调用回调函数
  }
}
