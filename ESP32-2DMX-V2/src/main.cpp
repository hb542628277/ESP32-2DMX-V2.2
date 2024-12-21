#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include "config.h"
#include <Adafruit_NeoPixel.h>
#include "dmx/ESP32DMX.h"
#include "artnet/ArtnetNode.h"
#include "rdm/RDMHandler.h"
#include "pixels/PixelDriver.h"
#include "web/WebServer.h"
#include "ConfigManager.h"
#include "GlobalConfig.h"

// 全局变量
String apSSID = DEFAULT_AP_SSID;
String apPassword = DEFAULT_AP_PASS;
bool noWiFiMode = false;

// Function declarations
void loadConfig();
void validatePacket(uint8_t* dmxAData, uint8_t* dmxBData);

// 全局对象
ESP32DMX dmxA(1);  // UART1
ESP32DMX dmxB(2);  // UART2
ArtnetNode artnetNode;
RDMHandler rdmHandler;
PixelDriver pixelDriver;
ConfigManager::Config config;
Adafruit_NeoPixel pixels(config.pixelCount, PIXEL_PIN, static_cast<neoPixelType>(config.pixelType));
AsyncWebServer webServer(80);  // Initialize AsyncWebServer with port 80
GlobalConfig gConfig;

// Task handles
TaskHandle_t dmxTask;
TaskHandle_t networkTask;

