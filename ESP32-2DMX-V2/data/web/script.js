// script.js

// WebSocket连接管理类
class WebSocketManager {
    constructor() {
        this.ws = null;
        this.reconnectAttempts = 0;
        this.maxReconnectAttempts = 5;
        this.reconnectDelay = 3000;
        this.init();
    }

    init() {
        const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
        const wsUrl = `${protocol}//${window.location.host}/ws`;
        this.connect(wsUrl);
    }

    connect(url) {
        this.ws = new WebSocket(url);
        
        this.ws.onopen = () => {
            console.log('WebSocket已连接');
            this.reconnectAttempts = 0;
            this.updateConnectionStatus(true);
        };

        this.ws.onclose = () => {
            console.log('WebSocket已断开');
            this.updateConnectionStatus(false);
            this.attemptReconnect();
        };

        this.ws.onerror = (error) => {
            console.error('WebSocket错误:', error);
            this.updateConnectionStatus(false);
        };

        this.ws.onmessage = (event) => {
            try {
                const data = JSON.parse(event.data);
                this.handleMessage(data);
            } catch (e) {
                console.error('消息解析错误:', e);
            }
        };
    }

    attemptReconnect() {
        if (this.reconnectAttempts < this.maxReconnectAttempts) {
            this.reconnectAttempts++;
            console.log(`尝试重新连接... (${this.reconnectAttempts}/${this.maxReconnectAttempts})`);
            setTimeout(() => this.init(), this.reconnectDelay);
        }
    }

    updateConnectionStatus(connected) {
        const indicator = document.getElementById('connection-status');
        if (indicator) {
            indicator.classList.toggle('connected', connected);
            indicator.classList.toggle('disconnected', !connected);
            indicator.title = connected ? '已连接' : '已断开';
        }
    }

    handleMessage(data) {
        switch (data.type) {
            case 'status':
                this.updateStatus(data);
                break;
            case 'config':
                this.updateConfig(data);
                break;
            case 'ap_status':
                this.updateAPStatus(data);
                break;
            case 'pixel_test':
                this.handlePixelTestResponse(data);
                break;
            default:
                console.log('未知消息类型:', data.type);
        }
    }

    updateStatus(data) {
        if (data.uptime !== undefined) {
            document.getElementById('uptime').textContent = this.formatUptime(data.uptime);
        }
        if (data.rssi !== undefined) {
            document.getElementById('wifi-strength').textContent = `${data.rssi} dBm`;
        }
        if (data.freeHeap !== undefined) {
            document.getElementById('memory-usage').textContent = this.formatBytes(data.freeHeap);
        }
        if (data.ap_enabled !== undefined) {
            this.updateAPStatus(data);
        }
    }

    updateConfig(data) {
        Object.entries(data).forEach(([key, value]) => {
            const element = document.getElementById(this.camelToKebab(key));
            if (element) {
                if (element.type === 'checkbox') {
                    element.checked = value;
                } else if (element.type === 'number') {
                    element.value = parseInt(value);
                } else {
                    element.value = value;
                }
            }
        });
        
        // 触发DHCP状态更新
        const dhcpEnabled = document.getElementById('dhcp-enabled');
        if (dhcpEnabled) {
            this.handleDHCPChange(dhcpEnabled.checked);
        }
    }

    updateAPStatus(data) {
        const elements = {
            status: document.getElementById('ap-status'),
            text: document.getElementById('ap-status-text'),
            ip: document.getElementById('ap-ip'),
            stations: document.getElementById('ap-stations')
        };

        if (elements.status) {
            elements.status.style.display = data.ap_enabled ? 'block' : 'none';
            if (elements.text) elements.text.textContent = data.ap_enabled ? '运行中' : '已停止';
            if (data.ap_enabled) {
                if (elements.ip) elements.ip.textContent = data.ap_ip || '-';
                if (elements.stations) elements.stations.textContent = data.ap_stations || '0';
            }
        }
    }

    handlePixelTestResponse(data) {
        const testStatus = document.getElementById('pixel-test-status');
        if (testStatus) {
            testStatus.textContent = data.success ? '测试运行中...' : '测试失败';
            testStatus.className = data.success ? 'success' : 'error';
        }
    }

    formatUptime(seconds) {
        const days = Math.floor(seconds / 86400);
        const hours = Math.floor((seconds % 86400) / 3600);
        const minutes = Math.floor((seconds % 3600) / 60);
        const secs = seconds % 60;
        return `${days}天 ${hours}小时 ${minutes}分 ${secs}秒`;
    }

    formatBytes(bytes) {
        const sizes = ['B', 'KB', 'MB', 'GB'];
        if (bytes === 0) return '0 B';
        const i = Math.floor(Math.log(bytes) / Math.log(1024));
        return `${(bytes / Math.pow(1024, i)).toFixed(2)} ${sizes[i]}`;
    }

    camelToKebab(str) {
        return str.replace(/([a-z0-9]|(?=[A-Z]))([A-Z])/g, '$1-$2').toLowerCase();
    }

