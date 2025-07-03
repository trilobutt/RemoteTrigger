#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <commctrl.h>
#include <iostream>
#include <thread>
#include <atomic>
#include <string>
#include <cstdint>
#include <vector>
#include <algorithm>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "comctl32.lib")

// Control IDs
#define ID_WINDOW_COMBO     1001
#define ID_PORT_EDIT        1002
#define ID_KEY_EDIT         1003
#define ID_VALUE_EDIT       1004
#define ID_ADDRESS_EDIT     1005
#define ID_START_BUTTON     1006
#define ID_STOP_BUTTON      1007
#define ID_STATUS_EDIT      1008
#define ID_IP_EDIT          1009
#define ID_WINDOW_SELECT    1010
#define ID_KEY_CAPTURE      1011
#define ID_KEY_DISPLAY      1012

struct Config {
    std::string windowTitle = "YourTargetWindow";
    std::string ipAddress = "127.0.0.1";
    int port = 55525;
    int triggerKey = VK_SPACE;
    bool useCtrl = false;
    bool useShift = false;
    bool useAlt = false;
    int targetValue = 9;
    std::string oscAddress = "/flair/runstate";
    std::string keyString = "SPACE";
};

class OSCTrigger {
private:
    SOCKET udpSocket;
    sockaddr_in serverAddr;
    Config config;
    std::atomic<bool> running;
    HWND statusEdit;
    
public:
    OSCTrigger(HWND status) : udpSocket(INVALID_SOCKET), running(false), statusEdit(status) {}
    
    bool Start(const Config& cfg) {
        config = cfg;
        
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            LogStatus("WSAStartup failed");
            return false;
        }
        
        udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (udpSocket == INVALID_SOCKET) {
            LogStatus("Socket creation failed");
            WSACleanup();
            return false;
        }
        
        serverAddr.sin_family = AF_INET;
        inet_pton(AF_INET, config.ipAddress.c_str(), &serverAddr.sin_addr);
        serverAddr.sin_port = htons(config.port);
        
        if (bind(udpSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            LogStatus("Bind failed on port " + std::to_string(config.port));
            closesocket(udpSocket);
            WSACleanup();
            return false;
        }
        
        running = true;
        LogStatus("Listening on " + config.ipAddress + ":" + std::to_string(config.port));
        return true;
    }
    
    void Stop() {
        running = false;
        if (udpSocket != INVALID_SOCKET) {
            closesocket(udpSocket);
            udpSocket = INVALID_SOCKET;
        }
        WSACleanup();
        LogStatus("Stopped");
    }
    
    void Listen() {
        char buffer[4096];
        sockaddr_in clientAddr;
        int clientAddrSize = sizeof(clientAddr);
        
        while (running) {
            fd_set readSet;
            FD_ZERO(&readSet);
            FD_SET(udpSocket, &readSet);
            
            timeval timeout = {0, 100000}; // 100ms timeout
            int result = select(0, &readSet, nullptr, nullptr, &timeout);
            
            if (result > 0 && FD_ISSET(udpSocket, &readSet)) {
                int bytesReceived = recvfrom(udpSocket, buffer, sizeof(buffer), 0, 
                                           (SOCKADDR*)&clientAddr, &clientAddrSize);
                
                if (bytesReceived > 0) {
                    ProcessOSCData(buffer, bytesReceived);
                }
            }
        }
    }
    
    void ProcessOSCData(const char* data, int length) {
        if (length >= 8 && strncmp(data, "#bundle", 7) == 0) {
            ProcessBundle(data, length);
        } else {
            ProcessMessage(data, length);
        }
    }
    
    void ProcessBundle(const char* data, int length) {
        int pos = 16;
        while (pos < length) {
            if (pos + 4 > length) break;
            
            uint32_t elementSize = 
                (static_cast<uint8_t>(data[pos]) << 24) |
                (static_cast<uint8_t>(data[pos + 1]) << 16) |
                (static_cast<uint8_t>(data[pos + 2]) << 8) |
                static_cast<uint8_t>(data[pos + 3]);
            
            pos += 4;
            if (pos + elementSize > length) break;
            
            ProcessMessage(data + pos, elementSize);
            pos += elementSize;
        }
    }
    
    void ProcessMessage(const char* data, int length) {
        if (length > 20 && strstr(data, config.oscAddress.c_str()) != nullptr) {
            if (CheckTargetValue(data, length)) {
                TriggerButton();
            }
        }
    }
    
