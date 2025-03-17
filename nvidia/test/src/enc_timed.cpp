#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#include <iostream>
#include <cstring>
#include <chrono> // For high-resolution timing

void handleErrors() {
    std::cerr << "An error occurred." << std::endl;
    ERR_print_errors_fp(stderr); // This will print the error stack
    exit(EXIT_FAILURE);
}

void encrypt(unsigned char *plaintext, int plaintext_len, unsigned char *key,
             unsigned char *iv, unsigned char *ciphertext, int &ciphertext_len) {
    EVP_CIPHER_CTX *ctx;

    int len;

    if(!(ctx = EVP_CIPHER_CTX_new())) handleErrors();

    if(1 != EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv))
        handleErrors();

    if(1 != EVP_EncryptUpdate(ctx, ciphertext, &len, plaintext, plaintext_len))
        handleErrors();
    ciphertext_len = len;

    if(1 != EVP_EncryptFinal_ex(ctx, ciphertext + len, &len)) handleErrors();
    ciphertext_len += len;

    EVP_CIPHER_CTX_free(ctx);
}

void decrypt(unsigned char *ciphertext, int ciphertext_len, unsigned char *key,
             unsigned char *iv, unsigned char *plaintext, int &plaintext_len) {
    EVP_CIPHER_CTX *ctx;

    int len;

    if(!(ctx = EVP_CIPHER_CTX_new())) handleErrors();

    if(1 != EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv))
        handleErrors();

    if(1 != EVP_DecryptUpdate(ctx, plaintext, &len, ciphertext, ciphertext_len))
        handleErrors();
    plaintext_len = len;

    if(1 != EVP_DecryptFinal_ex(ctx, plaintext + len, &len)) handleErrors();
    plaintext_len += len;

    EVP_CIPHER_CTX_free(ctx);
}

int main() {
    const int buffer_size = 32768; // 32K buffer
    unsigned char key[32], iv[16];
    unsigned char *plaintext = new unsigned char[buffer_size];
    unsigned char *ciphertext = new unsigned char[buffer_size + EVP_MAX_BLOCK_LENGTH];
    unsigned char *decryptedtext = new unsigned char[buffer_size + EVP_MAX_BLOCK_LENGTH];
    int decryptedtext_len, ciphertext_len;

    memset(plaintext, 'A', buffer_size); // Fill plaintext with 'A'

    if(!RAND_bytes(key, sizeof(key)) || !RAND_bytes(iv, sizeof(iv)))
        handleErrors();

    auto start_enc = std::chrono::high_resolution_clock::now();
    encrypt(plaintext, buffer_size, key, iv, ciphertext, ciphertext_len);
    auto end_enc = std::chrono::high_resolution_clock::now();

    auto start_dec = std::chrono::high_resolution_clock::now();
    decrypt(ciphertext, ciphertext_len, key, iv, decryptedtext, decryptedtext_len);
    auto end_dec = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double, std::milli> enc_time = end_enc - start_enc;
    std::chrono::duration<double, std::milli> dec_time = end_dec - start_dec;

    std::cout << "Encryption time: " << enc_time.count() << " ms\n";
    std::cout << "Decryption time: " << dec_time.count() << " ms\n";

    // Clean up
    delete[] plaintext;
    delete[] ciphertext;
    delete[] decryptedtext;

    return 0;
}
