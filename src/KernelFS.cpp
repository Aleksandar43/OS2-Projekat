#include "KernelFS.h"

KernelFS::KernelFS()
{
    for (int i = 0; i < 26; i++) partitions[i] = nullptr;
    InitializeCriticalSection(&partitionsCriticalSection);
}

KernelFS::~KernelFS()
{
    DeleteCriticalSection(&partitionsCriticalSection);
    for (int i = 0; i < 26; i++) delete partitions[i];
}

char KernelFS::setBit(char part, ClusterNo pos, char* buffer)
{
    if (part<'A' || part>'Z') return 0;
    PartitionData *pd = getPartDataPointer(part);
    if (pd == nullptr) return 0;
    if (pos >= pd->part->getNumOfClusters()) return 0;
    ClusterNo byte = pos / 8;
    int bit = pos % 8;
    buffer[byte] |= 1 << (7 - bit);
    return 1;
}

char KernelFS::resetBit(char part, ClusterNo pos, char* buffer)
{
    if (part<'A' || part>'Z') return 0;
    PartitionData *pd = getPartDataPointer(part);
    if (pd == nullptr) return 0;
    if (pos >= pd->part->getNumOfClusters()) return 0;
    int byte = pos / 8;
    int bit = pos % 8;
    buffer[byte] &= ~(1 << (7 - bit));
    return 1;
}

ClusterNo KernelFS::findFreeBit(char part)
{
    if (part<'A' || part>'Z') return 0;
    PartitionData *pd = getPartDataPointer(part);
    if (pd == nullptr) return 0;
    char cluster0[ClusterSize];
    pd->part->readCluster(0, cluster0);
    for (ClusterNo byte = 0; byte < ClusterSize; byte++)
        for (int bit = 0; bit < 8; bit++) {
            char c = cluster0[byte];
            if (!(c & 1 << (7 - bit))) return byte * 8 + bit;
        }
    return (ClusterNo)~0;
}

char copyEntry(Entry & to, Entry & from)
{
    memcpy(to.name, from.name, FNAMELEN);
    memcpy(to.ext, from.ext, FEXTLEN);
    to.indexCluster = from.indexCluster;
    to.size = from.size;
    return 1;
}

int operator==(const Entry & e1, const Entry & e2)
{
    return sameFile(e1, e2) && e1.indexCluster == e2.indexCluster && e1.size == e2.size;
}

int sameFile(const Entry & e1, const Entry & e2)
{
    for (int i = 0; i < FNAMELEN; i++)
        if (e1.name[i] != e2.name[i]) return 0;
    for (int i = 0; i < FEXTLEN; i++)
        if (e1.ext[i] != e2.ext[i]) return 0;
    return 1;
}

Entry makeSearchEntry(char * fname)
{
    Entry tmp = { "","",0,0,0 };
    int i = 3;
    int pos = 0;
    while (pos < FNAMELEN && !(fname[i] == '.' || fname[i] == '\0')) {
        tmp.name[pos] = fname[i];
        i++;
        pos++;
    }
    if (pos == FNAMELEN && !(fname[i] == '.' || fname[i] == '\0')) {
    #ifdef CONSOLE_PRINTS
        std::cout << "makeSearchEntry: name shortened\n";
    #endif
    }
    while (!(fname[i] == '.' || fname[i] == '\0')) i++;
    if (fname[i] == '.') {
        pos = 0;
        i++;
        while (pos < FEXTLEN && !(fname[i] == '\0')) {
            tmp.ext[pos] = fname[i];
            i++;
            pos++;
        }
        if (fname[i] != '\0') {
        #ifdef CONSOLE_PRINTS
            std::cout << "makeSearchEntry: extension shortened\n";
        #endif
        }
    }
    return tmp;
}

ClusterNo PartitionData::findFreeBit()
{
    char cluster0[ClusterSize];
    part->readCluster(0, cluster0);
    for (ClusterNo byte = 0; byte < ClusterSize; byte++)
        for (int bit = 0; bit < 8; bit++) {
            if (byte * 8 + bit >= part->getNumOfClusters()) {
            #ifdef CONSOLE_PRINTS
                printf("Error: no free clusters\n");
            #endif
                return (ClusterNo)~0;
            }
            char c = cluster0[byte];
            if (!(c & 1 << (7 - bit))) return byte * 8 + bit;
        }
    return (ClusterNo)~0;
}

char PartitionData::setBit(ClusterNo pos, char * buffer)
{
    if (pos >= part->getNumOfClusters()) return 0;
    ClusterNo byte = pos / 8;
    int bit = pos % 8;
    buffer[byte] |= 1 << (7 - bit);
    return 1;
}

char PartitionData::resetBit(ClusterNo pos, char * buffer)
{
    if (pos >= part->getNumOfClusters()) return 0;
    int byte = pos / 8;
    int bit = pos % 8;
    buffer[byte] &= ~(1 << (7 - bit));
    return 1;
}

char PartitionData::_doesExist(char* fname, Entry & entry, ClusterNo & entryCluster, EntryNum & entryPos)
{
    Entry tmp = makeSearchEntry(fname);
    Index buffer1, buffer2;
    char temp[2048];
    RootData &rtref = (RootData&)temp;
    EnterCriticalSection(&diskRWCriticalSection);
    part->readCluster(1, (char*)buffer1);
    for (ClusterNo i = 0; i < sizeOfIndex; i++)
        if (buffer1[i] != 0)
            if (i < sizeOfIndex / 2) {
                part->readCluster(buffer1[i], temp);
                for (EntryNum k = 0; k < ClusterSize / sizeof(Entry); k++)
                    if (sameFile(rtref[k], tmp)) {
                    #ifdef CONSOLE_PRINTS
                        std::cout << "File " << tmp.name << (strcmp(tmp.ext, "") != 0 ? "." : "") << tmp.ext << " exists\n";
                    #endif
                        copyEntry(entry, rtref[k]);
                        entryPos = k;
                        entryCluster = buffer1[i];
                        LeaveCriticalSection(&diskRWCriticalSection);
                        return 1;
                    }
            }
            else {
                part->readCluster(buffer1[i], (char*)buffer2);
                for (ClusterNo j = 0; j < sizeOfIndex; j++) {
                    part->readCluster(buffer2[j], temp);
                    for (EntryNum k = 0; k < ClusterSize / sizeof(Entry); k++)
                        if (sameFile(rtref[k], tmp)) {
                        #ifdef CONSOLE_PRINTS
                            std::cout << "File " << tmp.name << (strcmp(tmp.ext, "") != 0 ? "." : "") << tmp.ext << " exists\n";
                        #endif
                            copyEntry(entry, rtref[k]);
                            entryPos = k;
                            entryCluster = buffer2[j];
                            LeaveCriticalSection(&diskRWCriticalSection);
                            return 1;
                        }
                }
            }
        #ifdef CONSOLE_PRINTS
            std::cout << "File " << tmp.name << (strcmp(tmp.ext, "") != 0 ? "." : "") << tmp.ext << " does not exist\n";
        #endif
            LeaveCriticalSection(&diskRWCriticalSection);
            return 0;
}

