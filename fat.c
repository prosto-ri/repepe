#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#define FILENAME_LENGTH  10

int CLUSTER_SIZE = 4096; //размер кластера в байтах
int MAX_POINTER = 1048576; //кол-во кластеров
int POINTER_SIZE = 4; //размер указателя

int FREE_CLUSTER = 0; //значение свободного кластера
int END_CLUSTER = 1048577;

FILE* fp;

typedef struct fat_header //структура метаданных
{
	char filename[FILENAME_LENGTH];
	int size;

} fat_header;

static int _readdir(const char *path);//чтение директории
static int _open(const char *path); //открыть файл
static int _read(const char *path); //чтение файла
static void _truncate(const char *path); //удаление файла
static int _create(const char *path); //создание пустого файла
static int _write(const char *path); //запись в файл

fat_header getClusterHeader(int cluster);//возвращает метаданные файла из кластера с индексом cluster
int getClusterPointer(int index);//возвращает данные из указателя с индексом index
int getFreeCluster();//возвращает индекс первого пустого указателя или -1

int main(int argc, char *argv[])
{
	if (argc != 3)
	{
		printf("Usage: %s [mount-point] [filesystem-file]", argv[0]);
		return 1;
	}
}

static int _readdir()
{
	int cluster;
	
	for (cluster = 0; cluster < MAX_POINTER * POINTER_SIZE; cluster+= POINTER_SIZE)
	{
		int index = getClusterPointer(cluster);
		if (index == END_CLUSTER)
		{
			fat_header header = getClusterHeader(cluster);
			printf("%s %d\n", header.filename, header.size);
		}
	}

	return 0;
}