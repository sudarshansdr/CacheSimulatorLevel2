#ifndef CACHE_H
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
class ItemsInCache
{
private:
   uint32_t tag;
   uint32_t index;
   bool valid;
   bool dirty;
   uint32_t counter; // For replacement policy (like LRU)

public:
   ItemsInCache(uint32_t t = 0, uint32_t i = 0, bool v = false, bool d = false, uint32_t c = 0);

   // Getters and Setters
   uint32_t getTag() const;
   void setTag(uint32_t t);

   uint32_t getIndex() const;
   void setIndex(uint32_t i);

   bool isValid() const;
   void setValid(bool v);

   bool isDirty() const;
   void setDirty(bool d);

   uint32_t getCounter() const;
   void setCounter(uint32_t c);

   // Method to display for debugging
   void display() const;
   std::string getFormattedItem() const;
};

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
   Cache *NextCacheLevel;
   uint32_t Reads = 0;
   uint32_t ReadMisses = 0;
   uint32_t Writes = 0;
   uint32_t WriteMisses = 0;
   uint32_t WriteBacks = 0;
   uint32_t counter;

   std::vector<std::vector<ItemsInCache>> cache; // 2D dynamic array for cache elements

public:
   // Constructor
   Cache(uint32_t blocksize, uint32_t size, uint32_t assoc);

   // Method to calculate blocks, sets, and index bits
   void GetNoOfBlocksSetsIndexBits();

   // Method to access cache elements
   ItemsInCache &getItemsInCache(uint32_t setIndex, uint32_t assocIndex);

   // Method to display the cache configuration
   void displayConfig();
   void displayCache();
   bool compareByCounter(const ItemsInCache &a, const ItemsInCache &b);

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

#endif
