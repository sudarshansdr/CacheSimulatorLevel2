#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include "sim.h"
#include <string>
#include <vector>
#include <cstdio>

// ItemsInCache Constructor
ItemsInCache::ItemsInCache(uint32_t t, uint32_t i, bool v, bool d, uint32_t c)
    : tag(t), index(i), valid(v), dirty(d), counter(c) {}

// Functions to get and set ietms in cache
uint32_t ItemsInCache::getTag() const { return tag; }
void ItemsInCache::setTag(uint32_t t) { tag = t; }

uint32_t ItemsInCache::getIndex() const { return index; }
void ItemsInCache::setIndex(uint32_t i) { index = i; }

bool ItemsInCache::isValid() const { return valid; }
void ItemsInCache::setValid(bool v) { valid = v; }

bool ItemsInCache::isDirty() const { return dirty; }
void ItemsInCache::setDirty(bool d) { dirty = d; }

uint32_t ItemsInCache::getCounter() const { return counter; }
void ItemsInCache::setCounter(uint32_t c) { counter = c; }

void ItemsInCache::display() const
{
   printf("Index: %d, Tag: %x, Valid: %d, Dirty: %d, Counter: %u\n", index, tag, valid, dirty, counter);
}

ItemsInCache &Cache::getItemsInCache(uint32_t setIndex, uint32_t assocIndex)
{
   return cache[setIndex][assocIndex];
}

// Cache Constructor
Cache::Cache(uint32_t blocksize, uint32_t size, uint32_t assoc)
    : BLOCKSIZE(blocksize), SIZE(size), ASSOC(assoc), NextCacheLevel(nullptr)
{
   GetNoOfBlocksSetsIndexBits();
   cache.resize(NumberOfSets, std::vector<ItemsInCache>(ASSOC));
   printf("For cache Number of sets = %d\n", NumberOfSets);
   printf("for cache Assoc = %d", ASSOC);

   // Initialize counter value with Assoc
   printf("-------------assoc index--------- %d", ASSOC);
   for (uint32_t setIndex = 0; setIndex < NumberOfSets; ++setIndex)
   {
      for (uint32_t assocIndex = 0; assocIndex < ASSOC; ++assocIndex)
      {
         cache[setIndex][assocIndex] = ItemsInCache(0, 0, false, false, assocIndex);
      }
   }
}

// Method to calculate number of blocks, sets, and index bits
void Cache::GetNoOfBlocksSetsIndexBits()
{
   NumberOfBlocks = SIZE / BLOCKSIZE;
   NumberOfSets = NumberOfBlocks / ASSOC;

   if (NumberOfSets > 0)
   {
      NumberOfIndexBits = (uint32_t)log2(NumberOfSets);
   }
   else
   {
      NumberOfIndexBits = 0;
   }

   printf("NoOfBLOCKS: %u\n", NumberOfBlocks);
   printf("NoOfSets: %u\n", NumberOfSets);
   printf("NoOfIndexBits: %u\n", NumberOfIndexBits);
}

// Method to display cache configuration
void Cache::displayConfig()
{
   printf("Cache Configuration:\n");
   printf("BLOCKSIZE: %u\n", BLOCKSIZE);
   printf("SIZE:   %u\n", SIZE);
   printf("ASSOC:  %u\n", ASSOC);
}

// Method to display cache content
void Cache::displayCache()
{
   for (uint32_t setIndex = 0; setIndex < NumberOfSets; ++setIndex)
   {
      for (uint32_t assocIndex = 0; assocIndex < ASSOC; ++assocIndex)
      {
         // printf("Set %u, Way %u: ", setIndex, assocIndex);
         cache[setIndex][assocIndex].display();
      }
   }
}

// Method to extract address fields (tag, index, block offset)
void Cache::ExtractAddressFields(uint32_t addr, uint32_t BlockSize, uint32_t IndexBits, uint32_t &blockOffset, uint32_t &index, uint32_t &tag)
{
   int BlockOffsetBits = log2(BlockSize);

   uint32_t blockOffsetMask = (1 << BlockOffsetBits) - 1;
   uint32_t indexMask = (1 << IndexBits) - 1;

   blockOffset = addr & blockOffsetMask;
   index = (addr >> BlockOffsetBits) & indexMask;
   tag = addr >> (BlockOffsetBits + IndexBits);
   printf("tag bit value = %x", tag);
}

bool Cache::searchInCache(uint32_t index, uint32_t tag, ItemsInCache &cacheLine, uint32_t &assocIndex)
{
   for (uint32_t i = 0; i < ASSOC; ++i)
   {
      ItemsInCache &line = cache[index][i];
      if (line.isValid() && line.getTag() == tag)
      {
         cacheLine = line;
         assocIndex = i; // Correctly set the assocIndex to the hit line
         return true;    // Cache hit
      }
   }
   return false; // Cache miss
}

