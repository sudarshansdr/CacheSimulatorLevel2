#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include "sim.h"
#include <string>
#include <vector>
#include <cstdio>

// #ifndef CACHE_H
#define CACHE_H

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <bitset>
#include <cmath>
#include <string>
#include <sstream>
#include <vector>

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

uint32_t MainMemTraffic = 0;
uint32_t Count = 0;

// Put additional data structures here as per your requirement.
// Cache Element class definition

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

   std::vector<std::vector<ItemsInCache>> cache; // 2D dynamic array for cache elements

public:
   // Constructor
   Cache(uint32_t blocksize, uint32_t size, uint32_t assoc);

   // Method to calculate blocks, sets, and index bits
   void GetNoOfBlocksSetsIndexBits()
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
   }

   // Method to access cache elements
   ItemsInCache &getItemsInCache(uint32_t setIndex, uint32_t assocIndex);

   // Method to display the cache configuration
   void displayConfig();
   void displayCache();

   // Method to extract tag, index, and block offset from address
   void ExtractAddressFields(uint32_t addr, uint32_t BlockSize, uint32_t IndexBits, uint32_t &blockOffset, uint32_t &index, uint32_t &tag);

   bool searchInCache(uint32_t index, uint32_t tag, ItemsInCache &cacheLine, uint32_t &assocIndex);
   bool ReadFunction(uint32_t addr, uint32_t &blockOffset, uint32_t &index, uint32_t &tag, Cache *NextCacheLevel, bool FromUpdate);
   bool writeFunction(uint32_t addr, uint32_t &blockOffset, uint32_t &index, uint32_t &tag, Cache *NextCacheLevel);
   void updateCache(uint32_t addr, Cache *NextCacheLevel, bool FromRead, bool);
   void updateLRUCounters(uint32_t setIndex, uint32_t assocIndex);
   uint32_t getLRUIndex(uint32_t index);
   void writeCache(uint32_t index, uint32_t tag);
   void updateCacheNextLevel(uint32_t addr, Cache *NextCacheLevel);
};

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

Cache::Cache(uint32_t blocksize, uint32_t size, uint32_t assoc)
    : BLOCKSIZE(blocksize), SIZE(size), ASSOC(assoc), NextCacheLevel(nullptr)
{
   GetNoOfBlocksSetsIndexBits();
   cache.resize(NumberOfSets, std::vector<ItemsInCache>(ASSOC));
   for (uint32_t setIndex = 0; setIndex < NumberOfSets; ++setIndex)
   {
      for (uint32_t assocIndex = 0; assocIndex < ASSOC; ++assocIndex)
      {
         cache[setIndex][assocIndex] = ItemsInCache(0, 0, false, false, assocIndex);
      }
   }
}

// This method calculates the number of blocks, sets, and index bits
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
}

// This method displays the cache configuration
void Cache::displayConfig()
{
   printf("Cache Configuration:\n");
   printf("BLOCKSIZE: %u\n", BLOCKSIZE);
   printf("SIZE:   %u\n", SIZE);
   printf("ASSOC:  %u\n", ASSOC);
}

void Cache::displayCache()
{
   for (uint32_t setIndex = 0; setIndex < NumberOfSets; ++setIndex)
   {
      for (uint32_t assocIndex = 0; assocIndex < ASSOC; ++assocIndex)
      {
         cache[setIndex][assocIndex].display();
      }
   }
}

void Cache::ExtractAddressFields(uint32_t addr, uint32_t BlockSize, uint32_t IndexBits, uint32_t &blockOffset, uint32_t &index, uint32_t &tag)
{
   int BlockOffsetBits = log2(BlockSize);

   uint32_t blockOffsetMask = (1 << BlockOffsetBits) - 1;
   uint32_t indexMask = (1 << IndexBits) - 1;

   blockOffset = addr & blockOffsetMask;
   index = (addr >> BlockOffsetBits) & indexMask;
   tag = addr >> (BlockOffsetBits + IndexBits);
}

