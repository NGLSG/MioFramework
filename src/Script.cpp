#include "Script.h"

#include "LoadManager.h"
#include "../MUI/ResourceManager.h"

Script::Script() {
    scriptPath = "";
}

bool Script::Initialize(std::string scriptsPath) {
    if (!RC::Utils::Directory::Exists(scriptsPath)) {
        std::cerr << "Scripts directory does not exist!" << std::endl;
        return false;
    }
    this->scriptPath = scriptsPath;
    binding();
    lua.open_libraries(sol::lib::base, sol::lib::io, sol::lib::math, sol::lib::os, sol::lib::string,
                       sol::lib::table, sol::lib::package, sol::lib::debug, sol::lib::count, sol::lib::coroutine);
    buildPackagePath(scriptsPath);
    if (!loadScript()) {
        return false;
    }
    return true;
}

sol::protected_function Script::getFunction(const std::string&funcName) {
    if (checkFunc(funcName)) {
        sol::protected_function func = lua[funcName];
        if (func.valid()) {
            return func;
        }
        std::cerr << "Function can not get: " << funcName << std::endl;
        return nullptr;
    }
    std::cerr << "Function not found: " << funcName << std::endl;
    return nullptr;
}


bool Script::checkFunc(const std::string&funcName) {
    for (const auto&pair: lua.globals()) {
        if (pair.second.is<sol::function>() && pair.first.as<std::string>() == funcName) {
            return true;
        }
    }
    return false;
}

void Script::PrintAllFunctions() {
    for (const auto&pair: lua.globals()) {
        if (pair.second.is<sol::function>()) {
            std::cout << pair.first.as<std::string>() << std::endl;
        }
    }
}

