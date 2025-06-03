# include "chunk.hpp"
# include <chrono>

std::chrono::system_clock::time_point start;
int get_time_microsec(void){
  return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - start).count();
}

namespace png {

class PNG{
private:
  uint64_t size_ = 0;
  uint32_t width_ = 0;
  uint32_t height_ = 0;
  std::vector<char> data_;
  std::vector<Chunk> chunks_;
  std::vector<uint8_t> image_data_compressed_;
  std::vector<uint8_t> image_data_decompressed_;
  std::vector<uint8_t> image_data_decompressed_nofilter_;
  void decompress_data(void); // データを解凍
  void compress_data(void); // データを圧縮
  void unset_filter(void); // データのフィルターを外す
  void set_filter(void); // データにフィルターをかける
  void load_chunks(void); // チャンク読み込み
  void extract_image_data(void); // 画像データを抽出
  void delete_idat(void); // チャンク配列からIDATチャンクを削除
  void insert_idat(void); // チャンク配列にIDATチャンクを追加
public:
  explicit PNG(const std::string& path);
  void reverse_color(void);
  void write(const std::string& path) const;
  void debug(void) const;
};

PNG::PNG(const std::string& path){
  // バイナリ読み込み
  std::ifstream ifs(path, std::ios::in | std::ios::binary);
  if(!ifs){
    throw std::runtime_error("Failed to open input file");
  }
  // ファイルサイズの取得
  ifs.seekg(0, std::ios::end);
  size_ = ifs.tellg();
  ifs.seekg(0);
  // 読み込んだデータをvector<char>型に変換
  data_.resize(size_);
  ifs.read(data_.data(), size_);
  load_chunks();
  extract_image_data();
  decompress_data();
}

void PNG::load_chunks(){
  uint64_t binary_idx = 8;  // PNGシグネチャの後の位置
  Chunk chunk;
  do {
    // チャンクのデータ長のみ先に取得
    uint32_t chunk_length = 0;
    for(int i = 0; i < BYTE_LENGTH; ++i){
      chunk_length = (chunk_length << 8) | static_cast<uint8_t>(data_[binary_idx + i]);
    }
    uint64_t chunk_total_size = BYTE_LENGTH + BYTE_TYPE + chunk_length + BYTE_CRC;
    // チャンク生成
    std::vector<char> chunk_data(chunk_total_size);
    std::copy(
      data_.begin() + binary_idx,
      data_.begin() + binary_idx + chunk_total_size,
      chunk_data.begin()
    );
    chunk.initialize();
    binary_idx += chunk.set(chunk_data);
    chunks_.push_back(chunk);
  } while (not utils::equal_stri(chunk.type_string(), "IEND"));
}

void PNG::extract_image_data(){
  for(const Chunk& chunk : chunks_){
    if(utils::equal_stri(chunk.type_string(), "IDAT")){
      const IDAT& idat = std::get<IDAT>(chunk.data());
      image_data_compressed_.insert(
        image_data_compressed_.end(),
        idat.image_data().begin(),
        idat.image_data().end()
      );
    }
  }
}

void PNG::decompress_data(){
  z_stream strm;
  strm.zalloc = Z_NULL;
  strm.zfree = Z_NULL;
  strm.opaque = Z_NULL;
  strm.avail_in = image_data_compressed_.size();
  strm.next_in = reinterpret_cast<Bytef*>(image_data_compressed_.data());
  if(inflateInit(&strm) != Z_OK){
    throw std::runtime_error("inflateInit failed");
  }
  // IHDRから画像サイズを取得
  for(const Chunk& chunk : chunks_){
    if (utils::equal_stri(chunk.type_string(), "IHDR")){
      const IHDR& ihdr = std::get<IHDR>(chunk.data());
      width_ = ihdr.width();
      height_ = ihdr.height();
      break;
    }
  }
  // 解凍後のデータサイズを計算
  size_t decompressed_size = width_ * height_ * 3 + height_;  // RGB形式の場合
  image_data_decompressed_.resize(decompressed_size);
  strm.avail_out = decompressed_size;
  strm.next_out = reinterpret_cast<Bytef*>(image_data_decompressed_.data());
  int ret = inflate(&strm, Z_FINISH);
  if(ret != Z_STREAM_END){
    inflateEnd(&strm);
    throw std::runtime_error("inflate failed");
  }
  image_data_decompressed_.resize(decompressed_size - strm.avail_out);
  inflateEnd(&strm);
}

void PNG::compress_data(){
  z_stream strm;
  strm.zalloc = Z_NULL;
  strm.zfree = Z_NULL;
  strm.opaque = Z_NULL;
  strm.avail_in = image_data_decompressed_.size();
  strm.next_in = reinterpret_cast<Bytef*>(image_data_decompressed_.data());
  if(deflateInit(&strm, Z_DEFAULT_COMPRESSION) != Z_OK){
    throw std::runtime_error("deflateInit failed");
  }
  size_t compressed_size = static_cast<size_t>(image_data_decompressed_.size() * 1.1) + 12;
  image_data_compressed_.resize(compressed_size);
  strm.avail_out = compressed_size;
  strm.next_out = reinterpret_cast<Bytef*>(image_data_compressed_.data());
  int ret = deflate(&strm, Z_FINISH);
  if(ret != Z_STREAM_END){
    deflateEnd(&strm);
    throw std::runtime_error("deflate failed");
  }
  image_data_compressed_.resize(compressed_size - strm.avail_out);
  deflateEnd(&strm);
}

void PNG::unset_filter(void){
  image_data_decompressed_nofilter_.resize(image_data_decompressed_.size());
  const size_t width_data = width_ * 3 + 1;
  const size_t height = height_;
  // フィルタ処理を最適化
  for(size_t y = 0; y < height; y++){
    const size_t row_start = y * width_data;
    const uint8_t filter_type = image_data_decompressed_[row_start];
    const bool has_prev_row = y > 0;
    // フィルタタイプに応じた処理
    switch(filter_type){
      case 1:{ // Subフィルタ
        for(size_t x = 1; x < width_data; x++){
          const size_t i = row_start + x;
          const uint8_t left = (x >= 3) ? image_data_decompressed_nofilter_[i-3] : 0;
          image_data_decompressed_nofilter_[i] = (image_data_decompressed_[i] + left) & 0xFF;
        }
        break;
      }
      case 2:{ // Upフィルタ
        if(has_prev_row){
          for(size_t x = 1; x < width_data; x++){
            const size_t i = row_start + x;
            image_data_decompressed_nofilter_[i] = (image_data_decompressed_[i] + image_data_decompressed_nofilter_[i-width_data]) & 0xFF;
          }
        }else{
          for(size_t x = 1; x < width_data; x++){
            image_data_decompressed_nofilter_[row_start + x] = image_data_decompressed_[row_start + x];
          }
        }
        break;
      }
      case 3:{ // Averageフィルタ
        for(size_t x = 1; x < width_data; x++){
          const size_t i = row_start + x;
          const uint8_t left = (x >= 3) ? image_data_decompressed_nofilter_[i-3] : 0;
          const uint8_t up = has_prev_row ? image_data_decompressed_nofilter_[i-width_data] : 0;
          image_data_decompressed_nofilter_[i] = (image_data_decompressed_[i] + ((left + up) >> 1)) & 0xFF;
        }
        break;
      }
      case 4:{ // Paethフィルタ
        for(size_t x = 1; x < width_data; x++){
          const size_t i = row_start + x;
          const uint8_t left = (x >= 3) ? image_data_decompressed_nofilter_[i-3] : 0;
          const uint8_t up = has_prev_row ? image_data_decompressed_nofilter_[i-width_data] : 0;
          const uint8_t upleft = (x >= 3 && has_prev_row) ? image_data_decompressed_nofilter_[i-width_data-3] : 0;
          const int p = left + up - upleft;
          const int pa = std::abs(p - left);
          const int pb = std::abs(p - up);
          const int pc = std::abs(p - upleft);
          const uint8_t predictor = (pa <= pb && pa <= pc) ? left : (pb <= pc ? up : upleft);
          image_data_decompressed_nofilter_[i] = (image_data_decompressed_[i] + predictor) & 0xFF;
        }
        break;
      }
      default:{
        for(size_t x = 1; x < width_data; x++){
          image_data_decompressed_nofilter_[row_start + x] = image_data_decompressed_[row_start + x];
        }
      }
    }
  }
}

void PNG::set_filter(void){
  image_data_decompressed_.resize(image_data_decompressed_nofilter_.size());
  const size_t width_data = width_ * 3 + 1;
  const size_t height = height_;
  // 各フィルタタイプの結果を保存する配列
  std::vector<std::vector<uint8_t>> filtered_results(5);
  std::vector<size_t> row_filter_scores(5, 0);  
  for(size_t y = 0; y < height; y++){
    const size_t row_start = y * width_data;
    const bool has_prev_row = y > 0;
    // フィルタ結果配列を初期化
    for(uint8_t filter_type = 0; filter_type < 5; filter_type++){
      filtered_results[filter_type].resize(width_data);
      row_filter_scores[filter_type] = 0;
    }
    // 全5種のフィルタについてそれぞれ試す
    for(uint8_t filter_type = 0; filter_type < 5; filter_type++){
      // フィルタタイプを設定
      filtered_results[filter_type][0] = filter_type;
      switch(filter_type){
        case 0: // Noneフィルタ
          for(size_t x = 1; x < width_data; x++){
            filtered_results[filter_type][x] = image_data_decompressed_nofilter_[row_start + x];
          }
          break;          
        case 1: // Subフィルタ
          for(size_t x = 1; x < width_data; x++){
            const uint8_t left = (x >= 3) ? image_data_decompressed_nofilter_[row_start + x - 3] : 0;
            filtered_results[filter_type][x] = (image_data_decompressed_nofilter_[row_start + x] - left) & 0xFF;
          }
          break;
        case 2: // Upフィルタ
          for(size_t x = 1; x < width_data; x++){
            const uint8_t up = has_prev_row ? image_data_decompressed_nofilter_[row_start + x - width_data] : 0;
            filtered_results[filter_type][x] = (image_data_decompressed_nofilter_[row_start + x] - up) & 0xFF;
          }
          break;
        case 3: // Averageフィルタ
          for(size_t x = 1; x < width_data; x++){
            const uint8_t left = (x >= 3) ? image_data_decompressed_nofilter_[row_start + x - 3] : 0;
            const uint8_t up = has_prev_row ? image_data_decompressed_nofilter_[row_start + x - width_data] : 0;
            filtered_results[filter_type][x] = (image_data_decompressed_nofilter_[row_start + x] - ((left + up) >> 1)) & 0xFF;
          }
          break;
        case 4: // Paethフィルタ
          for(size_t x = 1; x < width_data; x++){
            const uint8_t left = (x >= 3) ? image_data_decompressed_nofilter_[row_start + x - 3] : 0;
            const uint8_t up = has_prev_row ? image_data_decompressed_nofilter_[row_start + x - width_data] : 0;
            const uint8_t upleft = (x >= 3 && has_prev_row) ? image_data_decompressed_nofilter_[row_start + x - width_data - 3] : 0;
            const int p = left + up - upleft;
            const int pa = std::abs(p - left);
            const int pb = std::abs(p - up);
            const int pc = std::abs(p - upleft);
            const uint8_t predictor = (pa <= pb && pa <= pc) ? left : (pb <= pc ? up : upleft);
            filtered_results[filter_type][x] = (image_data_decompressed_nofilter_[row_start + x] - predictor) & 0xFF;
          }
          break;
      }      
      // この行のフィルタスコアを計算
      for(size_t x = 1; x < width_data; x++){
        row_filter_scores[filter_type] += std::abs(filtered_results[filter_type][x]);
      }
    }    
    // この行で最もスコアの低い（圧縮に適した）フィルタを選択
    size_t best_filter = 0;
    size_t best_score = row_filter_scores[0];
    for(size_t i = 1; i < 5; i++){
      if(row_filter_scores[i] < best_score){
        best_score = row_filter_scores[i];
        best_filter = i;
      }
    }    
    // 最適なフィルタの結果を最終的な出力にコピー
    image_data_decompressed_[row_start] = best_filter;
    std::copy(filtered_results[best_filter].begin(), filtered_results[best_filter].end(), image_data_decompressed_.begin() + row_start);
  }
}

void PNG::delete_idat(void){
  chunks_.erase(
    std::remove_if(chunks_.begin(), chunks_.end(),
      [](const Chunk& chunk){
        return utils::equal_stri(chunk.type_string(), "IDAT");
      }
    ),
    chunks_.end()
  );
}

void PNG::insert_idat(void){
  // 新しいIDATチャンクを作成
  Chunk idat_chunk;
  idat_chunk.initialize();
  idat_chunk.length() = image_data_compressed_.size();
  idat_chunk.type() = 0x49444154;  // "IDAT"
  idat_chunk.type_string() = "IDAT";
  // IDATデータを設定
  std::vector<char> compressed_data(image_data_compressed_.size());
  std::transform(
    image_data_compressed_.begin(),
    image_data_compressed_.end(),
    compressed_data.begin(),
    [](uint8_t c) { return static_cast<char>(c); }
  );
  idat_chunk.data() = IDAT{idat_chunk.length(), compressed_data};
  // CRCを計算
  std::vector<char> crc_data;
  crc_data.insert(crc_data.end(), idat_chunk.type_string().begin(), idat_chunk.type_string().end());
  crc_data.insert(crc_data.end(), compressed_data.begin(), compressed_data.end());
  idat_chunk.crc() = utils::calc_crc(crc_data, 0, crc_data.size());
  // チャンクを挿入
  chunks_.insert(chunks_.end() - 1, idat_chunk);
}

void PNG::write(const std::string& path) const{
  std::ofstream ofs(path, std::ios::out | std::ios::binary);
  if(!ofs){
    throw std::runtime_error("Failed to open output file");
  }
  // PNGシグネチャを書き込む
  const unsigned char signature[] = {
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A
  };
  ofs.write(reinterpret_cast<const char*>(signature), 8);
  // 各チャンクを書き込む
  for(const Chunk& chunk : chunks_){
    // チャンクの長さを書き込む
    std::vector<char> length_data = utils::int2vecchar(chunk.length());
    ofs.write(length_data.data(), 4);
    // チャンクタイプを書き込む
    ofs.write(chunk.type_string().c_str(), 4);
    // チャンクデータを書き込む
    std::vector<char> chunk_data = std::visit([](const auto& data){
      return data.get();
    }, chunk.data());
    ofs.write(chunk_data.data(), chunk_data.size());
    // CRCを計算して書き込む
    std::vector<char> crc_data;
    crc_data.insert(crc_data.end(), chunk.type_string().begin(), chunk.type_string().end());
    crc_data.insert(crc_data.end(), chunk_data.begin(), chunk_data.end());
    uint32_t crc = utils::calc_crc(crc_data, 0, crc_data.size());
    std::vector<char> crc_bytes = utils::int2vecchar(crc);
    ofs.write(crc_bytes.data(), 4);
  }
}

void PNG::debug() const{
  for(const Chunk& chunk : chunks_){
    chunk.debug();
  }
  std::cout << "Decompressed data size: " << image_data_decompressed_.size() << " bytes" << std::endl;
  std::cout << "Decompressed data:" << std::endl;
  for(size_t i = 0; i < image_data_decompressed_.size(); i++){
    if (i % (width_ * 3 + 1) == 0) std::cout << std::endl << "\t";
    std::cout << std::format("{:02X} ", image_data_decompressed_[i]);
  }
  std::cout << std::endl;
}

void PNG::reverse_color(){
  unset_filter();
  // 色反転処理
  const size_t width_data = width_ * 3 + 1;
  for(size_t i = 0; i < image_data_decompressed_.size(); i++){
    if(i % width_data == 0) image_data_decompressed_[i] = 0x00;
    else image_data_decompressed_nofilter_[i] = ~image_data_decompressed_nofilter_[i];
  }
  set_filter();
  compress_data();
  delete_idat();
  insert_idat();
}

} // namespace png

int main(int argc, char* argv[]){
  int n = 1;
  start = std::chrono::system_clock::now();
  for(int i = 0; i < n; i++){
    png::PNG png{argv[1]};
    // png.debug();
    png.reverse_color();
    png.write("./out_reverse.png");
  }
  std::cout << get_time_microsec() << std::endl;
  return 0;
}

