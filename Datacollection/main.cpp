#include <iostream>
#include <thread>
#include <string>
#include <vector>
#include <queue>
#include <fstream>
#include <sys/stat.h>  // ���ڼ��ʹ���Ŀ¼
#include <direct.h>    // �� Windows ��ʹ�� _mkdir
#include <nlohmann/json.hpp>
#include <iomanip>
#include <sstream>
#include <ctime>
#include <locale>
#include "MQTTClient.h"
using namespace std;

using json = nlohmann::json;

// ����MQTT����
#define ADDRESS     "tcp://localhost:1883"  // MQTT ��������ַ
#define CLIENTID    "ExampleClientSub"       // �ͻ��� ID
#define QOS         1                        // ���������ȼ�
#define TIMEOUT     10000L                   // ��ʱʱ��

queue<string> Messages;
queue<string> Focas;
queue<string> FocasNC;
queue<string> FocasPower;
queue<string> VibrationMain;
queue<string> VibrationFeed;
queue<string> Macro;
queue<string> PLC;

string ParseFocas(const string& msg, const string& type);
vector<string> ParseVibration(const string& msg, const string& type);
void SaveMessages();
void delivered(void* context, MQTTClient_deliveryToken dt);
int msgarrvd(void* context, char* topicName, int topicLen, MQTTClient_message* message);
void connlost(void* context, char* cause);
bool DirectoryExists(const string& dirPath);
bool CreateDirectory(const string& dirPath);

volatile MQTTClient_deliveryToken deliveredtoken;

void delivered(void* context, MQTTClient_deliveryToken dt) {
    cout << "Message with token value " << dt << " delivery confirmed" << endl;
    deliveredtoken = dt;
}

// �����Ӷ�ʧʱ����
void connlost(void* context, char* cause) {
    cout << "\nConnection lost" << endl;
    cout << "Cause: " << cause << endl;
}


bool DirectoryExists(const string& dirPath) {
    struct stat info;
    if (stat(dirPath.c_str(), &info) != 0) {
        return false;  // ·��������
    }
    else if (info.st_mode & S_IFDIR) {
        return true;   // ·��������ΪĿ¼
    }
    else {
        return false;  // ·�����ڵ�����Ŀ¼
    }
}

bool CreateDirectory(const string& dirPath) {
    return _mkdir(dirPath.c_str()) == 0 || errno == EEXIST;  // Windows ʹ�� _mkdir
}

