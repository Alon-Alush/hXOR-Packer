#include "packingInfo.h"
#include <iostream>
#include <fstream>
#include <string>
#include <memory>
#include <vector>
#include <algorithm>

// Error message strings
const char *PEerrors_str[] = {
    "Success",
    "Archive empty; no files to extract",
    "File in supplied path not found",
    "Could not create output archive file",
    "Failed to open one of the files",
    "Invalid Parameter",
    "Input file is not a valid executable file"
};

// Parameter description strings
const char *Parameter_str[] = {
    "None",
    "Compression Selected",
    "Encryption Selected",
    "Compression and Encryption Selected"
};

// Unpacker stub filename
const char unpackerStub[] = "unpackerLoadEXE.exe";

/**
 * Pack an EXE file into a self-extracting archive
 * 
 * Final structure:
 * [ Unpacker stub ] [ BIN signature ] [ pdata ] [ EXE Image ]
 * 
 * @param count Argument count
 * @param argv Arguments array
 * @return Status code from PEerrors enum
 */
int packFileIntoArchive(int count, char *argv[]) {
    // Extract source and destination paths
    const char *srcPath = argv[1];
    const char *dstPath = argv[2];
    
    // Display paths
    std::cout << "Input Path: " << srcPath << "\nOutput Path: " << dstPath << "\n\n";
    
    // Validate the input path
    if (GetFileAttributes(srcPath) == INVALID_FILE_ATTRIBUTES) {
        return PEerrorPath;
    }
    
    // Verify that the input file is a valid EXE
    if (validExeFile(srcPath) == PEerrorInputNotEXE) {
        return PEerrorInputNotEXE;
    }
    
    // Get file size
    HANDLE hFile = CreateFile(srcPath, 
                             GENERIC_READ, 
                             FILE_SHARE_READ | FILE_SHARE_WRITE,
                             nullptr, 
                             OPEN_EXISTING, 
                             FILE_FLAG_SEQUENTIAL_SCAN, 
                             nullptr);
    
    if (hFile == INVALID_HANDLE_VALUE) {
        return PEerrorCouldNotOpenArchive;
    }
    
    DWORD fileSize = GetFileSize(hFile, nullptr);
    CloseHandle(hFile);
    
    // Initialize packdata structure
    packdata_t pdata = {0};
    
    // Extract filename from the path
    char fullPath[MAX_PATH];
    char *filename;
    
    if (GetFullPathName(srcPath, MAX_PATH, fullPath, &filename) == 0) {
        return PEerrorPath;
    }
    
    // Copy filename and size to pdata
    strncpy(pdata.filename, filename, sizeof(pdata.filename) - 1);
    pdata.filename[sizeof(pdata.filename) - 1] = '\0';  // Ensure null termination
    pdata.filesize = fileSize;
    
    // Verify unpacker stub exists
    if (GetFileAttributes(unpackerStub) == INVALID_FILE_ATTRIBUTES) {
        std::cerr << "Unpacker stub exe not found!" << std::endl;
        return PEerrorCannotCreateArchive;
    }
    
    // Ensure destination has .exe extension
    std::string destPath(dstPath);
    if (destPath.length() < 4 || destPath.substr(destPath.length() - 4) != ".exe") {
        destPath += ".exe";
    }
    
    // Create destination file by copying the unpacker stub
    if (!CopyFile(unpackerStub, destPath.c_str(), FALSE)) {
        std::cerr << "Could not create SFX file!" << std::endl;
        return PEerrorCannotCreateArchive;
    }
    
    // Open the destination for updating
    FILE *packedEXE = fopen(destPath.c_str(), "rb+");
    if (!packedEXE) {
        return PEerrorCannotCreateArchive;
    }
    
    // Seek to the end and record stub size
    fseek(packedEXE, 0, SEEK_END);
    long stubSize = ftell(packedEXE);
    
    // Write signature
    const long myPEsignature = 'AFIF';
    fwrite(&myPEsignature, sizeof(myPEsignature), 1, packedEXE);
    
    // Process command-line parameters
    int parameter = PREmpty;
    int key = 0;
    bool keyProvided = false;
    
    if (count == 3) {
        parameter = PREmpty;
    } else if (count >= 4) {
        std::string param(argv[3]);
        
        if (param == "-c") {
            parameter = PRCompression;
        } else if (param == "-e") {
            parameter = PREncrpytion;
        } else if (param == "-ce") {
            parameter = PRBoth;
        } else {
            fclose(packedEXE);
            return PEerrorInvalidParameter;
        }
        
        // Check for encryption key
        if (count == 5) {
            try {
                key = std::stoi(argv[4]);
                if (key == 0) {
                    fclose(packedEXE);
                    return PEerrorInvalidParameter;
                }
                keyProvided = true;
            } catch (...) {
                fclose(packedEXE);
                return PEerrorInvalidParameter;
            }
        }
    }
    
    // Process the input file
    FILE *inFile = fopen(srcPath, "rb");
    if (!inFile) {
        fclose(packedEXE);
        return PEerrorCouldNotOpenArchive;
    }
    
    // Create a huffman compressor
    std::unique_ptr<huffman> huf(new huffman());
    
    // Read the input file
    std::vector<UCHAR> inputData(fileSize);
    if (fread(inputData.data(), 1, fileSize, inFile) != fileSize) {
        fclose(inFile);
        fclose(packedEXE);
        return PEerrorCouldNotOpenArchive;
    }
    
    // Apply compression and/or encryption based on parameters
    UCHAR *output = nullptr;
    int outSize = 0;
    std::vector<UCHAR> encryptedData;
    
    std::cout << "Option: " << Parameter_str[parameter] << std::endl;
    
    switch (parameter) {
        case PREmpty:  // No processing
            output = inputData.data();
            outSize = fileSize;
            break;
            
        case PRCompression:  // Compression only
            std::cout << "\nCompressing >>>> '" << pdata.filename << "' [" << pdata.filesize << "]\n";
            outSize = huf->Compress(inputData.data(), fileSize);
            std::cout << "Compressed Size: " << outSize << std::endl;
            output = huf->getOutput();
            break;
            
        case PREncrpytion:  // Encryption only
            std::cout << "\nEncrypting >>>> '" << pdata.filename << "'\n";
            if (keyProvided) {
                output = encryptFile(inputData.data(), fileSize, key);
            } else {
                output = encryptFile(inputData.data(), fileSize);
            }
            outSize = fileSize;
            break;
            
        case PRBoth:  // Both compression and encryption
            std::cout << "\nEncrypting >>>> '" << pdata.filename << "'\n";
            
            if (keyProvided) {
                encryptedData.resize(fileSize);
                UCHAR* encData = encryptFile(inputData.data(), fileSize, key);
                std::copy(encData, encData + fileSize, encryptedData.data());
                delete[] encData;
            } else {
                encryptedData.resize(fileSize);
                UCHAR* encData = encryptFile(inputData.data(), fileSize);
                std::copy(encData, encData + fileSize, encryptedData.data());
                delete[] encData;
            }
            
            std::cout << "\nCompressing >>>> '" << pdata.filename << "' [" << pdata.filesize << "]\n";
            outSize = huf->Compress(encryptedData.data(), fileSize);
            std::cout << "Compressed Size: " << outSize << std::endl;
            output = huf->getOutput();
            break;
    }
    
    // Write data to the output file
    std::cout << "\nWriting >>>> '" << pdata.filename << "' [" << outSize << "]\n";
    
    // Update the packdata structure
    pdata.filesize = outSize;
    pdata.key = key;
    pdata.parameter = parameter;
    
    // Write packdata and file content
    fwrite(&pdata, sizeof(pdata), 1, packedEXE);
    fwrite(output, outSize, 1, packedEXE);
    
    // Clean up resources
    fclose(inFile);
    fclose(packedEXE);
    
    // Update the DOS header to mark archive starting position
    setInsertPosition(const_cast<char*>(destPath.c_str()), stubSize);
    
    std::cout << "File created: " << destPath << std::endl;
    return PESuccess;
}

