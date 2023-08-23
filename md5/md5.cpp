#include <iostream>
#include <iomanip>
#include <cstring>
#include <string>
#include <sstream>
#include <stdint.h>

// Constants for MD5 algorithm
const uint32_t A = 0x67452301;
const uint32_t B = 0xefcdab89;
const uint32_t C = 0x98badcfe;
const uint32_t D = 0x10325476;

// Rotate left operation
#define LEFT_ROTATE(x, c) (((x) << (c)) | ((x) >> (32 - (c))))

// MD5 round functions
#define F(x, y, z) (((x) & (y)) | (~(x) & (z)))
#define G(x, y, z) (((x) & (z)) | ((y) & ~(z)))
#define H(x, y, z) ((x) ^ (y) ^ (z))
#define I(x, y, z) ((y) ^ ((x) | ~(z)))

// MD5 main loop
#define MD5_ROUND(a, b, c, d, f, k, s) \
    a = LEFT_ROTATE((a + f + k + block[i]), s) + b;

std::string md5(const std::string& input) {
    uint32_t block[16]; // 512-bit block
    uint32_t a = A, b = B, c = C, d = D;

    for (size_t i = 0; i < input.length(); i += 64) {
        memset(block, 0, sizeof(block));

        for (size_t j = 0; j < 64 && (i + j) < input.length(); ++j) {
            block[j / 4] |= static_cast<uint32_t>(input[i + j]) << (8 * (j % 4));
        }

        uint32_t aa = a, bb = b, cc = c, dd = d;

        MD5_ROUND(a, b, c, d, F(b, c, d), 0xd76aa478, 7);
        MD5_ROUND(d, a, b, c, F(a, b, c), 0xe8c7b756, 12);
        MD5_ROUND(c, d, a, b, F(d, a, b), 0x242070db, 17);
        MD5_ROUND(b, c, d, a, F(c, d, a), 0xc1bdceee, 22);

        MD5_ROUND(a, b, c, d, F(b, c, d), 0xf57c0faf, 7);
        MD5_ROUND(d, a, b, c, F(a, b, c), 0x4787c62a, 12);
        MD5_ROUND(c, d, a, b, F(d, a, b), 0xa8304613, 17);
        MD5_ROUND(b, c, d, a, F(c, d, a), 0xfd469501, 22);

        MD5_ROUND(a, b, c, d, F(b, c, d), 0x698098d8, 7);
        MD5_ROUND(d, a, b, c, F(a, b, c), 0x8b44f7af, 12);
        MD5_ROUND(c, d, a, b, F(d, a, b), 0xffff5bb1, 17);
        MD5_ROUND(b, c, d, a, F(c, d, a), 0x895cd7be, 22);

        MD5_ROUND(a, b, c, d, H(b, c, d), 0x6b901122, 7);
        MD5_ROUND(d, a, b, c, H(a, b, c), 0xfd987193, 12);
        MD5_ROUND(c, d, a, b, H(d, a, b), 0xa679438e, 17);
        MD5_ROUND(b, c, d, a, H(c, d, a), 0x49b40821, 22);

        a += aa;
        b += bb;
        c += cc;
        d += dd;
    }

    // Format the result
    std::stringstream stream;
    stream << std::hex << std::setfill('0') << std::setw(8) << a
           << std::setw(8) << b
           << std::setw(8) << c
           << std::setw(8) << d;

    return stream.str();
}

int main() {
    std::string input = "Hello, world!";
    std::string md5Hash = md5(input);

    std::cout << "MD5 Hash: " << md5Hash << std::endl;

    return 0;
}
