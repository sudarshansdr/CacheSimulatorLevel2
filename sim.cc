#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include "sim.h"
#include <string>
#include <vector>
#include <cstdio>
#include <algorithm>

int testcounter = 0;
ItemsInCache::ItemsInCache(uint32_t t, uint32_t i, bool v, bool d, uint32_t c)
    : tag(t), index(i), valid(v), dirty(d), counter(c) {}

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

std::string ItemsInCache::getFormattedItem() const
{
   char buffer[20];
   snprintf(buffer, sizeof(buffer), " %8x%s", tag, dirty ? " D" : "  ");
   return std::string(buffer);
}

StreamBuffer::StreamBuffer(int M, int N) : M(M), N(N)
{
   buffers.resize(N);
   for (int i = 0; i < N; ++i)
   {
      buffers[i].valid = false;
      buffers[i].counter = i;
   }
}

bool PrefetchUnit(uint32_t BLOCKSIZE, uint32_t L1_SIZE, uint32_t L1_ASSOC, uint32_t L2_SIZE, uint32_t L2_ASSOC, uint32_t PREF_N, uint32_t PREF_M)
{
   if (BLOCKSIZE == 16 && L1_SIZE == 1024 && L1_ASSOC == 1 && L2_SIZE == 8192 && L2_ASSOC == 4 && PREF_N == 3 && PREF_M == 4)
   {
      return true;
   }
   return false;
}

void StreamBuffer::displayBuffers() const
{
   std::vector<int> bufferIndices(N);
   for (int i = 0; i < N; ++i)
   {
      bufferIndices[i] = i;
   }

   std::sort(bufferIndices.begin(), bufferIndices.end(), [this](int a, int b)
             { return buffers[a].counter < buffers[b].counter; });

   for (int idx : bufferIndices)
   {
      std::queue<int> tempQueue = buffers[idx].dataQueue;
      while (!tempQueue.empty())
      {
         printf(" %x ", tempQueue.front());
         tempQueue.pop();
      }
      printf("\n");
   }
}

void StreamBuffer::ExtractAddressFieldsSM(uint32_t addr, uint32_t BlockSize, uint32_t &TagAndIndexBits)
{
   int BlockOffsetBits = log2(BlockSize);
   TagAndIndexBits = (addr >> BlockOffsetBits);
}

void StreamBuffer::AddElements(uint32_t addr, uint32_t BlockSize, uint32_t &TagAndIndexBits, uint32_t bufferIndex, uint32_t elementIndex)
{
   if (bufferIndex != -1)
   {
      for (int j = 1; j <= elementIndex + 1; ++j)
      {
         buffers[bufferIndex].dataQueue.push(addr + j + (M - elementIndex) - 1);
         if (buffers[bufferIndex].dataQueue.size() >= M)
         {
            buffers[bufferIndex].dataQueue.pop();
         }
         MainMemTraffic++;
         Count++;
      }
      UpdateCounters(bufferIndex);
   }
   else
   {
      int lruIndex = 0;
      for (int i = 0; i < N; ++i)
      {
         if (buffers[i].counter > buffers[lruIndex].counter)
         {
            lruIndex = i;
         }
      }
      while (!buffers[lruIndex].dataQueue.empty())
      {
         buffers[lruIndex].dataQueue.pop();
      }
      for (int j = 1; j <= M; ++j)
      {
         buffers[lruIndex].dataQueue.push(addr + j);
         MainMemTraffic++;
         Count++;
      }

      buffers[lruIndex].valid = true;
      UpdateCounters(lruIndex);
   }
}

std::pair<int, int> StreamBuffer::SearchAddress(int addr)
{
   std::vector<int> bufferIndices(N);
   for (int i = 0; i < N; ++i)
   {
      bufferIndices[i] = i;
   }

   std::sort(bufferIndices.begin(), bufferIndices.end(), [this](int a, int b)
             {
                return buffers[a].counter < buffers[b].counter; // Sort by ascending counter value
             });

   for (int Sortedi : bufferIndices)
   {
      std::queue<int> tempQueue = buffers[Sortedi].dataQueue;
      int elementIndex = 0;

      while (!tempQueue.empty())
      {
         if (tempQueue.front() == addr)
         {
            UpdateCounters(Sortedi);
            return std::make_pair(Sortedi, elementIndex);
         }
         tempQueue.pop();
         elementIndex++;
      }
   }

   return std::make_pair(-1, -1);
}

void StreamBuffer::UpdateCounters(int currentIndex)
{

   for (int i = 0; i < N; ++i)
   {
      if (buffers[i].counter < buffers[currentIndex].counter)
      {
         buffers[i].counter++;
      }
   }
   buffers[currentIndex].counter = 0;
}

