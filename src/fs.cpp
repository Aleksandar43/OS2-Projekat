#include "fs.h"
#include <iostream>

KernelFS* FS::myImpl = new KernelFS();

FS::~FS()
{
    delete myImpl;
}

char FS::mount(Partition * partition)
{
    char res = '0';
    int i = 0;
    EnterCriticalSection(&myImpl->partitionsCriticalSection);
    while (i < 26 && myImpl->partitions[i] != nullptr) i++;
    if (i < 26) {
        myImpl->partitions[i] = new PartitionData(partition);
        res = 'A' + i;
    #ifdef CONSOLE_PRINTS
        std::cout << "Partition mounted sucessfully on letter " << res << "\n";
    #endif
    }
#ifdef CONSOLE_PRINTS
    else std::cout << "Error: a maximum of 26 partitions can be mounted\n";
#endif
    LeaveCriticalSection(&myImpl->partitionsCriticalSection);
    return res;
}

char FS::unmount(char part)
{
    if (part<'A' || part>'Z') {
    #ifdef CONSOLE_PRINTS
        std::cout << "Error: not a capital letter\n";
    #endif
        return 0;
    }
    PartitionData *pd = myImpl->getPartDataPointer(part);
    if (pd == nullptr) {
    #ifdef CONSOLE_PRINTS
        std::cout << "Error: no partition on letter " << part << "\n";
    #endif
        return 0;
    }
    EnterCriticalSection(&pd->filesOpenCriticalSection);
    while (pd->filesOpen > 0)
        SleepConditionVariableCS(&pd->filesOpenConditionVariable, &pd->filesOpenCriticalSection, INFINITE);
    LeaveCriticalSection(&pd->filesOpenCriticalSection);
    EnterCriticalSection(&myImpl->partitionsCriticalSection);
    delete pd;
    myImpl->partitions[part - 'A'] = nullptr;
    LeaveCriticalSection(&myImpl->partitionsCriticalSection);
#ifdef CONSOLE_PRINTS
    std::cout << "Partition on letter " << part << " unmounted sucessfully\n";
#endif
    return 1;
}

char FS::format(char part)
{
    if (part<'A' || part>'Z') {
    #ifdef CONSOLE_PRINTS
        std::cout << "Error: not a capital letter\n";
    #endif
        return 0;
    }
    PartitionData *pd = myImpl->getPartDataPointer(part);
    if (pd == nullptr) {
    #ifdef CONSOLE_PRINTS
        std::cout << "Error: no partition on letter " << part << "\n";
    #endif
        return 0;
    }
    pd->formattingRequest = true;
    EnterCriticalSection(&pd->filesOpenCriticalSection);
    while (pd->filesOpen > 0 || pd->threadsReadingRootDir > 0)
        SleepConditionVariableCS(&pd->filesOpenConditionVariable, &pd->filesOpenCriticalSection, INFINITE);
    LeaveCriticalSection(&pd->filesOpenCriticalSection);
    pd->formatting = true;
    pd->formattingRequest = false;
    char buffer[ClusterSize] = "";
    for (ClusterNo i = 1; i < pd->part->getNumOfClusters(); i++)
        pd->part->writeCluster(i, buffer);
    buffer[0] = '\xC0';
    pd->part->writeCluster(0, buffer);
    pd->formatting = false;
    WakeAllConditionVariable(&pd->diskRWConditionVariable);
#ifdef CONSOLE_PRINTS
    std::cout << "Partition on letter " << part << " formatted sucessfully\n";
#endif
    return 1;
}