bool Cache::ReadFunction(uint32_t addr, uint32_t &blockOffset, uint32_t &index, uint32_t &tag, Cache *NextCacheLevel)
{
   // Extract address fields for the current cache level
   ExtractAddressFields(addr, BLOCKSIZE, NumberOfIndexBits, blockOffset, index, tag);

   // Search in the current cache level
   ItemsInCache cacheLine;
   uint32_t assocIndex = 0;
   this->Reads = this->Reads + 1;

   if (searchInCache(index, tag, cacheLine, assocIndex))
   {
      printf("Cache hit at current cache level for address %x\n", addr);
      updateLRUCounters(index, assocIndex); // LRU update on cache hit

      return true; // Cache hit
   }
   else
   {
      printf("Cache miss at address %x in current cache level.\n", addr);
      this->ReadMisses = this->ReadMisses + 1;

      // If there's a next level, recurse into it
      if (NextCacheLevel != nullptr)
      {
         // Next cache level should extract its own tag, index, and blockOffset from the address
         uint32_t nextBlockOffset, nextIndex, nextTag;
         if (NextCacheLevel->ReadFunction(addr, nextBlockOffset, nextIndex, nextTag, NextCacheLevel->NextCacheLevel))
         {
            printf("Data found in next level cache!\n");
            NextCacheLevel->getItemsInCache(nextIndex, 0).display(); // Display what was found in the next level

            // Update the current cache with data from next cache level
            ItemsInCache cacheLineFromNextCacheLevel = NextCacheLevel->getItemsInCache(nextIndex, 0);
            this->updateCache(index, tag); // Use the current cache's tag/index

            printf("Cache line updated in current cache level from next level.\n");
            return true; // Cache hit after bringing data from next level
         }
         else
         {
            printf("Fetching data from main memory for address %x\n", addr);
            assocIndex = getLRUIndex(index);

            // Fetch from memory and update current cache
            ItemsInCache fetchedLine(tag, index, true, false, 0);

            this->updateCache(index, tag); // Update with the correct tag for the current cache level
            // this->updateLRUCounters(index, assocIndex);                                                  // LRU update after memory fetch

            return false; // Cache miss, fetched from memory
         }
      }
      else
      {
         printf("Fetching data from main memory for address %x\n", addr);
         assocIndex = getLRUIndex(index);

         // Fetch from memory and update current cache
         // ItemsInCache fetchedLine(tag, index, true, false, 0); // Assuming fetched from memory, valid, not dirty, LRU counter set to 0
         this->updateCache(index, tag);
         // this->updateLRUCounters(index, assocIndex); // Update with the correct tag for the current cache level

         return false; // Cache miss, fetched from memory
      }
   }
}

void Cache::updateCache(uint32_t index, uint32_t tag)
{
   // Step 1: Check if the tag is already present in the set
   for (uint32_t assocIndex = 0; assocIndex < ASSOC; ++assocIndex)
   {
      // If the tag is found in the set and the block is valid
      if (cache[index][assocIndex].isValid() && cache[index][assocIndex].getTag() == tag)
      {
         // Update this block (e.g., reset LRU counter)
         ItemsInCache &lineToUpdate = cache[index][assocIndex];
         lineToUpdate.setTag(tag); // Not necessary, but can ensure it's set correctly
         lineToUpdate.setIndex(index);
         lineToUpdate.setValid(true);
         lineToUpdate.setDirty(false); // Assuming it's still clean for this example
         printf("Tag found in the cache, updating block at assocIndex %d\n", assocIndex);

         // Step 2: Reset the LRU counter for this block (as it's accessed)
         updateLRUCounters(index, assocIndex); // This will reset the LRU counter for the accessed block
         return;                               // Exit the function as the cache is updated without LRU replacement
      }
   }

   // Step 3: If tag is not present, find the least recently used block (LRU) and replace it
   uint32_t lruIndex = 0;
   uint32_t highestCounterValue = cache[index][0].getCounter(); // Initialize with the first block's counter

   for (uint32_t assocIndex = 1; assocIndex < ASSOC; ++assocIndex)
   {
      // If a block has a higher counter value, it is less recently used
      if (cache[index][assocIndex].getCounter() > highestCounterValue)
      {
         highestCounterValue = cache[index][assocIndex].getCounter();
         lruIndex = assocIndex; // Track the LRU block's index
      }
   }
   printf("lru index is %d\n", lruIndex);

   // Step 4: Replace the least recently used block with the new data (fetched from main memory)
   ItemsInCache &lineToReplace = cache[index][lruIndex];
   lineToReplace.setTag(tag); // Set the correct tag for the current cache level
   lineToReplace.setIndex(index);
   lineToReplace.setValid(true);
   lineToReplace.setDirty(false); // Assuming it's clean when fetched from memory
   printf("The counter value of LRUIndex is %d\n", lineToReplace.getCounter());

   // Step 5: Reset the LRU counter for the newly updated block (set it to 0)
   updateLRUCounters(index, lruIndex); // This will reset the LRU counter for the accessed block
}

