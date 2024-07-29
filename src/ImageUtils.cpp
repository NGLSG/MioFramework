#include "ImageUtils.h"

#include <backends/imgui_impl_opengl3_loader.h>

#include "utils.h"
using namespace RC;

cv::Mat ImageUtils::PrintScreen(std::shared_ptr<ADBC::ADBClient> adbc) {
    adbc->printScreen();
    if (Utils::File::Exists("assets/screenshot.png")) {
        return cv::imread("assets/screenshot.png");
    }
    std::cout << "Error: Screenshot not found" << std::endl;
    return {};
}

cv::Mat ImageUtils::Binary(cv::Mat src) {
    cv::Mat dst;
    cv::cvtColor(src, dst, cv::COLOR_BGR2GRAY);
    cv::threshold(dst, dst, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
    return dst;
}

ADBC::Point ImageUtils::Find(cv::Mat&src, const cv::Mat&templateImage, const std::string&outputPath) {
    ADBC::Point point;
    point.x = -1;
    point.y = -1;

    cv::Mat result;
    cv::matchTemplate(src, templateImage, result, cv::TM_CCOEFF_NORMED);
    cv::threshold(result, result, 0.9, 1.0, cv::THRESH_TOZERO);

    // 找到非零点
    std::vector<cv::Point> cvPoints;
    cv::findNonZero(result, cvPoints);

    if (!cvPoints.empty()) {
        cv::Point cvPoint = cvPoints[0];
        point.x = cvPoint.x + templateImage.cols / 2; // 计算矩形中心的x坐标
        point.y = cvPoint.y + templateImage.rows / 2; // 计算矩形中心的y坐标

        // 在源图像上绘制匹配点
        cv::rectangle(src, cv::Point(cvPoint.x, cvPoint.y),
                      cv::Point(cvPoint.x + templateImage.cols, cvPoint.y + templateImage.rows),
                      cv::Scalar(0, 255, 0), 2);
    }

    // 保存结果图像到指定路径
    if (!outputPath.empty()) {
        cv::imwrite(outputPath, src);
    }

    return point;
}

GLuint ImageUtils::LoadTexture(const char* filename) {
    int width, height, channels;
    unsigned char* data = stbi_load(filename, &width, &height, &channels, 0);
    if (!data) {
        std::cerr << "Failed to load image: " << filename << std::endl;
        return 0;
    }

    GLuint texture_id;
    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glBindTexture(GL_TEXTURE_2D, 0);

    stbi_image_free(data);
    return texture_id;
}

ADBC::Point ImageUtils::Find(const std::string&srcPath, const std::string&templatePath,
                             const std::string&outputPath) {
    cv::Mat src = cv::imread(srcPath);
    cv::Mat templateImage = cv::imread(templatePath);
    return Find(src, templateImage, outputPath);
}