char FS::readRootDir(char part, EntryNum n, Directory & d)
{
    if (part<'A' || part>'Z') {
    #ifdef CONSOLE_PRINTS
        std::cout << "Error: not a capital letter\n";
    #endif
        return 0;
    }
    PartitionData *pd = myImpl->getPartDataPointer(part);
    if (pd == nullptr) {
    #ifdef CONSOLE_PRINTS
        std::cout << "Error: no partition on letter " << part << "\n";
    #endif
        return 0;
    }
    if (pd->formatting) {
    #ifdef CONSOLE_PRINTS
        std::cout << "Error: formatting in progress\n";
    #endif
        return 0;
    }
    pd->threadsReadingRootDir++;
    char entriesRead = 0;
    EntryNum entriesBeforeReading = n;
    Index buffer1, buffer2;
    char temp[2048];
    RootData &rtref = (RootData&)temp;
    pd->part->readCluster(1, (char*)buffer1);
    for (ClusterNo i = 0; i < sizeOfIndex; i++)
        if (buffer1[i] != 0)
            if (i < sizeOfIndex / 2) {
                pd->part->readCluster(buffer1[i], temp);
                for (EntryNum k = 0; k < ClusterSize / sizeof(Entry); k++)
                    if (!sameFile(rtref[k], emptyEntry)) {
                        if (entriesRead == ENTRYCNT) return ENTRYCNT + 1;
                        if (entriesBeforeReading > 0) entriesBeforeReading--;
                        else {
                            copyEntry(d[entriesRead], rtref[k]);
                            entriesRead++;
                        }
                    }
            }
            else {
                pd->part->readCluster(buffer1[i], (char*)buffer2);
                for (ClusterNo j = 0; j < sizeOfIndex; j++) {
                    pd->part->readCluster(buffer2[j], temp);
                    for (EntryNum k = 0; k < ClusterSize / sizeof(Entry); k++)
                        if (!sameFile(rtref[k], emptyEntry)) {
                            if (entriesRead == ENTRYCNT) return ENTRYCNT + 1;
                            if (entriesBeforeReading > 0) entriesBeforeReading--;
                            else {
                                copyEntry(d[entriesRead], rtref[k]);
                                entriesRead++;
                            }
                        }
                }
            }
    pd->threadsReadingRootDir--;
    return entriesRead;
}

char FS::doesExist(char * fname)
{
    char letter = fname[0];
    if (letter<'A' || letter>'Z') {
    #ifdef CONSOLE_PRINTS
        std::cout << "Error: not a capital letter\n";
    #endif
        return 0;
    }
    PartitionData *pd = myImpl->getPartDataPointer(letter);
    if (pd == nullptr) {
    #ifdef CONSOLE_PRINTS
        std::cout << "Error: no partition on letter " << letter << "\n";
    #endif
        return 0;
    }
    if (!(fname[1] == ':' && fname[2] == '\\' && fname[3] != '\0')) {
    #ifdef CONSOLE_PRINTS
        std::cout << "Error: invalid path\n";
    #endif
        return 0;
    }
    Entry e;
    ClusterNo c;
    EntryNum n;
    return pd->_doesExist(fname, e, c, n);
}

