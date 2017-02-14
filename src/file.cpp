#include "file.h"
//#include <iostream>

File::File() {
    myImpl = new KernelFile();
}

File::~File() {
#ifdef CONSOLE_PRINTS
    printf("File %s%s%s closed\n", myImpl->entry.name, strcmp(myImpl->entry.ext, "") != 0 ? "." : "", myImpl->entry.ext);
#endif
    delete myImpl;
}

char File::write(BytesCnt n, char * buffer)
{
    if (n == 0) {
    #ifdef CONSOLE_PRINTS
        std::cout << "Warning: writing 0 bytes into a file requested\n";
    #endif
        return 1;
    }
    EnterCriticalSection(&myImpl->pd->listsCriticalSection);
    if (myImpl->pd->openForReading.doesExist(myImpl->entry)) {
    #ifdef CONSOLE_PRINTS
        std::cout << "Error: file is opened only for reading\n";
    #endif
        LeaveCriticalSection(&myImpl->pd->listsCriticalSection);
        return 0;
    }
    LeaveCriticalSection(&myImpl->pd->listsCriticalSection);
    BytesCnt bytesWritten = 0;
    Index index;
    char cluster0[ClusterSize];
    if (myImpl->entry.size == 0) {
        EnterCriticalSection(&myImpl->pd->diskRWCriticalSection);
        ClusterNo cluster = myImpl->pd->findFreeBit();
        if (cluster == ~0) {
        #ifdef CONSOLE_PRINTS
            std::cout << "Error: no free clusters\n";
        #endif
            LeaveCriticalSection(&myImpl->pd->diskRWCriticalSection);
            return 0;
        }
        myImpl->pd->part->readCluster(0, cluster0);
        myImpl->pd->setBit(cluster, cluster0);
        myImpl->pd->part->writeCluster(0, cluster0);
        LeaveCriticalSection(&myImpl->pd->diskRWCriticalSection);
        myImpl->entry.indexCluster = cluster;
    }
    myImpl->pd->part->readCluster(myImpl->entry.indexCluster, (char*)index);
    char data[ClusterSize];
    ClusterNo cluster = ~0;
    //myImpl->pd->part->readCluster(index[0], data); //!!!
    while (bytesWritten < n) {
        if (myImpl->filePos == myImpl->entry.size) { // na kraju fajla
            if (myImpl->entry.size%ClusterSize == 0) {
                EnterCriticalSection(&myImpl->pd->diskRWCriticalSection);
                ClusterNo x = myImpl->entry.size / ClusterSize;
                if (x < sizeOfIndex / 2) {
                    ClusterNo y = myImpl->pd->findFreeBit();
                    if (y == ~0) {
                        LeaveCriticalSection(&myImpl->pd->diskRWCriticalSection);
                        return 0;
                    }
                    myImpl->pd->part->readCluster(0, cluster0);
                    myImpl->pd->setBit(y, cluster0);
                    myImpl->pd->part->writeCluster(0, cluster0);
                    index[x] = y;
                    myImpl->pd->part->writeCluster(myImpl->entry.indexCluster, (char*)index);
                }
                else if (x < sizeOfIndex / 2 + sizeOfIndex / 2 * sizeOfIndex) {
                    ClusterNo z = myImpl->pd->findFreeBit();
                    if (z == ~0) {
                        LeaveCriticalSection(&myImpl->pd->diskRWCriticalSection);
                        return 0;
                    }
                    myImpl->pd->part->readCluster(0, cluster0);
                    myImpl->pd->setBit(z, cluster0);
                    myImpl->pd->part->writeCluster(0, cluster0);
                    ClusterNo level1pos = sizeOfIndex / 2 + (x - sizeOfIndex / 2) / sizeOfIndex;
                    ClusterNo level2pos = (x - sizeOfIndex / 2) % sizeOfIndex;
                    if (level2pos == 0) {
                        ClusterNo y = myImpl->pd->findFreeBit();
                        if (y == ~0) {
                            myImpl->pd->part->readCluster(0, cluster0);
                            myImpl->pd->resetBit(z, cluster0);
                            myImpl->pd->part->writeCluster(0, cluster0);

                            LeaveCriticalSection(&myImpl->pd->diskRWCriticalSection);
                            return 0;
                        }
                        myImpl->pd->part->readCluster(0, cluster0);
                        myImpl->pd->setBit(y, cluster0);
                        myImpl->pd->part->writeCluster(0, cluster0);
                        index[level1pos] = y;
                    }
                    Index level2index;
                    myImpl->pd->part->readCluster(index[level1pos], (char*)level2index);
                    level2index[level2pos] = z;
                    myImpl->pd->part->writeCluster(index[level1pos], (char*)level2index);
                    myImpl->pd->part->writeCluster(myImpl->entry.indexCluster, (char*)index);
                }
                else {
                #ifdef CONSOLE_PRINTS
                    printf("Error: maximum file size reached\n");
                #endif
                    LeaveCriticalSection(&myImpl->pd->diskRWCriticalSection);
                    return 0;
                }
                LeaveCriticalSection(&myImpl->pd->diskRWCriticalSection);
            }
            myImpl->entry.size++;
        }
        ClusterNo posCluster = myImpl->filePos / ClusterSize;
        if (posCluster != cluster) {
            if (posCluster < sizeOfIndex / 2) {
                myImpl->pd->part->readCluster(index[posCluster], data);
            }
            else {
                ClusterNo level1pos = sizeOfIndex / 2 + (posCluster - sizeOfIndex / 2) / sizeOfIndex;
                Index level2;
                myImpl->pd->part->readCluster(index[level1pos], (char*)level2);
                ClusterNo level2pos = (posCluster - sizeOfIndex / 2) % sizeOfIndex;
                myImpl->pd->part->readCluster(level2[level2pos], data);
            }
            cluster = posCluster;
        }
        unsigned int posByte = myImpl->filePos % ClusterSize;
        data[posByte] = buffer[bytesWritten];
        if (posByte == ClusterSize - 1) {
            if (posCluster < sizeOfIndex / 2) {
                myImpl->pd->part->writeCluster(index[posCluster], data);
            }
            else {
                ClusterNo level1pos = sizeOfIndex / 2 + (posCluster - sizeOfIndex / 2) / sizeOfIndex;
                Index level2;
                myImpl->pd->part->readCluster(index[level1pos], (char*)level2);
                ClusterNo level2pos = (posCluster - sizeOfIndex / 2) % sizeOfIndex;
                myImpl->pd->part->writeCluster(level2[level2pos], data);
            }
        }
        myImpl->filePos++;
        bytesWritten++;
    }
    if (cluster < sizeOfIndex / 2) {
        myImpl->pd->part->writeCluster(index[cluster], data);
    }
    else {
        ClusterNo level1pos = sizeOfIndex / 2 + (cluster - sizeOfIndex / 2) / sizeOfIndex;
        Index level2;
        myImpl->pd->part->readCluster(index[level1pos], (char*)level2);
        ClusterNo level2pos = (cluster - sizeOfIndex / 2) % sizeOfIndex;
        myImpl->pd->part->writeCluster(level2[level2pos], data);
    }
    return 1;
}

