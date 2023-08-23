#include <iostream>
#include <cmath>
#include <vector>


const int NUM_SAMPLES = 1024;
int32_t sine_wave[NUM_SAMPLES];

int main() {
    std::vector<uint32_t> ivec;

    // Frequency and amplitude for the sine wave
    const double frequency = 1.0;
    const double amplitude = (1 << 23) - 1; // 24-bit maximum amplitude

    // Fill the array with the sine wave values
    for(int i = 0; i < NUM_SAMPLES; ++i) {
        double t = (double)i / NUM_SAMPLES;
        double value = amplitude * std::sin(2.0 * M_PI * frequency * t);
    
        // Convert the floating-point value to 24-bit integer
        sine_wave[i] = static_cast<int32_t>(value); 
        ivec.emplace_back(sine_wave[i]);

    }

    // Print the first 10 samples to verify
    for(int i = 0; i < 10; ++i) {
        std::cout << sine_wave[i] << '\n';
    }
    std::cout << " processing data " << std::endl;
    uint32_t ilast=0;
    int idx = 0;
    double ddiff = 0.0;
    double mdiff = 0.0;
    int fours = 0;
    int threes = 0;
    int twos = 0;
    int ones = 0;
    for ( auto ival:ivec)
    {
        auto diff = (int)(ilast-ival);
        ddiff += diff;
        if (diff > mdiff)
            mdiff = diff;
        if ( std::abs(diff) > 0x7fffff)
        {
            fours++;
            //std::cout <<std::dec << " idx :"<<idx << " val :" << ival << " diff " << std::hex<< std::abs(diff) <<'\n';

        }
        else if ( std::abs(diff) > 0x7fff)
        {
            threes++;
            //std::cout <<std::dec << " idx :"<<idx << " val :" << ival << " diff " << std::hex<< std::abs(diff) <<'\n';

        }
        else if ( std::abs(diff) > 0x7f)
        {
            twos++;
        }
        else 
        {
            ones++;
        }


        //std::cout << ival << " diff " << std::hex<<(int)(ilast-ival) <<'\n';
        ilast = ival;
        idx++;
    }
    std::cout << " avg diff " << ddiff/NUM_SAMPLES  <<'\n';
    std::cout << " max diff " << std::hex << (int)mdiff  <<'\n';
    std::cout << " fours  " << std::dec<< fours  <<'\n';
    std::cout << " threes " << std::dec<< threes  <<'\n';
    std::cout << " twos   " << std::dec<< twos  <<'\n';
    std::cout << " ones   " << std::dec<< ones  <<'\n';
    std::cout << " raw size   " << std::dec<< (NUM_SAMPLES*4)  <<'\n';
    int new_size = ( fours * 4) + ( threes * 3) + (twos * 2) + ones;
    std::cout << " new size   " << std::dec<< new_size  <<'\n';
    std::cout << " comp   " << std::dec<< 100.0 - (new_size * 100.0 / (NUM_SAMPLES*4))  <<'\n';
    
}
