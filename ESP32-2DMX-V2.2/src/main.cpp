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

// Remove this duplicate setup function

void validatePacket(uint8_t* dmxAData, uint8_t* dmxBData) {
    // Add your packet validation logic here
}

void setup() {
    Serial.begin(115200);
    
    // 加载配置
    loadConfig();

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
        // rdmHandler.begin(); // Already called later with &dmxA
    }
    
    // 初始化WiFi
    WiFi.hostname(gConfig.deviceName);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    // 配置RDM
    if (config.rdmEnabled) {
        rdmHandler.begin(&dmxA); // Initialize RDM with the new begin method
    }
        Serial.print(".");
    
    Serial.println("\nWiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());

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
        
        }
// Function definitions
void loadConfig() {
    // Add your configuration loading logic here
}

void loop() {
    // 主循环保持空闲，主要工作由任务处理
    vTaskDelay(pdMS_TO_TICKS(100));
}