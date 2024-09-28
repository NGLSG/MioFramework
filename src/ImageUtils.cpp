#include "ImageUtils.h"

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

cv::Mat ImageUtils::Image(const std::string&srcPath) {
    return cv::imread(srcPath);
}

ADBC::Point ImageUtils::Find(cv::Mat&src, const cv::Mat&templateImage, const std::string&outputPath) {
    if (src.empty() || templateImage.empty()) {
        std::cerr << "Could not open or find the image" << std::endl;
        return {-1, -1};
    }
    ADBC::Point point = {-1, -1};

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

        cv::rectangle(src, cv::Point(cvPoint.x, cvPoint.y),
                      cv::Point(cvPoint.x + templateImage.cols, cvPoint.y + templateImage.rows),
                      cv::Scalar(0, 255, 0), 2);
    }

    if (!outputPath.empty()) {
        cv::imwrite(outputPath, src);
    }

    return point;
}

ADBC::Point ImageUtils::Find(const std::string&srcPath, const std::string&templatePath,
                             const std::string&outputPath) {
    cv::Mat src = cv::imread(srcPath);
    cv::Mat templateImage = cv::imread(templatePath);
    return Find(src, templateImage, outputPath);
}

ADBC::Point ImageUtils::Match(cv::Mat&src, const cv::Mat&templateImage, const std::string&outputPath) {
    if (src.empty() || templateImage.empty()) {
        std::cerr << "Could not open or find the image" << std::endl;
        return {-1, -1};
    }
    ADBC::Point point = {-1, -1};
    cv::Ptr<cv::ORB> detector = cv::ORB::create();
    std::vector<cv::KeyPoint> keypoints1, keypoints2;
    detector->detect(src, keypoints1);
    detector->detect(templateImage, keypoints2);

    cv::Mat descriptors1, descriptors2;
    detector->compute(src, keypoints1, descriptors1);
    detector->compute(templateImage, keypoints2, descriptors2);

    cv::BFMatcher matcher(cv::NORM_HAMMING);
    std::vector<cv::DMatch> matches;
    matcher.match(descriptors1, descriptors2, matches);

    int N = min(500, static_cast<int>(matches.size()));
    std::vector<cv::DMatch> topNMatches(matches.begin(), matches.begin() + N);
    cv::Mat imgMatches;
    cv::drawMatches(src, keypoints1, templateImage, keypoints2, topNMatches, imgMatches);

    float centerX = 0.0f;
    float centerY = 0.0f;
    for (int i = 0; i < N; i++) {
        cv::Point2f pt1 = keypoints1[matches[i].queryIdx].pt;
        cv::Point2f pt2 = keypoints2[matches[i].trainIdx].pt;
        centerX += (pt1.x + pt2.x) / 2.0f;
        centerY += (pt1.y + pt2.y) / 2.0f;
    }
    centerX /= static_cast<float>(N);
    centerY /= static_cast<float>(N);
    point = {centerX, centerY};
    return point;
}

ADBC::Point ImageUtils::Match(const std::string& srcPath, const std::string& templatePath,
    const std::string& outputPath) {
    cv::Mat src = cv::imread(srcPath);
    cv::Mat templateImage = cv::imread(templatePath);
    return Match(src, templateImage, outputPath);
}
