#include <string>

std::string base64Encode(const std::string& input);
std::string base64Decode(const std::string& input);

/* 从inputFile读取块内容进行编码，输出到outputFile */
void base64StreamEncode(const std::string& inputFile, const std::string& outputFile);

/* 从inputFile读取编码后的二进制文件内容，生成解码内容输出到outputFile */
void base64StreamDecode(const std::string& inputFile, const  std::string& outputFile);