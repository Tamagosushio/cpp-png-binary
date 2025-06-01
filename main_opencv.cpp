#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>
#include <iostream>
#include <string>
#include <chrono>

std::chrono::system_clock::time_point start;
int get_time_microsec(void){
  return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - start).count();
}

int main(int argc, char* argv[]){
  int n = 1;
  start = std::chrono::system_clock::now();
  for(int i = 0; i < n; i++){
    cv::Mat img = cv::imread(argv[1], cv::IMREAD_COLOR);
    if(img.empty()) return -1;
    cv::Mat dst = ~img;
    cv::imwrite("../out_reverse_opencv.png", dst);
  }
  std::cout << get_time_microsec() << std::endl;
  return 0;
}

