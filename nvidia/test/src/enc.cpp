#include <openssl/aes.h>
#include <openssl/rand.h>
#include <iostream>
#include <cstring>
//g++ enc.cpp -o enc -lssl -lcrypto
void handleErrors(void) {
    std::cerr << "Error occurred." << std::endl;
    exit(1);
}

void encrypt(unsigned char *plaintext, int plaintext_len, unsigned char *key,
             unsigned char *iv, unsigned char *ciphertext) {
    AES_KEY enc_key;
    if (AES_set_encrypt_key(key, 256, &enc_key) != 0)
        handleErrors();

    int num_blocks = plaintext_len / AES_BLOCK_SIZE;
    for (int i = 0; i < num_blocks; i++) {
        AES_cbc_encrypt(plaintext + (i * AES_BLOCK_SIZE), ciphertext + (i * AES_BLOCK_SIZE), AES_BLOCK_SIZE, &enc_key, iv, AES_ENCRYPT);
    }
}

void decrypt(unsigned char *ciphertext, int ciphertext_len, unsigned char *key,
             unsigned char *iv, unsigned char *plaintext) {
    AES_KEY dec_key;
    if (AES_set_decrypt_key(key, 256, &dec_key) != 0)
        handleErrors();

    int num_blocks = ciphertext_len / AES_BLOCK_SIZE;
    for (int i = 0; i < num_blocks; i++) {
        AES_cbc_encrypt(ciphertext + (i * AES_BLOCK_SIZE), plaintext + (i * AES_BLOCK_SIZE), AES_BLOCK_SIZE, &dec_key, iv, AES_DECRYPT);
    }
}

int main() {
    // Message to be encrypted
    unsigned char plaintext[] = "This is a test message for encryption";

    // Buffer for the ciphertext
    unsigned char ciphertext[128];

    // Buffer for the decrypted text
    unsigned char decryptedtext[128];

    // The key should be 256 bits (32 bytes)
    unsigned char key[32];
    RAND_bytes(key, sizeof(key));

    // The IV should be the AES block size
    unsigned char iv[AES_BLOCK_SIZE];
    RAND_bytes(iv, sizeof(iv));

    // Encrypt the plaintext
    encrypt(plaintext, sizeof(plaintext), key, iv, ciphertext);
    std::cout << "Ciphertext is:" << std::endl;
    for(int i = 0; i < sizeof(plaintext); i++)
        std::cout << std::hex << static_cast<int>(ciphertext[i]);
    std::cout << std::endl;

    // Decrypt the ciphertext
    decrypt(ciphertext, sizeof(plaintext), key, iv, decryptedtext);
    decryptedtext[sizeof(plaintext) - 1] = '\0'; // Null terminate the string

    std::cout << "Decrypted text is:" << std::endl;
    std::cout << decryptedtext << std::endl;

    return 0;
}