# include <iostream>
# include <string>
# include <fstream>
# include <cstdint>
# include <format>
# include <vector>
# include <algorithm>
# include <cassert>

bool equal_stri(const std::string& s1, const std::string& s2){
  std::string t1 = s1;
  std::transform(s1.begin(), s1.end(), t1.begin(),
    [](char c) {return std::toupper(c);}
  );
  std::string t2 = s2;
  std::transform(s2.begin(), s2.end(), t2.begin(),
    [](char c) {return std::toupper(c);}
  );
  return t1==t2;
}

const uint64_t BYTE_LENGTH = 4;
const uint64_t BYTE_TYPE = 4;
const uint64_t BYTE_CRC = 4;
class Chunk{
public:
  uint32_t length = 0;
  uint32_t type = 0;
  std::string type_string = "";
  std::vector<char> data;
  uint32_t crc = 0;
  Chunk(void);
  void initialize(void);
  uint32_t calc_crc(const std::vector<char>& data, size_t start, size_t length);
  uint64_t set(const std::vector<char>& data);
  void debug(bool valid_data = false) const;
};
Chunk::Chunk(void){
  initialize();
}
void Chunk::initialize(void){
  this->length = 0;
  this->type = 0;
  this->type_string = "";
  this->data.clear();
  this->crc = 0;
}
uint32_t Chunk::calc_crc(const std::vector<char>& data, size_t start, size_t length) {
  uint32_t crc = UINT32_C(0xFFFFFFFF);   // 0xFFFFFFFFで初期化
  const uint32_t magic = UINT32_C(0xEDB88320);  // 反転したマジックナンバー
  // データを処理
  for(size_t i = 0; i < length; i++){
    crc ^= static_cast<uint8_t>(data[start + i]);
    for(int j = 0; j < 8; j++){
      if(crc & 1){
        crc = (crc >> 1) ^ magic;
      }else{
        crc >>= 1;
      }
    }
  }
  return ~crc;  // 最終的なビット反転
}
uint64_t Chunk::set(const std::vector<char>& chunk_data){
  // チャンクのデータ長を取得
  for(int i = 0; i < BYTE_LENGTH; i++){
    this->length = (this->length << 8) | static_cast<uint8_t>(chunk_data[i]);
  }
  // チャンク名を取得
  for(int i = 0; i < BYTE_TYPE; i++){
    this->type = (this->type << 8) | static_cast<uint8_t>(chunk_data[BYTE_LENGTH+i]);
    type_string += chunk_data[BYTE_LENGTH+i];
  }
  // チャンクのデータを取得
  this->data.resize(this->length);
  std::copy(chunk_data.begin()+BYTE_LENGTH+BYTE_TYPE, chunk_data.begin()+BYTE_LENGTH+BYTE_TYPE+this->length, this->data.begin());
  // CRCディジットチェック
  for(int i = 0; i < BYTE_CRC; i++){
    this->crc = (this->crc << 8) | static_cast<uint8_t>(chunk_data[BYTE_LENGTH + BYTE_TYPE + this->length + i]);
  }
  assert(this->crc == calc_crc(chunk_data, BYTE_LENGTH, BYTE_TYPE+this->length));
  return BYTE_LENGTH + BYTE_TYPE + this->length + BYTE_CRC;
}
void Chunk::debug(bool valid_data) const{
  std::cout << std::format("length: {:08X}", this->length) << std::endl;
  std::cout << std::format("type  : {:08X} = {}", this->type, this->type_string) << std::endl;
  std::cout << std::format("crc   : {:08X}", this->crc) << std::endl;
  if(valid_data){
    std::cout << std::format("data  :");
    for(int i = 0; i < this->length; i++){
      std::cout << std::format(" {:02X}", this->data[i]);
      if(i%16 == 15) std::cout << std::endl << "       ";
    }
    std::cout << std::endl;
  }
}

class PNG{
private:
  uint64_t size;
  std::vector<char> data;
  std::vector<Chunk> chunks;
public:
  PNG(const std::string& path);
  void degug(void) const;
};
PNG::PNG(const std::string& path){
  // バイナリ読み込み
  std::ifstream ifs(path, std::ios::in | std::ios::binary);
  // ファイルサイズの取得
  ifs.seekg(0, std::ios::end);
  this->size = ifs.tellg();
  ifs.seekg(0);
  // 読み込んだデータをvector<char>型に変換
  this->data.resize(this->size);
  ifs.read(this->data.data(), this->size);
  // チャンクデータを全読み込み
  uint64_t binary_idx = 8;
  Chunk chunk;
  do{
    // チャンクのデータ長のみ先に取得
    uint32_t chunk_length = 0;
    for (int i = 0; i < BYTE_LENGTH; ++i) {
      chunk_length = (chunk_length << 8) | static_cast<uint8_t>(this->data[binary_idx + i]);
    }
    uint64_t chunk_total_size = BYTE_LENGTH + BYTE_TYPE + chunk_length + BYTE_CRC;
    // チャンク生成
    std::vector<char> chunk_data(chunk_total_size);
    std::copy(this->data.begin() + binary_idx, this->data.begin() + binary_idx + chunk_total_size, chunk_data.begin());
    chunk.initialize();
    binary_idx += chunk.set(chunk_data);
    chunks.push_back(chunk);
    chunk.debug();
  }while(not equal_stri(chunk.type_string, "IEND"));
}

int main(){
  PNG("./new.png");
  return 0;
}