void SaveMessages()
{
    string focas = "";
    string focasNC = "";
    string focasPower = "";
    string vibrationMain = "";
    string vibrationFeed = "";
    string macro = "";
    string plc = "";

    // ��������·��
    string desktopPath = "C:";  // �̶�·��
    string topic = "TW_part"; // �滻��ʵ�ʵ�����
    if (!topic.empty()) {
        desktopPath = desktopPath + "//" + topic;  // �ϳ�·��
        if (!DirectoryExists(desktopPath)) {
            if (!CreateDirectory(desktopPath)) {
                cerr << "�޷�����Ŀ¼: " << desktopPath << endl;
                return ;  // ����Ŀ¼ʧ�ܣ��˳�����
            }
        }
    }

    // �ļ�·��
    string FocasPath = desktopPath + "//Focas.txt";
    string FocasNCPath = desktopPath + "//FocasNC.txt";
    string FocasPowerPath = desktopPath + "//FocasPower.txt";
    string VibrationMainPath = desktopPath + "//VibrationMain.txt";
    string VibrationFeedPath = desktopPath + "//VibrationFeed.txt";
    string MacroPath = desktopPath + "//FocasMacro.txt";
    string PLCPath = desktopPath + "//FocasPLC.txt";

    // д���ļ�����
    ofstream focasFile(FocasPath, ios::app);
    if (focasFile.is_open()) {
        focasFile << "ʱ�� ���Ḻ�� X�Ḻ�� Y�Ḻ�� Z�Ḻ�� ����ת�� �����ٶ� X��е���� Y��е���� Z��е����\n";
        focasFile.close();
    }

    ofstream focasNCFile(FocasNCPath, ios::app);
    if (focasNCFile.is_open()) {
        focasNCFile << "ʱ�� ����� �ӳ���� ָ���к� GCode\n";
        focasNCFile.close();
    }

    ofstream focasPowerFile(FocasPowerPath, ios::app);
    if (focasPowerFile.is_open()) {
        focasPowerFile << "ʱ�� ���Ṧ��\n";
        focasPowerFile.close();
    }

    ofstream vibrationMainFile(VibrationMainPath, ios::app);
    if (vibrationMainFile.is_open()) {
        vibrationMainFile << "ʱ�� �������ź�X �������ź�Y �������ź�Z\n";
        vibrationMainFile.close();
    }

    ofstream vibrationFeedFile(VibrationFeedPath, ios::app);
    if (vibrationFeedFile.is_open()) {
        vibrationFeedFile << "ʱ�� ���������ź�X ���������ź�Y ���������ź�Z\n";
        vibrationFeedFile.close();
    }

    ofstream macroFile(MacroPath, ios::app);
    if (macroFile.is_open()) {
        macroFile << "ʱ�� ����1 ����2 #00510\n";
        macroFile.close();
    }

    ofstream plcFile(PLCPath, ios::app);
    if (plcFile.is_open()) {
        plcFile << "ʱ�� ѭ������ ���ߺ� �����ź� ������ť M180 M182\n";
        plcFile.close();
    }

    while (true) {
        // ���� Focas ����
        if (!Focas.empty()) {
            focas = Focas.front();
            Focas.pop();
            ofstream outFile(FocasPath, ios_base::app);
            outFile << ParseFocas(focas, "Focas") << "\n";
        }

        // ���� FocasNC ����
        if (!FocasNC.empty()) {
            focasNC = FocasNC.front();
            FocasNC.pop();
            ofstream outFile(FocasNCPath, ios_base::app);
            outFile << ParseFocas(focasNC, "FocasNC") << "\n";
        }

        // ���� FocasPower ����
        if (!FocasPower.empty()) {
            focasPower = FocasPower.front();
            FocasPower.pop();
            ofstream outFile(FocasPowerPath, ios_base::app);
            outFile << ParseFocas(focasPower, "FocasPower") << "\n";
        }

        // ���� Macro ����
        if (!Macro.empty()) {
            macro = Macro.front();
            Macro.pop();
            ofstream outFile(MacroPath, ios_base::app);
            outFile << ParseFocas(macro, "FocasMacro") << "\n";
        }

        // ���� PLC ����
        if (!PLC.empty()) {
            plc = PLC.front();
            PLC.pop();
            ofstream outFile(PLCPath, ios_base::app);
            outFile << ParseFocas(plc, "FocasPLC") << "\n";
        }

        // ���� VibrationMain ����
        if (!VibrationMain.empty()) {
            vibrationMain = VibrationMain.front();
            VibrationMain.pop();
            auto parsedVibration = ParseVibration(vibrationMain, "vibrationMain");
            ofstream outFile(VibrationMainPath, ios_base::app);
            outFile << parsedVibration[3] << "\n";
            outFile << parsedVibration[0] << "\n";
            outFile << parsedVibration[1] << "\n";
            outFile << parsedVibration[2] << "\n";
        }

        // ���� VibrationFeed ����
        if (!VibrationFeed.empty()) {
            vibrationFeed = VibrationFeed.front();
            VibrationFeed.pop();
            auto parsedVibration = ParseVibration(vibrationFeed, "vibrationFeed");
            ofstream outFile(VibrationFeedPath, ios_base::app);
            outFile << parsedVibration[3] << "\n";
            outFile << parsedVibration[0] << "\n";
            outFile << parsedVibration[1] << "\n";
            outFile << parsedVibration[2] << "\n";
        }

        //// �ȴ�һ��ʱ�䣬�Է�ֹ����ռ�� CPU
        //this_thread::sleep_for(chrono::milliseconds(100));
    }
}


// ģ�� Focas ����ģ��
struct FocusModel {
    time_t Time;                        // ʱ���
    vector<vector<string>> Data;        // ��ά���飬ģ���豸����
};

// ���� JSON �ַ���Ϊ FocusModel �ṹ��
FocusModel DeserializeFocas(const string& msg) {
    json j = json::parse(msg);
    FocusModel model;
    model.Time = j["Time"];
    model.Data = j["Data"].get<vector<vector<string>>>();
    return model;
}

// ʱ��ת������
string ConvertEpochToDateTime(time_t epoch_seconds) {
    // C++ �д���ʱ�䣬ת��Ϊ����ʱ���ʽ
    time_t raw_time = epoch_seconds;
    struct tm time_info;
    char buffer[80];

    // ��ȡ����ʱ��
    localtime_s(&time_info, &raw_time);

    // ��ʽ��ʱ���ַ���
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &time_info);
    return string(buffer);
}