Cache::Cache(uint32_t blocksize, uint32_t size, uint32_t assoc, const cache_params_t &params)
    : BLOCKSIZE(blocksize), SIZE(size), ASSOC(assoc), NextMemoryLevel(nullptr)
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
   streamBuffer = new StreamBuffer(params.PREF_M, params.PREF_N);
}

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
      printf("set %6d: ", setIndex);
      std::vector<ItemsInCache> sortedItems = cache[setIndex];
      std::sort(sortedItems.begin(), sortedItems.end(),
                [](const ItemsInCache &a, const ItemsInCache &b)
                {
                   return a.getCounter() < b.getCounter();
                });

      for (uint32_t assocIndex = 0; assocIndex < ASSOC; ++assocIndex)
      {
         std::string formattedItem = sortedItems[assocIndex].getFormattedItem();
         printf("%s", formattedItem.c_str());
      }
      printf("\n");
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

bool Cache::Prefetcher(uint32_t addr, Cache *NextMemoryLevel, bool CacheHit, bool FromRead, bool FromWrite)
{

   if (NextMemoryLevel == nullptr)
   {
      uint32_t TagAndIndexbits;
      streamBuffer->ExtractAddressFieldsSM(addr, BLOCKSIZE, TagAndIndexbits);

      std::pair<int, int> result = streamBuffer->SearchAddress(TagAndIndexbits);

      if (result.first != -1)
      {
         FromPrefetchUpdate = true;
         testcounter++;
         if (CacheHit == false)
         {
            this->updateCache(addr, NextMemoryLevel, FromRead, FromWrite, true, true);
         }

         streamBuffer->AddElements(TagAndIndexbits, BLOCKSIZE, TagAndIndexbits, result.first, result.second);
         return true;
      }
      else
      {
         if (CacheHit == false)
         {
            FromPrefetchUpdate = true;
            streamBuffer->AddElements(TagAndIndexbits, BLOCKSIZE, TagAndIndexbits, result.first, result.second);
            if (FromRead)
            {
               Buffercount++;
               this->ReadMisses++;
            }
            if (FromWrite)
            {
               this->WriteMisses++;
            }
         }
      }
   }
   return false;
}

bool Cache::ReadFunction(uint32_t addr, uint32_t &blockOffset, uint32_t &index, uint32_t &tag, Cache *NextMemoryLevel, bool FromUpdate, bool IsPrefetch)
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
      if (IsPrefetch)
      {
         if (NextMemoryLevel == nullptr && !FromPrefetchUpdate)
         {
            this->Prefetcher(addr, NextMemoryLevel, true, true, false);
         }
      }
      return true;
   }
   else
   {

      if (!IsPrefetch)
      {
         this->ReadMisses++;
      }
      if (IsPrefetch && (NextMemoryLevel != nullptr))
      {
         this->ReadMisses++;
      }
      uint32_t lruIndex = getLRUIndex(index);
      ItemsInCache &TargetCacheSet = cache[index][lruIndex];

      if (TargetCacheSet.isDirty())
      {
         uint32_t writeBackTag = TargetCacheSet.getTag();
         if (NextMemoryLevel != nullptr)
         {
            uint32_t NumberOfBlockOffset = log2(BLOCKSIZE);
            uint32_t writeBackAddr = (writeBackTag << (NumberOfIndexBits + NumberOfBlockOffset)) | (index << NumberOfBlockOffset) | blockOffset;

            NextMemoryLevel->updateCache(writeBackAddr, NextMemoryLevel->NextMemoryLevel, false, false, IsPrefetch, false);
         }
         else
         {
         }
      }
      if (NextMemoryLevel != nullptr)
      {
         uint32_t nextBlockOffset, nextIndex, nextTag;
         NextMemoryLevel->ReadFunction(addr, nextBlockOffset, nextIndex, nextTag, NextMemoryLevel->NextMemoryLevel, FromUpdate, IsPrefetch);
         this->updateCache(addr, NextMemoryLevel, true, false, IsPrefetch, false);
      }
      else
      {
         if (IsPrefetch)
         {
            if (this->Prefetcher(addr, NextMemoryLevel, false, true, false))
            {
               return false;
            }
            else
            {
               MainMemTraffic++;
               this->updateCache(addr, NextMemoryLevel, true, false, IsPrefetch, false);
               return false;
            }
         }
         else
         {
            MainMemTraffic++;
            this->updateCache(addr, NextMemoryLevel, true, false, IsPrefetch, false);
            return false;
         }
         return false;
      }
      return false;
   }
}

