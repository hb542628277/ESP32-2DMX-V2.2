#pragma once

#include <Arduino.h>

// WiFi配置
#define WIFI_SSID "YourSSID"      // WiFi名称
#define WIFI_PASS "YourPassword"   // WiFi密码

// 设备配置
#define DEVICE_NAME "ESP32-ArtNode"  // 设备名称
#define FIRMWARE_VERSION "1.0.0"         // 固件版本

// DMX配置
#define DMX_MAX_CHANNELS    512         // DMX最大通道数
#define DMX_UNIVERSE_SIZE   512         // DMX宇宙大小
#define DMX_START_ADDRESS   1           // DMX起始地址
#define DMX_TX_PIN         GPIO_NUM_17  // DMX输出引脚
#define DMX_DE_PIN         GPIO_NUM_16  // DMX方向控制引脚

// 双端口DMX配置
#define DMX_TX_A_PIN       GPIO_NUM_17  // DMX A端口输出引脚
#define DMX_DIR_A_PIN      GPIO_NUM_16  // DMX A端口方向控制引脚
#define DMX_TX_B_PIN       GPIO_NUM_18  // DMX B端口输出引脚
#define DMX_DIR_B_PIN      GPIO_NUM_19  // DMX B端口方向控制引脚

// 像素配置
#define PIXEL_PIN          GPIO_NUM_5   // LED像素数据引脚
#define MAX_PIXELS         1360         // 最大像素数量
#define DEFAULT_PIXELS     170          // 默认像素数量
#define PIXEL_COUNT        170          // 当前像素数量

// Art-Net配置
//#define ARTNET_PORT        6454u        // Art-Net端口（注意添加u后缀）
#define START_UNIVERSE     0            // 起始宇宙
#define START_SUBNET       0            // 起始子网

// 系统配置
#define RESTART_TIMEOUT    1000         // 重启超时时间(ms)
