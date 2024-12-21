// WebSocket连接管理
class WebSocketManager {
    constructor() {
        this.ws = null;
        this.reconnectAttempts = 0;
        this.maxReconnectAttempts = 5;
        this.reconnectDelay = 3000;
        this.callbacks = new Map();
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
            console.log('WebSocket connected');
            this.reconnectAttempts = 0;
            this.updateConnectionStatus(true);
        };

        this.ws.onclose = () => {
            console.log('WebSocket disconnected');
            this.updateConnectionStatus(false);
            this.attemptReconnect();
        };

        this.ws.onerror = (error) => {
            console.error('WebSocket error:', error);
        };

        this.ws.onmessage = (event) => {
            try {
                const data = JSON.parse(event.data);
                this.handleMessage(data);
            } catch (e) {
                console.error('Error parsing message:', e);
            }
        };
    }

    attemptReconnect() {
        if (this.reconnectAttempts < this.maxReconnectAttempts) {
            this.reconnectAttempts++;
            setTimeout(() => this.init(), this.reconnectDelay);
        }
    }

    updateConnectionStatus(connected) {
        const indicator = document.getElementById('connection-status');
        if (connected) {
            indicator.classList.add('connected');
            indicator.classList.remove('disconnected');
        } else {
            indicator.classList.add('disconnected');
            indicator.classList.remove('connected');
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
            default:
                console.log('Unknown message type:', data.type);
        }
    }

    updateStatus(data) {
        // 更新状态信息
        document.getElementById('uptime').textContent = this.formatUptime(data.uptime);
        document.getElementById('wifi-strength').textContent = `${data.rssi} dBm`;
        document.getElementById('memory-usage').textContent = this.formatBytes(data.freeHeap);
    }

    updateConfig(data) {
        // 更新配置表单
        Object.entries(data).forEach(([key, value]) => {
            const element = document.getElementById(this.camelToKebab(key));
            if (element) {
                if (element.type === 'checkbox') {
                    element.checked = value;
                } else {
                    element.value = value;
                }
            }
        });
        
        // 触发DHCP切换事件
        this.handleDHCPChange();
    }

    formatUptime(seconds) {
        const days = Math.floor(seconds / 86400);
        const hours = Math.floor((seconds % 86400) / 3600);
        const minutes = Math.floor((seconds % 3600) / 60);
        const remainingSeconds = seconds % 60;

        return `${days}天 ${hours}小时 ${minutes}分 ${remainingSeconds}秒`;
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
        }
    }
}

// UI管理器
class UIManager {
    constructor(wsManager) {
        this.wsManager = wsManager;
        this.initializeTabs();
        this.initializeFormHandlers();
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
                document.getElementById(tab.dataset.tab).classList.add('active');
            });
        });
    }

    initializeFormHandlers() {
        // 网络配置表单
        const networkForm = document.getElementById('network-form');
        networkForm.addEventListener('submit', (e) => this.handleFormSubmit(e, 'network'));

        // Art-Net配置表单
        const artnetForm = document.getElementById('artnet-form');
        artnetForm.addEventListener('submit', (e) => this.handleFormSubmit(e, 'artnet'));

        // 像素配置表单
        const pixelForm = document.getElementById('pixel-form');
        pixelForm.addEventListener('submit', (e) => this.handleFormSubmit(e, 'pixel'));

        // DHCP切换处理
        const dhcpCheckbox = document.getElementById('dhcp-enabled');
        dhcpCheckbox.addEventListener('change', () => this.handleDHCPChange());

        // 像素测试模式
        const pixelTest = document.getElementById('pixel-test');
        pixelTest.addEventListener('change', () => this.handlePixelTest());
    }

    initializeSystemControls() {
        // 重启按钮
        document.getElementById('reboot-btn').addEventListener('click', () => {
            if (confirm('确定要重启设备吗？')) {
                this.sendSystemCommand('reboot');
            }
        });

        // 恢复出厂设置按钮
        document.getElementById('factory-reset-btn').addEventListener('click', () => {
            if (confirm('确定要恢复出厂设置吗？所有配置将被清除！')) {
                this.sendSystemCommand('factory-reset');
            }
        });
    }

    async handleFormSubmit(e, formType) {
        e.preventDefault();
        const form = e.target;
        const formData = new FormData(form);
        const data = {};

        formData.forEach((value, key) => {
            if (form[key].type === 'checkbox') {
                data[key] = form[key].checked;
            } else if (form[key].type === 'number') {
                data[key] = parseInt(value);
            } else {
                data[key] = value;
            }
        });

        try {
            const response = await fetch(`/api/${formType}`, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify(data)
            });

            if (response.ok) {
                this.showToast('设置已保存', 'success');
            } else {
                throw new Error('保存失败');
            }
        } catch (error) {
            this.showToast('保存失败: ' + error.message, 'error');
        }
    }

    handleDHCPChange() {
        const dhcpEnabled = document.getElementById('dhcp-enabled').checked;
        const staticIpSettings = document.getElementById('static-ip-settings');
        staticIpSettings.style.display = dhcpEnabled ? 'none' : 'block';
    }

    handlePixelTest() {
        const mode = document.getElementById('pixel-test').value;
        this.wsManager.send({
            type: 'pixel-test',
            mode: parseInt(mode)
        });
    }

    async sendSystemCommand(command) {
        try {
            const response = await fetch(`/api/${command}`, {
                method: 'POST'
            });

            if (response.ok) {
                this.showToast(`${command === 'reboot' ? '重启' : '重置'}命令已发送`, 'success');
            } else {
                throw new Error('命令发送失败');
            }
        } catch (error) {
            this.showToast('命令发送失败: ' + error.message, 'error');
        }
    }

    showToast(message, type = 'info') {
        const toast = document.getElementById('toast');
        toast.textContent = message;
        toast.className = `toast show ${type}`;
        
        setTimeout(() => {
            toast.classList.remove('show');
        }, 3000);
    }
}

// 初始化应用
document.addEventListener('DOMContentLoaded', () => {
    const wsManager = new WebSocketManager();
    const uiManager = new UIManager(wsManager);
});