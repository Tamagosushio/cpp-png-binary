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
  inline bool equal_stri(const std::string& s1, const std::string& s2){
    std::string t1 = s1;
    std::transform(s1.begin(), s1.end(), t1.begin(),
      [](char c) { return std::toupper(c); }
    );
    std::string t2 = s2;
    std::transform(s2.begin(), s2.end(), t2.begin(),
      [](char c) { return std::toupper(c); }
    );
    return t1 == t2;
  }
  // 整数をバイト列に変換
  inline std::vector<char> int2vecchar(const uint32_t number){
    std::vector<char> res(4);
    res[0] = static_cast<char>((number >> 24) & 0xFF);
    res[1] = static_cast<char>((number >> 16) & 0xFF);
    res[2] = static_cast<char>((number >> 8) & 0xFF);
    res[3] = static_cast<char>((number >> 0) & 0xFF);
    return res;
  }
  // CRC計算
  uint32_t calc_crc(const std::vector<char>& data, size_t start, size_t length){
    uint32_t crc = UINT32_C(0xFFFFFFFF);
    const uint32_t magic = UINT32_C(0xEDB88320);
    for(size_t i = 0; i < length; i++){
      crc ^= static_cast<uint8_t>(data[start + i]);
      for(int j = 0; j < 8; j++){
        if(crc & 1) crc = (crc >> 1) ^ magic;
        else        crc >>= 1;
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
    for(int i = 0; i < 4; i++){
      image_width_ = (image_width_ << 8) | static_cast<uint8_t>(data[i]);
    }
    for(int i = 0; i < 4; i++){
      image_height_ = (image_height_ << 8) | static_cast<uint8_t>(data[i+4]);
    }
    bit_depth_          = static_cast<uint8_t>(data[8]);
    color_type_         = static_cast<uint8_t>(data[9]);
    compression_method_ = static_cast<uint8_t>(data[10]);
    filter_method_      = static_cast<uint8_t>(data[11]);
    interlace_method_   = static_cast<uint8_t>(data[12]);
  }
  inline std::vector<char> get() const override{
    std::vector<char> res;
    res.reserve(13);
    for(const char& c : utils::int2vecchar(image_width_)) res.push_back(c);
    for(const char& c : utils::int2vecchar(image_height_)) res.push_back(c);
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
    palettes_.reserve(length/3);
    for(int i = 0; i < length/3; i++){
      palettes_.emplace_back(3);
      for(int j = 0; j < 3; j++){
        palettes_.back()[j] = data[i*3+j];
      }
    }
  }
  inline std::vector<char> get() const override{
    std::vector<char> res;
    for(const auto& palette : palettes_){
      for(const auto& color : palette){
        res.push_back(static_cast<char>(color));
      }
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
    rendering_ = data[0];
  }
  inline std::vector<char> get() const override{
    return std::vector<char>{static_cast<char>(rendering_)};
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
    std::transform(data.begin(), data.end(), image_data_.begin(),
      [](char c) { return static_cast<uint8_t>(c); }
    );
  }
  inline std::vector<char> get() const override{
    std::vector<char> res(image_data_.size());
    std::transform(image_data_.begin(), image_data_.end(), res.begin(),
      [](uint8_t c) { return static_cast<char>(c); }
    );
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
    for(int i = 0; i < BYTE_LENGTH; i++){
      length_ = (length_ << 8) | static_cast<uint8_t>(chunk_data[i]);
    }
    // チャンク名を取得
    type_string_.clear();
    for(int i = 0; i < BYTE_TYPE; i++){
      type_ = (type_ << 8) | static_cast<uint8_t>(chunk_data[BYTE_LENGTH+i]);
      type_string_ += chunk_data[BYTE_LENGTH+i];
    }
    // チャンクのデータを取得
    data_raw_.resize(length_);
    std::copy(
      chunk_data.begin()+BYTE_LENGTH+BYTE_TYPE,
      chunk_data.begin()+BYTE_LENGTH+BYTE_TYPE+length_,
      data_raw_.begin()
    );
    // チャンクタイプに応じてデータを設定
    if(utils::equal_stri(type_string_, "IHDR"))      data_ = IHDR{length_, data_raw_};
    else if(utils::equal_stri(type_string_, "PLTE")) data_ = PLTE{length_, data_raw_};
    else if(utils::equal_stri(type_string_, "sRGB")) data_ = sRGB{length_, data_raw_};
    else if(utils::equal_stri(type_string_, "IDAT")) data_ = IDAT{length_, data_raw_};
    else if(utils::equal_stri(type_string_, "IEND")) data_ = IEND{length_, data_raw_};
    // CRCチェック
    for(int i = 0; i < BYTE_CRC; i++){
      crc_ = (crc_ << 8) | static_cast<uint8_t>(chunk_data[BYTE_LENGTH + BYTE_TYPE + length_ + i]);
    }
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