#pragma once

#include <Arduino.h>
#include <driver/uart.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

// 在头文件中添加以下常量定义（如果还没有的话）
#define DMX_MAX_CHANNELS 512


class ESP32DMX {
public:
    ESP32DMX(uart_port_t uartNum = UART_NUM_1);
    ~ESP32DMX();

    static const uint32_t DMX_BAUDRATE = 250000;
    static const uint32_t RDM_BAUDRATE = 250000;
    static const uint32_t DMX_BREAK_US = 176;
    void end();
    void update();

    // 新增 RDM 相关方法
    void sendRDM(uint8_t* data, uint16_t length);
    void sendBreak(uint32_t breakTime = 176); // 默认176微秒
    bool begin(gpio_num_t txPin, gpio_num_t dePin);
    void write(uint8_t* data, uint16_t length);
    void clearBuffer();

    // 获取DMX数据的方法
    uint8_t* getDMXData() { return buffer + 1; }  // +1 跳过起始码

    // DMX控制
    void startOutput();
    void stopOutput();
    bool isOutputting() const { return outputting; }
    
    // DMX数据操作
    void setChannel(uint16_t channel, uint8_t value);
    uint8_t getChannel(uint16_t channel) const;
    void clearChannels();

    // DMX帧控制
    void startFrame();
    void endFrame();
    bool write(const uint8_t* data, size_t length);

    // 状态查询
    bool isEnabled() const { return enabled; }
    uint32_t getFrameCount() const { return frameCount; }
    uint32_t getLastFrameTime() const { return lastFrameTime; }

private:
    // UART配置
    uart_port_t uartNum;
    gpio_num_t txPin;
    gpio_num_t dirPin;
    uart_config_t uart_config;

    uint8_t buffer[DMX_MAX_CHANNELS + 1];  // +1 for start code


    // 状态标志
    bool enabled;
    bool outputting;
    volatile bool transmitting;
    
    // DMX缓冲区
    uint8_t dmxBuffer[DMX_BUFFER_SIZE];
    
    // 统计信息
    uint32_t frameCount;
    uint32_t lastFrameTime;
    uint32_t frameErrors;


    // DMX缓冲区
    static const int DMX_BUFFER_SIZE = 513; // 包含起始码的DMX数据


    // 性能统计
    uint32_t lastFrameCount;
    uint32_t lastFrameRate;
    uint32_t lastUpdate;


    // 内部方法
    void configurePins();

    // DMX时序参数
    static const uint32_t DMX_BREAK_US = 176;  // Break时间(us)
    static const uint32_t DMX_MAB_US = 12;     // Mark After Break时间(us)
    static const uint32_t DMX_MAB_US = 12;     // Mark After Break时间(us)
    void waitForTransmitComplete();
    
    // UART中断处理
    static void IRAM_ATTR uartEventHandler(void *arg);
    void handleUARTEvent(uart_event_t& event);
    
    // 禁用拷贝
    ESP32DMX(const ESP32DMX&) = delete;
    ESP32DMX& operator=(const ESP32DMX&) = delete;
};