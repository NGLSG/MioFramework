#ifndef IMAGEUTILS_H
#define IMAGEUTILS_H

#include <fstream>
#include <opencv2/opencv.hpp>
#include <iostream>
#include <nlohmann/json.hpp>

#include "stb_image.h"
#include "../ADBClient/ADBClient.h"


class ImageUtils {
public:
    static cv::Mat PrintScreen(std::shared_ptr<ADBC::ADBClient> adbc);

    static cv::Mat Binary(cv::Mat src);

    static ADBC::Point Find(cv::Mat&src, const cv::Mat&templateImage,
                            const std::string&outputPath = "assets/tmp.png");

    static uint32_t LoadTexture(const char* filename);


    static ADBC::Point Find(const std::string&srcPath, const std::string&templatePath,
                            const std::string&outputPath = "assets/tmp.png");
};


#endif //IMAGEUTILS_H
