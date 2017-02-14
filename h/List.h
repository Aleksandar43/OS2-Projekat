#include "fs.h"
#ifndef LIST_H
#define LIST_H

struct Entry;
struct Node {
    Entry entry;
    Node *prev, *next;
};
class List {
private:
    Node *head, *tmp;
public:
    List() { head = nullptr; }
    void add(Entry &e);
    int remove(Entry &e);
    int doesExist(Entry &e);
    int empty() { return head == nullptr; }
    ~List() { while (head != nullptr) remove(head->entry); }
};

#endif