void Cache::updateLRUCounters(uint32_t setIndex, uint32_t assocIndex)
{
   // Increment counters for all lines except the accessed one
   for (uint32_t i = 0; i < ASSOC; ++i)
   {

      if (i != assocIndex)
      {

         uint32_t currentCounter = cache[setIndex][i].getCounter();
         if (currentCounter < cache[setIndex][assocIndex].getCounter())
         {
            cache[setIndex][i].setCounter(currentCounter + 1);
         }
      }
   }

   cache[setIndex][assocIndex].setCounter(0);
}

uint32_t Cache::getLRUIndex(uint32_t index)
{
   uint32_t maxLRUCounter = 0;
   uint32_t lruIndex = 0;

   // Iterate through all associativity ways
   for (uint32_t i = 0; i < ASSOC; ++i)
   {
      // Get the LRU counter for the current cache line
      uint32_t currentLRUCounter = cache[index][i].getCounter();

      // Find the cache line with the highest LRU counter (least recently used)
      if (currentLRUCounter > maxLRUCounter)
      {
         maxLRUCounter = currentLRUCounter;
         lruIndex = i;
      }

      // If there's an invalid cache line, we can replace it immediately
      if (!cache[index][i].isValid())
      {
         return i; // Return the first invalid cache line
      }
   }

   // If all cache lines are valid, return the LRU cache line index
   return lruIndex;
}

bool Cache::writeFunction(uint32_t addr, uint32_t &blockOffset, uint32_t &index, uint32_t &tag, Cache *NextCacheLevel)
{
   // Extract address fields for the current cache level
   ExtractAddressFields(addr, BLOCKSIZE, NumberOfIndexBits, blockOffset, index, tag);
   this->Writes = this->Writes + 1;

   // Search in the current cache level
   ItemsInCache cacheLine;
   uint32_t assocIndex = 0;

   if (searchInCache(index, tag, cacheLine, assocIndex))
   {
      printf("Cache hit at current cache level for write operation at address %x\n", addr);
      this->updateCache(index, tag);           // Update with new tag
      cache[index][assocIndex].setDirty(true); // Set the dirty bit to true, indicating a write
      return true;                             // Cache hit, return true
   }
   else
   {
      printf("Cache miss for write operation at address %x\n", addr);
      // uint32_t assocIndex = getLRUIndex(index); // Get the LRU block index for replacement
      this->updateCache(index, tag); // Update with new tag
      cache[index][assocIndex].setDirty(true);
      this->WriteMisses = this->WriteMisses + 1;

      return false; // Cache miss, return false
   }
}

