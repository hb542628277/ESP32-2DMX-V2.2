#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include "config.h"
#include "dmx/ESP32DMX.h"
#include "pixels/PixelDriver.h"
#include "rdm/RDMHandler.h"

// Art-Net包类型
enum ArtNetOpCodes {
    OpPoll = 0x2000,
    OpPollReply = 0x2100,
    OpDmx = 0x5000,
    OpNzs = 0x5100,
    OpSync = 0x5200,
    OpAddress = 0x6000,
    OpInput = 0x7000,
    OpRdm = 0x8400,
    OpIpProg = 0xF800,
    OpIpProgReply = 0xF900
};

class ArtnetNode {
public:
    // 节点配置结构体
    struct Config {
        char shortName[18];
        char longName[64];
        uint8_t net;
        uint8_t subnet;
        uint8_t universe;
        uint8_t dmxMode;
        uint16_t dmxStartAddress;
        uint16_t pixelCount;
        uint8_t pixelType;
        bool mergeMode;  // HTP = true, LTP = false
    };

    // 节点状态结构体
    struct Status {
        uint8_t ip[4];
        uint8_t mac[6];
        bool dhcp;
        uint8_t firmware;
        uint8_t ports;
        uint8_t portTypes[4];
        uint8_t goodInput;
        uint8_t goodOutput;
        uint8_t status1;
        uint8_t status2;
        uint16_t estab;
        uint8_t version;
    };

    ArtnetNode();
    ~ArtnetNode();

    // 初始化和更新
    bool begin();
    void update();

    // 配置方法
    void setConfig(const Config& config);
    const Config& getConfig() const { return config; }
    const Status& getStatus() const { return status; }

    // DMX输出控制
    void setDMXOutput(uint8_t* data, uint16_t length);
    void setPixelOutput(uint8_t* data, uint16_t length);

    // 处理程序注册
    void setDMXCallback(void (*callback)(uint16_t universe, uint8_t* data, uint16_t length));
    void setRDMCallback(void (*callback)(uint8_t* data, uint16_t length));
    void setPixelCallback(void (*callback)(uint8_t* data, uint16_t length));


protected:
    // 将 validatePacket 移到 protected 以供子类使用
    bool validatePacket(uint8_t* data, uint16_t length);


private:
    Config config;
    Status status;
    WiFiUDP udp;
    ESP32DMX* dmx;
    PixelDriver* pixels;

    // 数据缓冲区
    uint8_t artnetBuffer[1024];
    uint8_t dmxBuffer[DMX_UNIVERSE_SIZE];
    uint8_t pixelBuffer[MAX_PIXELS * 3];

    // 回调函数指针
    void (*dmxCallback)(uint16_t universe, uint8_t* data, uint16_t length);
    void (*rdmCallback)(uint8_t* data, uint16_t length);
    void (*pixelCallback)(uint8_t* data, uint16_t length);

    // Art-Net包处理方法
    void handleArtDmx(uint8_t* data, uint16_t length);
    void handleArtPoll();
    void handleArtAddress(uint8_t* data, uint16_t length);
    void handleArtRdm(uint8_t* data, uint16_t length);
    void handleArtSync();
    
    // 帮助方法
    void sendArtPollReply();
    void updateStatus();
    void initializeDefaults();

    // Art-Net常量
    static const uint16_t ARTNET_PORT = 6454; // Default Art-Net port
    static const uint8_t ARTNET_ID[8];
    static const uint16_t ARTNET_DMX_LENGTH = 512;
    static const uint8_t ARTNET_VERSION = 14;
};
