#include "encryption.h"
#include <iostream>
#include <cstdlib>
#include <ctime>
#include <vector>

/**
 * Encrypts the input data using XOR with a randomly generated key based on file size
 * 
 * @param input Pointer to the input data to be encrypted
 * @param size Size of the input data in bytes
 * @return Pointer to the encrypted data (caller must free this memory)
 */
UCHAR* encryptFile(UCHAR *input, long size) {
    if (!input || size <= 0) {
        std::cerr << "Error: Invalid input for encryption" << std::endl;
        return nullptr;
    }
    
    // Seed the random number generator with file size
    srand(static_cast<unsigned int>(size));
    
    // Generate a key (limiting to reasonable ASCII range)
    int key = rand() % 69;
    std::cout << "Generated encryption key: " << key << std::endl;
    
    // Allocate memory for encrypted output
    std::vector<UCHAR> encryptedData(size);
    
    // Encrypt using XOR with the generated key
    for (long i = 0; i < size; ++i) {
        encryptedData[i] = input[i] ^ key;
    }
    
    // Create a persistent copy to return
    UCHAR* result = new UCHAR[size];
    std::copy(encryptedData.begin(), encryptedData.end(), result);
    
    return result;
}

/**
 * Encrypts the input data using XOR with a derived key based on the user-provided key
 * 
 * @param input Pointer to the input data to be encrypted
 * @param size Size of the input data in bytes
 * @param userKey User-provided encryption key
 * @return Pointer to the encrypted data (caller must free this memory)
 */
UCHAR* encryptFile(UCHAR *input, long size, int userKey) {
    if (!input || size <= 0 || userKey <= 0) {
        std::cerr << "Error: Invalid input for encryption" << std::endl;
        return nullptr;
    }
    
    std::cout << "Using provided key: " << userKey << std::endl;
    
    // Derive a working key from the user-provided key
    // This ensures the key is in a reasonable ASCII range
    srand(static_cast<unsigned int>(userKey));
    int derivedKey = rand() % 69;
    std::cout << "Derived working key: " << derivedKey << std::endl;
    
    // Allocate memory for encrypted output
    std::vector<UCHAR> encryptedData(size);
    
    // Encrypt using XOR with the derived key
    for (long i = 0; i < size; ++i) {
        encryptedData[i] = input[i] ^ derivedKey;
    }
    
    // Create a persistent copy to return
    UCHAR* result = new UCHAR[size];
    std::copy(encryptedData.begin(), encryptedData.end(), result);
    
    return result;
}
