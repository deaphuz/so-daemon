#ifndef copy_h
#define copy_h

int _copyFile(const char* src, const char* dest, int threshold); //kopiowanie pliku
void copyFile(const char* srcPath, const char* destPath, const char* fileName, unsigned long long bigFileSize); //inizjalizacja kopiowania

#endif