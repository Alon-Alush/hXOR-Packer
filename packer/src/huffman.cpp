#include "huffman.h"
#include <algorithm>
#include <iostream>
#include <memory>
#include <vector>
#include <cstring>

/**
 * The constructor for the huffman object.
 * It initializes the trees array with empty nodes.
 * allocatedoutput is initially NULL, so getOutput() will return 
 * NULL if nothing has been compressed.
 */
huffman::huffman() {
    stepscount = 0;
    treescount = 0;
    allocatedoutput = nullptr;

    // Initialize node arrays
    for (int i = 0; i < 256; i++) {
        trees[i] = new node();
        trees_backup[i] = trees[i];
        leaves[i] = trees[i];
        STEPS[i] = 0;
    }
    
    // Clear nodes array
    memset(nodes, 0, 256 * sizeof(node));
}

/**
 * Compress an unsigned char array.
 * The compressed data can be retrieved using getOutput().
 * 
 * @param input Pointer to the input data to compress
 * @param inputlength Length of the input data in bytes
 * @return Size of the compressed data in bytes
 */
int huffman::Compress(UCHAR *input, int inputlength) {
    if (!input || inputlength <= 0) {
        std::cerr << "Error: Invalid input for compression" << std::endl;
        return 0;
    }

    // Free any previously allocated output buffer
    delete[] allocatedoutput;
    
    // Allocate a new output buffer with sufficient size
    // (5x input size should be more than enough for worst case)
    allocatedoutput = new UCHAR[5 * inputlength];
    if (!allocatedoutput) {
        std::cerr << "Error: Failed to allocate memory for compression" << std::endl;
        return 0;
    }
    
    UCHAR *inptr = input;
    UCHAR *stop = input + inputlength;  // Pointer to the end of input data
    UCHAR *outptrX = allocatedoutput;

    // 1. Count the frequency of each character in the input
    while (inptr != stop) {
        trees[*inptr]->count++;
        trees[*inptr]->chr = *inptr;
        inptr++;
    }

    // 2. Sort the trees array by frequency (highest first)
    std::qsort(trees, 256, sizeof(node*), compareFrequency);

    // 3. Calculate the number of unique characters (tree count)
    treescount = 0;
    for (int i = 0; i < 256; i++) {
        if (trees[i]->count > 0) {
            treescount++;
        } else {
            break;
        }
    }

    // 4. Write tree count at the beginning of the output file
    *outptrX = static_cast<UCHAR>(treescount - 1);
    ++outptrX;

    // 5. Write all unique characters to the output
    for (int i = 0; i < treescount; i++) {
        *outptrX = trees[i]->chr;
        ++outptrX;
    }

    // 6. Create the Huffman tree
    MakeHuffmanTree();

    // 7. Write steps count and steps data
    *outptrX = stepscount;
    outptrX++;

    for (int i = 0; i < stepscount; i++) {
        *outptrX = STEPS[i];
        outptrX++;
    }

    // 8. Write the original input file size using bit shifting
    *outptrX     = (inputlength >> 24) & 0xFF;
    *(outptrX+1) = (inputlength >> 16) & 0xFF;
    *(outptrX+2) = (inputlength >> 8) & 0xFF;
    *(outptrX+3) = inputlength & 0xFF;
    outptrX += 4;

    // 9. Assign codes and codelengths to each leaf
    setCodeAndLength(*trees, 0, 0);

    // 10. Write the encoded data
    int bitpath, bitswritten, outbitswritten = 0;
    inptr = input;
    *outptrX = 0;
    
    while (inptr != stop) {
        bitpath = leaves[*inptr]->code;
        int bitcount = leaves[*inptr]->codelength;
        bitswritten = 0;
        
        for (int i = 0; i < bitcount; i++) {
            (*outptrX) ^= ((bitpath >> bitswritten) & 1) << outbitswritten;
            bitswritten++;
            (*(outptrX+1)) = '\0';
            outptrX += ((outbitswritten >> 2) & (outbitswritten >> 1) & outbitswritten) & 1;
            outbitswritten++;
            outbitswritten &= 7;
        }
        inptr++;
    }

    // 11. Calculate the total bytes written
    int byteswritten = outptrX - allocatedoutput;
    if (outbitswritten > 0) {
        byteswritten++;
    }
    
    return byteswritten;
}

