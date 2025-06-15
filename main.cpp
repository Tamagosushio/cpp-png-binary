# include "chunk.hpp"
# include "png.hpp"
# include <chrono>

std::chrono::system_clock::time_point start;
int get_time_microsec(void){
  return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - start).count();
}

int main(int argc, char* argv[]){
  int n = 1;
  start = std::chrono::system_clock::now();
  for(int i = 0; i < n; i++){
    png::PNG png{argv[1]};
    // png.debug();
    png.collapse(10);
    // png.reverse_color();
    // png.resize_data(1.50, 2.00);
    png.write("./out_collapsed.png");
  }
  std::cout << get_time_microsec() << std::endl;
  // std::cout << "-------------------------------------------------------------------------" << std::endl;
  // png::PNG png{"./out_resized.png"};
  // png.debug();
  return 0;
}