    bool CheckTargetValue(const char* data, int length) {
        const char* addr = strstr(data, config.oscAddress.c_str());
        if (!addr) return false;
        
        int addrLen = strlen(addr) + 1;
        int padded = (addrLen + 3) & ~3;
        int pos = (addr - data) + padded;
        
        if (pos + 4 > length || data[pos] != ',') return false;
        if (data[pos + 1] != 'i' && data[pos + 1] != 'f') return false;
        
        pos += 4;
        if (pos + 4 > length) return false;
        
        uint32_t rawValue = 
            (static_cast<uint8_t>(data[pos]) << 24) |
            (static_cast<uint8_t>(data[pos + 1]) << 16) |
            (static_cast<uint8_t>(data[pos + 2]) << 8) |
            static_cast<uint8_t>(data[pos + 3]);
        
        if (data[pos - 3] == 'i') {
            int32_t value = static_cast<int32_t>(rawValue);
            return value == config.targetValue;
        } else {
            float value = *reinterpret_cast<float*>(&rawValue);
            return (value > config.targetValue - 0.01f && value < config.targetValue + 0.01f);
        }
    }
    
    void TriggerButton() {
        LogStatus("TRIGGER: " + config.oscAddress + " = " + std::to_string(config.targetValue) + " (Key: " + config.keyString + ")");
        
        HWND window = FindWindowA(nullptr, config.windowTitle.c_str());
        if (window) {
            SetForegroundWindow(window);
        }
        
        // Press modifier keys
        if (config.useCtrl) keybd_event(VK_CONTROL, 0, 0, 0);
        if (config.useShift) keybd_event(VK_SHIFT, 0, 0, 0);
        if (config.useAlt) keybd_event(VK_MENU, 0, 0, 0);
        
        // Press main key
        keybd_event(config.triggerKey, 0, 0, 0);
        keybd_event(config.triggerKey, 0, KEYEVENTF_KEYUP, 0);
        
        // Release modifier keys
        if (config.useAlt) keybd_event(VK_MENU, 0, KEYEVENTF_KEYUP, 0);
        if (config.useShift) keybd_event(VK_SHIFT, 0, KEYEVENTF_KEYUP, 0);
        if (config.useCtrl) keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);
    }
    
    void LogStatus(const std::string& message) {
        if (statusEdit) {
            std::string timeStr = "[" + std::to_string(GetTickCount() / 1000) + "s] ";
            std::string fullMessage = timeStr + message + "\r\n";
            
            int len = GetWindowTextLength(statusEdit);
            SendMessage(statusEdit, EM_SETSEL, len, len);
            SendMessage(statusEdit, EM_REPLACESEL, FALSE, (LPARAM)fullMessage.c_str());
        }
    }
    
    bool IsRunning() const { return running; }
};

int StringToVK(const std::string& key) {
    // Basic keys
    if (key == "SPACE") return VK_SPACE;
    if (key == "ENTER") return VK_RETURN;
    if (key == "TAB") return VK_TAB;
    if (key == "ESC" || key == "ESCAPE") return VK_ESCAPE;
    if (key == "BACKSPACE") return VK_BACK;
    if (key == "DELETE") return VK_DELETE;
    if (key == "INSERT") return VK_INSERT;
    if (key == "HOME") return VK_HOME;
    if (key == "END") return VK_END;
    if (key == "PAGEUP") return VK_PRIOR;
    if (key == "PAGEDOWN") return VK_NEXT;
    
    // Function keys
    if (key == "F1") return VK_F1;
    if (key == "F2") return VK_F2;
    if (key == "F3") return VK_F3;
    if (key == "F4") return VK_F4;
    if (key == "F5") return VK_F5;
    if (key == "F6") return VK_F6;
    if (key == "F7") return VK_F7;
    if (key == "F8") return VK_F8;
    if (key == "F9") return VK_F9;
    if (key == "F10") return VK_F10;
    if (key == "F11") return VK_F11;
    if (key == "F12") return VK_F12;
    
    // Arrow keys
    if (key == "LEFT") return VK_LEFT;
    if (key == "RIGHT") return VK_RIGHT;
    if (key == "UP") return VK_UP;
    if (key == "DOWN") return VK_DOWN;
    
    // Number pad
    if (key == "NUM0") return VK_NUMPAD0;
    if (key == "NUM1") return VK_NUMPAD1;
    if (key == "NUM2") return VK_NUMPAD2;
    if (key == "NUM3") return VK_NUMPAD3;
    if (key == "NUM4") return VK_NUMPAD4;
    if (key == "NUM5") return VK_NUMPAD5;
    if (key == "NUM6") return VK_NUMPAD6;
    if (key == "NUM7") return VK_NUMPAD7;
    if (key == "NUM8") return VK_NUMPAD8;
    if (key == "NUM9") return VK_NUMPAD9;
    
    // Single character keys
    if (key.length() == 1) {
        char c = toupper(key[0]);
        if (c >= 'A' && c <= 'Z') return c;
        if (c >= '0' && c <= '9') return c;
    }
    
    return VK_SPACE; // Default fallback
}

