#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <malloc.h>
#include <assert.h>

void *Malloc(size_t _size) { return malloc(_size); }
void Free(void *_pointer) { free(_pointer); }
void *Memcpy(void *_dest, const void *_source, size_t _size) { return memcpy(_dest, _source, _size); }
void *Memset(void *_dest, int _value, size_t _size) { return memset(_dest, _value, _size); }
void *Memmove(void *_dest, const void *_source, size_t _size) { return memmove(_dest, _source, _size); }
void Qsort(void *_base, size_t _number, size_t _size, int (*_compare)(const void *, const void *)) { qsort(_base, _number, _size, _compare); }
char *Strcpy(char *_dest, const char *_source) { return strcpy(_dest, _source); }
char *Strcat(char *_dest, const char *_source) { return strcat(_dest, _source); }
char *Strncpy(char *_dest, const char *_source, size_t _size) { return strncpy(_dest, _source, _size); }
char *Strrchr(char *_string, int _character) { return strrchr(_string, _character); }
size_t Strlen(const char *_string) { return strlen(_string); }
void Assert(int _expression) { assert(_expression); }

int Printf(const char *_format, ...)
{
	int value;
	va_list list;

	va_start(list, _format);
	value = vprintf(_format, list);
	va_end(list);

	return value; 
}