#include "utils.h"

#include <sys/stat.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <syslog.h>
#include <dirent.h>      
#include <string.h>

//kopiowanie plikow
int _copyFile(const char *src, const char *dest, int threshold)
{
    int srcFile = open(src, O_RDONLY); //otworzenie pliku zrodlowego
    if (srcFile == -1) //blad otwarcia
    {
        syslog(LOG_INFO, "[so-daemon] Blad otwarcia pliku zrodlowego (%s)", src);
        return 1;
    }

    struct stat st; //struktura do statysk plikow
    if (fstat(srcFile, &st) == -1) //blad otwarcia informacji o pliku
    {
        syslog(LOG_INFO, "[so-daemon] Blad odczytu informacji o pliku zrodlowym (%s)", src);
        close(srcFile);
        return 1;
    }



    if (st.st_size <= threshold) //obsluga malych plikow (mniejszych niz ustawiony treshold domyslnie 100mb)
    {
        int destFile = open(dest, O_WRONLY | O_CREAT | O_TRUNC, st.st_mode); //otworzenie pliku docelowego, jezeli nie istnieje stworzenie go
        if (destFile == -1) //blad otwarcia
        {
            syslog(LOG_INFO, "[so-daemon] Blad otwarcia pliku docelowego (%s)", dest);
            close(srcFile);
            return 1;
        }

        int bufsize = 4096; //rozmiar buffora
		char* buffer = malloc(bufsize); //zaalokowanie pamieci na buffor
		if (!buffer) //niepowodzenie alokcji pamieci
		{
			syslog(LOG_INFO, "[so-daemon] Niepowodzenie alokacji pamieci dla bufora");
			close(srcFile);
			close(destFile);
			free(buffer);
			return 1;
		}

        ssize_t bytesRead, bytesWritten; //bufory w ktore beda zapisywane pakiety danych

	    do //petla do zapisu danych
		{
			bytesRead = read(srcFile, buffer, bufsize); //odczyt pakietu danych z pliku zrodlowego
			if (bytesRead == -1) //blad podczas odczytu
			{
				syslog(LOG_INFO, "[so-daemon] Blad podczas odczytu %s", src);
				close(srcFile);
				close(destFile);
				free(buffer);
				return 1;
			}
			else if (bytesRead == 0) //udalo sie odczytac calosc
				break;

			bytesWritten = write(destFile, buffer, bytesRead); //pakiet danych zapisanych
			if (bytesWritten != bytesRead) //nie udalo sie zapisac pakietu danych
			{
				syslog(LOG_INFO, "[so-daemon] Blad podczas zapisu do %s", dest);
				close(srcFile); //zamkniecie plikow i zwolnieni pamieci
				close(destFile);
				free(buffer);
				return 4;
			}
	    }
	    while (1);

        close(destFile); // zamkniecie pliku deocelowgo
		free(buffer); //zwolnieni bufforu
		syslog(LOG_INFO, "[so-daemon] Wykorzystano - read/write (%s)", src); //informacja do logow o uzyciu zapisu do malych plikow
    }
    else
    {
        // Kopiowanie przy pomocy mmap/write dla duzych plikow
        int destFile = open(dest, O_WRONLY | O_CREAT | O_TRUNC, st.st_mode); //otworzenie pliku docelowego, jezeli nie istnieje stworzenie go
        if (destFile == -1) //blad otwarcia
        {
            syslog(LOG_INFO, "[so-daemon] Blad otwarcia pliku docelowego (%s)", dest);
            close(srcFile);
            return 1;
        }

        char *srcData = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, srcFile, 0); //mapowanie pliku przy pomocy nmap
        if (srcData == MAP_FAILED)
        {
            syslog(LOG_INFO, "[so-daemon] Blad mapowania pliku zrodlowego w pamieci (%s)", src);
            close(srcFile);
            close(destFile);
            return 1;
        }

        ssize_t bytesWritten = write(destFile, srcData, st.st_size); //zapis do pliku docelowego
        if (bytesWritten != st.st_size) {
            syslog(LOG_INFO, "[so-daemon] Blad zapisu do pliku docelowego (%s)", dest);
	   		munmap(srcData, st.st_size);
            close(srcFile);
            close(destFile);
            return 1;
        }
        munmap(srcData, st.st_size); //mapa
        close(destFile); //zamkniecie pliku
		syslog(LOG_INFO, "[so-daemon] Wykorzystano - MMAP/write (%s)", src); //informacja do loga o uzyciu zapisu do duzych plikow
    }
    
    close(srcFile); //zamkniecie pliku zrodlowego
    syslog(LOG_INFO, "[so-daemon] Sukces przy kopiowaniu %s do %s", src, dest); //informacja o sukciesie
    return 0;
}

void copyFile(const char* srcPath, const char* destPath, const char* fileName, off_t bigFileSize)
{
   /**
    * dynamiczna alokacja pamieci na sciezke zrodlowa i docelowa pliku
    * zauwazmy, ze jest to spora oszczednosc biorac pod uwage ze
    * maksymalny rozmiar sciezki w linuksie (PATH_MAX) to standardowo
    * 4 KB !!!
   */
	char* pathFromCopy = calloc(strlen(srcPath) + strlen(fileName)+1, sizeof(char));
	char* pathToCopy = calloc(strlen(destPath) + strlen(fileName)+1, sizeof(char));
	printPath(pathFromCopy, srcPath, fileName);
	printPath(pathToCopy, destPath, fileName);
	//========================

	//log
	syslog(LOG_INFO, "[so-daemon] Kopiowanie z %s do %s", pathFromCopy, destPath); // pierwotnie fileName jako pierwszy string

	//kopiowanie ( najwazniejsze ;) )
	_copyFile(pathFromCopy, pathToCopy, bigFileSize);

	//zwalnianie pamieci
	free(pathFromCopy);
	free(pathToCopy);
}