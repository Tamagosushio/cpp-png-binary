# include <iostream>
# include <string>
# include <fstream>
# include <cstdint>
# include <format>
# include <vector>
# include <algorithm>
# include <cassert>
# include <zlib.h>

// 大文字小文字区別しない文字列比較
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

// チャンクデータのインターフェイス
class ChunkDataInterface{
public:
  ChunkDataInterface(void) = default;
  virtual void clear(void) = 0; // データのクリア
  virtual void set(const uint32_t length, const std::vector<char>& data) = 0; // データセット
  virtual void debug(void) const = 0; // デバッグ用標準出力
};

class IHDR : public ChunkDataInterface{
public:
  uint32_t image_width, image_height;
  uint8_t bit_depth, color_type, compression_method, filter_method, interlace_method;
  using ChunkDataInterface::ChunkDataInterface;
  IHDR(const uint32_t length, const std::vector<char>& data){
    set(length, data);
  }
  void set(const uint32_t length, const std::vector<char>& data) override{
    assert(length == 13);
    for(int i = 0; i < 4; i++){
      this->image_width = (this->image_width << 8) | static_cast<uint8_t>(data[i]);
    }
    for(int i = 0; i < 4; i++){
      this->image_height = (this->image_height << 8) | static_cast<uint8_t>(data[i+4]);
    }
    this->bit_depth = static_cast<uint8_t>(data[8]);
    this->color_type = static_cast<uint8_t>(data[9]);
    this->compression_method = static_cast<uint8_t>(data[10]);
    this->filter_method = static_cast<uint8_t>(data[11]);
    this->interlace_method = static_cast<uint8_t>(data[12]);
  }
  void clear(void) override{
    this->image_width = this->image_height = 0;
    this->bit_depth = this->color_type = this->compression_method = this->filter_method = this->interlace_method = 0;
  }
  void debug(void) const override{
    std::cout << std::format("\t             width: {:08X}", this->image_width) << std::endl;
    std::cout << std::format("\t            height: {:08X}", this->image_height) << std::endl;
    std::cout << std::format("\t         bit_depth: {:01X}", this->bit_depth) << std::endl;
    std::cout << std::format("\t        color_type: {:01X}", this->color_type) << std::endl;
    std::cout << std::format("\tcompression_method: {:01X}", this->compression_method) << std::endl;
    std::cout << std::format("\t     filter_method: {:01X}", this->filter_method) << std::endl;
    std::cout << std::format("\t  interlace_method: {:01X}", this->interlace_method) << std::endl;    
  }
};

class PLTE : public ChunkDataInterface{
public:
  std::vector<std::vector<uint8_t>> palettes;
  using ChunkDataInterface::ChunkDataInterface;
  PLTE(const uint32_t length, const std::vector<char>& data){
    set(length, data);
  }
  void set(const uint32_t length, const std::vector<char>& data) override{
    assert(length % 3 == 0);
    palettes.reserve(length/3);
    for(int i = 0; i < length/3; i++){
      palettes.emplace_back(3);
      for(int j = 0; j < 3; j++){
        palettes.back()[j] = data[i*3+j];
      }
    }
  }
  void clear(void) override{
    this->palettes.clear();
  }
  void debug(void) const override{
    for(int i = 0; i < palettes.size(); i++){
      std::cout << std::format("\tPalette{:08X}:", i) << std::endl;
      std::cout << std::format("\t\t  Red:{:08X}", palettes[i][0]) << std::endl;
      std::cout << std::format("\t\tGreen:{:08X}", palettes[i][1]) << std::endl;
      std::cout << std::format("\t\t Blue:{:08X}", palettes[i][2]) << std::endl;
    }
  }
};

class sRGB : public ChunkDataInterface{
public:
  uint8_t rendering;
  using ChunkDataInterface::ChunkDataInterface;
  sRGB(const uint32_t length, const std::vector<char>& data){
    set(length, data);
  }
  void set(const uint32_t length, const std::vector<char>& data) override{
    assert(length == 1);
    this->rendering = data[0];
  }
  void clear(void) override{
    this->rendering = 0;
  }
  void debug(void) const override{
    std::cout << std::format("\trendering: {:08X}", this->rendering) << std::endl;
  }
};

