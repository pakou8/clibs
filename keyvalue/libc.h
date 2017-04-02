#pragma once

void *Malloc(size_t _size);
void Free(void *_pointer);
void *Memcpy(void *_dest, const void *_source, size_t _size);
void *Memset(void *_dest, int _value, size_t _size);
void *Memmove(void *_dest, const void *_source, size_t _size);
void Qsort(void *_base, size_t _number, size_t _size, int (*_compare)(const void *, const void *));
int Printf(const char *_format, ...);
char *Strcpy(char *_dest, const char *_source);
char *Strcat(char *_dest, const char *_source);
char *Strncpy(char *_dest, const char *_source, size_t _size);
char *Strrchr(char *_string, int _character);
int Strlen(const char *_string);
void Assert(int _expression);