File * FS::open(char * fname, char mode)
{
    char letter = fname[0];
    if (letter<'A' || letter>'Z') {
    #ifdef CONSOLE_PRINTS
        std::cout << "Error: not a capital letter\n";
    #endif
        return nullptr;
    }
    PartitionData *pd = myImpl->getPartDataPointer(letter);
    if (pd == nullptr) {
    #ifdef CONSOLE_PRINTS
        std::cout << "Error: no partition on letter " << letter << "\n";
    #endif
        return nullptr;
    }
    if (!(fname[1] == ':' && fname[2] == '\\' && fname[3] != '\0')) {
    #ifdef CONSOLE_PRINTS
        std::cout << "Error: invalid path\n";
    #endif
        return nullptr;
    }
    if (pd->formattingRequest) {
    #ifdef CONSOLE_PRINTS
        std::cout << "Error: formatting has been requested\n";
    #endif
        return nullptr;
    }
    EnterCriticalSection(&pd->diskRWCriticalSection);
    if (pd->formatting) SleepConditionVariableCS(&pd->diskRWConditionVariable, &pd->diskRWCriticalSection, INFINITE);
    LeaveCriticalSection(&pd->diskRWCriticalSection);
    File *f = nullptr;
    int found;
    char temp[2048];
    RootData &rtref = (RootData&)temp;
    Entry returnEntry;
    ClusterNo returnCluster;
    EntryNum returnClusterPos;
    int getEntryAgain = 0;
    switch (mode) {
    case 'r':
        if (pd->_doesExist(fname,returnEntry,returnCluster,returnClusterPos)) {
            EnterCriticalSection(&pd->listsCriticalSection);
            while (pd->openForWriting.doesExist(returnEntry)) { 
                getEntryAgain = 1;
                SleepConditionVariableCS(&pd->listsConditionVariable, &pd->listsCriticalSection, INFINITE);
            }
            if (getEntryAgain) {
                EnterCriticalSection(&pd->diskRWCriticalSection);
                pd->part->readCluster(returnCluster, temp);
                copyEntry(returnEntry, rtref[returnClusterPos]);
                LeaveCriticalSection(&pd->diskRWCriticalSection);
            }
            /*if (!pd->openForReading.doesExist(returnEntry))*/ pd->openForReading.add(returnEntry);
            LeaveCriticalSection(&pd->listsCriticalSection);
            f = new File();
            EnterCriticalSection(&pd->filesOpenCriticalSection);
            pd->filesOpen++;
            LeaveCriticalSection(&pd->filesOpenCriticalSection);
            copyEntry(f->myImpl->entry, returnEntry);
            f->seek(0);
            f->myImpl->entryDataCluster = returnCluster;
            f->myImpl->entryDataClusterPos = returnClusterPos;
            f->myImpl->pd = pd;
        #ifdef CONSOLE_PRINTS
            printf("File %s%s%s opened\n", returnEntry.name, ".", returnEntry.ext);
        #endif
            return f;
        }
        else return nullptr;
    case 'w':
        Entry e = makeSearchEntry(fname);
        if (pd->_doesExist(fname, returnEntry, returnCluster, returnClusterPos)) {
            EnterCriticalSection(&pd->listsCriticalSection);
            while (pd->openForWriting.doesExist(returnEntry) || pd->openForReading.doesExist(returnEntry))
                SleepConditionVariableCS(&pd->listsConditionVariable, &pd->listsCriticalSection, INFINITE);
            FS::deleteFile(fname);
            LeaveCriticalSection(&pd->listsCriticalSection);
        }
        // naci slobodan ulaz
        Index cluster1, buffer2;
        found = 0;
        pd->part->readCluster(1, (char*)cluster1);
        for (ClusterNo i = 0; i < sizeOfIndex; i++) {
            if (cluster1[i] != 0)
                if (i < sizeOfIndex / 2) {
                    pd->part->readCluster(cluster1[i], temp);
                    for (EntryNum k = 0; k < ClusterSize / sizeof(Entry); k++)
                        if (rtref[k] == emptyEntry) {
                            copyEntry(returnEntry, e);
                            returnClusterPos = k;
                            returnCluster = cluster1[i];
                            copyEntry(rtref[k], e);
                            pd->part->writeCluster(cluster1[i], temp);
                            found = 1;
                            break;
                        }
                }
                else {
                    pd->part->readCluster(cluster1[i], (char*)buffer2);
                    for (ClusterNo j = 0; j < sizeOfIndex; j++) {
                        pd->part->readCluster(buffer2[j], temp);
                        for (EntryNum k = 0; k < ClusterSize / sizeof(Entry); k++)
                            if (rtref[k] == emptyEntry) {
                                copyEntry(returnEntry, e);
                                returnClusterPos = k;
                                returnCluster = buffer2[j];
                                copyEntry(rtref[k], e);
                                pd->part->writeCluster(buffer2[j], temp);
                                found = 1;
                                break;
                            }
                    }
                }
        }
        if (!found) {
            ClusterNo level1 = myImpl->findFreeBit(letter);
            if (level1 == ~0) {
            #ifdef CONSOLE_PRINTS
                std::cout << "Error: no free clusters\n";
            #endif
                return nullptr;
            }
            ClusterNo x = (ClusterNo)~0;
            for (ClusterNo i = 0; i < sizeOfIndex; i++)
                if (cluster1[i] == 0) {
                    x = i;
                    break;
                }
            if (x == ~0) {
            #ifdef CONSOLE_PRINTS
                std::cout << "Error: no free space for entry\n";
            #endif
                return nullptr;
            }
            char cluster0[ClusterSize];
            pd->part->readCluster(0, cluster0);
            /*myImpl->setBit(fname[0], level1, cluster0);*/
            pd->setBit(level1, cluster0);
            pd->part->writeCluster(0, cluster0);
            if (x < sizeOfIndex / 2) {
                cluster1[x] = level1;
                pd->part->writeCluster(1, (char*)cluster1);
                RootData *newRD;
                pd->part->readCluster(level1, temp);
                newRD = (RootData*)temp;
                copyEntry((*newRD)[0], e);
                copyEntry(returnEntry, e);
                returnCluster = level1;
                returnClusterPos = 0;
                pd->part->writeCluster(level1, temp);
            }
            else {
                ClusterNo level2 = myImpl->findFreeBit(letter);
                if (level2 == ~0) {
                #ifdef CONSOLE_PRINTS
                    std::cout << "Error: not enough free clusters\n";
                #endif
                    pd->part->readCluster(0, cluster0);
                    /*myImpl->resetBit(fname[0], level1, cluster0);*/
                    pd->resetBit(level1, cluster0);
                    pd->part->writeCluster(0, cluster0);
                    return nullptr;
                }
                ClusterNo y = (ClusterNo)~0;
                for (ClusterNo i = 0; i < sizeOfIndex; i++)
                    if (cluster1[i] == 0) {
                        y = i;
                        break;
                    }
                if (y == ~0) {
                #ifdef CONSOLE_PRINTS
                    std::cout << "Error: no free space for entry\n";
                #endif
                    pd->part->readCluster(0, cluster0);
                    /*myImpl->resetBit(fname[0], level1, cluster0);*/
                    pd->resetBit(level1, cluster0);
                    pd->part->writeCluster(0, cluster0);
                    return nullptr;
                }
                pd->part->readCluster(0, cluster0);
                /*myImpl->setBit(fname[0], level2, cluster0);*/
                pd->setBit(level2, cluster0);
                pd->part->writeCluster(0, cluster0);
                cluster1[x] = level1;
                pd->part->writeCluster(1, (char*)cluster1);
                pd->part->readCluster(level1, (char*)buffer2);
                buffer2[y] = level2;
                RootData *newRD;
                pd->part->readCluster(level2, temp);
                newRD = (RootData*)temp;
                copyEntry((*newRD)[0], e);
                copyEntry(returnEntry, e);
                returnCluster = level2;
                returnClusterPos = 0;
                pd->part->writeCluster(level2, temp);
            }
        }
        EnterCriticalSection(&pd->listsCriticalSection);
        pd->openForWriting.add(returnEntry);
        LeaveCriticalSection(&pd->listsCriticalSection);
        f = new File();
        EnterCriticalSection(&pd->filesOpenCriticalSection);
        pd->filesOpen++;
        LeaveCriticalSection(&pd->filesOpenCriticalSection);
        copyEntry(f->myImpl->entry, e);
        f->seek(0);
        f->myImpl->entryDataCluster = returnCluster;
        f->myImpl->entryDataClusterPos = returnClusterPos;
        f->myImpl->pd = pd;
        /*pd->filesOnPartition++;*/
    #ifdef CONSOLE_PRINTS
        printf("File %s%s%s opened\n", returnEntry.name, ".", returnEntry.ext);
    #endif
        return f;
    case 'a':
        if (pd->_doesExist(fname,returnEntry,returnCluster,returnClusterPos)) {
            EnterCriticalSection(&pd->listsCriticalSection);
            while (pd->openForWriting.doesExist(returnEntry) || pd->openForReading.doesExist(returnEntry)){
                getEntryAgain = 1;
                SleepConditionVariableCS(&pd->listsConditionVariable, &pd->listsCriticalSection, INFINITE);
            }
            if (getEntryAgain) {
                EnterCriticalSection(&pd->diskRWCriticalSection);
                pd->part->readCluster(returnCluster, temp);
                copyEntry(returnEntry, rtref[returnClusterPos]);
                LeaveCriticalSection(&pd->diskRWCriticalSection);
            }
            pd->openForWriting.add(returnEntry);
            LeaveCriticalSection(&pd->listsCriticalSection);
            f = new File();
            EnterCriticalSection(&pd->filesOpenCriticalSection);
            pd->filesOpen++;
            LeaveCriticalSection(&pd->filesOpenCriticalSection);
            copyEntry(f->myImpl->entry, returnEntry);
            f->seek(f->myImpl->entry.size);
            f->myImpl->entryDataCluster = returnCluster;
            f->myImpl->entryDataClusterPos = returnClusterPos;
            f->myImpl->pd = pd;
        #ifdef CONSOLE_PRINTS
            printf("File %s%s%s opened\n", returnEntry.name, ".", returnEntry.ext);
        #endif
            return f;
        }
        else return nullptr;
    default:
    #ifdef CONSOLE_PRINTS
        std::cout << "Error: incorrect mode\n";
    #endif
        return nullptr;
    }
}