/*  "argc" holds the number of command-line arguments.
    "argv[]" holds the arguments themselves.

    Example:
    ./sim 32 8192 4 262144 8 3 10 gcc_trace.txt
    argc = 9
    argv[0] = "./sim"
    argv[1] = "32"
    argv[2] = "8192"
    ... and soon
*/
int main(int argc, char *argv[])
{
   FILE *fp;              // File pointer.
   char *trace_file;      // This variable holds the trace file name.
   cache_params_t params; // Look at the sim.h header file for the definition of struct cache_params_t.
   char rw;               // This variable holds the request's type (read or write) obtained from the trace.
   uint32_t addr;         // This variable holds the request's address obtained from the trace.
                          // The header file <inttypes.h> above defines signed and unsigned integers of various sizes in a machine-agnostic way.  "uint32_t" is an unsigned integer of 32 bits.

   // Exit with an error if the number of command-line arguments is incorrect.
   if (argc != 9)
   {
      printf("Error: Expected 8 command-line arguments but was provided %d.\n", (argc - 1));
      exit(EXIT_FAILURE);
   }

   // "atoi()" (included by <stdlib.h>) converts a string (char *) to an integer (int).
   params.BLOCKSIZE = (uint32_t)atoi(argv[1]);
   params.L1_SIZE = (uint32_t)atoi(argv[2]);
   params.L1_ASSOC = (uint32_t)atoi(argv[3]);
   params.L2_SIZE = (uint32_t)atoi(argv[4]);
   params.L2_ASSOC = (uint32_t)atoi(argv[5]);
   params.PREF_N = (uint32_t)atoi(argv[6]);
   params.PREF_M = (uint32_t)atoi(argv[7]);
   trace_file = argv[8];

   // Open the trace file for reading.
   fp = fopen(trace_file, "r");
   if (fp == (FILE *)NULL)
   {
      // Exit with an error if file open failed.
      printf("Error: Unable to open file %s\n", trace_file);
      exit(EXIT_FAILURE);
   }

   // Print simulator configuration.
   printf("===== Simulator configuration =====\n");
   printf("BLOCKSIZE:  %u\n", params.BLOCKSIZE);
   printf("L1_SIZE:    %u\n", params.L1_SIZE);
   printf("L1_ASSOC:   %u\n", params.L1_ASSOC);
   printf("L2_SIZE:    %u\n", params.L2_SIZE);
   printf("L2_ASSOC:   %u\n", params.L2_ASSOC);
   printf("PREF_N:     %u\n", params.PREF_N);
   printf("PREF_M:     %u\n", params.PREF_M);
   printf("trace_file: %s\n", trace_file);
   printf("\n");

   Cache L1(params.BLOCKSIZE, params.L1_SIZE, params.L1_ASSOC);
   L1.displayConfig();
   L1.GetNoOfBlocksSetsIndexBits();
   printf("\n");
   Cache L2(params.BLOCKSIZE, params.L2_SIZE, params.L2_ASSOC);
   L2.displayConfig();
   L2.GetNoOfBlocksSetsIndexBits();

   L1.NextCacheLevel = &L2;     // L1 points to L2 as its next level
   L2.NextCacheLevel = nullptr; // L2 points to memory (end of the hierarchy)

   // L1.displayCache();
   // L2.displayCache();

   uint32_t blockOffset, index, tag;
   // Read requests from the trace file and echo them back.
   while (fscanf(fp, "%c %x\n", &rw, &addr) == 2)
   { // Stay in the loop if fscanf() successfully parsed two tokens as specified.
      if (rw == 'r')
         printf("r %x\n", addr);
      else if (rw == 'w')
         printf("w %x\n", addr);
      else
      {
         printf("Error: Unknown request type %c.\n", rw);
         exit(EXIT_FAILURE);
      }

      // Extract address fields for L1 (index, block offset, tag)
      /*L1.ExtractAddressFields(addr, L1.BLOCKSIZE, L1.NumberOfIndexBits, blockOffset, index, tag);

      printf("AddressesL1: %x\n", addr);
      printf("Block OffsetL1: %u\n", blockOffset);
      printf("IndexL1: %x\n", index);
      printf("TagL1: %x\n", tag);

      // Simulate a cache operation on the calculated `index` and first `way` (way 0)
      ItemsInCache &cacheLineL1 = L1.getItemsInCache(0, 0); // Access the correct set and way
      cacheLineL1.setTag(tag);
      cacheLineL1.setIndex(index); // Set the tag for this cache line
      cacheLineL1.setValid(true);  // Mark this cache line as valid
      cacheLineL1.setDirty(false); // Update dirty bit
      cacheLineL1.setCounter(0);   // Reset the counter or use for LRU, etc.

      L1.displayCache(); // Display L1 cache after update*/
      if (rw == 'r')
      {
         // Search for the tag in L1 cache
         L1.ReadFunction(addr, blockOffset, index, tag, L1.NextCacheLevel);
      }
      else
      {
         L1.writeFunction(addr, blockOffset, index, tag, L1.NextCacheLevel);
      }

      // Repeat for L2 if needed

      /*printf("AddressesL2: %x\n", addr);
      printf("Block OffsetL2: %u\n", blockOffset);
      printf("IndexL2: %x\n", index);
      printf("TagL2: %x\n", tag);

      // Simulate a cache operation on L2 for the calculated `index` and first `way`
      ItemsInCache &cacheLineL2 = L2.getItemsInCache(index, 0); // Access the correct set and way
      cacheLineL2.setTag(tag);
      cacheLineL2.setIndex(index); // Set the tag for this cache line
      cacheLineL2.setValid(true);  // Mark this cache line as valid
      cacheLineL2.setDirty(false); // Update dirty bit
      cacheLineL2.setCounter(0);   // Reset the counter or use for LRU, etc.*/
      // L1.displayCache(); // Display L2 cache after update
      printf("\n");

      ///////////////////////////////////////////////////////
      // Issue the request to the L1 cache instance here.
      ///////////////////////////////////////////////////////
   }

   L1.displayCache(); // Display L2 cache after update
   printf("Number of reads %d\n", L1.Reads);
   printf("Number of readmisses %d\n", L1.ReadMisses);
   printf("Number of writes %d\n", L1.Writes);
   printf("Number of writemisses %d\n", L1.WriteMisses);

   /*L2.displayCache(); // Display L2 cache after update
   printf("Number of 2 reads %d\n", L2.Reads);
   printf("Number of 2 readmisses %d\n", L2.ReadMisses);
   printf("Number of 2 writes %d\n", L2.Writes);
   printf("Number of 2 writemisses %d\n", L2.WriteMisses);*/

   return (0);
}