string ParseFocas(const string& msg, const string& type) {
    FocusModel model = DeserializeFocas(msg);
    stringstream res;

    // ��ʱ���ת��Ϊ����ʱ���ʽ
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

                // �ж��Ƿ����ӳ���
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




// ʮ�������ַ���תΪ ASCII �ַ���
string HexToAscii(const string& hexString) {
    string result;
    for (size_t i = 0; i < hexString.length(); i += 2) {
        string byte = hexString.substr(i, 2);
        char ch = static_cast<char>(stoi(byte, nullptr, 16));
        result += ch;
    }
    return result;
}

// Unix ʱ���ת��Ϊ����ʱ��
string UnixTimestampToDateTime(double seconds) {
    time_t raw_time = static_cast<time_t>(seconds);
    struct tm time_info;
    localtime_s(&time_info, &raw_time);

    ostringstream oss;
    oss << put_time(&time_info, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

// ����������
vector<string> ParseVibration(const string& msg, const string& type) {
    vector<string> res(4);
    int totalNumber;
    int number;

    // ��������
    string total = msg.substr(8, 4);
    number = stoi(total, nullptr, 16);
    totalNumber = 4 * number;

    // ���� X ����������
    string channel_x = msg.substr(16, totalNumber);

    // ���� Y ����������
    string channel_y = msg.substr(20 + totalNumber, totalNumber);

    // ���� Z ����������
    string channel_z = msg.substr(24 + 2 * totalNumber, totalNumber);

    // ����ʱ���
    string time_str = msg.substr(24 + 3 * totalNumber, msg.length() - 24 - 3 * totalNumber);
    double time = stod(HexToAscii(time_str));
    res[3] = UnixTimestampToDateTime(time);

    // ����ÿ��ͨ������
    auto parseChannel = [](const string& channel_data) {
        ostringstream oss;
        for (size_t i = 0; i < channel_data.length(); i += 4) {
            string hex_str = channel_data.substr(i, 4);
            int value = stoi(hex_str, nullptr, 16);

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


void GIVEMyMessages(const string& topic, const string& msg) {
    static unordered_map<string, queue<string>*> topicToQueue = {
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
        // ����û��ƥ�䣬ʲôҲ����
    }
}


int main() {

    MQTTClient client;
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    int rc;
    
    // ��ʼ���ͻ���
    MQTTClient_create(&client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);

    // ���ûص�����
    MQTTClient_setCallbacks(client, NULL, connlost, NULL , delivered);

    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;

    // ���ӵ�MQTT������
    if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS) {
        cout << "Failed to connect, return code " << rc << endl;
        return -1;
    }

    //��������
    MQTTClient_subscribe(client, "GASU/Focas/HW1/100ms/Double/NC", QOS);
    MQTTClient_subscribe(client, "GASU/Power/HW1/100ms/Double", QOS);
    MQTTClient_subscribe(client, "GASU/Focas/HW1/100ms/Double/PLC", QOS);
    MQTTClient_subscribe(client, "GASU/Focas/HW1/100ms/String/Info", QOS);
    MQTTClient_subscribe(client, "GASU/Focas/HW1/100ms/Double/Macro", QOS);
    MQTTClient_subscribe(client, "CBS/Dynamic/HW1/100ms/Double", QOS);
    MQTTClient_subscribe(client, "CBS/Dynamic/HW2/100ms/Double", QOS);


    thread t2(SaveMessages);
    t2.join();

    // ���ֳ������У��ȴ���Ϣ
    while (true) {

        char* topicName = NULL;
        int topicLen;
        MQTTClient_message* message = NULL;
        string msg;

        // ������Ϣ
        rc = MQTTClient_receive(client, &topicName, &topicLen, &message, 0); 
        if (rc != MQTTCLIENT_SUCCESS) {
            cout << "Error receiving message: " << rc << endl;
            continue;
        }

        if (message != NULL) {

            if (topicName == "CBS/Dynamic/HW1/100ms/Double" || topicName == "CBS/Dynamic/HW2/100ms/Double") {
                // �� payload ת��Ϊʮ�������ַ���
                unsigned char* payload = static_cast<unsigned char*>(message->payload);
                int payloadlen = message->payloadlen;

                stringstream hex;
                for (int i = 0; i < payloadlen; ++i) {
                    hex << setfill('0') << setw(2) << static_cast<int>(payload[i]);
                }
                msg = hex.str();
            }
            else {
                // ֱ�ӽ� payload ת��Ϊ�ַ���
                msg = string(string(static_cast<char*>(message->payload), message->payloadlen));
            }

            GIVEMyMessages(topicName, msg);
        }
    }
    

    //// �Ͽ����Ӳ�������Դ
    //MQTTClient_disconnect(client, 10000);
    //MQTTClient_destroy(&client);

    return rc;
}