bool Cache::searchInCache(uint32_t index, uint32_t tag, ItemsInCache &CacheSet, uint32_t &assocIndex)
{
   for (uint32_t i = 0; i < ASSOC; ++i)
   {
      ItemsInCache &line = cache[index][i];
      if (line.isValid() && line.getTag() == tag)
      {
         CacheSet = line;
         assocIndex = i;
         return true;
      }
   }
   return false;
}

bool Cache::ReadFunction(uint32_t addr, uint32_t &blockOffset, uint32_t &index, uint32_t &tag, Cache *NextCacheLevel, bool FromUpdate)
{
   ExtractAddressFields(addr, BLOCKSIZE, NumberOfIndexBits, blockOffset, index, tag);
   ItemsInCache CacheSet;
   uint32_t assocIndex = 0;

   if (FromUpdate)
   {
      this->Reads++;
   }

   if (searchInCache(index, tag, CacheSet, assocIndex))
   {
      updateLRUCounters(index, assocIndex);
      return true;
   }
   else
   {
      this->ReadMisses++;
      uint32_t lruIndex = getLRUIndex(index);
      ItemsInCache &TargetCacheSet = cache[index][lruIndex];

      if (TargetCacheSet.isDirty())
      {
         uint32_t writeBackTag = TargetCacheSet.getTag();
         if (NextCacheLevel != nullptr)
         {
            uint32_t NumberOfBlockOffset = log2(BLOCKSIZE);
            uint32_t writeBackAddr = (writeBackTag << (NumberOfIndexBits + NumberOfBlockOffset)) | (index << NumberOfBlockOffset) | blockOffset;
            NextCacheLevel->updateCache(writeBackAddr, NextCacheLevel->NextCacheLevel, false, false);
         }
         else
         {
         }
      }
      if (NextCacheLevel != nullptr)
      {
         uint32_t nextBlockOffset, nextIndex, nextTag;
         bool foundInNextLevel = NextCacheLevel->ReadFunction(addr, nextBlockOffset, nextIndex, nextTag, NextCacheLevel->NextCacheLevel, FromUpdate);

         if (foundInNextLevel)
         {
            this->updateCache(addr, NextCacheLevel, true, false);
            return true;
         }
         else
         {
            this->updateCache(addr, NextCacheLevel, true, false);
            return false;
         }
      }
      else
      {
         MainMemTraffic++;
         this->updateCache(addr, NextCacheLevel, true, false);
         return false;
      }
   }
}

void Cache::updateCache(uint32_t addr, Cache *NextCacheLevel, bool FromRead, bool FromWrite)
{
   uint32_t tag, index, blockOffset;
   ExtractAddressFields(addr, BLOCKSIZE, NumberOfIndexBits, blockOffset, index, tag);
   // Search for the tag in the current cache set
   for (uint32_t assocIndex = 0; assocIndex < ASSOC; ++assocIndex)
   {
      if (cache[index][assocIndex].isValid() && cache[index][assocIndex].getTag() == tag)
      {
         ItemsInCache &lineToUpdate = cache[index][assocIndex];
         if (FromRead)
         {
            lineToUpdate.setDirty(false); // Reads shouldn't make the dirty bit 1
         }
         else
         {
            lineToUpdate.setDirty(true); // Writes should make the dirty bit 1
         }
         updateLRUCounters(index, assocIndex);
         return;
      }
   }

   if (!FromRead)
   {
      this->WriteMisses++;
   }
   // Replace the least recently used (LRU) block
   uint32_t lruIndex = getLRUIndex(index); // Getting the least recently used block index
   ItemsInCache &TargetCacheSet = cache[index][lruIndex];

   // Write-back if the evicted block is dirty
   if (TargetCacheSet.isDirty())
   {
      this->WriteBacks++;
      uint32_t writeBackTag = TargetCacheSet.getTag();
      if (NextCacheLevel != nullptr)
      {
         uint32_t NumberOfBlockoffset = log2(BLOCKSIZE);
         uint32_t writeBackAddr = (writeBackTag << (NumberOfIndexBits + NumberOfBlockoffset)) | (index << NumberOfBlockoffset) | blockOffset;
         NextCacheLevel->updateCache(writeBackAddr, NextCacheLevel->NextCacheLevel, false, true);
         NextCacheLevel->Writes++;
      }
      else
      {
         MainMemTraffic++;
      }

      TargetCacheSet.setDirty(false);
   }
   if (NextCacheLevel != nullptr)
   {
      uint32_t nextBlockOffset, nextIndex, nextTag;
      if (NextCacheLevel->ReadFunction(addr, nextBlockOffset, nextIndex, nextTag, NextCacheLevel->NextCacheLevel, true))
      {
      }
   }
   else
   {
      if (!FromRead)
      {
         MainMemTraffic++;
      }
   }

   TargetCacheSet.setTag(tag);
   TargetCacheSet.setIndex(index);
   TargetCacheSet.setValid(true);

   if (FromRead)
   {
      TargetCacheSet.setDirty(false);
   }
   else
   {
      TargetCacheSet.setDirty(true);
   }
   updateLRUCounters(index, lruIndex);
}

