#ifndef ADBCLIENT_H
#define ADBCLIENT_H

#include <iostream>
#include <sstream>
#include <thread>
#include <vector>
#include <atomic>
#include <fstream>
#include <iomanip>
#include <map>

namespace ADBC {
    inline std::string Execute(std::string executable, std::string args);

    struct Point {
        float x;
        float y;
    };

    enum KeyEvent {
        KEYCODE_HOME = 3,

        KEYCODE_BACK = 4,

        KEYCODE_MENU = 82,

        KEYCODE_VOLUME_UP = 24,

        KEYCODE_VOLUME_DOWN = 25,

        KEYCODE_POWER = 26,

        KEYCODE_CAMERA = 27,

        KEYCODE_ENTER = 66,

        KEYCODE_DEL = 67,

        KEYCODE_SEARCH = 84,
    };

    struct AndroidEvent {
        std::string type;
        std::vector<Point> points;
        float start;
        float end;
        float duration;
    };

    struct Resolution {
        int width;
        int height;

        Resolution(int w, int h) {
            width = w;
            height = h;
        }

        std::string toString() {
            return std::to_string(width) + "x" + std::to_string(height);
        }
    };

    class ADBClient {
    public :
        explicit ADBClient(std::string adbPath, std::string serial);

        static std::shared_ptr<ADBClient> Create(std::string adbPath, std::string serial);

        static std::vector<std::string> Devices(std::string adbPath);

        std::vector<std::string> devices() const;

        std::string shell(std::string command) const;

        std::string text(std::string command);

        std::string textUTF_8(std::string command);

        std::string tap(Point p);

        std::string swipe(Point start, Point end, float duartion = 0);

        std::string push(std::string source, std::string destination);

        std::string openActivity(std::string packageName, std::string activityName);

        std::string broadcast(std::string action);

        std::string pull(std::string source, std::string destination);

        std::string install(std::string apk);

        std::string printScreen(const std::string&destPath = "assets/screenshot.png");

        std::string inputKey(KeyEvent key);

        Resolution getResolution();

        Resolution getAxisResolution();

        float AxisXToScreen(std::string hex);

        float AxisYToScreen(std::string hex);

        Point AxisToScreen(std::pair<std::string, std::string> hex);

        void startRecordingAct();

        std::vector<AndroidEvent> stopRecordingAct();

        void ReplayEvents(std::vector<AndroidEvent> events);

        void ReplayEvents(std::string name);

        bool checkPackage(std::string packageName);

        void setID(std::string serial);

        void loadEvents(std::string name, std::vector<AndroidEvent> event);

        std::vector<AndroidEvent> getEvents(std::string name);

    private:
        static int HexToDec(std::string hexString) {
            // 去除字符串中的空格
            std::string trimmedHexString = hexString;
            trimmedHexString.erase(std::remove(trimmedHexString.begin(), trimmedHexString.end(), ' '),
                                   trimmedHexString.end());

            try {
                int decimalValue = std::stoi(trimmedHexString, nullptr, 16);
                return decimalValue;
            }
            catch (const std::invalid_argument&e) {
                // 如果输入字符串不是有效的16进制格式，抛出异常
                throw std::invalid_argument("Invalid hexadecimal string");
            } catch (const std::out_of_range&e) {
                // 如果转换结果超出了 int 的范围，抛出异常
                throw std::out_of_range("Hexadecimal value out of range for int");
            }
        }


        void recordAct();

        Resolution resolution = Resolution(0, 0);
        Resolution AxisResolution = Resolution(0, 0);
        std::atomic<bool> recording = false;
        std::thread recordingThread;
        std::vector<AndroidEvent> recordedEvents;
        std::map<std::string, std::vector<AndroidEvent>> events;
        std::string adbPath;
        std::string serial;
    };
}


#endif //ADBCLIENT_H
