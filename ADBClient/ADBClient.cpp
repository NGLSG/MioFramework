#include "ADBClient.h"

#include <map>
#include <regex>

namespace ADBC {
    std::string Execute(std::string executable, std::string args) {
        std::string cmd = executable + " " + args;
        std::string output;
        FILE* pipe = _popen(cmd.c_str(), "r");
        if (!pipe) throw std::runtime_error("popen() failed!");
        try {
            char buffer[128];
            while (!feof(pipe)) {
                if (fgets(buffer, 128, pipe) != NULL)
                    output += buffer;
            }
        }
        catch (...) {
            _pclose(pipe);
            throw;
        }
        _pclose(pipe);
        return output;
    }

    ADBClient::ADBClient(std::string adbPath, std::string serial): adbPath(adbPath), serial(serial) {
        resolution = getResolution();
        AxisResolution = getAxisResolution();
    }

    std::shared_ptr<ADBClient> ADBClient::Create(std::string adbPath, std::string serial) {
        return std::make_shared<ADBClient>(adbPath, serial);
    }

    std::vector<std::string> ADBClient::Devices(std::string adbPath) {
        auto output = Execute(adbPath, "devices");
        std::stringstream ss(output);
        std::vector<std::string> devices;
        for (std::string line; std::getline(ss, line);) {
            if (line.find("device") != std::string::npos && line.find("unauthorized") == std::string::npos && line.
                find("devices") == std::string::npos) {
                std::stringstream ss(line);
                std::string token;
                std::getline(ss, token, '\t');
                devices.push_back(token);
            }
        }
        return devices;
    }

    std::vector<std::string> ADBClient::devices() const {
        return Devices(adbPath);
    }

    std::string ADBClient::shell(std::string command) const {
        return Execute(adbPath, "-s " + serial + " shell " + command);
    }

    std::string ADBClient::text(std::string command) {
        return shell("input text " + command);
    }

    std::string ADBClient::textUTF_8(std::string command) {
        if (!checkPackage("com.android.adbkeyboard")) {
            std::cout << "Please install ADB Keyboard" << std::endl;
            install("assets/apk.apk");
        }
        if (shell("settings get secure default_input_method").find("com.android.adbkeyboard/.AdbIME") ==
            std::string::npos) {
            std::cout << "Please enable ADB Keyboard" << std::endl;
            openActivity("com.android.settings", "Settings");
        }
        return broadcast("ADB_INPUT_TEXT --es msg " + command);
    }

    std::string ADBClient::tap(Point p) {
        return shell("input tap " + std::to_string(p.x) + " " + std::to_string(p.y));
    }

    std::string ADBClient::swipe(Point start, Point end, float duration) {
        // 使用 std::round 对坐标进行四舍五入
        int roundStartX = std::round(start.x);
        int roundStartY = std::round(start.y);
        int roundEndX = std::round(end.x);
        int roundEndY = std::round(end.y);
        int durationInMs = std::round(duration * 1000);

        return shell(
            "input swipe " +
            std::to_string(roundStartX) + " " +
            std::to_string(roundStartY) + " " +
            std::to_string(roundEndX) + " " +
            std::to_string(roundEndY) + " " +
            std::to_string(durationInMs)
        );
    }

    std::string ADBClient::push(std::string source, std::string destination) {
        return Execute(adbPath, "-s " + serial + " push " + source + " " + destination);
    }

    std::string ADBClient::openActivity(std::string packageName, std::string activityName) {
        return shell("am start -n " + packageName + "/." + activityName);
    }

    std::string ADBClient::broadcast(std::string action) {
        return shell("am broadcast -a " + action);
    }

    std::string ADBClient::pull(std::string source, std::string destination) {
        return Execute(adbPath, "-s " + serial + " pull " + source + " " + destination);
    }

    std::string ADBClient::install(std::string apk) {
        return Execute(adbPath, "-s " + serial + " install -r " + apk);
    }

    std::string ADBClient::printScreen(const std::string&destPath) {
        shell("screencap -p /sdcard/Screenshot.png");
        return pull("/sdcard/Screenshot.png", destPath);
    }

    std::string ADBClient::inputKey(KeyEvent key) {
        return shell("input keyevent " + std::to_string(key));
    }

    Resolution ADBClient::getResolution() {
        std::string output = shell("wm size");
        std::regex resolutionPattern(R"((\d+)x(\d+))");
        std::smatch match;

        if (std::regex_search(output, match, resolutionPattern) && match.size() == 3) {
            int width = std::stoi(match.str(1));
            int height = std::stoi(match.str(2));
            return {width, height};
        }
        else {
            throw std::runtime_error("Failed to extract resolution");
        }
    }