/**
 * Generate the Huffman tree from the frequency-sorted nodes
 */
void huffman::MakeHuffmanTree() {
    // Clear the nodes array
    memset(nodes, 0, sizeof(node) * 256);
    
    node *nptr = nodes;
    node **endtree = trees + treescount - 1;
    node **backupptr = trees_backup + treescount - 1;
    
    // Continue until only one tree remains
    while (treescount > 1) {
        // Create a new internal node
        node *n = nptr++;
        
        // Connect the two least frequent nodes as children
        n->right = *(endtree - 1);    // Higher frequency
        n->left = *endtree;           // Lower frequency
        
        // Sum the frequencies
        n->count = n->right->count + n->left->count;

        // Update tree pointers
        *endtree = *backupptr;
        *(endtree - 1) = n;
        --treescount;
        --endtree;
        --backupptr;
        
        // Record steps for decompression
        STEPS[stepscount] = treescount;

        // Attempt to reorder the tree for better compression
        tryToRelocate();
        stepscount++;
    }
}

/**
 * Recursively set the code and path length for each leaf node
 * in the Huffman tree
 * 
 * @param n Current node
 * @param path Current bit path to this node
 * @param level Current depth in the tree
 */
void huffman::setCodeAndLength(node *n, int path, int level) {
    // Check if this is an internal node
    bool isLeaf = true;
    
    // Process right child (append 1 to the path)
    if (n->right) {
        setCodeAndLength(n->right, path ^ (1 << level), level + 1);
        isLeaf = false;
    }
    
    // Process left child (keep same path)
    if (n->left) {
        setCodeAndLength(n->left, path, level + 1);
        isLeaf = false;
    }
    
    // If this is a leaf node, store its code and length
    if (isLeaf) {
        n->codelength = level;
        n->code = path;
    }
}

/**
 * Try to relocate nodes in the tree to improve compression
 */
void huffman::tryToRelocate() {
    node **ptr = trees + treescount - 2;
    node **ourtree = trees + treescount - 1;
    node **begin = trees;
    
    while (ptr >= begin) {
        if (ourtree != ptr) {
            if (((*ptr)->count > (*ourtree)->count) || (ptr == begin)) {
                moveTreesToRight(ptr);
                return;
            }
        }
        ptr--;
    }
}

/**
 * Move trees to a better position in the array
 * 
 * @param toTree Target position for the tree
 */
void huffman::moveTreesToRight(node **toTree) {
    node *a, *b;
    node **ptr = trees + treescount - 1;
    node **_toTree = toTree;
    node **end = trees + treescount;
    
    while (ptr > _toTree) {
        a = *(ptr - 1);
        b = *ptr;
        
        if (a->count >= b->count) {
            STEPS[stepscount] = treescount - (end - ptr);
            return;
        }
        
        // Swap the nodes
        *(ptr - 1) = b;
        *ptr = a;
        ptr--;
    }
    
    STEPS[stepscount] = treescount - (end - ptr);
}

/**
 * Get the compressed output data
 * 
 * @return Pointer to the compressed data, or nullptr if nothing compressed
 */
UCHAR *huffman::getOutput() {
    return allocatedoutput;
}

/**
 * Compare the frequencies of two nodes for sorting
 * Used with qsort() to sort the trees array by frequency
 * 
 * @param A First node pointer
 * @param B Second node pointer
 * @return Comparison result (-1, 0, or 1)
 */
int huffman::compareFrequency(const void *A, const void *B) {
    const node* nodeA = *(const node**)A;
    const node* nodeB = *(const node**)B;
    
    if (nodeA->count == nodeB->count) {
        return 0;
    }
    
    return (nodeA->count < nodeB->count) ? 1 : -1;
}
