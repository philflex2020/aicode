#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#include <iostream>
#include <cstring>
//g++ enc_new.cpp -o enc -lcrypto
void handleErrors() {
    std::cerr << "An error occurred." << std::endl;
    ERR_print_errors_fp(stderr);
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
    unsigned char key[32], iv[16];
    unsigned char *plaintext = (unsigned char *)"The quick brown fox jumps over the lazy dog";
    unsigned char ciphertext[128], decryptedtext[128];
    int decryptedtext_len, ciphertext_len;

    if(!RAND_bytes(key, sizeof(key)) || !RAND_bytes(iv, sizeof(iv)))
        handleErrors();

    encrypt(plaintext, strlen((char *)plaintext), key, iv, ciphertext, ciphertext_len);
    std::cout << "Ciphertext is:\n";
    BIO_dump_fp(stdout, (const char *)ciphertext, ciphertext_len);

    decrypt(ciphertext, ciphertext_len, key, iv, decryptedtext, decryptedtext_len);
    decryptedtext[decryptedtext_len] = '\0';

    std::cout << "Decrypted text is:\n" << decryptedtext << std::endl;

    return 0;
}