void Cache::updateLRUCounters(uint32_t setIndex, uint32_t assocIndex)
{
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
   for (uint32_t i = 0; i < ASSOC; ++i)
   {
      uint32_t currentLRUCounter = cache[index][i].getCounter();
      if (currentLRUCounter > maxLRUCounter)
      {
         maxLRUCounter = currentLRUCounter;
         lruIndex = i;
      }
      if (!cache[index][i].isValid())
      {
         return i;
      }
   }
   return lruIndex;
}

bool Cache::writeFunction(uint32_t addr, uint32_t &blockOffset, uint32_t &index, uint32_t &tag, Cache *NextCacheLevel)
{
   ExtractAddressFields(addr, BLOCKSIZE, NumberOfIndexBits, blockOffset, index, tag);
   this->Writes++;
   ItemsInCache CacheSet;
   uint32_t assocIndex = 0;

   if (searchInCache(index, tag, CacheSet, assocIndex))
   {
      cache[index][assocIndex].setDirty(true); // Set the dirty bit on a write
      updateLRUCounters(index, assocIndex);    // Update LRU on cache hit
      return true;
   }
   else
   {

      this->updateCache(addr, NextCacheLevel, false, true);
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
         printf("Error: Unknown request type %c.\n", rw);
         exit(EXIT_FAILURE);
      }
      if (rw == 'r')
      {
         // printf("testing\n\n");
         L1.ReadFunction(addr, blockOffset, index, tag, L1.NextCacheLevel, false);
         L1.Reads++;
      }
      else
      {
         L1.writeFunction(addr, blockOffset, index, tag, L1.NextCacheLevel);
      }
   }

   L1.displayCache(); // Display L1 cache after update
   printf("Number of reads %d\n", L1.Reads);
   printf("Number of readmisses %d\n", L1.ReadMisses);
   printf("Number of writes %d\n", L1.Writes);
   printf("Number of writemisses %d\n", L1.WriteMisses);
   printf("Number of Writebacks %d\n", L1.WriteBacks);
   printf("L1 MissRate %.4f\n", (float)(L1.ReadMisses + L1.WriteMisses) / (L1.Reads + L1.Writes));

   if (L2)
   {
      L2->displayCache();
      printf("Number of 2 reads %d\n", L2->Reads);
      printf("Number of 2 readmisses %d\n", L2->ReadMisses);
      printf("Number of 2 writes %d\n", L2->Writes);
      printf("Number of 2 writemisses %d\n", L2->WriteMisses);
      printf("Number of Writebacks %d\n", L2->WriteBacks);
      printf("L2 MissRate %.4f\n", (float)(L2->ReadMisses) / (L2->Reads));

      delete L2; // Clean up dynamically allocated L2 cache
   }
   printf("Main Memory Traffic is %d\n", MainMemTraffic);
   fclose(fp); // Close the trace file
   return 0;
}
