#define _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#include "utils.h"
#include "copy.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/fs.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <utime.h>
#include <syslog.h>
#include <dirent.h>

#define DOT_CONDITION strcmp(sourceDirEntry->d_name, ".") && strcmp(sourceDirEntry->d_name, "..") //Wyklucznie z przeszukiwania linków na katalog biezacy i nadrzedny.

#define CALLOC_ADDBYTES 5 //dodatkowe bajty pamięci na alokacje nazw sciezek

volatile int wake_up = 0; //Zmienna do osblugi Sygnalow

/**
 * funkcja obsługi przerwania
 * LINUX zamienia definicję na sighandler_t
*/
static void sigHandler(int sigNum)
{
	if(sigNum == SIGUSR1)
	{
		wake_up = 1;
		return;
	}
}

int catalogSearch(const char* srcDirPath, const char* destDirPath, const char* appName, int bigFileSize, int customTime, bool folderInclude) //Glowna funkcja programu przeszukajacy katalog srcDirPath.
{
	DIR* sourceDir = opendir(srcDirPath); //wskaznik na katalog zrodlowy
	DIR* destinationDir = opendir(destDirPath); //wskaznik na katalog docelowy

    if (sourceDir == NULL) { //Czy udalo sie otworzyc sourceDir`
		syslog(LOG_INFO, "[%s] Nie mozna otworzyc katalogu zrodlowego %s", appName, srcDirPath);
        return 1;
    }

    if (destinationDir == NULL) { //Czy udalo sie otworzyc destinationDir
        syslog(LOG_INFO, "[%s] Nie mozna otworzyc katalogu docelowego %s", appName, destDirPath);
        return 1;
    }
	//powrot na poczatek katalogow
	rewinddir(sourceDir);
	rewinddir(destinationDir);
	errno = 0;


	struct dirent* sourceDirEntry; //struktura przechowujaca nazwe i typ pliku o danym wpisie w katalogu zrodlowym
	struct dirent* destDirEntry; //struktura przechowujaca nazwe i typ pliku o danym wpisie w katalogu docelowym
	struct stat srcFileStats; //struktura przechowujaca dodatkowe informacje o danym wpisie w katalogu zrodlowym
	struct stat destFileStats;//struktura przechowujaca dodatkowe informacje o danym wpisie w katalogu docelowym
	int found = 0;//zmienna pomocnicza do odnajdywania pliku
	do 
	{ 	
		//petla do na przeszukiwanie katalogu zrodlowego
		errno = 0;
		/** !!!
		 * jesli jest plik w katalogu źródłowym i jest ZWYKLYM plikiem
		*/
		sourceDirEntry = readdir(sourceDir);
		if ( ( sourceDirEntry != NULL ) && sourceDirEntry->d_type==DT_REG )
		{
			//to nastepuje szukanie go w katalogu docelowym
			found = 0;

			do // petla do na przeszukiwanie katalogu docelowego
			{	
				destDirEntry = readdir(destinationDir); //odczytanie kolejnego wpisu
				if( (destDirEntry != NULL) && destDirEntry->d_type==DT_REG ) //jezeli wpis jest zwyklym plikiem
				{

					//jesli znajdzie plik o takiej nazwie
					if( strcmp(sourceDirEntry->d_name, destDirEntry->d_name)==0 )
					{
						found = 1;

						//alokacja pamieci na sciezki do plikow
						char* srcFullPathName = calloc(strlen(srcDirPath)+strlen(sourceDirEntry->d_name) + CALLOC_ADDBYTES, sizeof(char));
						char* destFullPathName = calloc(strlen(destDirPath)+strlen(destDirEntry->d_name) + CALLOC_ADDBYTES, sizeof(char));
						printPath(srcFullPathName, srcDirPath, sourceDirEntry->d_name);
						printPath(destFullPathName, destDirPath, destDirEntry->d_name);
						

						//odczyt stat pliku zrodlowego i docelowego w celu
						//pozniejszego porownania daty modyfikacji plikow
						stat(srcFullPathName, &srcFileStats);
						stat(destFullPathName, &destFileStats);


						//to porownuje daty modyfikacji obu plikow
						// jesli czasy modyfikacji pliku zrodlowego jest wiekszy (pozniejszy) od docelowego
      					if ((long long)srcFileStats.st_mtim.tv_sec > (long long)destFileStats.st_mtim.tv_sec)
						{
							syslog(LOG_INFO, "[%s] Wykryto modyfikacje pliku %s", appName, sourceDirEntry->d_name);
							//jesli plik jest nowszy to kopiuj go do katalogu docelowego

							copyFile(srcDirPath, destDirPath, sourceDirEntry->d_name, bigFileSize);

							//i popraw czas modyfikacji docelowego
							struct utimbuf timeBuffer;
							timeBuffer.actime = srcFileStats.st_atim.tv_sec;
  						    timeBuffer.modtime = srcFileStats.st_mtim.tv_sec;
							utime(destFullPathName, &timeBuffer);

						}
						//zwolnienie pamieci na sciezki
						free(srcFullPathName);
						free(destFullPathName);
						//break zeby nie powtarzac operacji
						break;
					}
				}

			} while (destDirEntry != NULL);
			//powrot na poczatek katalogu docelowego
			rewinddir(destinationDir);
			//jesli nie znaleziono pliku nalezy go skopiowac do docelowego katalogu
			if(found == 0)
			{
				copyFile(srcDirPath, destDirPath, sourceDirEntry->d_name, bigFileSize);
			}
		}
		//jesli wpis w kat zrodlowym jest katalogiem
		else if(( (sourceDirEntry != NULL ) && sourceDirEntry->d_type==DT_DIR && folderInclude && DOT_CONDITION))
		{
			//to nastepuje szukanie go w katalogu docelowym
			found = 0;
			do // petla do na przeszukiwanie katalogu docelowego
			{	
				if( ( (destDirEntry = readdir(destinationDir)) != NULL) && strcmp(sourceDirEntry->d_name, destDirEntry->d_name)==0 && destDirEntry->d_type==DT_DIR && DOT_CONDITION)
				{
					//jesli zostanie znaleziony to informujemy i breakujemy
					found = 1;
					break;
				}

			} while (destDirEntry != NULL); //szukaj tak dlugo az beda kolejne wpisy w katalogu

			//alokacja pamieci na bufory sciezek
			char* sourcePathBuffer = calloc(strlen(srcDirPath)+strlen(sourceDirEntry->d_name) + CALLOC_ADDBYTES, sizeof(char));
			char* destPathBuffer = calloc(strlen(destDirPath)+strlen(sourceDirEntry->d_name) + CALLOC_ADDBYTES, sizeof(char));
			printPath(sourcePathBuffer, srcDirPath, sourceDirEntry->d_name);
			printPath(destPathBuffer, destDirPath, sourceDirEntry->d_name);

			int dirCreateSuccess = 1; //zmienna pomocnicza
			//jesli nie znaleziono w katalogu DOCELOWYM wpisu ktory jest katalogiem
			if (!found)
			{
				//to nalezy stworzyc katalog
				if(!mkdir(destPathBuffer,0777)) //jezeli utworzono katalog
					syslog(LOG_INFO, "[%s] Pomyslnie utworzono katalog %s", appName, destPathBuffer);
				else //jezeli nie utworzono katalogu zmien dirCreateSuccess i stworz wpis w logach
				{
					syslog(LOG_INFO, "[%s] Nie udalo sie stworzyc katalogu %s", appName, destPathBuffer);
					dirCreateSuccess = 0;
				}

			}
			//jezeli udalo sie utworzyc katalog ktorego nie bylo w katDocelowym(lub byl tam wczesniej), to wejdz do niego i rekurencyjne odpal funkcje przeszukiwania katalogu
			if(dirCreateSuccess)
				catalogSearch(sourcePathBuffer, destPathBuffer, appName, bigFileSize, customTime, folderInclude);
			//zwolnij buffery na sciezki do plikow
			free(sourcePathBuffer);
			free(destPathBuffer);

			//powrot na poczatek katalogu docelowego
			rewinddir(destinationDir);
		}
	} while (sourceDirEntry != NULL);

	//Cofniecie wskaznika na poczatek katalogu zrodlowego i docelowego (rewind)
	rewinddir(sourceDir);
	rewinddir(destinationDir);

	//petla na przeszukiwanie katalogu docelowego i usuwanie wpisów ktorych nie ma w katalogu zrodlowym
	do
	{		
		//jesli znaleziono jakis (jakikolwiek) wpis w katalogu DOCELOWYM ktory jest PLIKIEM lub KATALOGIEM
		if ( ( (destDirEntry = readdir(destinationDir)) != NULL ) && ( destDirEntry->d_type==DT_REG || (destDirEntry->d_type==DT_DIR && folderInclude ) ) )
		{
			found = 0;
			//to nastepuje szukanie go w zrodlowym
			do
			{	//jezeli znaleziono ten plik/katalog w katalogu zrodlowym ustaw found na 1 i pomin
				if (( (sourceDirEntry = readdir(sourceDir)) != NULL ) && strcmp(sourceDirEntry->d_name, destDirEntry->d_name)==0)
				{
					found = 1;

					break;
				}
			} while (sourceDirEntry != NULL);

			/**
			 * jesli NIE ZNALEZIONO WPISU w katalogu zrodlowym to
			 * w katalogu docelowym jest cos co nalezy
			 * usunac z niego (katalogu docelowego)
			 */
			if(!found)
			{
				//to nalezy stworzyc sciezke
				char* pathBuffer = calloc(strlen(destDirPath)+strlen(destDirEntry->d_name) + CALLOC_ADDBYTES, sizeof(char));
				printPath(pathBuffer, destDirPath, destDirEntry->d_name);

				//usuwanie pliku/katalogu z katalogu zrodlowego
				if(destDirEntry->d_type == DT_REG) //wpis jest plikiem
				{
					if(!remove(pathBuffer))
						syslog(LOG_INFO, "[%s] Usunieto pomyslnie plik %s", appName, pathBuffer);
					else
						syslog(LOG_INFO, "[%s] Blad przy usuwaniu pliku %s", appName, pathBuffer);
				}

				else if (destDirEntry->d_type == DT_DIR) //wpis jest katalogiem
				{
					if(!remove(pathBuffer)) //pomyslne usuniecie katalogu
						syslog(LOG_INFO, "[%s] Usunieto pomyslnie katalog %s", appName, pathBuffer);
					else
					{
						if(errno == ENOTEMPTY) //jezeli katalog nie byl pusty(mkdir nie moze go usunac) odpal rekurencyjne usuwanie plikow z katalogu
						{
							removeNotEmptyDir(pathBuffer);
						}
						else //inny blad usuwania katalogu
							syslog(LOG_INFO, "[%s] Blad przy usuwaniu katalogu %s", appName, pathBuffer);
					}
						
					
				}
				errno = 0;
				free(pathBuffer); //zwolnienie pamieci na sciezke

			}
		}
		rewinddir(sourceDir); //przywroc wskaznik katalogu na poczatek katalogu zrodlowego
		
	} while (destDirEntry != NULL);
	//przywroc wskaznik katalogu na poczatek katalogu zrodlowego i docelowego
	rewinddir(destinationDir);
	rewinddir(sourceDir);

	//zamykanie glownych katalogow
	closedir(sourceDir);
	closedir(destinationDir);

	return 0;
}


