#pragma once

#include <Arduino.h>
#include "config.h"
#include "ESP32DMX.h"  // Include the header file for ESP32DMX

// RDM常量定义
#define RDM_SUB_START_CODE 0x01
#define RDM_UID_LENGTH     6
#define RDM_MAX_FRAME_SIZE 257

// RDM命令类
enum RDMCommand {
    RDM_DISCOVERY = 0x10,
    RDM_DISCOVERY_RESPONSE = 0x11,
    RDM_GET_COMMAND = 0x20,
    RDM_GET_COMMAND_RESPONSE = 0x21,
    RDM_SET_COMMAND = 0x30,
    RDM_SET_COMMAND_RESPONSE = 0x31
};

// RDM参数ID
enum RDMParameter {
    PARAM_DEVICE_INFO = 0x0060,
    PARAM_DEVICE_MODEL = 0x0080,
    PARAM_MANUFACTURER = 0x0081,
    PARAM_DEVICE_LABEL = 0x0082,
    PARAM_DMX_START_ADDRESS = 0x00F0,
    PARAM_IDENTIFY_DEVICE = 0x1000,
    PARAM_DEVICE_POWER_CYCLES = 0x0405
};

// RDM UID结构体
struct RDMUID {
    uint8_t id[RDM_UID_LENGTH];
    
    bool operator==(const RDMUID& other) const {
        return memcmp(id, other.id, RDM_UID_LENGTH) == 0;
    }
};

// RDM消息头
struct RDMHeader {
    uint8_t startCode;
    uint8_t subStartCode;
    uint8_t messageLength;
    RDMUID destinationUID;
    RDMUID sourceUID;
    uint8_t transactionNumber;
    uint8_t portID;
    uint8_t messageCount;
    uint8_t subDevice;
    uint8_t commandClass;
    uint16_t parameterID;
    uint8_t parameterDataLength;
};

class RDMHandler {
public:
    RDMHandler();
    ~RDMHandler();

    // 基本功能
    void begin();
    void update();
    void begin(ESP32DMX* dmx);  // 添加新的初始化方法
    
    // RDM命令处理
    void handleCommand(uint8_t* data, uint16_t length);
    void sendResponse(uint8_t* data, uint16_t length);
    
    // 设备标识
    void setDeviceInfo(const char* manufacturer, const char* model, const char* label);
    void setDMXStartAddress(uint16_t address);
    
    // RDM发现
    void enableDiscovery(bool enable);
    bool isDiscoveryEnabled() const { return discoveryEnabled; }

private:
    // 添加 DMX 端口指针
    ESP32DMX* dmxPort;  // 这是之前缺失的成员变量

    // RDM设备信息
    struct DeviceInfo {
        char manufacturer[32];
        char model[32];
        char label[32];
        uint16_t dmxStartAddress;
        uint32_t powerCycles;
        bool identifyMode;
    };

    DeviceInfo deviceInfo;
    RDMUID uid;
    bool discoveryEnabled;
    bool respondToDiscovery;
    uint8_t transactionNumber;

    // 命令处理方法
    void handleDiscovery(RDMHeader* header);
    void handleGetCommand(RDMHeader* header);
    void handleSetCommand(RDMHeader* header);
    
    // 参数处理
    void handleDeviceInfo(RDMHeader* header);
    void handleDMXStartAddress(RDMHeader* header);
    void handleIdentifyDevice(RDMHeader* header);
    
    // 帮助方法
    bool validateHeader(RDMHeader* header);
    void generateUID();
    bool isRDMBroadcast(RDMUID* testUID);
    void prepareResponse(RDMHeader* header, uint8_t cmdClass);
};