    send(data) {
        if (this.ws && this.ws.readyState === WebSocket.OPEN) {
            this.ws.send(JSON.stringify(data));
        } else {
            console.warn('WebSocket未连接，无法发送消息');
        }
    }
}

// UI管理类
class UIManager {
    constructor(wsManager) {
        this.wsManager = wsManager;
        this.initializeTabs();
        this.initializeFormHandlers();
        this.initializeSpecialControls();
        this.initializeSystemControls();
    }

    initializeTabs() {
        const tabs = document.querySelectorAll('.tab-nav li');
        const contents = document.querySelectorAll('.tab-content');

        tabs.forEach(tab => {
            tab.addEventListener('click', () => {
                tabs.forEach(t => t.classList.remove('active'));
                contents.forEach(c => c.classList.remove('active'));
                tab.classList.add('active');
                const content = document.getElementById(tab.dataset.tab);
                if (content) content.classList.add('active');
            });
        });
    }

    initializeFormHandlers() {
        const forms = {
            network: '网络',
            artnet: 'Art-Net',
            pixel: '像素',
            ap: 'AP模式'
        };

        Object.entries(forms).forEach(([type, label]) => {
            const form = document.getElementById(`${type}-form`);
            if (form) {
                form.addEventListener('submit', async (e) => {
                    e.preventDefault();
                    await this.handleFormSubmit(e, type, label);
                });
            }
        });
    }

    initializeSpecialControls() {
        // DHCP切换
        const dhcpEnabled = document.getElementById('dhcp-enabled');
        if (dhcpEnabled) {
            dhcpEnabled.addEventListener('change', () => this.handleDHCPChange());
        }

        // 像素测试
        const pixelTest = document.getElementById('pixel-test');
        if (pixelTest) {
            pixelTest.addEventListener('change', () => this.handlePixelTest());
        }

        // AP启用状态
        const apEnabled = document.getElementById('ap-enabled');
        if (apEnabled) {
            apEnabled.addEventListener('change', (e) => {
                const apStatus = document.getElementById('ap-status');
                if (apStatus) {
                    apStatus.style.display = e.target.checked ? 'block' : 'none';
                }
            });
        }
    }

    initializeSystemControls() {
        const commands = {
            reboot: '重启',
            'factory-reset': '恢复出厂设置'
        };

        Object.entries(commands).forEach(([cmd, label]) => {
            const btn = document.getElementById(`${cmd}-btn`);
            if (btn) {
                btn.addEventListener('click', () => {
                    const confirmMsg = cmd === 'reboot' 
                        ? '确定要重启设备吗？'
                        : '确定要恢复出厂设置吗？所有配置将被清除！';

                    if (confirm(confirmMsg)) {
                        this.sendSystemCommand(cmd);
                    }
                });
            }
        });
    }

    async handleFormSubmit(e, formType, label) {
        const form = e.target;
        const formData = new FormData(form);
        const data = {};

        formData.forEach((value, key) => {
            if (form[key]) {
                if (form[key].type === 'checkbox') {
                    data[key] = form[key].checked;
                } else if (form[key].type === 'number') {
                    data[key] = parseInt(value);
                } else {
                    data[key] = value;
                }
            }
        });

        try {
            const response = await fetch(`/api/${formType}`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(data)
            });

            if (!response.ok) {
                throw new Error(`HTTP error! status: ${response.status}`);
            }

            this.showToast(`${label}设置已保存`, 'success');

            if (formType === 'ap' && data.enabled) {
                const apStatus = document.getElementById('ap-status');
                if (apStatus) apStatus.style.display = 'block';
            }
        } catch (error) {
            this.showToast(`${label}设置保存失败: ${error.message}`, 'error');
        }
    }

    handleDHCPChange() {
        const dhcpEnabled = document.getElementById('dhcp-enabled');
        const staticSettings = document.getElementById('static-ip-settings');
        if (dhcpEnabled && staticSettings) {
            staticSettings.style.display = dhcpEnabled.checked ? 'none' : 'block';
        }
    }

    handlePixelTest() {
        const testSelect = document.getElementById('pixel-test');
        if (testSelect) {
            this.wsManager.send({
                type: 'pixel-test',
                mode: parseInt(testSelect.value)
            });
        }
    }

    async sendSystemCommand(command) {
        try {
            const response = await fetch(`/api/${command}`, { method: 'POST' });
            if (!response.ok) throw new Error('命令执行失败');
            
            const label = command === 'reboot' ? '重启' : '重置';
            this.showToast(`${label}命令已发送`, 'success');
        } catch (error) {
            this.showToast(`命令执行失败: ${error.message}`, 'error');
        }
    }

    showToast(message, type = 'info') {
        const toast = document.getElementById('toast');
        if (toast) {
            toast.textContent = message;
            toast.className = `toast show ${type}`;
            setTimeout(() => toast.classList.remove('show'), 3000);
        }
    }
}

// 页面加载完成后初始化应用
document.addEventListener('DOMContentLoaded', () => {
    const wsManager = new WebSocketManager();
    const uiManager = new UIManager(wsManager);
});