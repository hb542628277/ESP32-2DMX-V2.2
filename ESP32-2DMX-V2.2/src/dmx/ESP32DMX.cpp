#include "ESP32DMX.h"

// Class definition removed to avoid redefinition error

ESP32DMX::ESP32DMX(uart_port_t uartNum)
    : uartNum(uartNum)
    , enabled(false)
    , outputting(false)
    , transmitting(false)
    , frameCount(0)
    , lastFrameTime(0)
    , frameErrors(0) {
    
    // 初始化DMX缓冲区
    memset(dmxBuffer, 0, DMX_BUFFER_SIZE);
    dmxBuffer[0] = 0;  // DMX起始码
    
    // 配置UART参数
    uart_config.baud_rate = DMX_BAUDRATE;
    uart_config.data_bits = UART_DATA_8_BITS;
    uart_config.parity = UART_PARITY_DISABLE;
    uart_config.stop_bits = UART_STOP_BITS_2;
    uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uart_config.rx_flow_ctrl_thresh = 0;
    uart_config.source_clk = UART_SCLK_APB;
}

void ESP32DMX::sendRDM(uint8_t* data, uint16_t length) {
    if (!enabled || !data || length == 0) return;

    // 保存当前状态
    bool wasOutputting = outputting;
    if (wasOutputting) {
        stopOutput();
    }

    // 发送RDM帧
    sendBreak(176);  // RDM break time = 176µs
    sendMAB();       // Mark After Break

    // 设置RDM波特率 (250kbps)
    uart_set_baudrate(uartNum, 250000);  // 使用 uartNum 而不是 uart
    
    // 设置DE引脚为发送模式
    if (dirPin != GPIO_NUM_NC) {  // 使用 dirPin 而不是 dePin
        gpio_set_level(dirPin, 1);  // 使用 gpio_set_level 而不是 digitalWrite
    }

    // 发送RDM数据
    uart_write_bytes(uartNum, (const char*)data, length);
    uart_wait_tx_done(uartNum, portMAX_DELAY);

    // 添加额外延迟以确保数据发送完成
    delayMicroseconds(50);

    // 设置DE引脚为接收模式
    if (dirPin != GPIO_NUM_NC) {
        gpio_set_level(dirPin, 0);
    }

    // 恢复DMX波特率
    uart_set_baudrate(uartNum, DMX_BAUDRATE);

    // 恢复之前的状态
    if (wasOutputting) {
        startOutput();
    }
}

ESP32DMX::~ESP32DMX() {
    end();
}

bool ESP32DMX::begin(gpio_num_t txPin, gpio_num_t dirPin) {
    this->txPin = txPin;
    this->dirPin = dirPin;

    // 配置GPIO
    configurePins();
    
    // 配置UART
    esp_err_t err = uart_param_config(uartNum, &uart_config);
    if (err != ESP_OK) {
        log_e("UART config failed");
        return false;
    }

    // 安装UART驱动
    err = uart_driver_install(uartNum, 0, 0, 0, NULL, 0);
    if (err != ESP_OK) {
        log_e("UART driver install failed");
        return false;
    }

    // 设置UART引脚
    err = uart_set_pin(uartNum, txPin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) {
        log_e("UART pin config failed");
        return false;
    }

    enabled = true;
    return true;
}

void ESP32DMX::configurePins() {
    // 配置DMX方向控制引脚
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << dirPin);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);
    
    // 默认为接收模式
    gpio_set_level(dirPin, 0);
}

void ESP32DMX::startOutput() {
    if (!enabled) return;
    
    gpio_set_level(dirPin, 1);  // 设置为输出模式
    outputting = true;
}

void ESP32DMX::stopOutput() {
    if (!enabled) return;
    
    gpio_set_level(dirPin, 0);  // 设置为接收模式
    outputting = false;
}

void ESP32DMX::setChannel(uint16_t channel, uint8_t value) {
    if (validateChannel(channel)) {
        dmxBuffer[channel] = value;
    }
}

uint8_t ESP32DMX::getChannel(uint16_t channel) const {
    if (validateChannel(channel)) {
        return dmxBuffer[channel];
    }
    return 0;
}

void ESP32DMX::clearChannels() {
    memset(dmxBuffer + 1, 0, DMX_BUFFER_SIZE - 1);
}

void ESP32DMX::startFrame() {
    if (!enabled || !outputting) return;
    
    sendBreak(DMX_BREAK_US);
    sendMAB();
}

void ESP32DMX::endFrame() {
    if (!enabled || !outputting) return;
    
    waitForTransmitComplete();
    frameCount++;
    lastFrameTime = millis();
}

void ESP32DMX::sendBreak(uint32_t breakTime) {
    uart_set_baudrate(uartNum, 1000000 / breakTime);
    uart_write_bytes(uartNum, "\0", 1);
    uart_wait_tx_done(uartNum, portMAX_DELAY);
    uart_set_baudrate(uartNum, DMX_BAUDRATE);
}

void ESP32DMX::sendMAB() {
    delayMicroseconds(DMX_MAB_US);
}
void ESP32DMX::sendMAB() {
    delayMicroseconds(DMX_MAB_US);
}
void ESP32DMX::update() {
    if (!enabled || !outputting) return;

    startFrame();
    write(dmxBuffer, DMX_BUFFER_SIZE);
    endFrame();
}

void ESP32DMX::waitForTransmitComplete() {
    uart_wait_tx_done(uartNum, portMAX_DELAY);
}

bool ESP32DMX::validateChannel(uint16_t channel) const {
    return channel < DMX_BUFFER_SIZE;
}

void ESP32DMX::end() {
    if (enabled) {
        stopOutput();
        uart_driver_delete(uartNum);
        enabled = false;
    }
}