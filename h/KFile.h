#ifndef KFILE_H
#define KFILE_H
#include "KernelFS.h"
#include "fs.h"
class PartitionData;
class KernelFile {
    friend class File;
    friend class FS;
private:
    Entry entry;
    ClusterNo entryDataCluster;
    EntryNum entryDataClusterPos;
    unsigned long filePos;
    PartitionData *pd;
public:
    KernelFile();
    ~KernelFile();
};
#endif