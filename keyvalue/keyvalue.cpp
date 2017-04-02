#include "keyvalue.h"
#include "libc.h"
#include "platform.h"

const char INDEX_EXT[] = ".idx";
const char FAT_EXT[] = ".fat";
const char DATA_EXT[] = ".dat";
const char TEMP_EXT[] = ".tmp";

const size_t ALIGN_SIZE = 4;

struct Entry { size_t offset, size; };
struct Header { size_t keys, size, count, offset; };
struct KeyValue { Filemap *index, *fat, *data; Header *header; Key *keys; Entry *entries; char *buffer; char name[1024]; };

static inline uint64 BinarySearch(const Key *_keys, uint64 _min, uint64 _max, Key _key)
{
	while(_max > _min)
	{
		Key it = (_min + _max) >> 1;
		Key it1 = it + 1;
		bool test = _keys[it] < _key;
		_min = test ? it1 : _min;
		_max = test ? _max : it;
	}

	return _min;
}

static inline size_t Align(size_t _offset, size_t _align)
{
	size_t remainder = _offset & (_align - 1);
	if(remainder > 0) _offset = (_offset & ~(_align - 1)) + _align;
	return _offset;
}

KeyValue *KeyValueCreate(const char *_name, FilemapAccess _access, size_t _keys, size_t _size)
{		
	char file[1024];
	bool initialize = true;

	Strcpy(file, _name); Strcat(file, INDEX_EXT);
	Filemap *tmp = FilemapCreate(file);
	if(tmp != 0) { initialize = false; FilemapDestroy(tmp); }

	size_t sizeIndex = (_keys > 0) ? _keys * sizeof(Key) + sizeof(Header) : 0;
	Filemap *index = FilemapCreate(file, _access, sizeIndex);
	if(index == 0) { return 0; }

	Strcpy(file, _name); Strcat(file, FAT_EXT);
	Filemap *fat = FilemapCreate(file, _access, _keys * sizeof(Entry));
	if(fat == 0) { FilemapDestroy(index); return 0; }

	Strcpy(file, _name); Strcat(file, DATA_EXT);
	Filemap *data = FilemapCreate(file, _access, _size);
	if(data == 0) { FilemapDestroy(fat); FilemapDestroy(index); return 0; }

	KeyValue *keyvalue = (KeyValue *) Malloc(sizeof(KeyValue));
	keyvalue->index = index; keyvalue->fat = fat; keyvalue->data = data;
	keyvalue->header = (Header *) FilemapGetAddress(keyvalue->index);
	keyvalue->keys = (Key *) (keyvalue->header + 1);
	keyvalue->entries = (Entry *) FilemapGetAddress(keyvalue->fat);
	keyvalue->buffer = (char *) FilemapGetAddress(keyvalue->data);
	Strcpy(keyvalue->name, _name);
		
	if(sizeIndex > 0) keyvalue->header->keys = _keys;
	if(_size > 0) keyvalue->header->size = _size;
	if(initialize) { keyvalue->header->count = 0; keyvalue->header->offset = 0; }

	return keyvalue;
}

void KeyValueDestroy(KeyValue *_keyvalue)
{
	FilemapDestroy(_keyvalue->data);
	FilemapDestroy(_keyvalue->fat);
	FilemapDestroy(_keyvalue->index);
	Free(_keyvalue);
}

struct Item { size_t offset, index; };

static int ItemCompare(const void *_item1, const void *_item2)
{
	int result = 0;
	const Item *item1 = (const Item *)_item1;
	const Item *item2 = (const Item *)_item2;
	result = (item1->offset < item2->offset) ? -1 : result;
	result = (item1->offset > item2->offset) ? 1 : result;
	return result;
}

static void KeyValueCompact(KeyValue *_keyvalue)
{
	char file[1024];
	Strcpy(file, _keyvalue->name); Strcat(file, TEMP_EXT);
	Filemap *tmp = FilemapCreate(file, FILEMAP_READWRITE, sizeof(Item) * _keyvalue->header->count);

	if(tmp != 0)
	{
		size_t i;
		Item *items = (Item *) FilemapGetAddress(tmp); 
		for(i = 0; i < _keyvalue->header->count; ++i) { items[i].offset = _keyvalue->entries[i].offset;	items[i].index = i; }
		Qsort(items, _keyvalue->header->count, sizeof(Item), &ItemCompare);

		_keyvalue->header->offset = 0;
		for(i = 0; i < _keyvalue->header->count; ++i)
		{
			size_t index = items[i].index;
			Memcpy(_keyvalue->buffer + _keyvalue->header->offset, _keyvalue->buffer + _keyvalue->entries[index].offset, _keyvalue->entries[index].size);
			_keyvalue->entries[index].offset = _keyvalue->header->offset;
			_keyvalue->header->offset += Align(_keyvalue->entries[index].size, ALIGN_SIZE);
		}
		FilemapDestroy(tmp, true);
	}
}

