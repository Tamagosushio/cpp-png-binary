# include "chunk.hpp"

class PNG{
public:
  uint64_t size;
  uint32_t width, height;
  std::vector<char> data;
  std::vector<Chunk> chunks;
  std::vector<uint8_t> image_data_compressed;
  std::vector<uint8_t> image_data_decompressed;
public:
  PNG(const std::string& path);
  void decompress_data(void);
  void compress_data(void);
  void write(const std::string& path) const;
  void debug(void) const;
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
  }while(not equal_stri(chunk.type_string, "IEND"));
  for(const Chunk& chunk: chunks){
    if(chunk.type_string == "IDAT"){
      const IDAT idat = std::get<IDAT>(chunk.data);
      this->image_data_compressed.insert(this->image_data_compressed.end(), idat.image_data.begin(), idat.image_data.end());
    }
  }
  decompress_data();
}
void PNG::decompress_data(void){
  // zlibのストリーム構造体
  // 圧縮/解凍処理の状態を保持
  z_stream strm;
  // メモリ割り当て関数を指定
  // Z_NULLを指定して、zlibのデフォルトのメモリ割り当て関数を使用
  strm.zalloc = Z_NULL;
  strm.zfree = Z_NULL;
  strm.opaque = Z_NULL;
  // 入力バッファの設定
  strm.avail_in = image_data_compressed.size(); // 入力バッファの残りのバイト数
  strm.next_in = reinterpret_cast<Bytef*>(image_data_compressed.data()); // 入力バッファの現在位置へのポインタ
  // inflateInitでzlibの解凍処理を初期化
  if(inflateInit(&strm) != Z_OK){
    throw std::runtime_error("inflateInit failed");
  }
  // IHDRから画像サイズを取得
  uint32_t width = 0, height = 0;
  for(const Chunk& chunk: chunks){
    if(equal_stri(chunk.type_string, "IHDR")){
      const IHDR& ihdr = std::get<IHDR>(chunk.data);
      this->width = ihdr.image_width;
      this->height = ihdr.image_height;
      break;
    }
  }
  // 解凍後のデータサイズを計算
  // 各行の先頭にフィルタタイプ（1バイト）があるため、heightを加算
  size_t decompressed_size = this->width * this->height * 3 + this->height;  // RGB形式の場合
  image_data_decompressed.resize(decompressed_size);
  // 出力バッファの設定
  strm.avail_out = decompressed_size; // 出力バッファの残りのバイト数
  strm.next_out = reinterpret_cast<Bytef*>(image_data_decompressed.data()); // 出力バッファの現在位置へのポインタ
  // 解凍処理の実行
  // Z_FINISH: 入力データの終端まで解凍を続ける
  // 戻り値:
  //   Z_STREAM_END: 正常に解凍が完了
  //   Z_OK: 解凍は続行中
  //   Z_BUF_ERROR: 出力バッファが不足
  //   Z_DATA_ERROR: 入力データが不正
  int ret = inflate(&strm, Z_FINISH);
  if(ret != Z_STREAM_END){
    inflateEnd(&strm);
    throw std::runtime_error("inflate failed");
  }  
  // 実際に解凍されたデータサイズに合わせてバッファをリサイズ
  image_data_decompressed.resize(decompressed_size - strm.avail_out);
  // 使用したリソースを解放
  inflateEnd(&strm);
}
void PNG::compress_data(void){
  z_stream strm;
  strm.zalloc = Z_NULL;
  strm.zfree = Z_NULL;
  strm.opaque = Z_NULL;
  strm.avail_in = image_data_decompressed.size();
  strm.next_in = reinterpret_cast<Bytef*>(image_data_decompressed.data());
  if(deflateInit(&strm, Z_DEFAULT_COMPRESSION) != Z_OK){
    throw std::runtime_error("deflateInit failed");
  }
  size_t compressed_size = static_cast<size_t>(image_data_decompressed.size() * 1.1) + 12;
  image_data_compressed.resize(compressed_size);
  strm.avail_out = compressed_size;
  strm.next_out = reinterpret_cast<Bytef*>(image_data_compressed.data());
  int ret = deflate(&strm, Z_FINISH);
  if(ret != Z_STREAM_END){
    deflateEnd(&strm);
    throw std::runtime_error("deflate failed");
  }
  image_data_compressed.resize(compressed_size - strm.avail_out);
  deflateEnd(&strm);
}
void PNG::write(const std::string& path) const{
  // 出力ファイルを開く
  std::ofstream ofs(path, std::ios::out | std::ios::binary);
  if(!ofs){
    throw std::runtime_error("Failed to open output file");
  }
  // PNGシグネチャを書き込む
  const unsigned char signature[] = {
    static_cast<unsigned char>(0x89),
    static_cast<unsigned char>(0x50),
    static_cast<unsigned char>(0x4E),
    static_cast<unsigned char>(0x47),
    static_cast<unsigned char>(0x0D),
    static_cast<unsigned char>(0x0A),
    static_cast<unsigned char>(0x1A),
    static_cast<unsigned char>(0x0A)
  };
  ofs.write(reinterpret_cast<const char*>(signature), 8);
  // 各チャンクを書き込む
  for(const Chunk& chunk : chunks){
    // チャンクの長さを書き込む
    std::vector<char> length_data = int2vecchar(chunk.length);
    ofs.write(length_data.data(), 4);
    // チャンクタイプを書き込む
    ofs.write(chunk.type_string.c_str(), 4);
    // チャンクデータを書き込む
    std::vector<char> chunk_data = std::visit([](const auto& data) {
      return data.get();
    }, chunk.data);
    ofs.write(chunk_data.data(), chunk_data.size());
    // CRCを計算して書き込む
    std::vector<char> crc_data;
    crc_data.insert(crc_data.end(), chunk.type_string.begin(), chunk.type_string.end());
    crc_data.insert(crc_data.end(), chunk_data.begin(), chunk_data.end());
    uint32_t crc = chunk.calc_crc(crc_data, 0, crc_data.size());
    std::vector<char> crc_bytes = int2vecchar(crc);
    ofs.write(crc_bytes.data(), 4);
  }
  ofs.close();
}
void PNG::debug(void) const{
  // チャンク情報の表示
  for(const Chunk& chunk: chunks){
    chunk.debug();
  }
  // 解凍されたデータの表示
  std::cout << "Decompressed data size: " << image_data_decompressed.size() << " bytes" << std::endl;
  std::cout << "Decompressed data:" << std::endl;
  for(int i = 0; i < image_data_decompressed.size(); i++){
    if(i%(this->width*3 + 1) == 0) std::cout << std::endl << "\t";
    std::cout << std::format("{:02X} ", image_data_decompressed[i]);
  }
  std::cout << std::endl;
}

int main(){
  PNG png{"./new.png"};
  png.debug();
  return 0;
}

