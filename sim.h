#ifndef CACHE_H
#define CACHE_H

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <bitset>
#include <cmath>
#include <string>
#include <sstream>

typedef struct
{
   uint32_t BLOCKSIZE;
   uint32_t L1_SIZE;
   uint32_t L1_ASSOC;
   uint32_t L2_SIZE;
   uint32_t L2_ASSOC;
   uint32_t PREF_N;
   uint32_t PREF_M;
} cache_params_t;

// Put additional data structures here as per your requirement.

// Cache Class Declaration
class Cache
{
public:
   uint32_t BLOCKSIZE;
   uint32_t SIZE;
   uint32_t ASSOC;
   uint32_t NumberOfBlocks;
   uint32_t NumberOfSets;
   uint32_t NumberOfIndexBits;

public:
   // Constructor
   Cache(uint32_t blocksize, uint32_t size, uint32_t assoc);

   // Method to calculate blocks, sets, and index bits
   void GetNoOfBlocksSetsIndexBits();

   // Method to display the cache configuration
   void displayConfig();

   // Method to extract tag, index, and block offset from address
   void ExtractAddressFields(uint32_t addr, uint32_t BlockSize, uint32_t IndexBits);
};

#endif // CACHE_H
