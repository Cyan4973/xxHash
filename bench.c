/*
    bench.c - Demo program to benchmark open-source algorithm
    Copyright (C) Yann Collet 2012-2013

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

	You can contact the author at :
	- Blog homepage : http://fastcompression.blogspot.com/
	- Discussion group : https://groups.google.com/forum/?fromgroups#!forum/lz4c
*/

//**************************************
// Compiler Options
//**************************************
 // Visual warning messages (must be first line)
#define _CRT_SECURE_NO_WARNINGS   

// Under Linux at least, pull in the *64 commands
#define _LARGEFILE64_SOURCE

// MSVC does not support S_ISREG
#ifndef S_ISREG
#define S_ISREG(x) (((x) & S_IFMT) == S_IFREG)
#endif



//**************************************
// Includes
//**************************************
#include <stdlib.h>     // malloc
#include <stdio.h>      // fprintf, fopen, ftello64
#include <sys/timeb.h>  // timeb
#include <sys/types.h>  // stat64
#include <sys/stat.h>   // stat64


//**************************************
// Hash Functions to test
//**************************************
#include "xxhash.h"
#define DEFAULTHASH XXH32
#define HASH0 XXH32



//**************************************
// Basic Types
//**************************************
#if defined(_MSC_VER)    // Visual Studio does not support 'stdint' natively
#define BYTE	unsigned __int8
#define U16		unsigned __int16
#define U32		unsigned __int32
#define S32		__int32
#define U64		unsigned __int64
#else
#include <stdint.h>
#define BYTE	uint8_t
#define U16		uint16_t
#define U32		uint32_t
#define S32		int32_t
#define U64		uint64_t
#endif


//**************************************
// Constants
//**************************************
#define PROGRAM_NAME "Benchmark utility using xxHash algorithm"
#define PROGRAM_VERSION ""
#define COMPILED __DATE__
#define AUTHOR "Yann Collet"
#define WELCOME_MESSAGE "*** %s %s, by %s (%s) ***\n", PROGRAM_NAME, PROGRAM_VERSION, AUTHOR, COMPILED

#define NBLOOPS		3           // Default number of benchmark iterations
#define TIMELOOP	2000        // Minimum timing per iteration

#define MAX_MEM		(1984<<20)


//**************************************
// Local structures
//**************************************

struct hashFunctionPrototype
{
	unsigned int (*hashFunction)(const void*, int, unsigned int);
};


//**************************************
// MACRO
//**************************************
#define DISPLAY(...) fprintf(stderr, __VA_ARGS__)



//**************************************
// Benchmark Parameters
//**************************************
static int nbIterations = NBLOOPS;

void BMK_SetNbIterations(int nbLoops)
{
	nbIterations = nbLoops;
	DISPLAY("- %i iterations-", nbIterations);
}



//*********************************************************
// Benchmark Functions
//*********************************************************

static int BMK_GetMilliStart()
{
  // Supposed to be portable
  // Rolls over every ~ 12.1 days (0x100000/24/60/60)
  // Use GetMilliSpan to correct for rollover
  struct timeb tb;
  int nCount;
  ftime( &tb );
  nCount = tb.millitm + (tb.time & 0xfffff) * 1000;
  return nCount;
}


static int BMK_GetMilliSpan( int nTimeStart )
{
  int nSpan = BMK_GetMilliStart() - nTimeStart;
  if ( nSpan < 0 )
    nSpan += 0x100000 * 1000;
  return nSpan;
}


static size_t BMK_findMaxMem(U64 requiredMem)
{
	size_t step = (64U<<20);   // 64 MB
	BYTE* testmem=NULL;

	requiredMem = (((requiredMem >> 25) + 1) << 26);
	if (requiredMem > MAX_MEM) requiredMem = MAX_MEM;

	requiredMem += 2*step;
	while (!testmem)
	{
		requiredMem -= step;
		testmem = malloc ((size_t)requiredMem);
	}

	free (testmem);
	return (size_t) (requiredMem - step);
}


static U64 BMK_GetFileSize(char* infilename)
{
	int r;
#if defined(_MSC_VER)
	struct _stat64 statbuf;
	r = _stat64(infilename, &statbuf);
#else
	struct stat statbuf;
	r = stat(infilename, &statbuf);
#endif
	if (r || !S_ISREG(statbuf.st_mode)) return 0;   // No good...
	return (U64)statbuf.st_size;
}


