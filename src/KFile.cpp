#include "KFile.h"

KernelFile::KernelFile() { entry = emptyEntry; entryDataCluster = entryDataClusterPos = filePos = 0; pd = nullptr; }

KernelFile::~KernelFile()
{
    EnterCriticalSection(&pd->listsCriticalSection);
    if (pd->openForReading.remove(entry)) {
        EnterCriticalSection(&pd->filesOpenCriticalSection);
        pd->filesOpen--;
        LeaveCriticalSection(&pd->filesOpenCriticalSection);
        WakeAllConditionVariable(&pd->listsConditionVariable);
    }
    else if (pd->openForWriting.remove(entry)) {
        EnterCriticalSection(&pd->filesOpenCriticalSection);
        pd->filesOpen--;
        LeaveCriticalSection(&pd->filesOpenCriticalSection);
        char temp[2048];
        RootData *rt;
        EnterCriticalSection(&pd->diskRWCriticalSection);
        pd->part->readCluster(entryDataCluster, temp);
        rt = (RootData*)temp;
        RootData &rtref = (RootData&)temp;
        copyEntry(rtref[entryDataClusterPos], entry);
        pd->part->writeCluster(entryDataCluster, temp);
        LeaveCriticalSection(&pd->diskRWCriticalSection);
        WakeAllConditionVariable(&pd->listsConditionVariable);
    }
    LeaveCriticalSection(&pd->listsCriticalSection);
    if (pd->filesOpen == 0) WakeAllConditionVariable(&pd->filesOpenConditionVariable);
}
