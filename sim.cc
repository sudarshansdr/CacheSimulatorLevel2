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
   // printf("tag bit value = %x", tag);
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

bool Cache::ReadFunction(uint32_t addr, uint32_t &blockOffset, uint32_t &index, uint32_t &tag, Cache *NextCacheLevel, bool FromUpdate)
{
   // Extract address fields for the current cache level
   ExtractAddressFields(addr, BLOCKSIZE, NumberOfIndexBits, blockOffset, index, tag);

   // Search in the current cache level
   ItemsInCache cacheLine;
   uint32_t assocIndex = 0;

   // Increment read count for the current cache level
   if (!FromUpdate)
   {
      this->Reads++;
   }

   if (searchInCache(index, tag, cacheLine, assocIndex))
   {
      // Cache hit
      // printf("Cache hit at current cache level for address %x\n", addr);
      updateLRUCounters(index, assocIndex); // LRU update on cache hit
      return true;                          // Cache hit
   }
   else
   {
      // Cache miss at the current level
      // printf("Cache miss at address %x in current cache level.\n", addr);

      this->ReadMisses++; // Increment read miss counter

      // this->WriteMisses++;

      // If there's a dirty block in the current cache level, evict it
      uint32_t lruIndex = getLRUIndex(index);               // Find the LRU block for eviction
      ItemsInCache &lineToReplace = cache[index][lruIndex]; // Get the line to replace

      if (lineToReplace.isDirty())
      {
         // Write back the dirty block to the next cache level or memory
         // this->WriteBacks++;
         uint32_t writeBackTag = lineToReplace.getTag();
         // printf("Writing back dirty data for eviction at index %d and tag %d\n", index, writeBackTag);

         // Write back to the next cache level if it exists
         if (NextCacheLevel != nullptr)
         {
            uint32_t NumberOfBlockOffset = log2(BLOCKSIZE);
            // Recalculate the full address for the write-back block using its tag, index, and block offset
            uint32_t writeBackAddr = (writeBackTag << (NumberOfIndexBits + NumberOfBlockOffset)) | (index << NumberOfBlockOffset) | blockOffset;
            // printf("Writing back to next level with address %x\n", writeBackAddr);
            NextCacheLevel->updateCache(writeBackAddr, NextCacheLevel->NextCacheLevel, false, false); // Write back dirty block
         }
         else
         {

            // this->WriteBacks++;

            //  Write back to memory if there's no next cache level
            // printf("Writing dirty data back to main memory\n");
         }
      }

      // After eviction (if necessary), attempt to read from the next cache level
      if (NextCacheLevel != nullptr)
      {
         uint32_t nextBlockOffset, nextIndex, nextTag;
         bool foundInNextLevel = NextCacheLevel->ReadFunction(addr, nextBlockOffset, nextIndex, nextTag, NextCacheLevel->NextCacheLevel, FromUpdate);

         if (foundInNextLevel)
         {
            // Data found in the next level cache
            // printf("Data found in next level cache for address %x\n", addr);
            // Update the current cache with data fetched from the next cache level
            this->updateCache(addr, NextCacheLevel, true, false);
            return true; // Data fetched successfully
         }
         else
         {

            // No data found in next level, fetching from memory
            // MainMemTraffic++;
            // printf("Fetching data from main memory for address %x\n", addr);
            this->updateCache(addr, NextCacheLevel, true, false); // Update cache with data from memory
            return false;                                         // Cache miss, data fetched from memory
         }
      }
      else
      {

         // If no next level cache, assume fetching directly from memory
         MainMemTraffic++;
         // printf("Fetching data from main memory for address %x\n", addr);
         this->updateCache(addr, NextCacheLevel, true, false); // Update cache with data from memory
         return false;                                         // Cache miss, data fetched from memory
      }
   }
}

