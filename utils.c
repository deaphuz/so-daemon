#define _DEFAULT_SOURCE
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <linux/limits.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>

#include <dirent.h>
#include <syslog.h>

/**
 * printPath() jest funkcja, ktora otrzymuje 3 parametry:
 * char* buffer - wskaznik na bufor gdzie ma byc zapisany wynik dzialania funkcji
 * char* path - folder w ktorym znajduje sie plik
 * char* fileWord - nazwa pliku (bez sciezki)
 * 
 * PrintPath zamienia nazwe pliku na jego sciezke bezwzgledna.
*/
void printPath(char* buffer, const char* path, const char* fileWord)
{
    //pierwsza funkcja to strcpy w celu eliminacji smieci w lancuchu znakow
	strcpy(buffer, path);
	strcat(buffer, "/");
	strcat(buffer, fileWord);
}

/**
 * funkcja tworząca ścieżkę pliku ze ścieżki katalogu w którym jest plik i nazwy pliku
*/
int removeNotEmptyDir(const char* path)
{
	DIR* directory = opendir(path); //wskaznik na folder
    if (directory == NULL) { //blad otwarcia
		syslog(LOG_INFO, "[so-daemon] Nie mozna otworzyc katalogu przeznaczonego do usuniecia: %s", path);
        return 1;
    }
	struct dirent* dirEntry; //struktura przechowujaca wpis w katalogu

	do
	{
		if ( (dirEntry = readdir(directory)) != NULL ) //odczyt wpisow w katalogu
		{
			if(dirEntry->d_type != DT_DIR) //jest nie katalogiem
			{   //alokowanie pamieci na sciezke do pliku
				char* pathBuffer = calloc(strlen(path)+strlen(dirEntry->d_name) + 1, sizeof(char));
				printPath(pathBuffer, path, dirEntry->d_name);
				
				//usuniecie pliku wraz wraz z warunkiem na niepowodzenie
				if(remove(pathBuffer))
				{
					syslog(LOG_INFO, "[so-deamon] Niepowodzenie przy usuwaniu niepustego folderu: nie mozna usunac pliku %s", path);
					free(pathBuffer);
					return 1;
				} //zwolnienie buffora
				free(pathBuffer);
			}
			else if (strcmp(dirEntry->d_name, ".") && strcmp(dirEntry->d_name, "..") ) //jesli wpis jest katalogiem i nie jest katalogiem biezacym i nadrzednym
			{
				char* pathBuffer = calloc(strlen(path)+strlen(dirEntry->d_name) + 1, sizeof(char)); //alokowanie pamieci na sciezke do katalogu
				printPath(pathBuffer, path, dirEntry->d_name);
				removeNotEmptyDir(pathBuffer); //rekurencyjne przeszukiwanie katalogow i usuwanie plikow (gdyz nie da sie usunac niepustego katalogu!!!)
				free(pathBuffer); //zwolnienie buffora
			} 
			
		}
	} while (dirEntry != NULL);
	rewinddir(directory); //rewind wskaznika na poczatek katalogow zr/doc
	closedir(directory);
	rmdir(path); //ostatni krok, kiedy juz wszystkie katalogi wewnatrz biezacego katalogu i pliki zostaly usniete mozna usunac katalog rmdir
	return 0;
}


int isRegFile(const char *path) //jest zwyklym plikiem
{
    struct stat fileStats;
    stat(path, &fileStats);
    return S_ISREG(fileStats.st_mode); //zwraca non-zero jesli jest plikiem
}

int isDir(const char *path) //jest katalogiem
{
    struct stat fileStats;
    stat(path, &fileStats);
    return S_ISDIR(fileStats.st_mode); //zwraca non-zero jesli jest katalogiem
}