int main(int argc, char* argv[])
{
	/**
	 * Program przyjmuje następujące parametry:
	 * 
	 * argv[1] - katalog źródłowy
	 * argv[2] - katalog docelowy
	 * -t <czas> - czas aktywacji demona (w s)
	 * -s <bajty> - rozmiar "duzego" pliku przy ktorym zmieniany jest styl kopiowania
	 * -R - demon analizuje rowniez podkatalogi
	 * 
	 * argv[0] jest sciezka/nazwa programu
	*/

	//sprawdzenie liczby argumentow
	if (argc <= 2) //brak wskazania katalogu zrodlowego i docelowego - blad
	{
		fprintf(stderr, "Uzycie: %s <sciezka katalogu zrodlowego> <sciezka katalogu docelowego> [-R] [-t <czas>] [-s <bajty>]\n", argv[0]);
        exit(EXIT_FAILURE);
    }
	//inicjalizacja zmiennych
	int running = 0; //demon dziala
	
	char* appName = argv[0]; //nazwa aplikacji

	//FIX konwersja sciezek na realpaths
	char srcDirPathBuf[PATH_MAX];
	char destDirPathBuf[PATH_MAX];
	const char* srcDirPath = realpath(argv[1], srcDirPathBuf);
	const char* destDirPath = realpath(argv[2], destDirPathBuf);
	//==============================
	bool folderInclude = false; //rekurencyjne przeszukiwanie katalogow -R
	int customTime = 300; //domyslnie czas to 5 minut (300 s) -t
	int bigFileSize = 104857600; // 100 MB, granica pomiedzy obsluga pliku jako maly/duzy -s

	// === koniec inicjalizacji ===

	//sprawdzenie czy sciezki prowadza do katalogow
	if (!isDir(srcDirPath))
	{
		fprintf(stderr, "[%s] Blad: Sciezka zrodlowa nie wskazuje na katalog\n", appName);
        exit(EXIT_FAILURE);
	}
	if (!isDir(destDirPath))
	{
		fprintf(stderr, "[%s] Blad: Sciezka docelowa nie wskazuje na katalog\n", appName);
        exit(EXIT_FAILURE);
	}
	//detekcja argumentow i wpisywanie ich do zmiennych
	for (int i = 3; i < argc; ++i) {
		if (strcmp(argv[i], "-R") == 0) {
			folderInclude = true;
		}
		else if (strcmp(argv[i], "-s") == 0) {
			bigFileSize = atoi(argv[i + 1]);
		}
		else if (strcmp(argv[i], "-t") == 0) {
			customTime = atoi(argv[i + 1]);
			if (customTime < 1) //nieprawidłowy czas
			{
				fprintf(stderr, "[%s] Blad: Podano nieprawidlowy czas spania demona.\n", appName);
    		    exit(EXIT_FAILURE);
			}
		}
	}

	errno = 0; //kod bledu
	if( daemon(0, 0) ) //czy udalo sie uruchomic demona
	{
		fprintf(stderr, "[%s] Nie udalo sie przeksztalcic procesu w demona. Program konczy dzialanie.", appName);
        exit(EXIT_FAILURE);		
	}
	/*
	DAEMONIZE MUSI BYC OBOWIAZKOWO PRZED OPENDIRAMI!!!!!!!
	BARDZO WAZNE!!!!!!!!
	*/

	// uruchomienie logu
	openlog(argv[0], LOG_PID|LOG_CONS, LOG_DAEMON);
	syslog(LOG_INFO, "[%s] Uruchomiono demona synchronizujacego katalogi", appName);

	/**
	 * obsluga sygnalow przez demona
	 * (registering signal handler)
	*/
	signal(SIGUSR1, sigHandler);

	//informacja ze demon ma dzialac
	running = 1;

	while(running) //funkcje wykownywane przez demona w nieskoczonej petli
	{
		// otrzymanie przez demona sygnalu przerwie spanie
		for(int i=0; i<customTime; ++i)
		{
			sleep(1); //spij	
			if(wake_up)
			{
				wake_up = 0;
				break;
			}		
		}
		// odpalenie funkcji do przeszukiwania katalogow
		catalogSearch(srcDirPath, destDirPath, appName, bigFileSize, customTime, folderInclude);
	}

	// informacja o zatrzymaniu aplikacji
	syslog(LOG_INFO, "[%s] Zatrzymano demona.", appName);
	closelog();

	return EXIT_SUCCESS;
}
