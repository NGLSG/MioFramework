#ifndef ADBCLIENT_H
#define ADBCLIENT_H

#if !defined(__STRICT_ANSI__) || defined(_XOPEN_SOURCE) || defined(_GNU_SOURCE) || defined(_BSD_SOURCE) || defined(_USE_MATH_DEFINES)
#define M_E                  2.7182818284590452354
#define M_LOG2E        1.4426950408889634074
#define M_LOG10E      0.43429448190325182765
#define M_LN2              0.69314718055994530942
#define M_LN10            2.30258509299404568402
#define M_PI                 3.14159265358979323846
#define M_PI_2             1.57079632679489661923
#define M_PI_4             0.78539816339744830962
#define M_1_PI             0.31830988618379067154
#define M_2_PI             0.63661977236758134308
#define M_2_SQRTPI   1.12837916709551257390
#define M_SQRT2        1.41421356237309504880
#define M_SQRT1_2    0.70710678118654752440
#endif

#include <iostream>
#include <sstream>
#include <thread>
#include <vector>
#include <atomic>
#include <fstream>
#include <iomanip>
#include <map>
#include <memory>


namespace ADBC {
    inline std::string Execute(std::string executable, std::string args);

    struct Point {
        float x;
        float y;

        bool operator==(const Point&p) const {
            return x == p.x && y == p.y;
        }
    };

    struct PointHash {
        std::size_t operator()(const Point&p) const {
            return std::hash<float>()(p.x) ^ std::hash<float>()(p.y);
        }
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
        std::vector<std::pair<float, Point>> points; //time:point
        float start, end, duration;
        std::string id = "";
    };

    struct TaskPoint {
        Point p1;
        Point p2;
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
        explicit ADBClient(const std::string&adbPath, const std::string&serial);

        ADBClient(const std::string&adbPath);

        static std::shared_ptr<ADBClient> Create(const std::string&adbPath, const std::string&serial);

        static std::shared_ptr<ADBClient> Create(const std::string&adbPath);

        static std::vector<std::string> Devices(std::string adbPath);

        std::vector<std::string> devices() const;

        std::string shell(const std::string&command) const;

        std::string text(std::string command) const;

        std::string textUTF_8(const std::string&command);

        std::string tap(Point p, float duration) const;

        std::string swipe(Point start, Point end, float duartion = 0) const;

        std::string push(const std::string&source, const std::string&destination) const;

        std::string openActivity(const std::string&packageName, const std::string&activityName) const;

        std::string broadcast(const std::string&action) const;

        std::string pull(const std::string&source, const std::string&destination) const;

        std::string install(const std::string&apk) const;

        std::string printScreen(const std::string&destPath = "assets/screenshot.png") const;

        std::string inputKey(KeyEvent key) const;

        Resolution getResolution() const;

        Resolution getAxisResolution() const;

        int AxisXToScreen(const std::string&hex) const;

        int AxisYToScreen(const std::string&hex) const;

        Point AxisToScreen(const std::pair<std::string, std::string>&hex) const;

        void startRecordingAct();

        std::vector<AndroidEvent> stopRecordingAct();

        void ReplayEvents(const std::vector<AndroidEvent>&events, bool control = true) const;

        void ReplayEvents(const std::string&name, bool control = true);

        bool checkPackage(const std::string&packageName) const;

        void setID(const std::string&serial);

        void loadEvents(const std::string&name, const std::vector<AndroidEvent>&event);

        std::vector<AndroidEvent> getEvents(const std::string&name);

    private:
        static std::vector<std::pair<float, Point>> Lerp(std::pair<float, Point> start, std::pair<float, Point> end,
                                                         int steps = 16);

        static std::vector<std::pair<float, Point>> RemoveDuplicates(const std::vector<std::pair<float, Point>>&);

        static std::vector<std::pair<float, Point>>
        Optimize(std::pair<float, Point> point1, std::pair<float, Point> point2);

        static std::vector<std::pair<float, Point>>
        Optimize(const std::vector<std::pair<float, Point>>&points);

        static float DistanceToLine(const Point&p1, const Point&p2, const Point&p);

        //Liner Correlation Coefficient
        static float Rho(const std::vector<std::pair<float, Point>>&);

        static int HexToDec(std::string hexString) {
            std::string trimmedHexString = hexString;
            std::erase(trimmedHexString, ' ');

            try {
                int decimalValue = std::stoi(trimmedHexString, nullptr, 16);
                return decimalValue;
            }
            catch (const std::invalid_argument&e) {
                throw std::invalid_argument("Invalid hexadecimal string");
            } catch (const std::out_of_range&e) {
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
