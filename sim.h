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
#include <queue>

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
uint32_t Buffercount = 0;
bool FromPrefetchUpdate = false;
bool PrefetchUnit(uint32_t Blocksize, uint32_t l1size, uint32_t l1assoc, uint32_t l2size, uint32_t l2assoc, uint32_t n, uint32_t m);

class StreamBuffer
{
private:
   struct Buffer
   {
      std::queue<int> dataQueue; // Queue to hold buffer values
      bool valid;                // Indicates if the queue is valid
      int counter;               // Counter for the stream buffer
   };

   std::vector<Buffer> buffers; // Array of stream buffers
   int M;                       // Number of elements in each buffer
   int N;                       // Number of stream buffers

public:
   // Constructor to initialize buffers with given sizes
   StreamBuffer(int M, int N);

   // Method to display the state of the buffers
   void displayBuffers() const;
   void AddElements(uint32_t addr, uint32_t BlockSize, uint32_t &TagAndIndexBits, uint32_t bufferIndex, uint32_t elementIndex);
   void ExtractAddressFieldsSM(uint32_t addr, uint32_t BlockSize, uint32_t &TagAndIndexBits);
   void UpdateCounters(int currentIndex);
   std::pair<int, int> SearchAddress(int addr);
};
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
   Cache *NextMemoryLevel;
   uint32_t Reads = 0;
   uint32_t ReadMisses = 0;
   uint32_t Writes = 0;
   uint32_t WriteMisses = 0;
   uint32_t WriteBacks = 0;
   uint32_t Prefetches = 0;
   uint32_t counter;
   uint32_t M;
   uint32_t N;

   std::vector<std::vector<ItemsInCache>> cache; // 2D dynamic array for cache elements
public:
   // Other member variables...

   StreamBuffer *streamBuffer; // Pointer to StreamBuffer instance

   // Constructor
   Cache(uint32_t blocksize, uint32_t size, uint32_t assoc);

public:
   //  Constructor
   Cache(uint32_t blocksize, uint32_t size, uint32_t assoc, const cache_params_t &params);

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
   bool ReadFunction(uint32_t addr, uint32_t &blockOffset, uint32_t &index, uint32_t &tag, Cache *NextMemoryLevel, bool FromUpdate, bool IsPrefetch);
   bool writeFunction(uint32_t addr, uint32_t &blockOffset, uint32_t &index, uint32_t &tag, Cache *NextMemoryLevel, bool IsPrefetch);
   void updateCache(uint32_t addr, Cache *NextMemoryLevel, bool FromRead, bool FromWrite, bool IsPrefetch, bool FromPrefetch);
   void updateLRUCounters(uint32_t setIndex, uint32_t assocIndex);
   uint32_t getLRUIndex(uint32_t index);
   void writeCache(uint32_t index, uint32_t tag);
   void updateCacheNextLevel(uint32_t addr, Cache *NextMemoryLevel);
   bool Prefetcher(uint32_t addr, Cache *NextMemoryLevel, bool CacheHit, bool FromRead, bool FromWrite);
};

#endif