class IDAT : public ChunkDataInterface{
public:
  using ChunkDataInterface::ChunkDataInterface;
  std::vector<uint8_t> image_data;
  IDAT(const uint32_t length, const std::vector<char>& data){
    set(length, data);
  }
  void set(const uint32_t length, const std::vector<char>& data) override{
    this->image_data.resize(data.size());
    std::transform(data.begin(), data.end(), this->image_data.begin(),
      [](char c) { return static_cast<uint8_t>(c); }
    );
  }
  void clear(void) override{
    this->image_data.clear();
  }
  void debug(void) const override{
    // for(int i = 0; i < this->image_data.size(); i++){
    //   if(i%16 == 0) std::cout << std::endl << "\t";
    //   std::cout << std::format("{:02X} ", this->image_data[i]);
    // }std::cout << std::endl;
  }
};

class IEND : public ChunkDataInterface{
public:
  using ChunkDataInterface::ChunkDataInterface;
  std::vector<uint8_t> image_data_decompressed;
  IEND(const uint32_t length, const std::vector<char>& data){ assert(length == 0); }
  void set(const uint32_t length, const std::vector<char>& data) override{}
  void clear(void) override{}
  void debug(void) const override{}
};

using ChunkData = std::variant<IHDR, PLTE, sRGB, IDAT, IEND>;

const uint64_t BYTE_LENGTH = 4;
const uint64_t BYTE_TYPE = 4;
const uint64_t BYTE_CRC = 4;
class Chunk{
public:
  uint32_t length = 0;
  uint32_t type = 0;
  std::string type_string = "";
  std::vector<char> data_raw;
  ChunkData data;
  uint32_t crc = 0;
  Chunk(void);
  void initialize(void);
  uint32_t calc_crc(const std::vector<char>& data, size_t start, size_t length);
  uint64_t set(const std::vector<char>& data);
  void debug(void) const;
};
Chunk::Chunk(void){
  initialize();
}
void Chunk::initialize(void){
  this->length = 0;
  this->type = 0;
  this->type_string = "";
  this->data_raw.clear();
  this->crc = 0;
}
uint32_t Chunk::calc_crc(const std::vector<char>& data, size_t start, size_t length) {
  uint32_t crc = UINT32_C(0xFFFFFFFF);   // 0xFFFFFFFFで初期化
  const uint32_t magic = UINT32_C(0xEDB88320);  // 反転したマジックナンバー
  // データを処理
  for(size_t i = 0; i < length; i++){
    crc ^= static_cast<uint8_t>(data[start + i]);
    for(int j = 0; j < 8; j++){
      if(crc & 1) crc = (crc >> 1) ^ magic;
      else        crc >>= 1;
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
  this->data_raw.resize(this->length);
  std::copy(chunk_data.begin()+BYTE_LENGTH+BYTE_TYPE, chunk_data.begin()+BYTE_LENGTH+BYTE_TYPE+this->length, this->data_raw.begin());
  if(equal_stri(type_string, "IHDR"))      this->data = IHDR{this->length, this->data_raw};
  else if(equal_stri(type_string, "PLTE")) this->data = PLTE{this->length, this->data_raw};
  else if(equal_stri(type_string, "sRGB")) this->data = sRGB{this->length, this->data_raw};
  else if(equal_stri(type_string, "IDAT")) this->data = IDAT{this->length, this->data_raw};
  else if(equal_stri(type_string, "IEND")) this->data = IEND{this->length, this->data_raw};
  // CRCディジットチェック
  for(int i = 0; i < BYTE_CRC; i++){
    this->crc = (this->crc << 8) | static_cast<uint8_t>(chunk_data[BYTE_LENGTH + BYTE_TYPE + this->length + i]);
  }
  assert(this->crc == calc_crc(chunk_data, BYTE_LENGTH, BYTE_TYPE+this->length));
  return BYTE_LENGTH + BYTE_TYPE + this->length + BYTE_CRC;
}
void Chunk::debug() const{
  std::cout << std::format("length: {:08X}", this->length) << std::endl;
  std::cout << std::format("type  : {:08X} = {}", this->type, this->type_string) << std::endl;
  std::cout << std::format("crc   : {:08X}", this->crc) << std::endl;
  std::visit([](const auto& data) {
    data.debug();
  }, this->data);
}