void Script::binding() {
    auto Directory = lua.create_table("Directory");
    Directory.set_function("IsDirectory", &RC::Utils::Directory::IsDirectory);
    Directory.set_function("Exists", &RC::Utils::Directory::Exists);
    Directory.set_function("Create", &RC::Utils::Directory::Create);
    Directory.set_function("List", &RC::Utils::Directory::List);
    Directory.set_function("ListAll", &RC::Utils::Directory::ListAll);
    Directory.set_function("Delete", &RC::Utils::Directory::Remove);
    Directory.set_function("Move", &RC::Utils::Directory::Move);
    Directory.set_function("Copy", &RC::Utils::Directory::Copy);
    Directory.set_function("SizeOf", &RC::Utils::Directory::SizeOf);
    lua.set("Directory", Directory);
    auto File = lua.create_table("File");
    File.set_function("Exists", &RC::Utils::File::Exists);
    File.set_function("FileExtension", &RC::Utils::File::FileExtension);
    File.set_function("FileSize", &RC::Utils::File::FileSize);
    File.set_function("Move", &RC::Utils::File::Move);
    File.set_function("Copy", &RC::Utils::File::Copy);
    File.set_function("Delete", &RC::Utils::File::Remove);
    File.set_function("AbsolutePath", &RC::Utils::File::AbsolutePath);
    File.set_function("GetCurrentDirectory", &RC::Utils::File::GetCurrentDirectoryA);
    File.set_function("PlatformPath", &RC::Utils::File::PlatformPath);
    File.set_function("GetParentDirectory", &RC::Utils::File::GetParentDirectory);
    lua.set("File", File);
    lua.new_usertype<GeneralHeader>("GeneralHeader",
                                    "CacheControl", &GeneralHeader::CacheControl,
                                    "Connection", &GeneralHeader::Connection,
                                    "Date", &GeneralHeader::Date
    );

    lua.new_usertype<RequestHeader>("RequestHeader",
                                    sol::constructors<RequestHeader()>(),
                                    "ContentType", &RequestHeader::ContentType,
                                    "UserAgent", &RequestHeader::UserAgent,
                                    "Authorization", &RequestHeader::Authorization,
                                    "Accept", &RequestHeader::Accept,
                                    "Host", &RequestHeader::Host,
                                    "Referer", &RequestHeader::Referer,
                                    sol::base_classes, sol::bases<GeneralHeader>()
    );

    lua.new_usertype<ResponseHeader>("ResponseHeader",
                                     sol::constructors<ResponseHeader()>(),
                                     "ContentType", &ResponseHeader::ContentType,
                                     "ContentLength", &ResponseHeader::ContentLength,
                                     "Server", &ResponseHeader::Server,
                                     "TransferEncoding", &ResponseHeader::TransferEncoding,
                                     "Connection", &ResponseHeader::Connection,
                                     "Date", &ResponseHeader::Date,
                                     "Expires", &ResponseHeader::Expires,
                                     "LastModified", &ResponseHeader::LastModified,
                                     "Pragma", &ResponseHeader::Pragma,
                                     "SetCookie", &ResponseHeader::SetCookie,
                                     "XPoweredBy", &ResponseHeader::XPoweredBy,
                                     "XRequestId", &ResponseHeader::XRequestId,
                                     sol::base_classes, sol::bases<GeneralHeader>()
    );

    lua.new_usertype<EntityHeader>("EntityHeader",
                                   sol::constructors<EntityHeader()>(),
                                   "ContentType", &EntityHeader::ContentType,
                                   "ContentLength", &EntityHeader::ContentLength,
                                   "ContentEncoding", &EntityHeader::ContentEncoding
    );
    auto Eurl = lua.create_table("eurl");
    Eurl.set_function("Get", [](const char* url,
                                const sol::optional<std::string>&proxy) -> std::string {
        std::string response;
        bool result = eurl::Get(url, response, proxy.value_or("").c_str());
        return response;
    });
    Eurl.set_function("Post", [](const char* url, std::string data, RequestHeader header,
                                 sol::optional<WriteCallback> callback,
                                 const sol::optional<std::string>&proxy) -> nlohmann::json {
        json response;
        bool result = eurl::Post(url, data, response, header, callback.value_or(nullptr),
                                 proxy.value_or("").c_str());

        return response;
    });
    Eurl.set_function("Download", [](const char* url, const char* savePath, sol::this_state s,
                                     sol::optional<std::string> proxy) -> sol::object {
        bool result = eurl::Download(url, savePath, proxy.value_or("").c_str());
        if (result) {
            return sol::make_object(s, true);
        }
        else {
            return sol::nil;
        }
    });
    Eurl.set_function("MultiThreadedDownload", [](const std::string&url, const std::string&savePath,
                                                  sol::optional<size_t> numThreads) {
        eurl::MultiThreadedDownload(url, savePath, numThreads.value_or(16));
    });
    lua.set("eurl", Eurl);
    auto Compression = lua.create_table("Compression");
    Compression.set_function(
        "List", [](const std::string&file, sol::optional<const RC::Type> type, sol::optional<std::string> pwd) {
            return RC::Compression::List(file, type.value_or(RC::Count), pwd.value_or(""));
        });

    Compression.set_function(
        "Extract", [](const std::string&file, const std::string&dest, sol::optional<const RC::Type> type,
                      sol::optional<std::string> pwd) {
            return RC::Compression::Extract(file, dest, type.value_or(RC::Count), pwd.value_or(""));
        });

    Compression.set_function("ExtractSelectedFile", [](const std::string&filePath, const std::string&selectedFile,
                                                       const std::string&outputPath, sol::optional<const RC::Type> type,
                                                       sol::optional<std::string> pwd) {
        return RC::Compression::ExtractSelectedFile(filePath, selectedFile, outputPath, type.value_or(RC::Count),
                                                    pwd.value_or(""));
    });

    Compression.set_function("Compress", [](const std::string&filePath, const std::string&outputPath,
                                            sol::optional<const RC::Type> type,
                                            sol::optional<int> level,
                                            sol::optional<std::string> password, sol::optional<long> split) {
        return RC::Compression::Compress(filePath, outputPath, type.value_or(RC::Count), level.value_or(1),
                                         password.value_or(""),
                                         split.value_or(1));
    });

    Compression.set_function("MergeParts",
                             [](const std::string&outputZipPath, sol::optional<std::string> destPath,
                                sol::optional<const RC::Type> type,
                                sol::optional<std::string> pwd) {
                                 return RC::Compression::MergeParts(outputZipPath, destPath.value_or(""),
                                                                    type.value_or(RC::Count),
                                                                    pwd.value_or(""));
                             });
    lua.set("Compression", Compression);
    lua.new_usertype<cv::Mat>("Mat",
                              sol::constructors<cv::Mat(), cv::Mat(int, int, int)>(),
                              "rows", &cv::Mat::rows,
                              "cols", &cv::Mat::cols,
                              "channels", [](const cv::Mat&mat) { return mat.channels(); },
                              "empty", &cv::Mat::empty,
                              "clone", &cv::Mat::clone,
                              "release", &cv::Mat::release,
                              "save", [](const cv::Mat&mat, const std::string&filename) {
                                  return cv::imwrite(filename, mat);
                              },
                              "load", [](const std::string&filename) {
                                  return cv::imread(filename);
                              },
                              "display", [](const cv::Mat&mat) {
                                  cv::imshow("Display", mat);
                                  cv::waitKey(0);
                              }
    );
    auto IU = lua.create_table("ImageUtils");
    IU.set_function("Binary", [](const cv::Mat&src) {
        return ImageUtils::Binary(src);
    });
    IU.set_function("FindFromStr", [](const std::string&srcPath, const std::string&templatePath,
                                      sol::optional<float> thresh) {
        return ImageUtils::Find(srcPath, templatePath, thresh.value_or(0.5f));
    });
    IU.set_function("FindFromMat", [](cv::Mat&src, const cv::Mat&templateImage,
                                      sol::optional<float> thresh) {
        return ImageUtils::Find(src, templateImage, thresh.value_or(0.5f));
    });
    IU.set_function("PrintScreen", [](std::shared_ptr<ADBC::ADBClient> adbc) {
        return ImageUtils::PrintScreen(adbc);
    });

    IU.set_function("MatchFromStr", [](const std::string&srcPath, const std::string&templatePath,
                                       sol::optional<std::string> outputPath) {
        return ImageUtils::Match(srcPath, templatePath, outputPath.value_or("assets/tmp.png"));
    });
    IU.set_function("MatchFromMat", [](cv::Mat&src, const cv::Mat&templateImage,
                                       sol::optional<std::string> outputPath) {
        return ImageUtils::Match(src, templateImage, outputPath.value_or("assets/tmp.png"));
    });
    IU.set_function("Image", [](std::string path) {
        return ImageUtils::Image(path);
    });
    lua.set("ImageUtils", IU);
    lua.new_usertype<ADBC::Point>("Point",
                                  sol::constructors<ADBC::Point(float, float)>(),
                                  "x", &ADBC::Point::x,
                                  "y", &ADBC::Point::y
    );
    lua.new_usertype<ADBC::AndroidEvent>("AndroidEvent",
                                         "type", &ADBC::AndroidEvent::type,
                                         "start", &ADBC::AndroidEvent::start,
                                         "end", &ADBC::AndroidEvent::end
    );

    lua.new_usertype<ADBC::ADBClient>("ADBClient",
                                      sol::constructors<ADBC::ADBClient(std::string, std::string)>(),
                                      "broadcast", &ADBC::ADBClient::broadcast,
                                      "checkPackage", &ADBC::ADBClient::checkPackage,
                                      "devices", &ADBC::ADBClient::devices,
                                      "install", &ADBC::ADBClient::install,
                                      "openActivity", &ADBC::ADBClient::openActivity,
                                      "printScreen", &ADBC::ADBClient::printScreen,
                                      "pull", &ADBC::ADBClient::pull,
                                      "push", &ADBC::ADBClient::push,
                                      "setID", &ADBC::ADBClient::setID,
                                      "shell", &ADBC::ADBClient::shell,
                                      "swipe", &ADBC::ADBClient::swipe,
                                      "tap", [](ADBC::ADBClient&client, ADBC::Point p, sol::optional<float> duration) {
                                          return client.tap(p, duration.value_or(0));
                                      },
                                      "text", &ADBC::ADBClient::text,
                                      "textUTF_8", &ADBC::ADBClient::textUTF_8,
                                      "inputKey", &ADBC::ADBClient::inputKey,
                                      "startRecordingAct", &ADBC::ADBClient::startRecordingAct,
                                      "stopRecordingAct", &ADBC::ADBClient::stopRecordingAct,
                                      "Create", [](std::string adbPath, std::string serial) {
                                          return ADBC::ADBClient::Create(adbPath, serial);
                                      },
                                      "getEvents", &ADBC::ADBClient::getEvents,
                                      "getResolution", &ADBC::ADBClient::getResolution,
                                      "getAxisResolution", &ADBC::ADBClient::getAxisResolution,
                                      "AxisToScreen", &ADBC::ADBClient::AxisToScreen,
                                      "AxisXToScreen", &ADBC::ADBClient::AxisXToScreen,
                                      "AxisYToScreen", &ADBC::ADBClient::AxisYToScreen,
                                      "loadEvents", &ADBC::ADBClient::loadEvents,
                                      "ReplayEvents", sol::overload(
                                          [](ADBC::ADBClient&client, std::vector<ADBC::AndroidEvent> events,
                                             sol::optional<bool> control) {
                                              client.ReplayEvents(events, control.value_or(true));
                                          },
                                          [](ADBC::ADBClient&client, std::string name, sol::optional<bool> control) {
                                              client.ReplayEvents(name, control.value_or(true));
                                          }
                                      )
    );

    lua.set_function("Save", &LoadManager::Save<std::vector<ADBC::AndroidEvent>>);
    lua.set_function("Load", &LoadManager::Load<std::vector<ADBC::AndroidEvent>>);
    lua.set_function("Sleep", &RC::Utils::sleep);
}

