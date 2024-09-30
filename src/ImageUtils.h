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

    static cv::Mat Image(const std::string&srcPath);

    static ADBC::Point Find(cv::Mat&src, const cv::Mat&templateImage,float thresh = 0.5f);

    static ADBC::Point Find(const std::string&srcPath, const std::string&templatePath,float thresh = 0.5f);

    static ADBC::Point Match(cv::Mat&src, const cv::Mat&templateImage, const std::string&outputPath = "assets/tmp.png");

    static ADBC::Point Match(const std::string&srcPath, const std::string&templatePath, const std::string&outputPath = "assets/tmp.png");
};


#endif //IMAGEUTILS_H
