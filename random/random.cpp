#include <iostream>
#include <random>
#include <vector>

int main() {
    std::vector<uint32_t> ivec;
    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_int_distribution<uint32_t> distribution(0, (1 << 24) - 1);

    // Print 10 random 24-bit integers
    for(int i = 0; i < 100; ++i) {
        uint32_t random_24bit = distribution(generator);
        ivec.emplace_back((uint32_t)random_24bit);
        std::cout << random_24bit << '\n';
    }
    std::cout << " processing data " << std::endl;
    uint32_t ilast=0;
    for ( auto ival:ivec)
    {

        std::cout << ival << " diff " << (int)(ilast-ival) <<'\n';
        ilast = ival;

    }
    return 0;
}
