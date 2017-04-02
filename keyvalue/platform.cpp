#include "platform.h"
#include "libc.h"
#include <windows.h>

struct Filemap
{
	HANDLE file;
	HANDLE mapping;
	LPVOID address;
	char name[1024];
};

static const DWORD filemapAccess[FILEMAP_COUNT] = { GENERIC_READ, GENERIC_READ|GENERIC_WRITE, GENERIC_READ|GENERIC_WRITE };
static const DWORD filemapShare[FILEMAP_COUNT] = { FILE_SHARE_READ, 0, 0 };
static const DWORD filemapCreation[FILEMAP_COUNT] = { OPEN_EXISTING, CREATE_ALWAYS, OPEN_ALWAYS };
static const DWORD filemapProtect[FILEMAP_COUNT] = { PAGE_READONLY, PAGE_READWRITE, PAGE_READWRITE };
static const DWORD filemapMapAccess[FILEMAP_COUNT] = { FILE_MAP_READ, FILE_MAP_WRITE, FILE_MAP_READ|FILE_MAP_WRITE };

Filemap *FilemapCreate(const char *_file, FilemapAccess _access, size_t _size)
{
	Filemap *filemap = 0;
	SECURITY_ATTRIBUTES security = { sizeof(SECURITY_ATTRIBUTES), 0, TRUE };
	HANDLE file = CreateFile(_file, filemapAccess[_access], filemapShare[_access], &security, filemapCreation[_access], FILE_ATTRIBUTE_NORMAL, 0);
	if(file != INVALID_HANDLE_VALUE)
	{
		filemap = (Filemap *) Malloc(sizeof(Filemap));
		filemap->file = file;
		filemap->mapping = CreateFileMapping(filemap->file, &security, filemapProtect[_access], (DWORD)(_size >> 32), (DWORD)(_size&0xffffffff), _file);
		filemap->address = MapViewOfFile(filemap->mapping, filemapMapAccess[_access], 0, 0, _size);
		Strcpy(filemap->name, _file);
	}

	return filemap;
}

void FilemapDestroy(Filemap *_filemap, bool _delete)
{
	if(_filemap != 0)
	{
		UnmapViewOfFile(_filemap->address);
		CloseHandle(_filemap->mapping);
		CloseHandle(_filemap->file);
		if(_delete) DeleteFile(_filemap->name);
		Free(_filemap);
	}
}

void *FilemapGetAddress(Filemap *_filemap)
{
	return _filemap->address;
}