char FS::deleteFile(char * fname)
{
    char letter = fname[0];
    if (letter<'A' || letter>'Z') {
    #ifdef CONSOLE_PRINTS
        std::cout << "Error: not a capital letter\n";
    #endif
        return 0;
    }
    PartitionData *pd = myImpl->getPartDataPointer(letter);
    if (pd == nullptr) {
    #ifdef CONSOLE_PRINTS
        std::cout << "Error: no partition on letter " << letter << "\n";
    #endif
        return 0;
    }
    if (!(fname[1] == ':' && fname[2] == '\\' && fname[3] != '\0')) {
    #ifdef CONSOLE_PRINTS
        std::cout << "Error: invalid path\n";
    #endif
        return 0;
    }
    Entry returnEntry;
    ClusterNo returnCluster;
    EntryNum returnClusterPos;
    if (!pd->_doesExist(fname,returnEntry,returnCluster,returnClusterPos)) return 0; // doesExist() na konzoli ispisuje "File does not exist"
    EnterCriticalSection(&pd->listsCriticalSection);
    if (pd->openForWriting.doesExist(returnEntry) || pd->openForReading.doesExist(returnEntry)) {
    #ifdef CONSOLE_PRINTS
        std::cout << "Error: file is open and cannot be deleted\n";
    #endif
        LeaveCriticalSection(&pd->listsCriticalSection);
        return 0;
    }
    LeaveCriticalSection(&pd->listsCriticalSection);
    Index indexCluster;
    char cluster0[ClusterSize];
    EnterCriticalSection(&pd->diskRWCriticalSection);
    pd->part->readCluster(returnEntry.indexCluster, (char*)indexCluster);
    pd->part->readCluster(0, cluster0);
    for (ClusterNo i = 0; i < sizeOfIndex; i++) {
        if (indexCluster[i] != 0) { /*myImpl->resetBit(fname[0], indexCluster[i], cluster0);*/
            if (i < sizeOfIndex / 2) {}
            else {
                Index level2cluster;
                pd->part->readCluster(indexCluster[i], (char*)level2cluster);
                for (ClusterNo j = 0; j < sizeOfIndex; j++) {
                    if (level2cluster[j] != 0) pd->resetBit(level2cluster[j], cluster0);
                }
            }
            pd->resetBit(indexCluster[i], cluster0);
        }
    }
    /*myImpl->resetBit(fname[0], pd->lastAccessedEntry.indexCluster, cluster0);*/
    pd->resetBit(returnEntry.indexCluster, cluster0);
    char temp[2048];
    RootData *rt;
    pd->part->readCluster(returnCluster, temp);
    rt = (RootData*)temp;
    (*rt)[returnClusterPos] = emptyEntry;
    pd->part->writeCluster(returnCluster, temp);
    pd->part->writeCluster(0, cluster0);
    LeaveCriticalSection(&pd->diskRWCriticalSection);
    //pd->part->writeCluster(pd->lastAccessedEntry.indexCluster, (char*)indexCluster);
    /*pd->filesOnPartition--;*/
    /*pd->lastAccessedEntry.indexCluster = 0;*/
#ifdef CONSOLE_PRINTS
    printf("File %s%s%s deleted\n", returnEntry.name, ".", returnEntry.ext);
#endif
    return 1;
}

FS::FS()
{
}
