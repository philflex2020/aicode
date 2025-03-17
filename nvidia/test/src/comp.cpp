#include <iostream>
#include <zlib.h>
#include <cstring>
#include <chrono>
#include <random>
#include <cstdint>
//g++ -o comp comp.cpp -lz

// Function to fill a buffer with random data
void fillRandomData(uint8_t* buffer, size_t size) {
    // Obtain a seed from the system clock
    unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
    std::mt19937 generator(seed); // Standard mersenne_twister_engine seeded with rd()
    std::uniform_int_distribution<int> distribution(0, 255);

    for (size_t i = 0; i < size; ++i) {
        buffer[i] = static_cast<uint8_t>(distribution(generator));
    }
}


void compressBuffer(unsigned char* source, size_t sourceLen, unsigned char* dest, size_t& destLen) {
    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = sourceLen;
    strm.next_in = source;
    strm.avail_out = destLen;
    strm.next_out = dest;

    deflateInit(&strm, Z_DEFAULT_COMPRESSION);
    
    int ret = deflate(&strm, Z_FINISH); // Finish the compression
    if (ret != Z_STREAM_END) {
        std::cerr << "Deflate failed with error " << ret << std::endl;
    }
    destLen = strm.total_out;

    deflateEnd(&strm);
}

void decompressBuffer(unsigned char* source, size_t sourceLen, unsigned char* dest, size_t& destLen) {
    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = sourceLen;
    strm.next_in = source;
    strm.avail_out = destLen;
    strm.next_out = dest;

    inflateInit(&strm);
    
    int ret = inflate(&strm, Z_FINISH);
    if (ret != Z_STREAM_END) {
        std::cerr << "Inflate failed with error " << ret << std::endl;
    }
    destLen = strm.total_out;

    inflateEnd(&strm);
}
// void compressBuffer(unsigned char* source, size_t sourceLen, unsigned char* dest, size_t& destLen) {
//     z_stream strm;
//     strm.zalloc = Z_NULL;
//     strm.zfree = Z_NULL;
//     strm.opaque = Z_NULL;
    
//     if (deflateInit(&strm, Z_DEFAULT_COMPRESSION) != Z_OK) {
//         std::cerr << "deflateInit failed!" << std::endl;
//         return;
//     }

//     strm.avail_in = sourceLen;
//     strm.next_in = source;
//     strm.avail_out = destLen;
//     strm.next_out = dest;

//     if (deflate(&strm, Z_FINISH) != Z_STREAM_END) {
//         std::cerr << "deflate failed!" << std::endl;
//     }
//     destLen = strm.total_out;

//     deflateEnd(&strm);
// }

// void decompressBuffer(unsigned char* source, size_t sourceLen, unsigned char* dest, size_t& destLen) {
//     z_stream strm;
//     strm.zalloc = Z_NULL;
//     strm.zfree = Z_NULL;
//     strm.opaque = Z_NULL;

//     if (inflateInit(&strm) != Z_OK) {
//         std::cerr << "inflateInit failed!" << std::endl;
//         return;
//     }

//     strm.avail_in = sourceLen;
//     strm.next_in = source;
//     strm.avail_out = destLen;
//     strm.next_out = dest;

//     if (inflate(&strm, Z_NO_FLUSH) != Z_STREAM_END) {
//         std::cerr << "inflate failed!" << std::endl;
//     }
//     destLen = strm.total_out;

//     inflateEnd(&strm);
// }

int main() {
    const size_t bufferSize = 32768; // 32K buffer
    unsigned char original[bufferSize];
    unsigned char compressed[bufferSize];
    unsigned char decompressed[bufferSize];

    memset(original, 'A', bufferSize); // Fill original with 'A'
    fillRandomData(original, bufferSize-1024);

    size_t compressedSize = bufferSize;
    size_t decompressedSize = bufferSize;

    auto start_compression = std::chrono::high_resolution_clock::now();
    compressBuffer(original, bufferSize-1024, compressed, compressedSize);
    auto end_compression = std::chrono::high_resolution_clock::now();

    auto start_decompression = std::chrono::high_resolution_clock::now();
    decompressBuffer(compressed, compressedSize, decompressed, decompressedSize);
    auto end_decompression = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double, std::milli> compression_time = end_compression - start_compression;
    std::chrono::duration<double, std::milli> decompression_time = end_decompression - start_decompression;

    std::cout << "Compression time: " << compression_time.count() << " ms\n";
    std::cout << "Decompression time: " << decompression_time.count() << " ms\n";
    std::cout << "Original size: " << bufferSize << " bytes\n";
    std::cout << "Compressed size: " << compressedSize << " bytes\n";
    std::cout << "Decompressed size: " << decompressedSize << " bytes\n";

    return 0;
}
