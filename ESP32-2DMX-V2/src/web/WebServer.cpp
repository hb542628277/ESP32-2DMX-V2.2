#include "WebServer.h"
#include "ConfigManager.h"

// 构造函数，初始化成员变量
WebServer::WebServer(ArtnetNode* node)
    : artnetNode(node), server(new AsyncWebServer(80)), ws(new AsyncWebSocket("/ws")) {

    // 初始化默认配置
    memset(&config, 0, sizeof(Config));
    strncpy(config.deviceName, DEVICE_NAME, sizeof(config.deviceName));
    config.dhcpEnabled = true;
    config.pixelCount = MAX_PIXELS;
    config.pixelEnabled = true;
}

// 析构函数，释放资源
WebServer::~WebServer() {
    delete server;
    delete ws;
}

// 启动Web服务器
void WebServer::begin() {
    // 初始化文件系统
    if (!initFS()) {
        Serial.println("文件系统初始化失败!");
        return;
    }

    // 使用lambda函数处理WebSocket事件
    ws->onEvent([this](AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len) {
        handleWebSocket(client, type, arg, data, len);
    });
    server->addHandler(ws);

    // 加载配置
    loadConfig();

    // 设置API路由
    server->on("/api/config", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleConfig(request);
    });
    server->on("/api/config", HTTP_POST, [](AsyncWebServerRequest* request) {}, NULL, [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
        handleConfigUpdate(request, data, len);
    });
    server->on("/api/reboot", HTTP_POST, [this](AsyncWebServerRequest* request) {
        handleReboot(request);
    });
    server->on("/api/factory-reset", HTTP_POST, [this](AsyncWebServerRequest* request) {
        handleFactoryReset(request);
    });

    // 静态文件服务
    server->serveStatic("/", LittleFS, "/web/").setDefaultFile("index.html");

    // 404处理
    server->onNotFound([](AsyncWebServerRequest* request) {
        request->send(404, "text/plain", "Not Found");
    });

    server->begin();
}

// 处理WebSocket事件
void WebServer::handleWebSocket(AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len) {
    switch (type) {
        case WS_EVT_CONNECT:
            Serial.printf("WebSocket client #%u connected\n", client->id());
            sendStatus(client);
            break;
        case WS_EVT_DISCONNECT:
            Serial.printf("WebSocket client #%u disconnected\n", client->id());
            break;
        case WS_EVT_DATA:
            if (len) {
                data[len] = 0;
                handleWsMessage(client, (char*)data);
            }
            break;
        case WS_EVT_ERROR:
            Serial.println("WebSocket error");
            break;
    }
}

// 处理获取配置的请求
void WebServer::handleConfig(AsyncWebServerRequest* request) {
    DynamicJsonDocument doc(1024);
    createConfigJson(doc);
    sendJsonResponse(request, doc);
}

