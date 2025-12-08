#ifndef PERSISTENCE_H
#define PERSISTENCE_H

#include <stddef.h>

extern "C" {
#include <libyrs.h>
}

// File-based persistence
bool persistence_load(YDoc *doc);
bool persistence_save(YDoc *doc);

#endif // PERSISTENCE_H

