#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "config.h"
#include "artnet/ArtnetNode.h"

class WebServer {
public:
    WebServer(ArtnetNode* node);
    ~WebServer();

    // 基本功能
    void begin();
    void update();

      // 添加配置更新处理方法声明
    void handleConfigUpdate(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    void handleConfig(AsyncWebServerRequest* request);
    void applyConfig();


    // 配置管理
    struct Config {
        char deviceName[32];
        char wifiSSID[32];
        char wifiPassword[32];
        bool dhcpEnabled;
        uint8_t staticIP[4];
        uint8_t staticMask[4];
        uint8_t staticGateway[4];
        uint8_t artnetNet;
        uint8_t artnetSubnet;
        uint8_t artnetUniverse;
        uint16_t dmxStartAddress;
        uint16_t pixelCount;
        uint8_t pixelType;
        bool pixelEnabled;
    };

    void loadConfig();
    void saveConfig();
    const Config& getConfig() const { return config; }

private:
    AsyncWebServer* server;
    AsyncWebSocket* ws;
    ArtnetNode* artnetNode;
    Config config;

    // Web事件处理
    void handleWebSocket(AsyncWebSocketClient* client, 
                        AwsEventType type,
                        void* arg, 
                        uint8_t* data, 
                        size_t len);
    void handleUpdate(AsyncWebServerRequest* request);
    void handleReboot(AsyncWebServerRequest* request);
    void handleFactoryReset(AsyncWebServerRequest* request);

    // JSON处理
    void sendJsonResponse(AsyncWebServerRequest* request, const JsonDocument& doc);
    void parseConfig(const JsonDocument& doc);
    void createConfigJson(JsonDocument& doc);

    // WebSocket通信
    void sendStatus(AsyncWebSocketClient* client);
    void broadcastStatus();
    void handleWsMessage(AsyncWebSocketClient* client, const char* message);

    // 文件系统
    bool initFS();
    bool loadConfigFile();
    bool saveConfigFile();

    // 实用函数
    void notifyConfigChange();
    String ipToString(const uint8_t* ip);
    void stringToIP(const char* str, uint8_t* ip);
};