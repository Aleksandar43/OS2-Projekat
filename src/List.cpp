#include "List.h"

void List::add(Entry & e)
{
    Node *n = new Node();
    copyEntry(n->entry, e);
    n->next = head;
    n->prev = nullptr;
    if (head) head->prev = n;
    head = n;
}

int List::remove(Entry & e)
{
    if (doesExist(e)) {
        if (tmp != head) tmp->prev->next = tmp->next;
        else head = head->next;
        delete tmp;
        tmp = nullptr;
        return 1;
    }
    else return 0;
}

int List::doesExist(Entry & e)
{
    tmp = head;
    while (tmp)
        if (sameFile(tmp->entry, e)) return 1;
        else tmp = tmp->next;
    return 0;
}
