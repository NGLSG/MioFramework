#ifndef LOADMANAGER_H
#define LOADMANAGER_H
#include <string>
#include <yaml-cpp/yaml.h>

#include "ADBClient.h"

namespace YAML {
    template<>
    struct convert<ADBC::AndroidEvent> {
        static Node encode(const ADBC::AndroidEvent&event) {
            Node node;
            node["type"] = event.type;
            node["points"] = event.points;
            node["start"] = event.start;
            node["end"] = event.end;
            node["duration"] = event.end - event.start;
            return node;
        }

        static bool decode(const Node&node, ADBC::AndroidEvent&event) {
            event.type = node["type"].as<std::string>();
            event.points = node["points"].as<std::vector<ADBC::Point>>();
            event.start = node["start"].as<float>();
            event.end = node["end"].as<float>();
            event.duration = event.end - event.start;
            return true;
        }
    };

    template<>
    struct convert<ADBC::Point> {
        static Node encode(const ADBC::Point&point) {
            Node node;
            node["x"] = point.x;
            node["y"] = point.y;
            return node;
        }

        static bool decode(const Node&node, ADBC::Point&point) {
            point.x = node["x"].as<float>();
            point.y = node["y"].as<float>();
            return true;
        }
    };

    template<>
    struct convert<std::vector<ADBC::AndroidEvent>> {
        static Node encode(const std::vector<ADBC::AndroidEvent>&events) {
            Node node;
            for (const auto&event: events) {
                node.push_back(event);
            }
            return node;
        }

        static bool decode(const Node&node, std::vector<ADBC::AndroidEvent>&events) {
            for (const auto&event: node) {
                events.push_back(event.as<ADBC::AndroidEvent>());
            }
            return true;
        }
    };
}

class LoadManager {
public:
    template<typename T>
    static bool Save(const T&data, const std::string&path) {
        std::ofstream file(path);
        if (!file.is_open()) {
            return false;
        }
        file << YAML::convert<T>::encode(data);
        file.close();
        return true;
    }

    template<typename T>
    static T Load(const std::string&path) {
        std::ifstream file(path);
        if (!file.is_open()) {
            throw std::runtime_error("Unable to open file for loading");
        }
        YAML::Node node = YAML::Load(file);
        file.close();
        return node.as<T>();
    }
};

#endif //LOADMANAGER_H
