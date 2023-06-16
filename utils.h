#ifndef utils_h
#define utils_h

void printPath(char* buffer, const char* path, const char* fileWord); //Do buffora 'buffer' wkleja napis skladajacy sie ze sciezki, w ktorej znajduje sie plik i nazwy tego pliku.
int removeNotEmptyDir(const char* path); //Usuwanie rekurencyjne folderow niepustych z katalogu Docelowego.
int isRegFile(const char *path); //Sciezka wskazuje na zwykly plik
int isDir(const char *path); //Sciezka wskazuje na zwykly katalog

#endif