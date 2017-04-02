#pragma once

typedef unsigned __int64 uint64;

struct Filemap;
enum FilemapAccess { FILEMAP_READ = 0, FILEMAP_WRITE, FILEMAP_READWRITE, FILEMAP_COUNT };

Filemap *FilemapCreate(const char *_file, FilemapAccess _access = FILEMAP_READ, size_t _size = 0);
void FilemapDestroy(Filemap *_filemap, bool _delete = false);
void *FilemapGetAddress(Filemap *_filemap);