// 处理更新配置的请求
void WebServer::handleConfigUpdate(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, data, len);

    if (error) {
        request->send(400, "text/plain", String("Invalid JSON: ") + error.f_str());
        return;
    }

    Config newConfig = config;

    // 更新所有可能的配置项
    if (doc.containsKey("deviceName")) {
        strlcpy(newConfig.deviceName, doc["deviceName"], sizeof(newConfig.deviceName));
    }
    if (doc.containsKey("dhcpEnabled")) {
        newConfig.dhcpEnabled = doc["dhcpEnabled"];
    }
    if (doc.containsKey("staticIP")) {
        JsonArray ip = doc["staticIP"];
        for (size_t i = 0; i < 4; i++) {
            newConfig.staticIP[i] = ip[i];
        }
    }
    if (doc.containsKey("staticMask")) {
        JsonArray mask = doc["staticMask"];
        for (size_t i = 0; i < 4; i++) {
            newConfig.staticMask[i] = mask[i];
        }
    }
    if (doc.containsKey("staticGateway")) {
        JsonArray gateway = doc["staticGateway"];
        for (size_t i = 0; i < 4; i++) {
            newConfig.staticGateway[i] = gateway[i];
        }
    }
    if (doc.containsKey("artnetNet")) {
        newConfig.artnetNet = doc["artnetNet"];
    }
    if (doc.containsKey("artnetSubnet")) {
        newConfig.artnetSubnet = doc["artnetSubnet"];
    }
    if (doc.containsKey("artnetUniverse")) {
        newConfig.artnetUniverse = doc["artnetUniverse"];
    }
    if (doc.containsKey("dmxStartAddress")) {
        newConfig.dmxStartAddress = doc["dmxStartAddress"];
    }
    if (doc.containsKey("pixelCount")) {
        newConfig.pixelCount = doc["pixelCount"];
    }
    if (doc.containsKey("pixelType")) {
        newConfig.pixelType = doc["pixelType"];
    }
    if (doc.containsKey("pixelEnabled")) {
        newConfig.pixelEnabled = doc["pixelEnabled"];
    }

    // 保存并应用新配置
    if (ConfigManager::save((const ConfigManager::Config&)newConfig)) {
        config = newConfig;  // 更新当前配置
        applyConfig();       // 应用新配置

        // 发送成功响应，并包含更新后的配置
        DynamicJsonDocument response(1024);
        response["status"] = "success";
        response["message"] = "Configuration updated successfully";

        String responseStr;
        serializeJson(response, responseStr);
        request->send(200, "application/json", responseStr);

        // 通知所有连接的WebSocket客户端配置已更新
        notifyConfigChange();
    } else {
        request->send(500, "text/plain", "Failed to save configuration");
    }
}

// 创建配置的JSON表示
void WebServer::createConfigJson(JsonDocument& doc) {
    doc["deviceName"] = config.deviceName;
    doc["dhcpEnabled"] = config.dhcpEnabled;
    doc["staticIP"] = ipToString(config.staticIP);
    doc["staticMask"] = ipToString(config.staticMask);
    doc["staticGateway"] = ipToString(config.staticGateway);
    doc["artnetNet"] = config.artnetNet;
    doc["artnetSubnet"] = config.artnetSubnet;
    doc["artnetUniverse"] = config.artnetUniverse;
    doc["dmxStartAddress"] = config.dmxStartAddress;
    doc["pixelCount"] = config.pixelCount;
    doc["pixelType"] = config.pixelType;
    doc["pixelEnabled"] = config.pixelEnabled;
}

// 解析配置的JSON表示
void WebServer::parseConfig(const JsonDocument& doc) {
    if (doc.containsKey("deviceName")) {
        strlcpy(config.deviceName, doc["deviceName"], sizeof(config.deviceName));
    }
    if (doc.containsKey("dhcpEnabled")) {
        config.dhcpEnabled = doc["dhcpEnabled"];
    }
    if (doc.containsKey("staticIP")) {
        stringToIP(doc["staticIP"], config.staticIP);
    }
    if (doc.containsKey("staticMask")) {
        stringToIP(doc["staticMask"], config.staticMask);
    }
    if (doc.containsKey("staticGateway")) {
        stringToIP(doc["staticGateway"], config.staticGateway);
    }
    if (doc.containsKey("artnetNet")) {
        config.artnetNet = doc["artnetNet"];
    }
    if (doc.containsKey("artnetSubnet")) {
        config.artnetSubnet = doc["artnetSubnet"];
    }
    if (doc.containsKey("artnetUniverse")) {
        config.artnetUniverse = doc["artnetUniverse"];
    }
    if (doc.containsKey("dmxStartAddress")) {
        config.dmxStartAddress = doc["dmxStartAddress"];
    }
    if (doc.containsKey("pixelCount")) {
        config.pixelCount = doc["pixelCount"];
    }
    if (doc.containsKey("pixelType")) {
        config.pixelType = doc["pixelType"];
    }
    if (doc.containsKey("pixelEnabled")) {
        config.pixelEnabled = doc["pixelEnabled"];
    }
}