BytesCnt File::read(BytesCnt max, char * buffer)
{
    BytesCnt bytesRead = 0;
    Index index;
    myImpl->pd->part->readCluster(myImpl->entry.indexCluster, (char*)index);
    char data[ClusterSize];
    ClusterNo cluster = 0;
    myImpl->pd->part->readCluster(index[0], data);
    while (bytesRead < max && !eof()) { // proveriti eof()
        ClusterNo posCluster = myImpl->filePos / ClusterSize;
        if (posCluster != cluster) {
            if (posCluster < sizeOfIndex / 2) {
                myImpl->pd->part->readCluster(index[posCluster], data);
                cluster = posCluster;
            }
            else {
                ClusterNo level1pos = sizeOfIndex / 2 + (posCluster - sizeOfIndex / 2) / sizeOfIndex;
                Index level2;
                myImpl->pd->part->readCluster(index[level1pos], (char*)level2);
                ClusterNo level2pos = (posCluster - sizeOfIndex / 2) % sizeOfIndex;
                myImpl->pd->part->readCluster(level2[level2pos], data);
            }
        }
        unsigned int posByte = myImpl->filePos % ClusterSize;
        buffer[bytesRead] = data[posByte];
        myImpl->filePos++;
        bytesRead++;
    }
    return bytesRead;
}

char File::seek(BytesCnt value)
{
    if (value < myImpl->entry.size) myImpl->filePos = value;
    else myImpl->filePos = myImpl->entry.size;
    return 1;
}

BytesCnt File::filePos()
{
    return myImpl->filePos;
}

char File::eof()
{
    if (myImpl->filePos == myImpl->entry.size) return 2;
    else if (myImpl->filePos < myImpl->entry.size) return 0;
    else return 1; //greska
}

BytesCnt File::getFileSize()
{
    return myImpl->entry.size;
}

char File::truncate()
{
    EnterCriticalSection(&myImpl->pd->listsCriticalSection);
    if (myImpl->pd->openForReading.doesExist(myImpl->entry)) {
    #ifdef CONSOLE_PRINTS
        std::cout << "Error: file is opened only for reading\n";
    #endif
        LeaveCriticalSection(&myImpl->pd->listsCriticalSection);
        return 0;
    }
    LeaveCriticalSection(&myImpl->pd->listsCriticalSection);
    myImpl->entry.size = myImpl->filePos;
    return 1;
}