// DMX处理任务
void dmxTaskFunction(void *parameter) {
    while (true) {
        dmxA.update();
        dmxB.update();
        rdmHandler.update();
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

// 网络处理任务
void networkTaskFunction(void *parameter) {
    while (true) {
        validatePacket(dmxA.getDMXData(), dmxB.getDMXData());
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void validatePacket(uint8_t* dmxAData, uint8_t* dmxBData) {
    // Add your packet validation logic here
}

// WiFi事件处理
void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
    Serial.printf("[WiFi-event] event: %d\n", event);
    
    switch(event) {
        case SYSTEM_EVENT_AP_START:
            Serial.println("AP Started");
            Serial.printf("AP SSID: %s\n", apSSID.c_str());
            Serial.printf("AP IP: %s\n", WiFi.softAPIP().toString().c_str());
            break;
            
        case SYSTEM_EVENT_AP_STACONNECTED:
            Serial.print("Station connected to AP - ");
            Serial.println(WiFi.softAPgetStationNum());
            break;
            
        case SYSTEM_EVENT_AP_STADISCONNECTED:
            Serial.print("Station disconnected from AP - ");
            Serial.println(WiFi.softAPgetStationNum());
            break;
            
        case SYSTEM_EVENT_STA_START:
            Serial.println("Station Mode Started");
            break;
            
        case SYSTEM_EVENT_STA_CONNECTED:
            Serial.println("Connected to AP");
            break;
            
        case SYSTEM_EVENT_STA_DISCONNECTED:
            Serial.println("Disconnected from AP");
            break;
    }
}


// 改进的AP模式启动函数
bool startAPMode() {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(100);
    
    WiFi.mode(WIFI_AP);
    delay(100);
    
    // 配置AP
    WiFi.softAPConfig(
        IPAddress(192, 168, 4, 1),    // AP IP
        IPAddress(192, 168, 4, 1),    // Gateway
        IPAddress(255, 255, 255, 0)   // Subnet mask
    );
    
    // 启动AP
    bool success = WiFi.softAP(
        apSSID.c_str(),
        apPassword.c_str(),
        AP_CHANNEL,
        false,                // 隐藏SSID: false
        MAX_AP_CONNECTIONS   // 最大连接数
    );
    
    if (success) {
        Serial.println("AP Mode Started Successfully");
        Serial.print("AP SSID: ");
        Serial.println(apSSID);
        Serial.print("AP IP: ");
        Serial.println(WiFi.softAPIP().toString());
        return true;
    } else {
        Serial.println("AP Mode Failed to Start");
        return false;
    }
}


void setup() {
    Serial.begin(115200);
    Serial.println("\nStarting...");

    // 加载配置
    loadConfig();

    // 注册WiFi事件处理
    WiFi.onEvent(onWiFiEvent);

    // 初始化文件系统
    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS Mount Failed");
        return;
    }

    // 加载配置
    if (!ConfigManager::load(config)) {
        Serial.println("配置加载失败,使用默认配置");
        ConfigManager::setDefaults(config);
    }

    // 应用配置
    WiFi.setHostname(config.deviceName);

    if (!config.dhcpEnabled) {
        IPAddress ip(config.staticIP);
        IPAddress gateway(config.staticGateway);
        IPAddress subnet(config.staticMask);
        WiFi.config(ip, gateway, subnet);
    }

    // 尝试连接WiFi
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(config.deviceName);
    
    if (!config.dhcpEnabled) {
        IPAddress ip(config.staticIP);
        IPAddress gateway(config.staticGateway);
        IPAddress subnet(config.staticMask);
        WiFi.config(ip, gateway, subnet);
    }

    WiFi.begin(WIFI_SSID, WIFI_PASS);

    // 等待WiFi连接
    unsigned long startAttemptTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
        Serial.print(".");
        delay(1000);
    }

   // 处理WiFi连接结果
    if (WiFi.status() != WL_CONNECTED && !noWiFiMode) {
        Serial.println("\nWiFi连接失败，启动AP模式");
        if (!startAPMode()) {
            Serial.println("AP模式启动失败，请检查硬件！");
            return;
        }
    } else if (noWiFiMode) {
        Serial.println("\n无WiFi工作模式已启用");
    } else {
        Serial.println("\nWiFi connected");
        Serial.printf("IP address: %s\n", WiFi.localIP().toString().c_str());
    }

    // 配置Art-Net节点
    ArtnetNode::Config artnetConfig = {
        .net = config.artnetNet,
        .subnet = config.artnetSubnet,
        .universe = config.artnetUniverse,
        .dmxStartAddress = config.dmxStartAddress
    };
    artnetNode.setConfig(artnetConfig);

    // 配置像素控制
    if (config.pixelEnabled) {
        pixels.begin();
        pixels.setBrightness(config.brightness);
    }

    // 配置RDM
    if (config.rdmEnabled) {
        rdmHandler.begin(&dmxA); // Initialize RDM with the new begin method
    }

    // 初始化各个组件
    dmxA.begin(DMX_TX_A_PIN, DMX_DIR_A_PIN);
    dmxB.begin(DMX_TX_B_PIN, DMX_DIR_B_PIN);
    artnetNode.begin();
    rdmHandler.begin(&dmxA);
    pixelDriver.begin(PIXEL_PIN, PIXEL_COUNT);
    webServer.begin();

    // 创建任务
    xTaskCreatePinnedToCore(
        dmxTaskFunction,
        "DMX Task",
        8192,
        NULL,
        1,
        &dmxTask,
        0
    );

    xTaskCreatePinnedToCore(
        networkTaskFunction,
        "Network Task",
        8192,
        NULL,
        1,
        &networkTask,
        1
    );

    // 处理AP模式配置请求
    webServer.on("/api/ap", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (request->hasParam("apSSID", true) && request->hasParam("apPassword", true)) {
            apSSID = request->getParam("apSSID", true)->value();
            apPassword = request->getParam("apPassword", true)->value();
            if (request->hasParam("noWiFiMode", true)) {
                noWiFiMode = request->getParam("noWiFiMode", true)->value().equals("true");
            }
            WiFi.softAP(apSSID.c_str(), apPassword.c_str());
            request->send(200, "application/json", "{\"status\":\"success\"}");
        } else {
            request->send(400, "application/json", "{\"status\":\"fail\", \"message\":\"Missing parameters\"}");
        }
    });
}

void loadConfig() {
    // Add your configuration loading logic here
}

void loop() {
    static unsigned long lastCheck = 0;
    const unsigned long checkInterval = 10000; // 10秒检查一次
    
    if (millis() - lastCheck >= checkInterval) {
        lastCheck = millis();
        
        // 检查AP状态
        if (WiFi.getMode() & WIFI_AP) {
            Serial.printf("AP Status - Connected Stations: %d\n", 
                        WiFi.softAPgetStationNum());
        }
        
        // 检查WiFi连接状态
        if (WiFi.getMode() & WIFI_STA) {
            Serial.printf("WiFi Status: %d\n", WiFi.status());
        }
    }
    
    // 原有的任务处理
    vTaskDelay(pdMS_TO_TICKS(100));
}