int BMK_benchFile(char** fileNamesTable, int nbFiles, int selection)
{
  int fileIdx=0;
  FILE* fileIn;
  char* infilename;
  U64 largefilesize;
  size_t benchedsize;
  size_t readSize;
  char* in_buff;
  struct hashFunctionPrototype hashP;
  unsigned int hashResult=0;

  U64 totals = 0;
  double totalc = 0.;


  // Init
  switch (selection)
  {
#ifdef HASH0
  case 0 : hashP.hashFunction = HASH0; break;
#endif
#ifdef HASH1
  case 1 : hashP.hashFunction = HASH1; break;
#endif
#ifdef HASH2
  case 2 : hashP.hashFunction = HASH2; break;
#endif
  default: hashP.hashFunction = DEFAULTHASH;
  }

  // Loop for each file
  while (fileIdx<nbFiles)
  {
	  // Check file existence
	  infilename = fileNamesTable[fileIdx++];
	  fileIn = fopen( infilename, "rb" );
	  if (fileIn==NULL)
	  {
		DISPLAY( "Pb opening %s\n", infilename);
		return 11;
	  }

	  // Memory allocation & restrictions
	  largefilesize = BMK_GetFileSize(infilename);
	  benchedsize = (size_t) BMK_findMaxMem(largefilesize);
	  if ((U64)benchedsize > largefilesize) benchedsize = (size_t)largefilesize;
	  if (benchedsize < largefilesize)
	  {
		  DISPLAY("Not enough memory for '%s' full size; testing %i MB only...\n", infilename, (int)(benchedsize>>20));
	  }

	  in_buff = malloc((size_t )benchedsize);

	  if(!in_buff)
	  {
		DISPLAY("\nError: not enough memory!\n");
		free(in_buff);
		fclose(fileIn);
		return 12;
	  }

	  // Fill input buffer
	  DISPLAY("Loading %s...       \r", infilename);
	  readSize = fread(in_buff, 1, benchedsize, fileIn);
	  fclose(fileIn);

	  if(readSize != benchedsize)
	  {
		DISPLAY("\nError: problem reading file '%s' !!    \n", infilename);
		free(in_buff);
		return 13;
	  }


	  // Bench
	  {
		int loopNb, nb_loops;
	    int milliTime;
		double fastestC = 100000000.;

		DISPLAY("\r%79s\r", "");       // Clean display line
		for (loopNb = 1; loopNb <= nbIterations; loopNb++)
		{
		  // Hash
		  DISPLAY("%1i-%-14.14s : %10i ->\r", loopNb, infilename, (int)benchedsize);

		  nb_loops = 0;
		  milliTime = BMK_GetMilliStart();
		  while(BMK_GetMilliStart() == milliTime);
		  milliTime = BMK_GetMilliStart();
		  while(BMK_GetMilliSpan(milliTime) < TIMELOOP)
		  {
            hashResult = hashP.hashFunction(in_buff, (int)benchedsize, 0);
			nb_loops++;
		  }
		  milliTime = BMK_GetMilliSpan(milliTime);

		  if ((double)milliTime < fastestC*nb_loops) fastestC = (double)milliTime/nb_loops;

		  DISPLAY("%1i-%-14.14s : %10i -> %7.1f MB/s\r", loopNb, infilename, (int)benchedsize, (double)benchedsize / fastestC / 1000.);

		}

		DISPLAY("%-16.16s : %10i -> %7.1f MB/s   0x%08X\n", infilename, (int)benchedsize, (double)benchedsize / fastestC / 1000., hashResult);

		totals += benchedsize;
		totalc += fastestC;
	  }

	  free(in_buff);
  }

  if (nbFiles > 1)
		printf("%-16.16s :%11llu -> %7.1f MB/s\n", "  TOTAL", (long long unsigned int)totals, (double)totals/totalc/1000.);

  return 0;
}


//*********************************************************
//  Main
//*********************************************************

int usage(char* exename)
{
	DISPLAY( "Usage :\n");
	DISPLAY( "      %s [arg] filename\n", exename);
	DISPLAY( "Arguments :\n");
	DISPLAY( " -i# : number of iterations \n");
	DISPLAY( " -h  : help (this text)\n");
	return 0;
}


int badusage(char* exename)
{
	DISPLAY("Wrong parameters\n");
	usage(exename);
	return 0;
}


int main(int argc, char** argv)
{
  int i,
	  filenamesStart=2;
  char* input_filename=0;

  // Welcome message
  DISPLAY( WELCOME_MESSAGE );

  if (argc<2) { badusage(argv[0]); return 1; }

  for(i=1; i<argc; i++)
  {
    char* argument = argv[i];

    if(!argument) continue;   // Protection if argument empty

	// Select command
	if (argument[0]=='-')
	{
		argument ++;

		// Display help on usage
		if ( argument[0] =='h' ) { usage(argv[0]); return 0; }

		// Modify Nb Iterations (benchmark only)
		if ( argument[0] =='i' ) { int iters = argument[1] - '0'; BMK_SetNbIterations(iters); continue; }

	}

	// first provided filename is input
    if (!input_filename) { input_filename=argument; filenamesStart=i; continue; }

  }

  // No input filename ==> Error
  if(!input_filename) { badusage(argv[0]); return 1; }

  return BMK_benchFile(argv+filenamesStart, argc-filenamesStart, 0);

}

