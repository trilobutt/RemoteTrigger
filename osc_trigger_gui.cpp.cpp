#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <commctrl.h>
#include <iostream>
#include <thread>
#include <atomic>
#include <string>
#include <cstdint>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "comctl32.lib")

// Control IDs
#define ID_WINDOW_EDIT      1001
#define ID_PORT_EDIT        1002
#define ID_KEY_COMBO        1003
#define ID_VALUE_EDIT       1004
#define ID_ADDRESS_EDIT     1005
#define ID_START_BUTTON     1006
#define ID_STOP_BUTTON      1007
#define ID_STATUS_EDIT      1008

struct Config {
    std::string windowTitle = "YourTargetWindow";
    int port = 55525;
    int triggerKey = VK_SPACE;
    int targetValue = 9;
    std::string oscAddress = "/flair/runstate";
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
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        serverAddr.sin_port = htons(config.port);
        
        if (bind(udpSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            LogStatus("Bind failed on port " + std::to_string(config.port));
            closesocket(udpSocket);
            WSACleanup();
            return false;
        }
        
        running = true;
        LogStatus("Listening on port " + std::to_string(config.port));
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
        LogStatus("TRIGGER: " + config.oscAddress + " = " + std::to_string(config.targetValue));
        
        HWND window = FindWindowA(nullptr, config.windowTitle.c_str());
        if (window) {
            SetForegroundWindow(window);
        }
        
        keybd_event(config.triggerKey, 0, 0, 0);
        keybd_event(config.triggerKey, 0, KEYEVENTF_KEYUP, 0);
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

// Global variables
OSCTrigger* g_trigger = nullptr;
std::thread* g_listenerThread = nullptr;

int StringToVK(const std::string& key) {
    if (key == "SPACE") return VK_SPACE;
    if (key == "ENTER") return VK_RETURN;
    if (key == "F1") return VK_F1;
    if (key == "F2") return VK_F2;
    if (key == "F3") return VK_F3;
    if (key == "F4") return VK_F4;
    if (key == "F5") return VK_F5;
    if (key.length() == 1) return toupper(key[0]);
    return VK_SPACE;
}

std::string GetWindowText(HWND hwnd) {
    int len = GetWindowTextLength(hwnd);
    std::string result(len, 0);
    GetWindowTextA(hwnd, &result[0], len + 1);
    return result;
}

void StartListener(HWND hwnd) {
    if (g_trigger && g_trigger->IsRunning()) return;
    
    Config config;
    config.windowTitle = GetWindowText(GetDlgItem(hwnd, ID_WINDOW_EDIT));
    config.port = GetDlgItemInt(hwnd, ID_PORT_EDIT, nullptr, FALSE);
    config.targetValue = GetDlgItemInt(hwnd, ID_VALUE_EDIT, nullptr, FALSE);
    config.oscAddress = GetWindowText(GetDlgItem(hwnd, ID_ADDRESS_EDIT));
    
    char keyText[50];
    GetDlgItemTextA(hwnd, ID_KEY_COMBO, keyText, sizeof(keyText));
    config.triggerKey = StringToVK(keyText);
    
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

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE:
        {
            // Window title
            CreateWindow("STATIC", "Window Title:", WS_VISIBLE | WS_CHILD,
                        10, 10, 100, 20, hwnd, nullptr, nullptr, nullptr);
            CreateWindow("EDIT", "YourTargetWindow", WS_VISIBLE | WS_CHILD | WS_BORDER,
                        120, 10, 200, 20, hwnd, (HMENU)ID_WINDOW_EDIT, nullptr, nullptr);
            
            // Port
            CreateWindow("STATIC", "Port:", WS_VISIBLE | WS_CHILD,
                        10, 40, 100, 20, hwnd, nullptr, nullptr, nullptr);
            CreateWindow("EDIT", "55525", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER,
                        120, 40, 100, 20, hwnd, (HMENU)ID_PORT_EDIT, nullptr, nullptr);
            
            // Trigger key
            CreateWindow("STATIC", "Trigger Key:", WS_VISIBLE | WS_CHILD,
                        10, 70, 100, 20, hwnd, nullptr, nullptr, nullptr);
            HWND keyCombo = CreateWindow("COMBOBOX", "", WS_VISIBLE | WS_CHILD | CBS_DROPDOWN,
                        120, 70, 100, 200, hwnd, (HMENU)ID_KEY_COMBO, nullptr, nullptr);
            SendMessage(keyCombo, CB_ADDSTRING, 0, (LPARAM)"SPACE");
            SendMessage(keyCombo, CB_ADDSTRING, 0, (LPARAM)"ENTER");
            SendMessage(keyCombo, CB_ADDSTRING, 0, (LPARAM)"F1");
            SendMessage(keyCombo, CB_ADDSTRING, 0, (LPARAM)"F2");
            SendMessage(keyCombo, CB_ADDSTRING, 0, (LPARAM)"F3");
            SendMessage(keyCombo, CB_ADDSTRING, 0, (LPARAM)"F4");
            SendMessage(keyCombo, CB_ADDSTRING, 0, (LPARAM)"F5");
            SendMessage(keyCombo, CB_SETCURSEL, 0, 0); // Select SPACE
            
            // Target value
            CreateWindow("STATIC", "Target Value:", WS_VISIBLE | WS_CHILD,
                        10, 100, 100, 20, hwnd, nullptr, nullptr, nullptr);
            CreateWindow("EDIT", "9", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER,
                        120, 100, 100, 20, hwnd, (HMENU)ID_VALUE_EDIT, nullptr, nullptr);
            
            // OSC address
            CreateWindow("STATIC", "OSC Address:", WS_VISIBLE | WS_CHILD,
                        10, 130, 100, 20, hwnd, nullptr, nullptr, nullptr);
            CreateWindow("EDIT", "/flair/runstate", WS_VISIBLE | WS_CHILD | WS_BORDER,
                        120, 130, 200, 20, hwnd, (HMENU)ID_ADDRESS_EDIT, nullptr, nullptr);
            
            // Buttons
            CreateWindow("BUTTON", "START", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                        10, 170, 80, 30, hwnd, (HMENU)ID_START_BUTTON, nullptr, nullptr);
            CreateWindow("BUTTON", "STOP", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                        100, 170, 80, 30, hwnd, (HMENU)ID_STOP_BUTTON, nullptr, nullptr);
            
            // Status
            CreateWindow("STATIC", "Status:", WS_VISIBLE | WS_CHILD,
                        10, 210, 100, 20, hwnd, nullptr, nullptr, nullptr);
            CreateWindow("EDIT", "", WS_VISIBLE | WS_CHILD | WS_BORDER | WS_VSCROLL | ES_MULTILINE | ES_READONLY,
                        10, 230, 420, 150, hwnd, (HMENU)ID_STATUS_EDIT, nullptr, nullptr);
            
            EnableWindow(GetDlgItem(hwnd, ID_STOP_BUTTON), FALSE);
        }
        break;
        
    case WM_COMMAND:
        if (LOWORD(wParam) == ID_START_BUTTON) {
            StartListener(hwnd);
        } else if (LOWORD(wParam) == ID_STOP_BUTTON) {
            StopListener(hwnd);
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
    
    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = className;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    
    RegisterClass(&wc);
    
    HWND hwnd = CreateWindow(className, "OSC Button Trigger",
        WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME,
        CW_USEDEFAULT, CW_USEDEFAULT, 460, 420,
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