#include "libbase64.h"
#include <string>

std::string base64Encode(const std::string& input) {
    size_t encoded_len = input.length() * 4/3 + 2;
    char* encoded = new char[encoded_len + 1]; // +1 for null termination
    base64_encode(input.c_str(), input.length(), encoded, &encoded_len, 0);
    encoded[encoded_len] = '\0';
    std::string result(encoded);
    delete[] encoded;
    return result;
}
