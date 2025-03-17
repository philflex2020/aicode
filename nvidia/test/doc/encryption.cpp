To utilize OpenSSL for AES encryption with optimizations such as NEON, you typically don't need to handle the SIMD (Single Instruction, Multiple Data) aspects directly in your code. OpenSSL has been optimized internally to use NEON on ARM architectures where available. Your role is to correctly implement AES using the OpenSSL APIs, and the library will handle the performance optimizations behind the scenes.

Here’s how you can set up and use OpenSSL for AES encryption in a simple C++ application. This example will focus on setting up AES encryption and decryption using the AES-256-CBC mode, which is common for many applications.

### Step 1: Install OpenSSL
Ensure that OpenSSL is installed on your system. If you're using a Debian-based system, you can install it via:

```bash
sudo apt-get install libssl-dev
```

### Step 2: Sample Code for AES Encryption/Decryption

Here's a basic example of how to encrypt and decrypt data using OpenSSL's AES functionalities:

```cpp
#include <openssl/aes.h>
#include <openssl/rand.h>
#include <iostream>
#include <cstring>

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
```

### Step 3: Compile the Program
To compile this program, link it against OpenSSL's libraries:

```bash
g++ aes_example.cpp -o aes_example -lssl -lcrypto
```

### Explanation
- **Key and IV Generation**: The `RAND_bytes` function is used to generate a random key and initialization vector (IV). Both are crucial for security in AES encryption.
- **Encryption and Decryption**: `AES_cbc_encrypt` is used for both encryption and decryption. The encrypt and decrypt functions handle the process block by block.

### Note
While the above example uses CBC mode for its simplicity in demonstration, you should evaluate the appropriate mode (like GCM for authenticated encryption) based on your security requirements.

By using OpenSSL, you leverage its internally optimized routines for encryption, which include SIMD optimizations on platforms supporting NEON. This setup allows you to focus on correctly implementing encryption logic without delving into the low-level details of cryptographic optimizations.