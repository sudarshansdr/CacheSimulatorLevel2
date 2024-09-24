#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include "sim.h"
#include <string>

// Cache Constructor
Cache::Cache(uint32_t blocksize, uint32_t size, uint32_t assoc)
    : BLOCKSIZE(blocksize), SIZE(size), ASSOC(assoc) {}

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

// Method to extract address fields (tag, index, block offset)
void ExtractAddressFields(uint32_t addr, uint32_t BlockSize, uint32_t IndexBits, uint32_t &blockOffset, uint32_t &index, uint32_t &tag)
{
   int BlockOffsetBits = log2(BlockSize);

   uint32_t blockOffsetMask = (1 << BlockOffsetBits) - 1;
   uint32_t indexMask = (1 << IndexBits) - 1;

   blockOffset = addr & blockOffsetMask;
   index = (addr >> BlockOffsetBits) & indexMask;
   tag = addr >> (BlockOffsetBits + IndexBits);
}

/*  "argc" holds the number of command-line arguments.
    "argv[]" holds the arguments themselves.

    Example:
    ./sim 32 8192 4 262144 8 3 10 gcc_trace.txt
    argc = 9
    argv[0] = "./sim"
    argv[1] = "32"
    argv[2] = "8192"
    ... and so on
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

   Cache L2(params.BLOCKSIZE, params.L2_SIZE, params.L2_ASSOC);
   L2.displayConfig();
   L2.GetNoOfBlocksSetsIndexBits();
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
      ExtractAddressFields(addr, L1.BLOCKSIZE, L1.NumberOfIndexBits, blockOffset, index, tag);
      printf("Addresses: %x\n", addr);
      printf("Block Offset: %u\n", blockOffset);
      printf("Index: %x\n", index);
      printf("Tag: %x\n", tag);
      break;

      ///////////////////////////////////////////////////////
      // Issue the request to the L1 cache instance here.
      ///////////////////////////////////////////////////////
   }

   return (0);
}