static bool KeyValueUpdate(KeyValue *_keyvalue, uint64 _index, const Value *_value)
{
	bool result = false;
	if(_value->size <= _keyvalue->entries[_index].size)
	{
		Memcpy(_keyvalue->buffer + _keyvalue->entries[_index].offset, _value->data, _value->size);
		_keyvalue->entries[_index].size = _value->size;
		result = true;
	}
	else
	{
		if(_keyvalue->header->offset + _value->size > _keyvalue->header->size)
			KeyValueCompact(_keyvalue);

		if(_keyvalue->header->offset + _value->size <= _keyvalue->header->size)
		{
			Memcpy(_keyvalue->buffer + _keyvalue->header->offset, _value->data, _value->size);
			_keyvalue->entries[_index].offset = _keyvalue->header->offset;
			_keyvalue->entries[_index].size = _value->size;
			_keyvalue->header->offset += Align(_value->size, ALIGN_SIZE);
			result = true;
		}
	}

	return result;
}

static bool KeyValueInsert(KeyValue *_keyvalue, Key _key, uint64 _index, const Value *_value)
{
	bool result = false;
	if(_keyvalue->header->count < _keyvalue->header->keys)
	{
		if(_keyvalue->header->offset + _value->size > _keyvalue->header->size)
			KeyValueCompact(_keyvalue);

		if(_keyvalue->header->offset + _value->size <= _keyvalue->header->size)
		{
			Memcpy(_keyvalue->buffer + _keyvalue->header->offset, _value->data, _value->size);
			Memmove(_keyvalue->keys + _index + 1, _keyvalue->keys + _index, sizeof(Key) * (_keyvalue->header->count - _index));			
			Memmove(_keyvalue->entries + _index + 1, _keyvalue->entries + _index, sizeof(Entry) * (_keyvalue->header->count - _index));
			_keyvalue->keys[_index] = _key;
			_keyvalue->entries[_index].offset = _keyvalue->header->offset;
			_keyvalue->entries[_index].size = _value->size;
			_keyvalue->header->offset += Align(_value->size, ALIGN_SIZE);
			++_keyvalue->header->count;
			result = true;
		}
	}

	return result;
}

static bool KeyValueDelete(KeyValue *_keyvalue, uint64 _index)
{
	Memcpy(_keyvalue->keys + _index, _keyvalue->keys + _index + 1, sizeof(Key) * (_keyvalue->header->count - _index - 1));
	Memcpy(_keyvalue->entries + _index, _keyvalue->entries + _index + 1, sizeof(Entry) * (_keyvalue->header->count - _index - 1));
	--_keyvalue->header->count;
	return true;
}

bool KeyValueGet(KeyValue *_keyvalue, Key _key, Value *_value)
{
	bool result = false;
	uint64 index = BinarySearch(_keyvalue->keys, 0, _keyvalue->header->count, _key);
	if(index < _keyvalue->header->count && _keyvalue->keys[index] == _key)
	{
		_value->data = _keyvalue->buffer + _keyvalue->entries[index].offset;
		_value->size = _keyvalue->entries[index].size;
		result = true;
	}

	return result;
}

bool KeyValuePut(KeyValue *_keyvalue, Key _key, const Value *_value)
{
	bool result = false;
	uint64 index = BinarySearch(_keyvalue->keys, 0, _keyvalue->header->count, _key);
	if(index < _keyvalue->header->count && _keyvalue->keys[index] == _key)
	{
		if(_value != 0) result = KeyValueUpdate(_keyvalue, index, _value); 
		else result = KeyValueDelete(_keyvalue, index);
	}
	else if(_value != 0) result = KeyValueInsert(_keyvalue, _key, index, _value);

	return result;
}

void KeyValueStats(KeyValue *_keyvalue, Stats *_stats)
{
	_stats->keys = _keyvalue->header->keys;
	_stats->size = _keyvalue->header->size;
	_stats->count = _keyvalue->header->count;
	_stats->free = _stats->size - _keyvalue->header->offset;

	size_t i;
	_stats->used = 0;
	for(i = 0; i < _keyvalue->header->count; ++i) _stats->used += _keyvalue->entries[i].size;
}