/**
 * Update the DOS header in the executable to mark the starting position
 * of the packed archive
 * 
 * @param filename Path to the executable file
 * @param pos Starting position of the archive
 * @return Status code from PEerrors enum
 */
int setInsertPosition(char *filename, long pos) {
    // Open the file for update
    FILE *packArchive = fopen(filename, "rb+");
    if (!packArchive) {
        return PEerrorCouldNotOpenArchive;
    }
    
    // Read the DOS header
    IMAGE_DOS_HEADER idh;
    if (fread(&idh, sizeof(idh), 1, packArchive) != 1) {
        fclose(packArchive);
        return PEerrorCouldNotOpenArchive;
    }
    
    // Update the reserved field with archive position
    *reinterpret_cast<long*>(&idh.e_res2[0]) = pos;
    
    // Write the updated header back
    rewind(packArchive);
    if (fwrite(&idh, sizeof(idh), 1, packArchive) != 1) {
        fclose(packArchive);
        return PEerrorCouldNotOpenArchive;
    }
    
    fclose(packArchive);
    return PESuccess;
}

/**
 * Verify that a file is a valid Windows executable
 * 
 * @param sPath Path to the file
 * @return 1 if valid, PEerrorInputNotEXE if invalid
 */
int validExeFile(const char *sPath) {
    // Open the file
    FILE *fp = fopen(sPath, "rb");
    if (!fp) {
        return PEerrorCouldNotOpenArchive;
    }
    
    // Determine file size
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    // Allocate memory and read the file
    std::vector<UCHAR> content(size);
    if (fread(content.data(), size, 1, fp) != 1) {
        fclose(fp);
        return PEerrorCouldNotOpenArchive;
    }
    
    fclose(fp);
    
    std::cout << "Checking " << sPath << std::endl;
    
    // Check DOS header
    IMAGE_DOS_HEADER dosHeader;
    if (size < sizeof(dosHeader)) {
        std::cout << "File too small for a valid executable" << std::endl;
        return PEerrorInputNotEXE;
    }
    
    memcpy(&dosHeader, content.data(), sizeof(dosHeader));
    
    // Verify DOS signature
    if (dosHeader.e_magic != IMAGE_DOS_SIGNATURE) {
        std::cout << "DOS Signature (MZ): INVALID\n\n";
        return PEerrorInputNotEXE;
    }
    
    std::cout << "DOS signature (MZ): VALID" << std::endl;
    
    // Check PE header
    if (size < dosHeader.e_lfanew + static_cast<long>(sizeof(IMAGE_NT_HEADERS))) {
        std::cout << "File too small for a valid PE executable" << std::endl;
        return PEerrorInputNotEXE;
    }
    
    IMAGE_NT_HEADERS ntHeader;
    memcpy(&ntHeader, content.data() + dosHeader.e_lfanew, sizeof(ntHeader));
    
    // Verify PE signature
    if (ntHeader.Signature != IMAGE_NT_SIGNATURE) {
        std::cout << "PE Signature (PE00): INVALID\n\n\n";
        return PEerrorInputNotEXE;
    }
    
    std::cout << "PE Signature (PE00): VALID\n\n\n";
    return 1;
}