void Cache::updateCache(uint32_t addr, Cache *NextCacheLevel, bool FromRead, bool FromWrite)
{
   uint32_t tag, index, blockOffset;
   ExtractAddressFields(addr, BLOCKSIZE, NumberOfIndexBits, blockOffset, index, tag);

   // Step 1: Search for the tag in the current cache set
   for (uint32_t assocIndex = 0; assocIndex < ASSOC; ++assocIndex)
   {
      if (cache[index][assocIndex].isValid() && cache[index][assocIndex].getTag() == tag)
      {
         // Cache hit, no need to replace the block
         ItemsInCache &lineToUpdate = cache[index][assocIndex];

         // Ensure dirty bit is only modified during a write operation
         if (FromRead)
         {
            lineToUpdate.setDirty(false); // Reads shouldn't mark the block dirty
         }
         else
         {
            lineToUpdate.setDirty(true); // Writes should mark the block dirty
         }

         // Update LRU counters
         updateLRUCounters(index, assocIndex);
         return;
      }
   }

   if (!FromRead)
   {
      this->WriteMisses++; // Increment write miss counter
   }

   // this->WriteMisses++;

   // Step 2: Cache miss, replace the least recently used (LRU) block
   uint32_t lruIndex = getLRUIndex(index); // Get the least recently used block index
   ItemsInCache &lineToReplace = cache[index][lruIndex];

   // Step 3: Write-back if the evicted block is dirty
   if (lineToReplace.isDirty())
   {
      this->WriteBacks++; // Increment the write-back count
      uint32_t writeBackTag = lineToReplace.getTag();
      // printf("Writing back dirty block at index %d with tag %d\n", index, writeBackTag);

      if (NextCacheLevel != nullptr)
      {
         uint32_t NumberOfBlockoffset = log2(BLOCKSIZE);
         uint32_t writeBackAddr = (writeBackTag << (NumberOfIndexBits + NumberOfBlockoffset)) | (index << NumberOfBlockoffset) | blockOffset;
         // printf("Writing back to next cache level for address %x\n", writeBackAddr);

         // NextCacheLevel->writeFunction(addr, blockOffset, index, tag, NextCacheLevel->NextCacheLevel);
         NextCacheLevel->updateCache(writeBackAddr, NextCacheLevel->NextCacheLevel, false, true); // Write-back to the next cache level
         NextCacheLevel->Writes++;
      }
      else
      {

         MainMemTraffic++;

         // printf("Writing back dirty block to main memory for address %x\n", addr);
      }

      lineToReplace.setDirty(false); // After writing back, reset the dirty bit
   }

   // Step 4: Fetch data from the next level or memory if the block is not present
   if (NextCacheLevel != nullptr)
   {

      uint32_t nextBlockOffset, nextIndex, nextTag;
      if (NextCacheLevel->ReadFunction(addr, nextBlockOffset, nextIndex, nextTag, NextCacheLevel->NextCacheLevel, true))
      {
         // printf("Data fetched from next level cache for address %x\n", addr);
      }
      else
      {

         // printf("Data fetched from main memory for address %x\n", addr);
      }
   }
   else
   {
      if (!FromRead)
      {
         MainMemTraffic++;
      }

      // printf("Fetching data from main memory for address %x\n", addr);
   }

   // Step 5: Replace the least recently used block with the new data
   lineToReplace.setTag(tag);     // Set the tag for the new block
   lineToReplace.setIndex(index); // Set the index
   lineToReplace.setValid(true);  // Mark the block as valid

   // Set dirty bit based on the operation (FromRead will be false during writes)
   if (FromRead)
   {
      lineToReplace.setDirty(false); // Read operation, no dirty bit
   }
   else
   {
      lineToReplace.setDirty(true); // Write operation, set the dirty bit
   }

   // Update LRU counters for the newly replaced block
   updateLRUCounters(index, lruIndex);
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
   this->Writes++; // Increment the write operation count

   // Search in the current cache level
   ItemsInCache cacheLine;
   uint32_t assocIndex = 0;

   if (searchInCache(index, tag, cacheLine, assocIndex))
   {
      // Cache hit
      // printf("Cache hit at current cache level for write operation at address %x\n", addr);

      // On a cache hit, update the block and set the dirty bit
      cache[index][assocIndex].setDirty(true); // Set the dirty bit on a write
      updateLRUCounters(index, assocIndex);    // Update LRU on cache hit

      return true; // Cache hit
   }
   else
   {
      // Cache miss: we first fetch the block (write-allocate)
      // printf("Cache miss for write operation at address %x\n", addr);
      // this->WriteMisses++; // Increment the write miss counter

      // Fetch block from next cache level or memory
      this->updateCache(addr, NextCacheLevel, false, true); // False indicates this is a write, set dirty bit
      if (NextCacheLevel != nullptr)
      {
         uint32_t nextBlockOffset, nextIndex, nextTag;
         if (NextCacheLevel->ReadFunction(addr, nextBlockOffset, nextIndex, nextTag, NextCacheLevel->NextCacheLevel, false))
         {
            // Data is fetched from next level
            // printf("Block fetched from next cache level for write allocate at address %x\n", addr);
         }
         else
         {

            // If no next cache level, fetch from memory

            // printf("Block fetched from main memory for write allocate at address %x\n", addr);
         }
      }
      else
      {
         // MainMemTraffic++;
      }

      // Now update the current cache with the new block and set the dirty bit

      return false; // Cache miss
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
   Cache *L2 = nullptr;   // Pointer to L2 cache, initialized to nullptr.

   // Exit with an error if the number of command-line arguments is incorrect.
   if (argc != 9)
   {
      printf("Error: Expected 8 command-line arguments but was provided %d.\n", (argc - 1));
      exit(EXIT_FAILURE);
   }

   // Convert command-line arguments to integers.
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

   // Create L1 cache
   Cache L1(params.BLOCKSIZE, params.L1_SIZE, params.L1_ASSOC);
   L1.displayConfig();
   L1.GetNoOfBlocksSetsIndexBits();
   // printf("\n");

   // Conditionally create L2 cache if size and associativity are greater than 0
   if (params.L2_SIZE > 0 && params.L2_ASSOC > 0)
   {
      L2 = new Cache(params.BLOCKSIZE, params.L2_SIZE, params.L2_ASSOC); // Dynamically allocate L2 cache
      L2->displayConfig();
      L2->GetNoOfBlocksSetsIndexBits();

      // L1 points to L2 as its next level
      L1.NextCacheLevel = L2;
      L2->NextCacheLevel = nullptr; // L2 points to memory (end of the hierarchy)
   }
   else
   {
      L1.NextCacheLevel = nullptr; // No L2 cache
   }

   uint32_t blockOffset, index, tag;
   // Read requests from the trace file and echo them back.
   while (fscanf(fp, "%c %x\n", &rw, &addr) == 2)
   {
      if (rw == 'r')
      {
         // printf("r %x\n", addr);
      }

      else if (rw == 'w')
      {
         // printf("w %x\n", addr);
      }

      else
      {
         // printf("Error: Unknown request type %c.\n", rw);
         exit(EXIT_FAILURE);
      }

      // Extract address fields for L1 (index, block offset, tag)
      // L1.ExtractAddressFields(addr, L1.BLOCKSIZE, L1.NumberOfIndexBits, blockOffset, index, tag);

      // Simulate a cache operation
      if (rw == 'r')
      {
         // printf("testing\n\n");
         L1.ReadFunction(addr, blockOffset, index, tag, L1.NextCacheLevel, false);
      }
      else
      {
         L1.writeFunction(addr, blockOffset, index, tag, L1.NextCacheLevel);
      }

      // printf("\n");
   }

   // Display L1 cache statistics
   L1.displayCache(); // Display L1 cache after update
   printf("Number of reads %d\n", L1.Reads);
   printf("Number of readmisses %d\n", L1.ReadMisses);
   printf("Number of writes %d\n", L1.Writes);
   printf("Number of writemisses %d\n", L1.WriteMisses);
   printf("Number of Writebacks %d\n", L1.WriteBacks);

   // If L2 cache was created, display its statistics
   if (L2)
   {
      L2->displayCache(); // Display L2 cache after update
      printf("Number of 2 reads %d\n", L2->Reads);
      printf("Number of 2 readmisses %d\n", L2->ReadMisses);
      printf("Number of 2 writes %d\n", L2->Writes);
      printf("Number of 2 writemisses %d\n", L2->WriteMisses);
      printf("Number of Writebacks %d\n", L2->WriteBacks);
      delete L2; // Clean up dynamically allocated L2 cache
   }

   printf("Main Memory Traffic is %d\n", MainMemTraffic);
   printf("Count is is %d\n", Count);

   fclose(fp); // Close the trace file
   return 0;
}
