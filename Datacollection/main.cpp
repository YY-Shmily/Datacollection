#include <iostream>
#include <thread>
#include <string>
#include <vector>
#include <queue>
#include <fstream>
#include <sys/stat.h>  // 用于检查和创建目录
#include <direct.h>    // 在 Windows 上使用 _mkdir
#include <nlohmann/json.hpp>
#include <iomanip>
#include <sstream>
#include <ctime>
#include <locale>
#include "MQTTClient.h"
using namespace std;

using json = nlohmann::json;

// 定义MQTT参数
#define ADDRESS     "tcp://localhost:1883"  // MQTT 服务器地址
#define CLIENTID    "ExampleClientSub"       // 客户端 ID
#define TOPIC       "your/topic"             // 订阅的主题
#define QOS         1                        // 服务质量等级
#define TIMEOUT     10000L                   // 超时时间

queue<string> Messages;
queue<string> Focas;
queue<string> FocasNC;
queue<string> FocasPower;
queue<string> VibrationMain;
queue<string> VibrationFeed;
queue<string> Macro;
queue<string> PLC;

volatile MQTTClient_deliveryToken deliveredtoken;

void delivered(void* context, MQTTClient_deliveryToken dt) {
    cout << "Message with token value " << dt << " delivery confirmed" << endl;
    deliveredtoken = dt;
}

// 当接收到消息时调用
int msgarrvd(void* context, char* topicName, int topicLen, MQTTClient_message* message) {
    cout << "Message arrived" << endl;
    cout << "Topic: " << topicName << endl;
    cout << "Message: " << (char*)message->payload << endl;
    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    return 1;
}

// 当连接丢失时调用
void connlost(void* context, char* cause) {
    cout << "\nConnection lost" << std::endl;
    cout << "Cause: " << cause << std::endl;
}


bool DirectoryExists(const std::string& dirPath) {
    struct stat info;
    if (stat(dirPath.c_str(), &info) != 0) {
        return false;  // 路径不存在
    }
    else if (info.st_mode & S_IFDIR) {
        return true;   // 路径存在且为目录
    }
    else {
        return false;  // 路径存在但不是目录
    }
}

bool CreateDirectory(const std::string& dirPath) {
    return _mkdir(dirPath.c_str()) == 0 || errno == EEXIST;  // Windows 使用 _mkdir
}