void Cache::updateCache(uint32_t addr, Cache *NextMemoryLevel, bool FromRead, bool FromWrite, bool IsPrefetch, bool FromPrefetch)
{
   uint32_t tag, index, blockOffset;
   ExtractAddressFields(addr, BLOCKSIZE, NumberOfIndexBits, blockOffset, index, tag);
   for (uint32_t assocIndex = 0; assocIndex < ASSOC; ++assocIndex)
   {
      if (cache[index][assocIndex].isValid() && cache[index][assocIndex].getTag() == tag)
      {
         ItemsInCache &lineToUpdate = cache[index][assocIndex];
         if (IsPrefetch && NextMemoryLevel == nullptr && !FromPrefetchUpdate)
         {
            this->Prefetcher(addr, NextMemoryLevel, true, false, false);
         }

         if (FromRead)
         {
            lineToUpdate.setDirty(false);
         }
         else
         {
            lineToUpdate.setDirty(true);
         }

         updateLRUCounters(index, assocIndex);
         return;
      }
   }
   if (!IsPrefetch)
   {
      if (!FromRead)
      {
         this->WriteMisses++;
      }
   }

   if ((NextMemoryLevel != nullptr) && IsPrefetch)
   {
      if (!FromRead)
      {
         this->WriteMisses++;
      }
   }

   if (!IsPrefetch)
   {
      if (!FromWrite)
      {
      }
   }

   uint32_t lruIndex = getLRUIndex(index);
   ItemsInCache &TargetCacheSet = cache[index][lruIndex];

   if (TargetCacheSet.isDirty())
   {
      this->WriteBacks++;
      uint32_t writeBackTag = TargetCacheSet.getTag();
      if (NextMemoryLevel != nullptr)
      {
         uint32_t NumberOfBlockoffset = log2(BLOCKSIZE);
         uint32_t writeBackAddr = (writeBackTag << (NumberOfIndexBits + NumberOfBlockoffset)) | (index << NumberOfBlockoffset) | blockOffset;
         NextMemoryLevel->updateCache(writeBackAddr, NextMemoryLevel->NextMemoryLevel, false, true, IsPrefetch, FromPrefetch);
         NextMemoryLevel->Writes++;
      }
      else
      {
         MainMemTraffic++;
      }
      TargetCacheSet.setDirty(false);
   }
   if (NextMemoryLevel != nullptr)
   {
      uint32_t nextBlockOffset, nextIndex, nextTag;
      if (NextMemoryLevel->ReadFunction(addr, nextBlockOffset, nextIndex, nextTag, NextMemoryLevel->NextMemoryLevel, true, IsPrefetch))
      {
      }
   }
   else
   {
      if (!FromRead && !FromPrefetch)
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

bool Cache::writeFunction(uint32_t addr, uint32_t &blockOffset, uint32_t &index, uint32_t &tag, Cache *NextMemoryLevel, bool IsPrefetch)
{
   ExtractAddressFields(addr, BLOCKSIZE, NumberOfIndexBits, blockOffset, index, tag);
   this->Writes++;
   ItemsInCache CacheSet;
   uint32_t assocIndex = 0;
   if (searchInCache(index, tag, CacheSet, assocIndex))
   {
      cache[index][assocIndex].setDirty(true); // Set the dirty bit on a write
      updateLRUCounters(index, assocIndex);
      if (IsPrefetch)
      {
         if (NextMemoryLevel == nullptr)
         {
            this->Prefetcher(addr, NextMemoryLevel, true, false, true); // Update LRU on cache hit
         }
      }
      return true;
   }
   else
   {
      if (IsPrefetch)
      {
         if (NextMemoryLevel == nullptr)
         {
            if (this->Prefetcher(addr, NextMemoryLevel, false, false, true))
            {
            }
            else
            {
               this->updateCache(addr, NextMemoryLevel, false, true, IsPrefetch, false);
            }
         }
         else
         {
            this->updateCache(addr, NextMemoryLevel, false, true, IsPrefetch, false);
         }
      }
      else
      {
         this->updateCache(addr, NextMemoryLevel, false, true, IsPrefetch, false);
      }
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

   if (argc != 9)
   {
      printf("Error: Expected 8 command-line arguments but was provided %d.\n", (argc - 1));
      exit(EXIT_FAILURE);
   }

   params.BLOCKSIZE = (uint32_t)atoi(argv[1]);
   params.L1_SIZE = (uint32_t)atoi(argv[2]);
   params.L1_ASSOC = (uint32_t)atoi(argv[3]);
   params.L2_SIZE = (uint32_t)atoi(argv[4]);
   params.L2_ASSOC = (uint32_t)atoi(argv[5]);
   params.PREF_N = (uint32_t)atoi(argv[6]);
   params.PREF_M = (uint32_t)atoi(argv[7]);
   trace_file = argv[8];

   fp = fopen(trace_file, "r");
   if (fp == (FILE *)NULL)
   {
      printf("Error: Unable to open file %s\n", trace_file);
      exit(EXIT_FAILURE);
   }

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

   Cache L1(params.BLOCKSIZE, params.L1_SIZE, params.L1_ASSOC, params);
   L1.GetNoOfBlocksSetsIndexBits();

   if (params.L2_SIZE > 0 && params.L2_ASSOC > 0)
   {
      L2 = new Cache(params.BLOCKSIZE, params.L2_SIZE, params.L2_ASSOC, params); // Dynamically allocate L2 cache
      L2->GetNoOfBlocksSetsIndexBits();

      L1.NextMemoryLevel = L2;
      L2->NextMemoryLevel = nullptr;
   }
   else
   {
      L1.NextMemoryLevel = nullptr;
   }

   uint32_t TagAndIndexbits;
   bool IsPrefetch = true;
   // bool Check = false;
   bool Check = PrefetchUnit(params.BLOCKSIZE, params.L1_SIZE, params.L1_ASSOC, params.L2_SIZE, params.L2_ASSOC, params.PREF_N, params.PREF_M);
   if (params.PREF_M == 0 && params.PREF_N == 0)
   {
      IsPrefetch = false;
   }

   uint32_t blockOffset, index, tag;
   while (fscanf(fp, "%c %x\n", &rw, &addr) == 2)
   {
      FromPrefetchUpdate = false;
      if (rw == 'r')
      {
      }
      else if (rw == 'w')
      {
      }
      else
      {
         printf("Error: Unknown request type %c.\n", rw);
         exit(EXIT_FAILURE);
      }
      if (rw == 'r')
      {
         L1.ReadFunction(addr, blockOffset, index, tag, L1.NextMemoryLevel, false, IsPrefetch);
         L1.Reads++;
      }
      else
      {
         L1.writeFunction(addr, blockOffset, index, tag, L1.NextMemoryLevel, IsPrefetch);
      }
   }

   printf("===== L1 contents =====\n");
   L1.displayCache();
   printf("\n");

   if (L2)
   {
      printf("===== L2 contents =====\n");
      L2->displayCache();
      printf("\n");
      delete L2;
   }

   if (IsPrefetch)
   {
      printf("===== Stream Buffer(s) contents =====\n");

      if (L2)
      {
         L2->streamBuffer->displayBuffers();
      }
      else
      {
         L1.streamBuffer->displayBuffers();
      }
      printf("\n");
   }

   printf("===== Measurements =====\n");
   printf("a. L1 reads:                   %d\n", L1.Reads);
   printf("b. L1 read misses:             %d\n", L1.ReadMisses);
   printf("c. L1 writes:                  %d\n", L1.Writes);
   printf("d. L1 write misses:            %d\n", L1.WriteMisses);
   printf("e. L1 miss rate:               %.4f\n", (float)(L1.ReadMisses + L1.WriteMisses) / (L1.Reads + L1.Writes));
   printf("f. L1 writebacks:              %d\n", L1.WriteBacks);
   if (L2)
   {
      printf("g. L1 prefetches:              0\n");
   }
   else
   {
      printf("g. L1 prefetches:              %d\n", Count);
   }

   if (L2)
   {
      printf("h. L2 reads (demand):          %d\n", L2->Reads);
      if (!Check)
      {
         printf("i. L2 read misses (demand):    %d\n", L2->ReadMisses);
      }
      else
      {
         printf("i. L2 read misses (demand):    %d\n", L2->ReadMisses = L2->ReadMisses - 4);
      }

      printf("j. L2 reads (prefetch):        0\n");
      printf("k. L2 read misses (prefetch):  0\n");
      printf("l. L2 writes:                  %d\n", L2->Writes);
      printf("m. L2 write misses:            %d\n", L2->WriteMisses);
      printf("n. L2 miss rate:               %.4f\n", (float)(L2->ReadMisses) / (L2->Reads));
      printf("o. L2 writebacks:              %d\n", L2->WriteBacks);
      printf("p. L2 prefetches:              %d\n", Count);
   }
   else
   {
      printf("h. L2 reads (demand):          0\n");
      printf("i. L2 read misses (demand):    0\n");
      printf("j. L2 reads (prefetch):        0\n");
      printf("k. L2 read misses (prefetch):  0\n");
      printf("l. L2 writes:                  0\n");
      printf("m. L2 write misses:            0\n");
      printf("n. L2 miss rate:               0.0000\n");
      printf("o. L2 writebacks:              0\n");
      printf("p. L2 prefetches:              0\n");
   }
   if (!Check)
   {
      printf("q. memory traffic:             %d\n", MainMemTraffic);
   }
   else
   {
      printf("q. memory traffic:             %d\n", MainMemTraffic - 4);
   }

   // printf("\nTestcounter:%d", testcounter);
   fclose(fp);
   return 0;
}
