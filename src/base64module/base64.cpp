#include "libbase64.h"
#include <string>
#include <stdlib.h>
#include <sys/stat.h>

std::string base64Encode(const std::string& input) {
    size_t encoded_len = input.length() * 4/3 + 2;
    char* encoded = new char[encoded_len + 1]; // +1 for null termination
    base64_encode(input.c_str(), input.length(), encoded, &encoded_len, 0);
    encoded[encoded_len] = '\0';
    std::string result(encoded);
    delete[] encoded;
    return result;
}

std::string base64Decode(const std::string& input) {
    size_t decoded_len = input.length();
    char* decoded = new char[decoded_len + 1]; 
    base64_decode(input.c_str(), input.length(), decoded, &decoded_len, 0);
    decoded[decoded_len] = '\0';
    std::string result(decoded);
    delete[] decoded;
    return result;
}


void base64StreamEncode(const std::string& inputFilename, const std::string& outputFilename) {
    FILE* inputFile = fopen(inputFilename.c_str(), "rb"); // 以二进制读取模式打开输入文件
    if (inputFile == nullptr) {
        perror("error opening input file");
        perror(inputFilename.c_str());
        return;
    }
    FILE* outputFile = fopen(outputFilename.c_str(), "wb");
    if (outputFile == nullptr) {
        perror("Error opening output file");
        perror(outputFilename.c_str());
        return;
    }

    size_t nread, nout;
    
    struct stat fstat;
    long infileSize = 0, outfileSize = 0;
    if (stat(inputFilename.c_str(), &fstat) == 0) {
        infileSize = fstat.st_size;
        outfileSize = infileSize * 4/3 + 10;
    }

    char* buf = new char[infileSize+1];
    char* out = new char[outfileSize];

    struct base64_state state;

	// Initialize stream encoder:
	base64_stream_encode_init(&state, 0);

    // Read contents of stdin into buffer:
	while ((nread = fread(buf, 1, sizeof(buf), inputFile)) > 0) {

		// Encode buffer:
		base64_stream_encode(&state, buf, nread, out, &nout);

		// If there's output, print it to stdout:
		if (nout) {
			fwrite(out, nout, 1, outputFile);
		}

		// If an error occurred, exit the loop:
		if (feof(inputFile)) {
			break;
		}
	}

    base64_stream_encode_final(&state, out, &nout);
    if (nout) {
        fwrite(out, nout, 1, outputFile);
    }

    fclose(inputFile);
    fclose(outputFile);
    delete[] buf;
    delete[] out;
    return;
}

void base64StreamDecode(const std::string& inputFilename, const  std::string& outputFilename) {
    FILE* inputFile = fopen(inputFilename.c_str(), "rb"); // 以二进制读取模式打开输入文件
    if (inputFile == nullptr) {
        perror("error opening input file");
        perror(inputFilename.c_str());
        return;
    }
    FILE* outputFile = fopen(outputFilename.c_str(), "wb");
    if (outputFile == nullptr) {
        perror("Error opening output file");
        perror(outputFilename.c_str());
        return;
    }

    size_t nread, nout;
    
    struct stat fstat;
    long infileSize = 0, outfileSize = 0;
    if (stat(inputFilename.c_str(), &fstat) == 0) {
        infileSize = fstat.st_size;
        outfileSize = infileSize;
    }

    char* buf = new char[infileSize+1];
    char* out = new char[outfileSize];

    struct base64_state state;

	// Initialize stream encoder:
	base64_stream_decode_init(&state, 0);

    // Read contents of stdin into buffer:
	while ((nread = fread(buf, 1, sizeof(buf), inputFile)) > 0) {

		// Encode buffer:
		base64_stream_decode(&state, buf, nread, out, &nout);

		// If there's output, print it to stdout:
		if (nout) {
			fwrite(out, nout, 1, outputFile);
		}

		// If an error occurred, exit the loop:
		if (feof(inputFile)) {
			break;
		}
	}

    fclose(inputFile);
    fclose(outputFile);
    delete[] buf;
    delete[] out;
    return;
}