void SaveMessages()
{
    std::string focas = "";
    std::string focasNC = "";
    std::string focasPower = "";
    std::string vibrationMain = "";
    std::string vibrationFeed = "";
    std::string macro = "";
    std::string plc = "";

    // 定义桌面路径
    std::string desktopPath = "C://desktop/WXF";  // 固定路径
    std::string topic = "YourTopic"; // 替换成实际的主题
    if (!topic.empty()) {
        desktopPath = desktopPath + "//" + topic;  // 合成路径
        if (!DirectoryExists(desktopPath)) {
            if (!CreateDirectory(desktopPath)) {
                std::cerr << "无法创建目录: " << desktopPath << std::endl;
                return ;  // 创建目录失败，退出程序
            }
        }
    }

    // 文件路径
    std::string FocasPath = desktopPath + "//Focas.txt";
    std::string FocasNCPath = desktopPath + "//FocasNC.txt";
    std::string FocasPowerPath = desktopPath + "//FocasPower.txt";
    std::string VibrationMainPath = desktopPath + "//VibrationMain.txt";
    std::string VibrationFeedPath = desktopPath + "//VibrationFeed.txt";
    std::string MacroPath = desktopPath + "//FocasMacro.txt";
    std::string PLCPath = desktopPath + "//FocasPLC.txt";

    // 写入文件内容
    std::ofstream focasFile(FocasPath, std::ios::app);
    if (focasFile.is_open()) {
        focasFile << "时间 主轴负载 X轴负载 Y轴负载 Z轴负载 主轴转速 进给速度 X机械坐标 Y机械坐标 Z机械坐标\n";
        focasFile.close();
    }

    std::ofstream focasNCFile(FocasNCPath, std::ios::app);
    if (focasNCFile.is_open()) {
        focasNCFile << "时间 程序号 子程序号 指令行号 GCode\n";
        focasNCFile.close();
    }

    std::ofstream focasPowerFile(FocasPowerPath, std::ios::app);
    if (focasPowerFile.is_open()) {
        focasPowerFile << "时间 主轴功率\n";
        focasPowerFile.close();
    }

    std::ofstream vibrationMainFile(VibrationMainPath, std::ios::app);
    if (vibrationMainFile.is_open()) {
        vibrationMainFile << "时间 主轴振动信号X 主轴振动信号Y 主轴振动信号Z\n";
        vibrationMainFile.close();
    }

    std::ofstream vibrationFeedFile(VibrationFeedPath, std::ios::app);
    if (vibrationFeedFile.is_open()) {
        vibrationFeedFile << "时间 进给轴振动信号X 进给轴振动信号Y 进给轴振动信号Z\n";
        vibrationFeedFile.close();
    }

    std::ofstream macroFile(MacroPath, std::ios::app);
    if (macroFile.is_open()) {
        macroFile << "时间 变量1 变量2 #00510\n";
        macroFile.close();
    }

    std::ofstream plcFile(PLCPath, std::ios::app);
    if (plcFile.is_open()) {
        plcFile << "时间 循环启动 刀具号 切削信号 进给旋钮 M180 M182\n";
        plcFile.close();
    }

    while (true) {
        // 处理 Focas 队列
        if (!Focas.empty()) {
            focas = Focas.front();
            Focas.pop();
            std::ofstream outFile(FocasPath, std::ios_base::app);
            outFile << ParseFocas(focas, "Focas") << "\n";
        }

        // 处理 FocasNC 队列
        if (!FocasNC.empty()) {
            focasNC = FocasNC.front();
            FocasNC.pop();
            std::ofstream outFile(FocasNCPath, std::ios_base::app);
            outFile << ParseFocas(focasNC, "FocasNC") << "\n";
        }

        // 处理 FocasPower 队列
        if (!FocasPower.empty()) {
            focasPower = FocasPower.front();
            FocasPower.pop();
            std::ofstream outFile(FocasPowerPath, std::ios_base::app);
            outFile << ParseFocas(focasPower, "FocasPower") << "\n";
        }

        // 处理 Macro 队列
        if (!Macro.empty()) {
            macro = Macro.front();
            Macro.pop();
            std::ofstream outFile(MacroPath, std::ios_base::app);
            outFile << ParseFocas(macro, "FocasMacro") << "\n";
        }

        // 处理 PLC 队列
        if (!PLC.empty()) {
            plc = PLC.front();
            PLC.pop();
            std::ofstream outFile(PLCPath, std::ios_base::app);
            outFile << ParseFocas(plc, "FocasPLC") << "\n";
        }

        // 处理 VibrationMain 队列
        if (!VibrationMain.empty()) {
            vibrationMain = VibrationMain.front();
            VibrationMain.pop();
            auto parsedVibration = ParseVibration(vibrationMain, "vibrationMain");
            std::ofstream outFile(VibrationMainPath, std::ios_base::app);
            outFile << parsedVibration[3] << "\n";
            outFile << parsedVibration[0] << "\n";
            outFile << parsedVibration[1] << "\n";
            outFile << parsedVibration[2] << "\n";
        }

        // 处理 VibrationFeed 队列
        if (!VibrationFeed.empty()) {
            vibrationFeed = VibrationFeed.front();
            VibrationFeed.pop();
            auto parsedVibration = ParseVibration(vibrationFeed, "vibrationFeed");
            std::ofstream outFile(VibrationFeedPath, std::ios_base::app);
            outFile << parsedVibration[3] << "\n";
            outFile << parsedVibration[0] << "\n";
            outFile << parsedVibration[1] << "\n";
            outFile << parsedVibration[2] << "\n";
        }

        //// 等待一段时间，以防止过度占用 CPU
        //std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}


// 模拟 Focas 数据模型
struct FocusModel {
    time_t Time;                        // 时间戳
    vector<vector<string>> Data;        // 二维数组，模拟设备数据
};

// 解析 JSON 字符串为 FocusModel 结构体
FocusModel DeserializeFocas(const string& msg) {
    json j = json::parse(msg);
    FocusModel model;
    model.Time = j["Time"];
    model.Data = j["Data"].get<vector<vector<string>>>();
    return model;
}

// 时间转换函数
string ConvertEpochToDateTime(time_t epoch_seconds) {
    // C++ 中处理时间，转换为本地时间格式
    time_t raw_time = epoch_seconds;
    struct tm* time_info;
    char buffer[80];

    // 获取本地时间
    time_info = localtime(&raw_time);

    // 格式化时间字符串
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", time_info);
    return string(buffer);
}


string ParseFocas(const string& msg, const string& type) {
    FocusModel model = DeserializeFocas(msg);
    stringstream res;

    // 将时间戳转换为日期时间格式
    res << ConvertEpochToDateTime(model.Time);

    if (type == "FocasPower") {
        int arrayIndex = 0;
        for (const auto& subArray : model.Data) {
            arrayIndex++;
            if (!subArray.empty() && arrayIndex == 9) {
                res << " " << subArray.back();
            }
        }
    }
    else if (type == "FocasNC") {
        int arrayIndex = 0;
        string NC_SubProcess, NC_Code;
        int NC_Number = 0;
        bool NC_Tag = false;

        for (const auto& subArray : model.Data) {
            arrayIndex++;
            if (!subArray.empty() && arrayIndex == 2) {
                string beforeNewline = subArray.back();

                // 判断是否是子程序
                if (beforeNewline.find('\n') != string::npos) {
                    beforeNewline = beforeNewline.substr(0, beforeNewline.find('\n'));
                }

                if (beforeNewline[0] == 'O') {
                    NC_SubProcess = beforeNewline;
                    NC_Tag = true;
                }

                if (NC_Tag) {
                    if (beforeNewline != NC_Code) {
                        NC_Number++;
                        NC_Code = beforeNewline;
                    }
                    res << " " << NC_SubProcess << " " << NC_Number << " " << beforeNewline;
                }
                else {
                    if (beforeNewline != NC_Code) {
                        NC_Number++;
                        NC_Code = beforeNewline;
                    }
                    res << " " << NC_Number << " " << beforeNewline;
                }
            }
            if (!subArray.empty() && arrayIndex == 1) {
                res << " " << subArray.back();
            }
        }
    }
    else if (type == "FocasMacro" || type == "FocasPLC") {
        for (const auto& subArray : model.Data) {
            if (!subArray.empty()) {
                res << " " << subArray.back();
            }
        }
    }
    else {
        for (const auto& subArray : model.Data) {
            if (!subArray.empty()) {
                res << " " << subArray.back();
            }
        }
    }

    return res.str();
}



// 十六进制字符串转为 ASCII 字符串
std::string HexToAscii(const std::string& hexString) {
    std::string result;
    for (size_t i = 0; i < hexString.length(); i += 2) {
        std::string byte = hexString.substr(i, 2);
        char ch = static_cast<char>(std::stoi(byte, nullptr, 16));
        result += ch;
    }
    return result;
}

// Unix 时间戳转换为日期时间
std::string UnixTimestampToDateTime(double seconds) {
    std::time_t raw_time = static_cast<std::time_t>(seconds);
    std::tm* time_info = std::localtime(&raw_time);

    std::ostringstream oss;
    oss << std::put_time(time_info, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

// 解析振动数据
std::vector<std::string> ParseVibration(const std::string& msg, const std::string& type) {
    std::vector<std::string> res(4);
    int totalNumber;
    int number;

    // 解析包号
    std::string total = msg.substr(8, 4);
    number = std::stoi(total, nullptr, 16);
    totalNumber = 4 * number;

    // 解析 X 方向振动数据
    std::string channel_x = msg.substr(16, totalNumber);

    // 解析 Y 方向振动数据
    std::string channel_y = msg.substr(20 + totalNumber, totalNumber);

    // 解析 Z 方向振动数据
    std::string channel_z = msg.substr(24 + 2 * totalNumber, totalNumber);

    // 解析时间戳
    std::string time_str = msg.substr(24 + 3 * totalNumber, msg.length() - 24 - 3 * totalNumber);
    double time = std::stod(HexToAscii(time_str));
    res[3] = UnixTimestampToDateTime(time);

    // 解析每个通道数据
    auto parseChannel = [](const std::string& channel_data) {
        std::ostringstream oss;
        for (size_t i = 0; i < channel_data.length(); i += 4) {
            std::string hex_str = channel_data.substr(i, 4);
            int value = std::stoi(hex_str, nullptr, 16);

            if (channel_data[i] >= '0' && channel_data[i] <= '7') {
                oss << (value / 32.768) << " ";
            }
            else {
                oss << (-(65536 - value) / 32.768) << " ";
            }
        }
        return oss.str();
        };

    res[0] = parseChannel(channel_x);
    res[1] = parseChannel(channel_y);
    res[2] = parseChannel(channel_z);

    return res;
}


void GIVEMyMessages(const std::string& topic, const std::string& msg) {
    static std::unordered_map<std::string, std::queue<std::string>*> topicToQueue = {
        {"GASU/Focas/HW1/100ms/Double/NC", &Focas},
        {"GASU/Power/HW1/100ms/Double", &FocasPower},
        {"GASU/Focas/HW1/100ms/String/Info", &FocasNC},
        {"CBS/Dynamic/HW1/100ms/Double", &VibrationMain},
        {"CBS/Dynamic/HW2/100ms/Double", &VibrationFeed},
        {"GASU/Focas/HW1/100ms/Double/Macro", &Macro},
        {"GASU/Focas/HW1/100ms/Double/PLC", &PLC}
    };

    auto it = topicToQueue.find(topic);
    if (it != topicToQueue.end()) {
        it->second->push(msg);
    }
    else {
        // 主题没有匹配，什么也不做
    }
}


int main() {

    


    MQTTClient client;
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    int rc;

    // 初始化客户端
    MQTTClient_create(&client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);

    // 设置回调函数
    MQTTClient_setCallbacks(client, NULL, connlost, msgarrvd, delivered);

    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;

    // 连接到MQTT服务器
    if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS) {
        std::cout << "Failed to connect, return code " << rc << std::endl;
        return -1;
    }

    std::cout << "Subscribing to topic " << TOPIC << " with QoS " << QOS << std::endl;
    MQTTClient_subscribe(client, TOPIC, QOS);

    // 保持程序运行，等待消息
    while (true) {
        this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    // 断开连接并清理资源
    MQTTClient_disconnect(client, 10000);
    MQTTClient_destroy(&client);

    return rc;
}
