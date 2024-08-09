#include "ADBClient.h"

#include <map>
#include <ranges>
#include <regex>
#include <future>
#include <numeric>
#include <unordered_set>
#include <cmath>
#include "../src/ThreadPool.h"

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

    ADBClient::ADBClient(const std::string&adbPath, const std::string&serial): adbPath(adbPath), serial(serial) {
        resolution = getResolution();
        AxisResolution = getAxisResolution();
    }

    ADBClient::ADBClient(const std::string&adbPath) : adbPath(adbPath) {
    }

    std::shared_ptr<ADBClient> ADBClient::Create(const std::string&adbPath, const std::string&serial) {
        return std::make_shared<ADBClient>(adbPath, serial);
    }

    std::shared_ptr<ADBClient> ADBClient::Create(const std::string&adbPath) {
        return std::make_shared<ADBClient>(adbPath);
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

    std::string ADBClient::shell(const std::string&command) const {
#ifndef NDEBUG
        std::cout << "ADBClient::shell: " << command << std::endl;
#endif
        return Execute(adbPath, "-s " + serial + " shell " + command);
    }

    std::string ADBClient::text(std::string command) const {
        return shell("input text " + command);
    }

    std::string ADBClient::textUTF_8(const std::string&command) {
        if (!checkPackage("com.android.adbkeyboard")) {
            std::cout << "Please install ADB Keyboard" << std::endl;
            install("Resources/assets/apk.apk");
        }
        if (shell("settings get secure default_input_method").find("com.android.adbkeyboard/.AdbIME") ==
            std::string::npos) {
            std::cout << "Please enable ADB Keyboard" << std::endl;
            openActivity("com.android.settings", "Settings");
        }
        return broadcast("ADB_INPUT_TEXT --es msg " + command);
    }

    std::string ADBClient::tap(const Point p, float duration) const {
        if (duration > 0) {
            return swipe(p, p, duration);
        }
        return shell("input tap " + std::to_string(p.x) + " " + std::to_string(p.y));
    }

    std::string ADBClient::swipe(const Point start, const Point end, const float duration) const {
        const int durationInMs = static_cast<int>(std::round(duration * 1000));

        return shell(
            "input swipe " +
            std::to_string(start.x) + " " +
            std::to_string(start.y) + " " +
            std::to_string(end.x) + " " +
            std::to_string(end.y) + " " +
            std::to_string(durationInMs)
        );
    }

    std::string ADBClient::push(const std::string&source, const std::string&destination) const {
        return Execute(adbPath, "-s " + serial + " push " + source + " " + destination);
    }

    std::string ADBClient::openActivity(const std::string&packageName, const std::string&activityName) const {
        return shell("am start -n " + packageName + "/." + activityName);
    }

    std::string ADBClient::broadcast(const std::string&action) const {
        return shell("am broadcast -a " + action);
    }

    std::string ADBClient::pull(const std::string&source, const std::string&destination) const {
        return Execute(adbPath, "-s " + serial + " pull " + source + " " + destination);
    }

    std::string ADBClient::install(const std::string&apk) const {
        return Execute(adbPath, "-s " + serial + " install -r " + apk);
    }

    std::string ADBClient::printScreen(const std::string&destPath) const {
        shell("screencap -p /sdcard/Screenshot.png");
        return pull("/sdcard/Screenshot.png", destPath);
    }

    std::string ADBClient::inputKey(const KeyEvent key) const {
        return shell("input keyevent " + std::to_string(key));
    }

    Resolution ADBClient::getResolution() const {
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

    Resolution ADBClient::getAxisResolution() const {
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

    int ADBClient::AxisXToScreen(const std::string&hex) const {
        try {
            const int x = HexToDec(hex);
            return x * resolution.width / AxisResolution.width;
        }
        catch (std::exception&e) {
            std::cerr << "Convertion error " << hex << ":" << e.what() << std::endl;
        }
        return -1;
    }

    int ADBClient::AxisYToScreen(const std::string&hex) const {
        try {
            const int y = HexToDec(hex);
            return y * resolution.width / AxisResolution.width;
        }
        catch (std::exception&e) {
            std::cerr << "Convertion error " << hex << ":" << e.what() << std::endl;
        }
        return -1;
    }

    Point ADBClient::AxisToScreen(const std::pair<std::string, std::string>&hex) const {
        return {static_cast<float>(AxisXToScreen(hex.first)), static_cast<float>(AxisYToScreen(hex.second))};
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

    void ADBClient::ReplayEvents(const std::vector<AndroidEvent>&events, bool control) const {
        for (int i = 0; i < events.size() && control; i++) {
            AndroidEvent event = events[i];

            if (event.type == "swipe") {
                std::map<float, std::vector<TaskPoint>> taskInfo;
                for (int j = 1; j < event.points.size(); j++) {
                    float duration = event.points[j].first - event.points[j - 1].first;
                    duration = std::max(duration, 0.05f);
                    swipe(event.points[j - 1].second, event.points[j].second, duration);

                    //Analog curve plotting using independent point-continuous integratorization
                }
                /*for (auto&tasks: taskInfo | std::views::values) {
                    ThreadPool pool(tasks.size());

                    std::vector<std::future<void>> futures;
                    for (auto&task: tasks) {
                        auto future = std::async(std::launch::async, [this, &task]() {
                            swipe(task.p1, task.p2, task.duration);
                        });
                        futures.push_back(std::move(future));
                    }

                    for (auto&future: futures) {
                        future.wait();
                    }
                }*/
            }
            else if (event.type == "tap") {
                tap(event.points[0].second, event.end - event.start);
            }
            if (i != events.size() - 1) {
                float delay = (events[i + 1].start - event.start) * 1000;
                std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(delay)));
            }
        }
    }

    void ADBClient::ReplayEvents(const std::string&name, bool control) {
        ReplayEvents(getEvents(name), control);
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
        std::vector<float> times;
        std::vector<std::vector<std::pair<float, Point>>> ProcessedPoints;
        std::string lastX, lastY = "";
        while (std::getline(f, line)) {
            if (EventBegin) {
                if (line.find("0003 0039") != std::string::npos) {
                    std::string uid = line.substr(line.find("0003 0039") + std::string("0003 0039 ").length());
                    if (std::regex_search(line, matches, timePattern)) {
                        event.end = std::stof(matches[1]);
                    }
                    for (int i = 0; i < points.size(); i++) {
                        event.points.emplace_back(times[i], AxisToScreen(points[i]));
                    }


                    if (points.size() > 1) {
                        event.type = "swipe";
                        float r = Rho(event.points);
                        if (r < 0.9) {
                            event.points = Optimize(event.points);
                            /*for (int j = 1; j < event.points.size() - 1; j++) {
                                ProcessedPoints.emplace_back(Optimize(event.points[j - 1], event.points[j]));
                            }
                            auto begin = event.points.front().first;
                            event.points.clear();
                            for (auto&p: ProcessedPoints) {
                                for (auto&it: p) {
                                    it.first -= begin;
                                    event.points.emplace_back(it);
                                }
                            }*/
                        }
                        else {
                            event.points.erase(event.points.begin() + 1, event.points.end() - 1);
                        }
                    }
                    else if (points.size() == 1) {
                        event.type = "tap";
                    }
                    else {
                        continue;
                    }
                    if (uid != "ffffffff") {
                        if (uid == event.id)
                            EventBegin = false;
                        else {
                            event.id = uid;

                            if (std::regex_search(line, matches, timePattern)) {
                                event.start = std::stof(matches[1]);
                            }
                        }
                    }
                    else {
                        EventBegin = false;
                    }
                    recordedEvents.push_back(event);
                    event.points.clear();
                    points.clear();
                    times.clear();
                }
                else if (line.find("0003 0035") != std::string::npos) {
                    if (std::regex_search(line, matches, timePattern)) {
                        times.push_back(std::stof(matches[1]));
                    }
                    size_t pos = line.find("0003 0035");
                    lastX = line.substr(pos + std::string("0003 0035 ").length());
                    if (std::getline(f, line)) {
                        if (line.find("0003 0036") != std::string::npos) {
                            size_t pos = line.find("0003 0036");
                            lastY = line.substr(pos + std::string("0003 0036 ").length());
                        }
                    }
                    points.emplace_back(lastX, lastY);
                }
            }
            else {
                if (line.find("0003 0039") != std::string::npos && line.find("ffffffff") == std::string::npos) {
                    EventBegin = true;
                    event.points.clear();
                    points.clear();
                    times.clear();
                    event.id = line.substr(line.find("0003 0039") + std::string("0003 0039 ").length());

                    if (std::regex_search(line, matches, timePattern)) {
                        event.start = std::stof(matches[1]);
                    }
                }
            }
        }
    }

    bool ADBClient::checkPackage(const std::string&packageName) const {
        if (shell("pm list packages").find(packageName) != std::string::npos)
            return true;
        else
            return false;
    }

    void ADBClient::setID(const std::string&serial) {
        this->serial = serial;
        resolution = getResolution();
        AxisResolution = getAxisResolution();
    }

    void ADBClient::loadEvents(const std::string&name, const std::vector<AndroidEvent>&event) {
        events[name] = event;
    }

    std::vector<AndroidEvent> ADBClient::getEvents(const std::string&name) {
        if (events.contains(name))
            return events[name];
        std::cerr << "No events found." << std::endl;
        return {};
    }

    std::vector<std::pair<float, Point>> ADBClient::Lerp(std::pair<float, Point> start, std::pair<float, Point> end,
                                                         int steps) {
        float duration = end.first - start.first;
        std::vector<std::pair<float, Point>> points;
        for (int i = 0; i < steps; i++) {
            float t = static_cast<float>(i) / (steps - 1);
            float x = start.second.x + (end.second.x - start.second.x) * t;
            float y = start.second.y + (end.second.y - start.second.y) * t;
            points.emplace_back(start.first + duration * t, Point{x, y});
        }
        return points;
    }

    std::vector<std::pair<float, Point>>
    ADBClient::RemoveDuplicates(const std::vector<std::pair<float, Point>>&points) {
        std::unordered_set<Point, PointHash> seenPoints;
        std::vector<std::pair<float, Point>> result;

        for (const auto&p: points) {
            if (seenPoints.insert(p.second).second) {
                result.push_back(p);
            }
        }

        return result;
    }


    std::vector<std::pair<float, Point>> ADBClient::Optimize(std::pair<float, Point> point1,
                                                             std::pair<float, Point> point2) {
        std::vector<std::pair<float, Point>> optimizedPoints;
        float deltaX = std::fabs(point1.second.x - point2.second.x);
        float deltaY = std::fabs(point2.second.y - point1.second.y);
        float distance = std::sqrt(
            std::pow(point1.second.x - point2.second.x, 2) + std::pow(point2.second.y - point1.second.y, 2));
        if (distance > 30) {
            optimizedPoints.emplace_back(point1);
            optimizedPoints.emplace_back(point2);
        }
        return optimizedPoints;
        //return {point1, point2};
    }

    std::vector<std::pair<float, Point>> ADBClient::Optimize(const std::vector<std::pair<float, Point>>&points) {
        std::vector<std::pair<float, Point>> optimized;

        if (points.size() < 2) {
            return points;
        }

        optimized.push_back(points[0]);

        for (size_t i = 1; i < points.size() - 1; ++i) {
            if (points[i].second.y >= points[i - 1].second.y && points[i].second.y >= points[i + 1].second.y) {
                optimized.push_back(points[i]);
            }
            else if (points[i].second.y <= points[i - 1].second.y && points[i].second.y <= points[i + 1].second.y) {
                optimized.push_back(points[i]);
            }
        }

        optimized.push_back(points.back());

        return optimized;
    }

    float ADBClient::DistanceToLine(const Point&p1, const Point&p2, const Point&p) {
        float a = p2.y - p1.y;
        float b = p1.x - p2.x;
        float c = -a * p1.x - b * p1.y;
        return std::abs(a * p.x + b * p.y + c) / std::sqrt(a * a + b * b);
    }

    float ADBClient::Rho(const std::vector<std::pair<float, Point>>&points) {
        std::vector<double> Xn, Yn;
        double sumX = 0, sumY = 0, avgX = 0, avgY = 0;
        for (const auto&p: points) {
            Xn.push_back(p.second.x);
            Yn.push_back(p.second.y);
            sumX += p.second.x;
            sumY += p.second.y;
        }
        avgX = sumX / Xn.size();
        avgY = sumY / Yn.size();

        double sumXY = 0, sumX2 = 0, sumY2 = 0;
        for (size_t i = 0; i < Xn.size(); ++i) {
            sumXY += (Xn[i] - avgX) * (Yn[i] - avgY);
            sumX2 += std::pow(Xn[i] - avgX, 2);
            sumY2 += std::pow(Yn[i] - avgY, 2);
        }

        float correlation = sumXY / std::sqrt(sumX2 * sumY2);
        return std::abs(correlation);
    }
}
