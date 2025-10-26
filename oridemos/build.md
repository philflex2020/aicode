sudo apt-get update && sudo apt-get install -y git build-essential cmake python3-pip
sudo apt install g++-12 gcc-12
sudo apt install ccache
sudo apt install g++-12 gcc-12
sudo apt install g++-10 gcc-10


export CPLUS_INCLUDE_PATH=/usr/include/c++/10:/usr/include/aarch64-linux-gnu/c++/10
export CPATH=/usr/lib/gcc/aarch64-linux-gnu/10/include
export LIBRARY_PATH=/usr/lib/gcc/aarch64-linux-gnu/10

cmake -B build   -DGGML_CUDA=ON -DLLAMA_NATIVE=OFF -DCMAKE_BUILD_TYPE=Release   -DCMAKE_C_COMPILER=/usr/bin/gcc-10   -DCMAKE_CXX_COMPILER=/usr/bin/g++-10   -DCMAKE_CUDA_HOST_COMPILER=/usr/bin/g++-10   -DCMAKE_CUDA_ARCHITECTURES=87   -DCMAKE_CUDA_FLAGS="--std=c++17 --expt-relaxed-constexpr --expt-extended-lambda \
    -Xcompiler=-fPIC,-O3 \
    -isystem /usr/include/c++/10 \
    -isystem /usr/include/aarch64-linux-gnu/c++/10 \
    -I/usr/lib/gcc/aarch64-linux-gnu/10/include"


