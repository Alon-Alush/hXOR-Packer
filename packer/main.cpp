#include "packingInfo.h"
#include <iostream>
#include <string>

/**
 * hXOR Packer by Afif, 2012
 * This program packs one valid EXE file into a self-extracting executable.
 * The output EXE will unpack the original EXE and run it.
 * This program operates solely via command-line arguments and does not accept
 * user input during runtime.
 */
int main(int argc, char *argv[]) {
    const std::string BANNER = "hXOR Packer by Afif, 2012\n"
                              "--------------------------------------------------------------------------\n";
    
    std::cout << BANNER;
    
    // Check if we have enough arguments to proceed
    if (argc < 3) {
        // Display usage instructions
        std::cout << "How to use?\n\n"
                  << "For Packing:\n"
                  << "<S> -> EXE File (Absolute Path)\n"
                  << "<D> -> Destination Output (Absolute Path)\n"
                  << "<P> -> Parameters (Optional)\n"
                  << "<K> -> Xor Encryption Key in numbers (Optional)\n"
                  << "\nAvailable Parameters (Optional):\n"
                  << "-c\t\tCompression\n"
                  << "-e\t\tEncryption\n"
                  << "-ce\t\tCompression & Encryption\n\n"
                  << "Examples:\n"
                  << ">>>packer.exe <S> <D> <P> <K>\n"
                  << ">>>packer.exe C:\\in.exe C:\\folder\\out.exe\n"
                  << ">>>packer.exe C:\\in.exe C:\\folder\\out.exe -ce 56213\n\n";
    } else {
        // Proceed with packing
        int resultError = packFileIntoArchive(argc, argv);
        
        // Display error message if operation failed
        if (resultError != PESuccess) {
            std::cerr << "Error: " << PEerrors_str[resultError] << std::endl;
        }
    }
    
    system("PAUSE");
    return 0;
}