    Resolution ADBClient::getAxisResolution() {
        std::string output = shell("getevent -lp");
        Resolution tmp = {0, 0};
        std::istringstream f(output);
        std::string line;

        while (std::getline(f, line)) {
            if (line.find("ABS_MT_POSITION_X") != std::string::npos) {
                size_t pos = line.find("max");
                if (pos != std::string::npos) {
                    size_t endPos = line.find(",", pos);
                    if (endPos != std::string::npos) {
                        int maxValue = std::stoi(line.substr(pos + 4, endPos - pos - 4));
                        tmp.width = std::max(tmp.width, maxValue);
                    }
                }
            }
            else if (line.find("ABS_MT_POSITION_Y") != std::string::npos) {
                size_t pos = line.find("max");
                if (pos != std::string::npos) {
                    size_t endPos = line.find(",", pos);
                    if (endPos != std::string::npos) {
                        int maxValue = std::stoi(line.substr(pos + 4, endPos - pos - 4));
                        tmp.height = std::max(tmp.height, maxValue);
                    }
                }
            }
        }
        return tmp;
    }

    float ADBClient::AxisXToScreen(std::string hex) {
        int x = HexToDec(hex);
        return static_cast<float>(x) / static_cast<float>(getAxisResolution().width) * static_cast<float>(
                   getResolution().width);
    }

    float ADBClient::AxisYToScreen(std::string hex) {
        int y = HexToDec(hex);
        return static_cast<float>(y) / static_cast<float>(getAxisResolution().height) * static_cast<float>(
                   getResolution().height);
    }

    Point ADBClient::AxisToScreen(std::pair<std::string, std::string> hex) {
        return {AxisXToScreen(hex.first), AxisYToScreen(hex.second)};
    }

    void ADBClient::startRecordingAct() {
        if (recording) {
            throw std::runtime_error("Recording is already in progress.");
        }
        std::cout << "Start recording" << std::endl;
        recording = true;
        recordingThread = std::thread(&ADBClient::recordAct, this);
    }

    std::vector<AndroidEvent> ADBClient::stopRecordingAct() {
        if (!recording) {
            throw std::runtime_error("No recording is in progress.");
        }
        recording = false;
        shell("pkill getevent");
        std::cout << "Stop recording" << std::endl;
        if (recordingThread.joinable()) {
            recordingThread.join();
        }
        return recordedEvents;
    }

    void ADBClient::ReplayEvents(std::vector<AndroidEvent> events) {
        for (int i = 0; i < events.size(); i++) {
            AndroidEvent event = events[i];
            if (event.type == "swipe") {
                auto time = event.end - event.start;
                swipe(event.points[0], event.points[1], time);
            }
            else if (event.type == "tap") {
                tap(event.points[0]);
            }
            if (i != events.size() - 1) {
                float delay = (events[i + 1].start - event.start) * 1000;
                std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(delay)));
            }
        }
    }

    void ADBClient::ReplayEvents(std::string name) {
        ReplayEvents(getEvents(name));
    }

    void ADBClient::recordAct() {
        std::string output = shell("getevent -t");
        std::istringstream f(output);
        std::string line;
        std::vector<std::pair<std::string, std::string>> points;
        bool EventBegin = false;
        std::regex timePattern(R"(\[\s*(\d+\.\d+)\s*\])");
        AndroidEvent event;
        std::smatch matches;
        while (std::getline(f, line)) {
            if (line.find("0001 014a 00000001") != std::string::npos) {
                EventBegin = true;
                event.points.clear();
                points.clear();

                if (std::regex_search(line, matches, timePattern)) {
                    event.start = std::stof(matches[1]);
                }
            }
            else if (line.find("0001 014a 00000000") != std::string::npos) {
                EventBegin = false;
                Point first = AxisToScreen(points[0]);
                if (std::regex_search(line, matches, timePattern)) {
                    event.end = std::stof(matches[1]);
                }
                if (points.size() > 1) {
                    Point last = AxisToScreen(points.back());
                    event.points = {first, last};
                    event.type = "swipe";
                }
                else {
                    event.points.push_back(first);
                    event.type = "tap";
                }
                recordedEvents.push_back(event);
            }
            if (EventBegin) {
                if (line.find("0003 0035") != std::string::npos) {
                    size_t pos = line.find("0003 0035");
                    auto str = line.substr(pos + std::string("0003 0035").length());
                    points.emplace_back(str, "");
                }
                else if (line.find("0003 0036") != std::string::npos) {
                    size_t pos = line.find("0003 0036");
                    auto str = line.substr(pos + std::string("0003 0036").length());
                    points.back().second = str;
                }
            }
        }
    }

    bool ADBClient::checkPackage(std::string packageName) {
        if (shell("pm list packages").find(packageName) != std::string::npos)
            return true;
        else
            return false;
    }

    void ADBClient::setID(std::string serial) {
        this->serial = serial;
        resolution = getResolution();
    }

    void ADBClient::loadEvents(std::string name, std::vector<AndroidEvent> event) {
        events[name] = event;
    }

    std::vector<AndroidEvent> ADBClient::getEvents(std::string name) {
        if (events.find(name) != events.end())
            return events[name];
        std::cerr << "No events found." << std::endl;
        return {};
    }
}
