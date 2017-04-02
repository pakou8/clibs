#pragma once

#include "platform.h"

typedef uint64 Key;
struct Value { const void *data; size_t size; };
struct Stats { size_t keys, size, count, used, free; };

struct KeyValue;
KeyValue *KeyValueCreate(const char *_name, FilemapAccess _access = FILEMAP_READ, size_t _keys = 0, size_t _size = 0);
void KeyValueDestroy(KeyValue *_keyvalue);
bool KeyValueGet(KeyValue *_keyvalue, Key _key, Value *_value);
bool KeyValuePut(KeyValue *_keyvalue, Key _key, const Value *_value);
void KeyValueStats(KeyValue *_keyvalue, Stats *_stats);