// Global variables
OSCTrigger* g_trigger = nullptr;
std::thread* g_listenerThread = nullptr;
bool g_capturingKey = false;
HWND g_keyEditHwnd = nullptr;
WNDPROC g_originalKeyEditProc = nullptr;
DWORD g_lastCaptureTime = 0;

bool IsValidKeyString(const std::string& keyString) {
    if (keyString.empty()) return false;
    
    std::string upper = keyString;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
    
    // Remove modifiers to check base key
    if (upper.find("CTRL+") != std::string::npos) {
        upper = upper.substr(upper.find("CTRL+") + 5);
    }
    if (upper.find("SHIFT+") != std::string::npos) {
        upper = upper.substr(upper.find("SHIFT+") + 6);
    }
    if (upper.find("ALT+") != std::string::npos) {
        upper = upper.substr(upper.find("ALT+") + 4);
    }
    
    // Check if remaining key is valid
    return StringToVK(upper) != VK_SPACE || upper == "SPACE";
}

void ParseKeyString(const std::string& keyString, Config& config) {
    std::string upper = keyString;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
    
    config.useCtrl = false;
    config.useShift = false;
    config.useAlt = false;
    
    // Check for modifier keys
    if (upper.find("CTRL+") != std::string::npos) {
        config.useCtrl = true;
        upper = upper.substr(upper.find("CTRL+") + 5);
    }
    if (upper.find("SHIFT+") != std::string::npos) {
        config.useShift = true;
        upper = upper.substr(upper.find("SHIFT+") + 6);
    }
    if (upper.find("ALT+") != std::string::npos) {
        config.useAlt = true;
        upper = upper.substr(upper.find("ALT+") + 4);
    }
    
    config.triggerKey = StringToVK(upper);
}

std::string GetWindowText(HWND hwnd) {
    int len = GetWindowTextLength(hwnd);
    std::string result(len, 0);
    GetWindowTextA(hwnd, &result[0], len + 1);
    return result;
}

struct WindowInfo {
    std::string title;
    HWND hwnd;
};

std::vector<WindowInfo> g_windows;

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    if (IsWindowVisible(hwnd) && GetWindowTextLength(hwnd) > 0) {
        std::string title = GetWindowText(hwnd);
        if (!title.empty() && title != "Program Manager") {
            g_windows.push_back({title, hwnd});
        }
    }
    return TRUE;
}

void RefreshWindowList(HWND parent) {
    HWND combo = GetDlgItem(parent, ID_WINDOW_COMBO);
    
    // Clear existing items
    SendMessage(combo, CB_RESETCONTENT, 0, 0);
    
    // Get current windows
    g_windows.clear();
    EnumWindows(EnumWindowsProc, 0);
    
    if (g_windows.empty()) {
        SendMessage(combo, CB_ADDSTRING, 0, (LPARAM)"No windows found");
        return;
    }
    
    // Add windows to combo box
    for (const auto& window : g_windows) {
        SendMessage(combo, CB_ADDSTRING, 0, (LPARAM)window.title.c_str());
    }
    
    // Select first item if available
    if (!g_windows.empty()) {
        SendMessage(combo, CB_SETCURSEL, 0, 0);
    }
}

