#ifndef KERNELFS_H
#define KERNELFS_H
#include <iostream>
//#define CONSOLE_PRINTS // <-- ispis za testiranje
#include <Windows.h>
#include "particija-VS2015\part.h"
#include "fs.h"
//const Entry emptyEntry = { "","",0,0,0 };
#include <cstring>
#include "List.h"
const Entry emptyEntry = { "","",0,0,0 };

class List;
class PartitionData {
    friend class FS;
    friend class KernelFS;
    friend class File;
    friend class KernelFile;
private:
    Partition *part = nullptr;
    /*int filesOnPartition = 0;*/
    int filesOpen = 0;
    bool formattingRequest = false;
    bool formatting = false;
    //Entry lastAccessedEntry = { "","",0,0,0 };
    //ClusterNo lastAccessedEntryCluster = 0;
    //EntryNum lastAccessedEntryPos = 0;
    List openForReading, openForWriting;
    CRITICAL_SECTION diskRWCriticalSection;
    CRITICAL_SECTION listsCriticalSection;
    CRITICAL_SECTION filesOpenCriticalSection;
    CONDITION_VARIABLE filesOpenConditionVariable;
    CONDITION_VARIABLE diskRWConditionVariable;
    CONDITION_VARIABLE listsConditionVariable;
    unsigned long threadsReadingRootDir = 0;
public:
    PartitionData(Partition* p) {
        part = p;
        openForReading = List();
        openForWriting = List();
        InitializeCriticalSection(&diskRWCriticalSection);
        InitializeCriticalSection(&listsCriticalSection);
        InitializeCriticalSection(&filesOpenCriticalSection);
        InitializeConditionVariable(&filesOpenConditionVariable);
        InitializeConditionVariable(&diskRWConditionVariable);
        InitializeConditionVariable(&listsConditionVariable);
    }
    ~PartitionData() {
        DeleteCriticalSection(&diskRWCriticalSection);
        DeleteCriticalSection(&listsCriticalSection);
        DeleteCriticalSection(&filesOpenCriticalSection);
    }
    ClusterNo findFreeBit();
    char setBit(ClusterNo bit, char* buffer); //buffer - niz bajtova koji se menja
    char resetBit(ClusterNo bit, char* buffer);
    char _doesExist(char * fname, Entry & entry, ClusterNo & entryCluster, EntryNum & entryPos);
};

const ClusterNo sizeOfIndex = ClusterSize / sizeof(ClusterNo);
typedef ClusterNo Index[sizeOfIndex];
typedef Entry RootData[ClusterSize / sizeof(Entry)];

class KernelFS {
    friend class FS;
private:
    PartitionData* partitions[26];
    CRITICAL_SECTION partitionsCriticalSection;
public:
    KernelFS();
    ~KernelFS();
    PartitionData* getPartDataPointer(char c) { return partitions[c - 'A']; }
    char setBit(char part, ClusterNo bit, char* buffer); //buffer - niz bajtova koji se menja
    char resetBit(char part, ClusterNo bit, char* buffer); //static ili r/w u funkciju?
    ClusterNo findFreeBit(char part);
};
char copyEntry(Entry & to, Entry & from);
int operator==(const Entry & e1, const Entry & e2);
int sameFile(const Entry & e1, const Entry & e2);
Entry makeSearchEntry(char* fname);
#endif