// 发送JSON响应
void WebServer::sendJsonResponse(AsyncWebServerRequest* request, const JsonDocument& doc) {
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

// 发送状态信息给WebSocket客户端
void WebServer::sendStatus(AsyncWebSocketClient* client) {
    DynamicJsonDocument doc(1024);
    doc["type"] = "status";
    doc["uptime"] = millis() / 1000;
    doc["freeHeap"] = ESP.getFreeHeap();
    doc["rssi"] = WiFi.RSSI();

    String status;
    serializeJson(doc, status);
    client->text(status);
}

// 初始化文件系统
bool WebServer::initFS() {
    if (!LittleFS.begin()) {
        Serial.println("LittleFS挂载失败");
        return false;
    }
    return true;
}

// 加载配置
void WebServer::loadConfig() {
    if (!loadConfigFile()) {
        Serial.println("使用默认配置");
        saveConfig();
    }
}

// 从文件加载配置
bool WebServer::loadConfigFile() {
    File file = LittleFS.open("/config.json", "r");
    if (!file) {
        return false;
    }

    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        return false;
    }

    parseConfig(doc);
    return true;
}

// 保存配置到文件
bool WebServer::saveConfigFile() {
    File file = LittleFS.open("/config.json", "w");
    if (!file) {
        return false;
    }

    DynamicJsonDocument doc(1024);
    createConfigJson(doc);
    serializeJson(doc, file);
    file.close();
    return true;
}

// 应用当前配置
void WebServer::applyConfig() {
    if (artnetNode) {
        ArtnetNode::Config artnetConfig;
        artnetConfig.net = config.artnetNet;
        artnetConfig.subnet = config.artnetSubnet;
        artnetConfig.universe = config.artnetUniverse;
        artnetConfig.dmxStartAddress = config.dmxStartAddress;
        artnetNode->setConfig(artnetConfig);
    }

    // 应用网络配置
    if (!config.dhcpEnabled) {
        IPAddress ip(config.staticIP[0], config.staticIP[1], config.staticIP[2], config.staticIP[3]);
        IPAddress gateway(config.staticGateway[0], config.staticGateway[1], config.staticGateway[2], config.staticGateway[3]);
        IPAddress subnet(config.staticMask[0], config.staticMask[1], config.staticMask[2], config.staticMask[3]);
        if (WiFi.status() == WL_CONNECTED) {
            WiFi.config(ip, gateway, subnet);
        } else {
            Serial.println("Wi-Fi not connected!");
        }
    }
}

// 通知所有WebSocket客户端配置已更改
void WebServer::notifyConfigChange() {
    DynamicJsonDocument doc(1024);
    doc["type"] = "config";
    createConfigJson(doc);

    String message;
    serializeJson(doc, message);
    ws->textAll(message);
}

// 将IP地址转换为字符串
String WebServer::ipToString(const uint8_t* ip) {
    return String(ip[0]) + "." + String(ip[1]) + "." + String(ip[2]) + "." + String(ip[3]);
}

// 将字符串转换为IP地址
void WebServer::stringToIP(const char* str, uint8_t* ip) {
    sscanf(str, "%hhu.%hhu.%hhu.%hhu", &ip[0], &ip[1], &ip[2], &ip[3]);
}

// 更新状态
void WebServer::update() {
    static uint32_t lastUpdate = 0;
    uint32_t now = millis();

    // 每秒更新一次状态
    if (now - lastUpdate >= 1000) {
        lastUpdate = now;
        ws->cleanupClients();
        broadcastStatus();
    }
}

// 广播状态信息给所有WebSocket客户端
void WebServer::broadcastStatus() {
    DynamicJsonDocument doc(1024);
    doc["uptime"] = millis() / 1000;
    doc["freeHeap"] = ESP.getFreeHeap();
    doc["rssi"] = WiFi.RSSI();

    String status;
    serializeJson(doc, status);
    ws->textAll(status);
}