void StartListener(HWND hwnd) {
    if (g_trigger && g_trigger->IsRunning()) return;
    
    Config config;
    
    // Get selected window from combo box
    int sel = SendMessage(GetDlgItem(hwnd, ID_WINDOW_COMBO), CB_GETCURSEL, 0, 0);
    if (sel != CB_ERR && sel < g_windows.size()) {
        config.windowTitle = g_windows[sel].title;
    } else {
        config.windowTitle = GetWindowText(GetDlgItem(hwnd, ID_WINDOW_COMBO));
    }
    
    config.ipAddress = GetWindowText(GetDlgItem(hwnd, ID_IP_EDIT));
    config.port = GetDlgItemInt(hwnd, ID_PORT_EDIT, nullptr, FALSE);
    config.targetValue = GetDlgItemInt(hwnd, ID_VALUE_EDIT, nullptr, FALSE);
    config.oscAddress = GetWindowText(GetDlgItem(hwnd, ID_ADDRESS_EDIT));
    
    char keyText[50];
    GetDlgItemTextA(hwnd, ID_KEY_EDIT, keyText, sizeof(keyText));
    config.keyString = keyText;
    
    // Validate key combination
    if (!IsValidKeyString(keyText)) {
        MessageBoxA(hwnd, "Invalid key combination. Please use format like: SPACE, ENTER, F1, A, CTRL+A, SHIFT+F1, etc.", "Invalid Key", MB_OK | MB_ICONERROR);
        return;
    }
    
    ParseKeyString(keyText, config);
    
    if (!g_trigger) {
        g_trigger = new OSCTrigger(GetDlgItem(hwnd, ID_STATUS_EDIT));
    }
    
    if (g_trigger->Start(config)) {
        g_listenerThread = new std::thread([]{
            g_trigger->Listen();
        });
        
        EnableWindow(GetDlgItem(hwnd, ID_START_BUTTON), FALSE);
        EnableWindow(GetDlgItem(hwnd, ID_STOP_BUTTON), TRUE);
    }
}

void StopListener(HWND hwnd) {
    if (g_trigger) {
        g_trigger->Stop();
        
        if (g_listenerThread) {
            g_listenerThread->join();
            delete g_listenerThread;
            g_listenerThread = nullptr;
        }
        
        EnableWindow(GetDlgItem(hwnd, ID_START_BUTTON), TRUE);
        EnableWindow(GetDlgItem(hwnd, ID_STOP_BUTTON), FALSE);
    }
}

LRESULT CALLBACK KeyCaptureProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    // Block character input for 100ms after capturing a key to prevent duplicates
    DWORD currentTime = GetTickCount();
    bool recentlyCapture = (currentTime - g_lastCaptureTime) < 100;
    
    // If we're capturing a key, handle all messages here and don't pass them to the original proc
    if (g_capturingKey) {
        switch (uMsg) {
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            {
                std::string keyStr = "";
                
                // Get main key first
                int vk = wParam;
                std::string mainKey = "";
                
                if (vk == VK_SPACE) mainKey = "SPACE";
                else if (vk == VK_RETURN) mainKey = "ENTER";
                else if (vk == VK_TAB) mainKey = "TAB";
                else if (vk == VK_ESCAPE) mainKey = "ESC";
                else if (vk >= VK_F1 && vk <= VK_F12) mainKey = "F" + std::to_string(vk - VK_F1 + 1);
                else if (vk >= 'A' && vk <= 'Z') mainKey = (char)vk;
                else if (vk >= '0' && vk <= '9') mainKey = (char)vk;
                else if (vk == VK_LEFT) mainKey = "LEFT";
                else if (vk == VK_RIGHT) mainKey = "RIGHT";
                else if (vk == VK_UP) mainKey = "UP";
                else if (vk == VK_DOWN) mainKey = "DOWN";
                else return 0; // Unknown key, ignore
                
                // Check for modifier keys and build the string
                if (GetKeyState(VK_CONTROL) & 0x8000) keyStr += "CTRL+";
                if (GetKeyState(VK_SHIFT) & 0x8000) keyStr += "SHIFT+";
                if (GetKeyState(VK_MENU) & 0x8000) keyStr += "ALT+";
                
                keyStr += mainKey;
                
                // Complete the capture
                g_capturingKey = false;
                g_lastCaptureTime = currentTime;  // Record capture time
                SetWindowTextA(GetDlgItem(GetParent(hwnd), ID_KEY_CAPTURE), "Click to Capture Key");
                
                // Clear the edit control first, then set the new text
                SendMessageA(hwnd, WM_SETTEXT, 0, (LPARAM)"");
                SendMessageA(hwnd, WM_SETTEXT, 0, (LPARAM)keyStr.c_str());
                
                return 0;
            }
            break;
            
        case WM_CHAR:
        case WM_SYSCHAR:
        case WM_KEYUP:
        case WM_SYSKEYUP:
            // Block all other keyboard messages during capture
            return 0;
            
        default:
            // Allow other messages to pass through
            break;
        }
    }
    
    // Block character messages for a short time after capture to prevent duplicates
    if (recentlyCapture && (uMsg == WM_CHAR || uMsg == WM_SYSCHAR)) {
        return 0;
    }
    
    if (g_originalKeyEditProc) {
        return CallWindowProcA(g_originalKeyEditProc, hwnd, uMsg, wParam, lParam);
    }
    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

