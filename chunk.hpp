# include <iostream>
# include <string>
# include <fstream>
# include <cstdint>
# include <format>
# include <vector>
# include <algorithm>
# include <cassert>
# include <zlib.h>

namespace png{
// 定数定義
constexpr uint64_t BYTE_LENGTH = 4;
constexpr uint64_t BYTE_TYPE = 4;
constexpr uint64_t BYTE_CRC = 4;
// ユーティリティ関数
namespace utils{
  // 大文字小文字区別しない文字列比較
  inline bool equal_stri(const std::string& s1, const std::string& s2) noexcept {
    if(s1.size() != s2.size()) return false;
    for(size_t i = 0; i < s1.size(); ++i){
      if(std::toupper(static_cast<unsigned char>(s1[i])) != 
         std::toupper(static_cast<unsigned char>(s2[i]))) return false;
    }
    return true;
  }
  // 整数をバイト列に変換
  inline std::vector<char> int2vecchar(const uint32_t number) noexcept {
    return {
      static_cast<char>((number >> 24) & 0xFF),
      static_cast<char>((number >> 16) & 0xFF),
      static_cast<char>((number >> 8) & 0xFF),
      static_cast<char>((number >> 0) & 0xFF)
    };
  }
  // CRC計算
  inline uint32_t calc_crc(const std::vector<char>& data, size_t start, size_t length) noexcept {
    uint32_t crc = UINT32_C(0xFFFFFFFF);
    const uint32_t magic = UINT32_C(0xEDB88320);
    const char* ptr = data.data() + start;
    for(size_t i = 0; i < length; i++){
      crc ^= static_cast<uint8_t>(ptr[i]);
      for(int j = 0; j < 8; j++){
        crc = (crc & 1) ? ((crc >> 1) ^ magic) : (crc >> 1);
      }
    }
    return ~crc;
  }
}

// チャンクデータのインターフェース
class ChunkDataInterface{
public:
  virtual ~ChunkDataInterface() = default;
  virtual void set(const uint32_t length, const std::vector<char>& data) = 0; // データセット
  virtual inline std::vector<char> get() const = 0; // バイナリデータ取得
  virtual void clear() = 0; // データクリア
  virtual void debug() const = 0; // デバッグ出力
};

// 基本チャンクデータクラス
class BaseChunkData : public ChunkDataInterface{
protected:
  uint32_t length_ = 0; // チャンクのデータ長
  std::vector<char> data_raw_; // チャンクの生データ
  // データクリア
  void clear() override{
    length_ = 0;
    data_raw_.clear();
  }
};

// IHDRチャンク
class IHDR : public BaseChunkData{
private:
  uint32_t image_width_ = 0; // 画像の幅
  uint32_t image_height_ = 0; // 画像の高さ
  uint8_t bit_depth_ = 0; // ビット深度
  uint8_t color_type_ = 0; // 色空間
  uint8_t compression_method_ = 0; // 圧縮方法
  uint8_t filter_method_ = 0; // フィルター方法
  uint8_t interlace_method_ = 0; // インターレース方法
public:
  IHDR() = default;
  IHDR(const uint32_t length, const std::vector<char>& data){
    set(length, data);
  }
  void set(const uint32_t length, const std::vector<char>& data) override{
    assert(length == 13);
    length_ = length;
    data_raw_ = data;
    const char* ptr = data.data();
    image_width_ = (static_cast<uint8_t>(ptr[0]) << 24)
                   | (static_cast<uint8_t>(ptr[1]) << 16)
                   | (static_cast<uint8_t>(ptr[2]) << 8)
                   |  static_cast<uint8_t>(ptr[3]);
    image_height_ = (static_cast<uint8_t>(ptr[4]) << 24)
                    | (static_cast<uint8_t>(ptr[5]) << 16)
                    | (static_cast<uint8_t>(ptr[6]) << 8)
                    | static_cast<uint8_t>(ptr[7]);
    bit_depth_          = static_cast<uint8_t>(ptr[8]);
    color_type_         = static_cast<uint8_t>(ptr[9]);
    compression_method_ = static_cast<uint8_t>(ptr[10]);
    filter_method_      = static_cast<uint8_t>(ptr[11]);
    interlace_method_   = static_cast<uint8_t>(ptr[12]);
  }
  inline std::vector<char> get() const override{
    std::vector<char> res;
    res.reserve(13);
    const std::vector<char> width_bytes = utils::int2vecchar(image_width_);
    const std::vector<char> height_bytes = utils::int2vecchar(image_height_);
    res.insert(res.end(), width_bytes.begin(), width_bytes.end());
    res.insert(res.end(), height_bytes.begin(), height_bytes.end());
    res.push_back(static_cast<char>(bit_depth_));
    res.push_back(static_cast<char>(color_type_));
    res.push_back(static_cast<char>(compression_method_));
    res.push_back(static_cast<char>(filter_method_));
    res.push_back(static_cast<char>(interlace_method_));
    return res;
  }
  void clear() override{
    BaseChunkData::clear();
    image_width_ = 0;
    image_height_ = 0;
    bit_depth_ = 0;
    color_type_ = 0;
    compression_method_ = 0;
    filter_method_ = 0;
    interlace_method_ = 0;
  }
  void debug() const override{
    std::cout << std::format("\t             width: {:08X}", image_width_) << std::endl;
    std::cout << std::format("\t            height: {:08X}", image_height_) << std::endl;
    std::cout << std::format("\t         bit_depth: {:01X}", bit_depth_) << std::endl;
    std::cout << std::format("\t        color_type: {:01X}", color_type_) << std::endl;
    std::cout << std::format("\tcompression_method: {:01X}", compression_method_) << std::endl;
    std::cout << std::format("\t     filter_method: {:01X}", filter_method_) << std::endl;
    std::cout << std::format("\t  interlace_method: {:01X}", interlace_method_) << std::endl;
  }
  // ゲッター
  uint32_t& width() { return image_width_; }
  const uint32_t& width() const { return image_width_; }
  uint32_t& height() { return image_height_; }
  const uint32_t& height() const { return image_height_; }
  uint8_t& bit_depth() { return bit_depth_; }
  const uint8_t& bit_depth() const { return bit_depth_; }
  uint8_t& color_type() { return color_type_; }
  const uint8_t& color_type() const { return color_type_; }
  uint8_t& compression_method() { return compression_method_; }
  const uint8_t& compression_method() const { return compression_method_; }
  uint8_t& filter_method() { return filter_method_; }
  const uint8_t& filter_method() const { return filter_method_; }
  uint8_t& interlace_method() { return interlace_method_; }
  const uint8_t& interlace_method() const { return interlace_method_; }
};

// PLTEチャンク
class PLTE : public BaseChunkData{
private:
  std::vector<std::vector<uint8_t>> palettes_;
public:
  PLTE() = default;
  PLTE(const uint32_t length, const std::vector<char>& data){
    set(length, data);
  }
  void set(const uint32_t length, const std::vector<char>& data) override{
    assert(length % 3 == 0);
    length_ = length;
    data_raw_ = data;
    const size_t num_palettes = length/3;
    palettes_.resize(num_palettes);
    const char* ptr = data.data();
    for(size_t i = 0; i < num_palettes; i++){
      palettes_[i] = {
        static_cast<uint8_t>(ptr[i*3]),
        static_cast<uint8_t>(ptr[i*3+1]),
        static_cast<uint8_t>(ptr[i*3+2])
      };
    }
  }
  inline std::vector<char> get() const override{
    std::vector<char> res;
    res.reserve(palettes_.size() * 3);
    for(const std::vector<uint8_t>& palette : palettes_){
      res.push_back(static_cast<char>(palette[0]));
      res.push_back(static_cast<char>(palette[1]));
      res.push_back(static_cast<char>(palette[2]));
    }
    return res;
  }
  void clear() override{
    BaseChunkData::clear();
    palettes_.clear();
  }
  void debug() const override{
    for(size_t i = 0; i < palettes_.size(); i++){
      std::cout << std::format("\tPalette{:08X}:", i) << std::endl;
      std::cout << std::format("\t\t  Red:{:08X}", palettes_[i][0]) << std::endl;
      std::cout << std::format("\t\tGreen:{:08X}", palettes_[i][1]) << std::endl;
      std::cout << std::format("\t\t Blue:{:08X}", palettes_[i][2]) << std::endl;
    }
  }
  // ゲッター
  std::vector<std::vector<uint8_t>>& palettes() { return palettes_; }
  const std::vector<std::vector<uint8_t>>& palettes() const { return palettes_; }
};

// sRGBチャンク
class sRGB : public BaseChunkData{
private:
  uint8_t rendering_ = 0;
public:
  sRGB() = default;
  sRGB(const uint32_t length, const std::vector<char>& data){
    set(length, data);
  }
  void set(const uint32_t length, const std::vector<char>& data) override{
    assert(length == 1);
    length_ = length;
    data_raw_ = data;
    rendering_ = static_cast<uint8_t>(data[0]);
  }
  inline std::vector<char> get() const override{
    return {static_cast<char>(rendering_)};
  }
  void clear() override{
    BaseChunkData::clear();
    rendering_ = 0;
  }
  void debug() const override{
    std::cout << std::format("\trendering: {:08X}", rendering_) << std::endl;
  }
  // ゲッター
  uint8_t& rendering() { return rendering_; }
  const uint8_t& rendering() const { return rendering_; }
};

// IDATチャンク
class IDAT : public BaseChunkData{
private:
  std::vector<uint8_t> image_data_;
public:
  IDAT() = default;
  IDAT(const uint32_t length, const std::vector<char>& data){
    set(length, data);
  }
  void set(const uint32_t length, const std::vector<char>& data) override{
    length_ = length;
    data_raw_ = data;
    image_data_.resize(data.size());
    const char* ptr = data.data();
    for(size_t i = 0; i < data.size(); ++i){
      image_data_[i] = static_cast<uint8_t>(ptr[i]);
    }
  }
  inline std::vector<char> get() const override{
    std::vector<char> res(image_data_.size());
    const uint8_t* ptr = image_data_.data();
    for(size_t i = 0; i < image_data_.size(); ++i){
      res[i] = static_cast<char>(ptr[i]);
    }
    return res;
  }
  void clear() override{
    BaseChunkData::clear();
    image_data_.clear();
  }
  void debug() const override {}
  // ゲッター
  std::vector<uint8_t>& image_data() { return image_data_; }
  const std::vector<uint8_t>& image_data() const { return image_data_; }
};

// IENDチャンク
class IEND : public BaseChunkData{
public:
  IEND() = default;
  IEND(const uint32_t length, const std::vector<char>& data){
    set(length, data);
  }
  void set(const uint32_t length, const std::vector<char>& data) override{
    assert(length == 0);
    length_ = length;
    data_raw_ = data;
  }
  inline std::vector<char> get() const override{
    return std::vector<char>();
  }
  void clear() override{
    BaseChunkData::clear();
  }
  void debug() const override {}
};

using ChunkData = std::variant<IHDR, PLTE, sRGB, IDAT, IEND>;

// チャンククラス
class Chunk{
private:
  uint32_t length_ = 0;
  uint32_t type_ = 0;
  std::string type_string_;
  std::vector<char> data_raw_;
  ChunkData data_;
  uint32_t crc_ = 0;
public:
  Chunk() = default;
  // 初期化
  void initialize(){
    length_ = 0;
    type_ = 0;
    type_string_.clear();
    data_raw_.clear();
    std::visit([](auto& data){
      data.clear();
    }, data_);
    crc_ = 0;
  }
  // チャンクデータセット
  uint64_t set(const std::vector<char>& chunk_data){
    // チャンクのデータ長を取得
    const char* ptr = chunk_data.data();
    length_ = (static_cast<uint8_t>(ptr[0]) << 24)
              | (static_cast<uint8_t>(ptr[1]) << 16)
              | (static_cast<uint8_t>(ptr[2]) << 8)
              | static_cast<uint8_t>(ptr[3]);
    // チャンク名を取得
    type_string_.clear();
    type_string_.reserve(4);
    type_ = 0;
    for(int i = 0; i < BYTE_TYPE; i++){
      const char c = ptr[BYTE_LENGTH+i];
      type_ = (type_ << 8) | static_cast<uint8_t>(c);
      type_string_ += c;
    }

    // チャンクのデータを取得
    const size_t data_size = length_;
    data_raw_.resize(data_size);
    std::copy_n(ptr + BYTE_LENGTH + BYTE_TYPE, data_size, data_raw_.begin());

    // チャンクタイプに応じてデータを設定
    if(utils::equal_stri(type_string_, "IHDR"))      data_ = IHDR{length_, data_raw_};
    else if(utils::equal_stri(type_string_, "PLTE")) data_ = PLTE{length_, data_raw_};
    else if(utils::equal_stri(type_string_, "sRGB")) data_ = sRGB{length_, data_raw_};
    else if(utils::equal_stri(type_string_, "IDAT")) data_ = IDAT{length_, data_raw_};
    else if(utils::equal_stri(type_string_, "IEND")) data_ = IEND{length_, data_raw_};

    // CRCチェック
    crc_ = (static_cast<uint8_t>(ptr[BYTE_LENGTH + BYTE_TYPE + length_]) << 24) |
           (static_cast<uint8_t>(ptr[BYTE_LENGTH + BYTE_TYPE + length_ + 1]) << 16) |
           (static_cast<uint8_t>(ptr[BYTE_LENGTH + BYTE_TYPE + length_ + 2]) << 8) |
           static_cast<uint8_t>(ptr[BYTE_LENGTH + BYTE_TYPE + length_ + 3]);
    assert(crc_ == utils::calc_crc(chunk_data, BYTE_LENGTH, BYTE_TYPE+length_));

    return BYTE_LENGTH + BYTE_TYPE + length_ + BYTE_CRC;
  }
  // デバッグ出力
  void debug() const{
    std::cout << std::format("length: {:08X}", length_) << std::endl;
    std::cout << std::format("type  : {:08X} = {}", type_, type_string_) << std::endl;
    std::cout << std::format("crc   : {:08X}", crc_) << std::endl;
    std::visit([](const auto& data) {
      data.debug();
    }, data_);
  }
  // ゲッター
  uint32_t& length() { return length_; }
  const uint32_t& length() const { return length_; }
  uint32_t& type() { return type_; }
  const uint32_t& type() const { return type_; }
  std::string& type_string() { return type_string_; }
  const std::string& type_string() const { return type_string_; }
  ChunkData& data() { return data_; }
  const ChunkData& data() const { return data_; }
  std::vector<char>& data_raw() { return data_raw_; }
  const std::vector<char>& data_raw() const { return data_raw_; }
  uint32_t& crc() { return crc_; }
  const uint32_t& crc() const { return crc_; }
};

} // namespace png