bool Script::loadScript() {
    auto script = scriptPath + "/main.lua";
    std::ifstream scriptFile(script);
    if (!scriptFile.is_open()) {
        std::cerr << "Unable to open the script file: " << script << std::endl;
        return false;
    }
    std::stringstream buffer;
    buffer << scriptFile.rdbuf();
    scriptFile.close();
    result = lua.script(buffer.str(), sol::script_default_on_error);
    if (!result.valid()) {
        sol::error err = result;
        std::cerr << "Error loading script: " << err.what() << std::endl;
        return false;
    }

    return true;
}

void Script::buildPackagePath(const std::string&rootPath) {
    std::string package_path = lua["package"]["path"].get<std::string>();

    std::string rootLuaPath = RC::Utils::File::AbsolutePath(RC::Utils::File::PlatformPath(rootPath + "/?.lua"));
    package_path += ";" + rootLuaPath;

    for (const auto&entry: std::filesystem::recursive_directory_iterator(rootPath)) {
        if (entry.is_directory()) {
            std::string dirPath = entry.path().string();
            std::string luaPath = dirPath + "/?.lua";
            package_path += ";" + RC::Utils::File::AbsolutePath(RC::Utils::File::PlatformPath(luaPath));
        }
    }
#ifndef NDEBUG
    std::cout << "package.path: " << package_path << std::endl;
#endif

    lua["package"]["path"] = package_path;
}