void ResizeControls(HWND hwnd) {
    RECT rect;
    GetClientRect(hwnd, &rect);
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    
    // Calculate responsive dimensions with margins
    int margin = 10;
    int labelWidth = 100;
    int controlHeight = 20;
    int buttonHeight = 30;
    int spacing = 5;
    
    // Calculate available width for controls
    int availableWidth = width - (margin * 2) - labelWidth - spacing;
    int controlWidth = max(150, availableWidth);
    int rightColX = labelWidth + spacing + margin;
    
    // Status area height (bottom 1/3 of window)
    int statusHeight = max(100, height / 3);
    int statusY = height - statusHeight - margin;
    
    // Current Y position for controls
    int currentY = margin;
    
    // Window title selection
    SetWindowPos(GetDlgItem(hwnd, ID_WINDOW_COMBO), nullptr,
                rightColX, currentY, controlWidth - 70, controlHeight,
                SWP_NOZORDER);
    SetWindowPos(GetDlgItem(hwnd, ID_WINDOW_SELECT), nullptr,
                rightColX + controlWidth - 65, currentY, 60, controlHeight,
                SWP_NOZORDER);
    currentY += controlHeight + spacing;
    
    // IP Address
    SetWindowPos(GetDlgItem(hwnd, ID_IP_EDIT), nullptr,
                rightColX, currentY, min(150, controlWidth / 2), controlHeight,
                SWP_NOZORDER);
    currentY += controlHeight + spacing;
    
    // Port
    SetWindowPos(GetDlgItem(hwnd, ID_PORT_EDIT), nullptr,
                rightColX, currentY, min(100, controlWidth / 2), controlHeight,
                SWP_NOZORDER);
    currentY += controlHeight + spacing;
    
    // Trigger key
    int keyEditWidth = min(100, controlWidth / 2);
    int captureButtonWidth = min(140, controlWidth - keyEditWidth - spacing);
    SetWindowPos(GetDlgItem(hwnd, ID_KEY_EDIT), nullptr,
                rightColX, currentY, keyEditWidth, controlHeight,
                SWP_NOZORDER);
    SetWindowPos(GetDlgItem(hwnd, ID_KEY_CAPTURE), nullptr,
                rightColX + keyEditWidth + spacing, currentY, captureButtonWidth, controlHeight,
                SWP_NOZORDER);
    currentY += controlHeight + spacing;
    
    // Key display
    SetWindowPos(GetDlgItem(hwnd, ID_KEY_DISPLAY), nullptr,
                margin, currentY, width - (margin * 2), controlHeight,
                SWP_NOZORDER);
    currentY += controlHeight + spacing;
    
    // Target value
    SetWindowPos(GetDlgItem(hwnd, ID_VALUE_EDIT), nullptr,
                rightColX, currentY, min(100, controlWidth / 2), controlHeight,
                SWP_NOZORDER);
    currentY += controlHeight + spacing;
    
    // OSC address
    SetWindowPos(GetDlgItem(hwnd, ID_ADDRESS_EDIT), nullptr,
                rightColX, currentY, controlWidth, controlHeight,
                SWP_NOZORDER);
    currentY += controlHeight + spacing * 2;
    
    // Buttons
    SetWindowPos(GetDlgItem(hwnd, ID_START_BUTTON), nullptr,
                margin, currentY, 80, buttonHeight,
                SWP_NOZORDER);
    SetWindowPos(GetDlgItem(hwnd, ID_STOP_BUTTON), nullptr,
                margin + 80 + spacing, currentY, 80, buttonHeight,
                SWP_NOZORDER);
    
    // Status area
    SetWindowPos(GetDlgItem(hwnd, ID_STATUS_EDIT), nullptr,
                margin, statusY, width - (margin * 2), statusHeight - margin,
                SWP_NOZORDER);
    
    // Reposition static labels
    currentY = margin;
    
    // Window title label
    SetWindowPos(FindWindowExA(hwnd, nullptr, "STATIC", "Window Title:"), nullptr,
                margin, currentY, labelWidth, controlHeight,
                SWP_NOZORDER);
    currentY += controlHeight + spacing;
    
    // IP Address label
    SetWindowPos(FindWindowExA(hwnd, nullptr, "STATIC", "IP Address:"), nullptr,
                margin, currentY, labelWidth, controlHeight,
                SWP_NOZORDER);
    currentY += controlHeight + spacing;
    
    // Port label
    SetWindowPos(FindWindowExA(hwnd, nullptr, "STATIC", "Port:"), nullptr,
                margin, currentY, labelWidth, controlHeight,
                SWP_NOZORDER);
    currentY += controlHeight + spacing;
    
    // Trigger key label
    SetWindowPos(FindWindowExA(hwnd, nullptr, "STATIC", "Trigger Key:"), nullptr,
                margin, currentY, labelWidth, controlHeight,
                SWP_NOZORDER);
    currentY += controlHeight + spacing * 2;
    
    // Target value label
    SetWindowPos(FindWindowExA(hwnd, nullptr, "STATIC", "Target Value:"), nullptr,
                margin, currentY, labelWidth, controlHeight,
                SWP_NOZORDER);
    currentY += controlHeight + spacing;
    
    // OSC address label
    SetWindowPos(FindWindowExA(hwnd, nullptr, "STATIC", "OSC Address:"), nullptr,
                margin, currentY, labelWidth, controlHeight,
                SWP_NOZORDER);
    
    // Status label
    SetWindowPos(FindWindowExA(hwnd, nullptr, "STATIC", "Status:"), nullptr,
                margin, statusY - controlHeight - spacing, labelWidth, controlHeight,
                SWP_NOZORDER);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE:
        {
            // Window title selection
            CreateWindowA("STATIC", "Window Title:", WS_VISIBLE | WS_CHILD,
                        10, 10, 100, 20, hwnd, nullptr, nullptr, nullptr);
            CreateWindowA("COMBOBOX", "", WS_VISIBLE | WS_CHILD | WS_BORDER | CBS_DROPDOWNLIST,
                        120, 10, 200, 200, hwnd, (HMENU)ID_WINDOW_COMBO, nullptr, nullptr);
            CreateWindowA("BUTTON", "Refresh", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                        330, 10, 60, 20, hwnd, (HMENU)ID_WINDOW_SELECT, nullptr, nullptr);
            
            // IP Address
            CreateWindowA("STATIC", "IP Address:", WS_VISIBLE | WS_CHILD,
                        10, 40, 100, 20, hwnd, nullptr, nullptr, nullptr);
            CreateWindowA("EDIT", "127.0.0.1", WS_VISIBLE | WS_CHILD | WS_BORDER,
                        120, 40, 100, 20, hwnd, (HMENU)ID_IP_EDIT, nullptr, nullptr);
            
            // Port
            CreateWindowA("STATIC", "Port:", WS_VISIBLE | WS_CHILD,
                        10, 70, 100, 20, hwnd, nullptr, nullptr, nullptr);
            CreateWindowA("EDIT", "55525", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER,
                        120, 70, 100, 20, hwnd, (HMENU)ID_PORT_EDIT, nullptr, nullptr);
            
            // Trigger key
            CreateWindowA("STATIC", "Trigger Key:", WS_VISIBLE | WS_CHILD,
                        10, 100, 100, 20, hwnd, nullptr, nullptr, nullptr);
            HWND keyEdit = CreateWindowA("EDIT", "SPACE", WS_VISIBLE | WS_CHILD | WS_BORDER,
                        120, 100, 100, 20, hwnd, (HMENU)ID_KEY_EDIT, nullptr, nullptr);
            CreateWindowA("BUTTON", "Click to Capture Key", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                        230, 100, 160, 20, hwnd, (HMENU)ID_KEY_CAPTURE, nullptr, nullptr);
            
            // Key display confirmation
            CreateWindowA("STATIC", "Current: SPACE", WS_VISIBLE | WS_CHILD | SS_SUNKEN,
                        10, 125, 200, 20, hwnd, (HMENU)ID_KEY_DISPLAY, nullptr, nullptr);
            
            // Subclass the key edit control
            g_originalKeyEditProc = (WNDPROC)SetWindowLongPtrA(keyEdit, GWLP_WNDPROC, (LONG_PTR)KeyCaptureProc);
            g_keyEditHwnd = keyEdit;
            
            // Target value
            CreateWindowA("STATIC", "Target Value:", WS_VISIBLE | WS_CHILD,
                        10, 155, 100, 20, hwnd, nullptr, nullptr, nullptr);
            CreateWindowA("EDIT", "9", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER,
                        120, 155, 100, 20, hwnd, (HMENU)ID_VALUE_EDIT, nullptr, nullptr);
            
            // OSC address
            CreateWindowA("STATIC", "OSC Address:", WS_VISIBLE | WS_CHILD,
                        10, 185, 100, 20, hwnd, nullptr, nullptr, nullptr);
            CreateWindowA("EDIT", "/flair/runstate", WS_VISIBLE | WS_CHILD | WS_BORDER,
                        120, 185, 200, 20, hwnd, (HMENU)ID_ADDRESS_EDIT, nullptr, nullptr);
            
            // Buttons
            CreateWindowA("BUTTON", "START", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                        10, 220, 80, 30, hwnd, (HMENU)ID_START_BUTTON, nullptr, nullptr);
            CreateWindowA("BUTTON", "STOP", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                        100, 220, 80, 30, hwnd, (HMENU)ID_STOP_BUTTON, nullptr, nullptr);
            
            // Status
            CreateWindowA("STATIC", "Status:", WS_VISIBLE | WS_CHILD,
                        10, 260, 100, 20, hwnd, nullptr, nullptr, nullptr);
            CreateWindowA("EDIT", "", WS_VISIBLE | WS_CHILD | WS_BORDER | WS_VSCROLL | ES_MULTILINE | ES_READONLY,
                        10, 280, 420, 150, hwnd, (HMENU)ID_STATUS_EDIT, nullptr, nullptr);
            
            EnableWindow(GetDlgItem(hwnd, ID_STOP_BUTTON), FALSE);
            
            // Initialize window list
            RefreshWindowList(hwnd);
            
            // Initial layout
            ResizeControls(hwnd);
        }
        break;
        
    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED) {
            ResizeControls(hwnd);
        }
        break;
        
    case WM_GETMINMAXINFO:
        {
            LPMINMAXINFO lpMMI = (LPMINMAXINFO)lParam;
            lpMMI->ptMinTrackSize.x = 400;
            lpMMI->ptMinTrackSize.y = 350;
        }
        break;
        
    case WM_COMMAND:
        if (LOWORD(wParam) == ID_START_BUTTON) {
            StartListener(hwnd);
        } else if (LOWORD(wParam) == ID_STOP_BUTTON) {
            StopListener(hwnd);
        } else if (LOWORD(wParam) == ID_WINDOW_SELECT) {
            RefreshWindowList(hwnd);
        } else if (LOWORD(wParam) == ID_KEY_CAPTURE) {
            g_capturingKey = true;
            SetWindowTextA(GetDlgItem(hwnd, ID_KEY_CAPTURE), "Press a key...");
            SetFocus(g_keyEditHwnd);
        } else if (LOWORD(wParam) == ID_KEY_EDIT && HIWORD(wParam) == EN_CHANGE) {
            // Update key display when text changes
            std::string keyText = GetWindowText(GetDlgItem(hwnd, ID_KEY_EDIT));
            if (!keyText.empty()) {
                std::string displayText = "Current: " + keyText;
                SetWindowTextA(GetDlgItem(hwnd, ID_KEY_DISPLAY), displayText.c_str());
            }
        }
        break;
        
    case WM_CLOSE:
        StopListener(hwnd);
        if (g_trigger) {
            delete g_trigger;
            g_trigger = nullptr;
        }
        DestroyWindow(hwnd);
        break;
        
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
        
    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
    
    const char* className = "OSCTriggerGUI";
    
    WNDCLASSA wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = className;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    
    RegisterClassA(&wc);
    
    HWND hwnd = CreateWindowA(className, "OSC Button Trigger",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 500, 500,
        nullptr, nullptr, hInstance, nullptr);
    
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);
    
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    return 0;
}