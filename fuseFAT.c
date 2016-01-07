//for compil and run:
//dd if=/dev/zero of=fuseFAT.file bs=1G count=1 && gcc -Wall fuseFAT.c `pkg-config fuse --cflags --libs` -o fuseFAT && ./fuseFAT ./mount fuseFAT.file

//dd if=/dev/zero of=fuseFAT.file bs=1G count=1 && gcc -Wall fuseFAT.c `pkg-config fuse --cflags --libs` -o fuseFAT && ./fuseFAT ./mount -d -f -s

#define FUSE_USE_VERSION  26

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#define NAME_LENGTH  10

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

int CLUSTER_SIZE = 4096; //размер кластера в байтах
int MAX_POINTER = 1048576; //кол-во кластеров
int POINTER_SIZE = sizeof(int); //размер указателя

int FREE_CLUSTER = 0; //значение свободного кластера
int END_CLUSTER = 1048577;

FILE* fp;
FILE* logFile;

typedef struct fat_header //структура метаданных
{
	char name[NAME_LENGTH];
	int firstClusterFi; // первые 2 байта индекса первого кластера
	int firstClusterLa; // последние 2 байта индекса первого кластера
	int size;
	char attr;
} fat_header;

static int _getattr(const char *path, struct stat *stbuf); //получение атрибутов файла
static int _readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi);//чтение директории
static int _open(const char *path, struct fuse_file_info *fi); //открыть файл
static int _read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info * fi); //чтение файла
static int _truncate(const char *path, off_t size); //удаление файла
static int _create(const char *path, mode_t mode, struct fuse_file_info *fi); //создание пустого файла
static int _write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi); //запись в файл
static int _mkdir(const char *path, mode_t mode, struct fuse_file_info *fi); //создание папки

static struct fuse_operations oper = {
    .readdir = _readdir,
    .create = _create,
    .open = _open,
    .read = _read,
    .write = _write,
    .getattr = _getattr,
    .mkdir 	= _mkdir,
    .truncate = _truncate
};

fat_header getClusterHeader(int cluster);//возвращает метаданные файла из кластера с индексом cluster
int getClusterPointer(int index);//возвращает данные из указателя с индексом index
void setClusterPointer(int index, int data);//установка нового значения указателя с индексом index
int getFreeCluster();//возвращает индекс первого пустого указателя или -1
int getNeededFolderPointer(const char* foldername);//возвращает адрес нужной папки
int getFirstCluster(fat_header header);
void setFirstCluster(fat_header* header, int index);

int fileExists(char* filename, int folderPointer); //проверка существования файла
int folderExists(char* filename, int folderPointer); //проверка сущ-ия папки

fat_header* getFileHeader(char* filename, int folderPointer);//возвращает метаданные файла
void setFileHeader(char* filename, int folderPointer, fat_header* header);//записывает метаданные файла
void _init(); //создание корневой папки

int main(int argc, char *argv[])
{
	if (argc < 2)
	{
		printf("Usage: %s [mount-dir]", argv[0]);
		return 1;
	}

	fp = fopen("fuseFAT.file", "rb+");

	// если нет корневой папки, то создаем ее
	if (getClusterPointer(0) == FREE_CLUSTER)
		_init();

	return fuse_main(argc, argv, &oper, NULL);
}

void _init()
{
	setClusterPointer(0, END_CLUSTER);
	fseek(fp, MAX_POINTER * POINTER_SIZE, SEEK_SET);

	fat_header header;
	setFirstCluster(&header, 0);
	header.attr = 0x10;
	printf("init, index %d, size %d\n", MAX_POINTER * POINTER_SIZE, (int)sizeof(fat_header));
	fwrite(&header, sizeof(fat_header), 1, fp);
}

static int _getattr(const char *path, struct stat *stbuf)
{
	printf("getattr %s\n", path);

	memset(stbuf, 0, sizeof(struct stat));

	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
		return 0;
	}

	int neededFolder = getNeededFolderPointer(path);
	if (neededFolder == -1) // если какой-то папки нет
		return -ENOENT;

	char* startIndex = NULL;
	char* endIndex;

	do
	{
		if (startIndex == NULL) 
			startIndex = strchr(path, '/') + 1;
		else
			startIndex = endIndex + 1;

		endIndex = strchr(startIndex, '/');

	} while (endIndex != NULL); // получаем имя файла

	if (folderExists(startIndex, neededFolder)) // папка существует
	{
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
		stbuf->st_size = 0;
		return 0;
	} 
	else if (fileExists(startIndex, neededFolder)) // файл сущ-ет
	{
		stbuf->st_mode = S_IFREG | 0444;
		stbuf->st_nlink = 1;
		stbuf->st_size = 0;
		return 0;
	}

	return -ENOENT;
}

static int _readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info * fi) {

    (void) offset;
	(void) fi;

	printf("readdir %s\n", path);

	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);
	int neededFolder = getNeededFolderPointer(path);
	if (neededFolder == -1) // какой-то папки нет
		return -1; 

	int record;
	for(record = sizeof(fat_header); record < CLUSTER_SIZE; record += sizeof(fat_header))
	{ // пробегаем по всему кластеру папки, ищем пустую ячейку
		int dataIndex = MAX_POINTER * POINTER_SIZE + neededFolder * CLUSTER_SIZE + record;
		fseek(fp, dataIndex, SEEK_SET);
		fat_header header = getClusterHeader(dataIndex);

		if (header.name[0] != '\0')
		{
			filler(buf, header.name, NULL, 0);
			printf("index2 %d: record %d filename %s size %d\n",dataIndex, record, header.name, header.size);
		}
	}

	return 0;
}

static int _open(const char *path, struct fuse_file_info * fi) {
    printf("open: %s\n", path);

    int neededFolder = getNeededFolderPointer(path);
	if (neededFolder == -1) // какой-то папки нет
		return -1;

	char* startIndex = NULL;
	char* endIndex;

	do
	{
		if (startIndex == NULL) 
			startIndex = strchr(path, '/') + 1;
		else
			startIndex = endIndex + 1;
		endIndex = strchr(startIndex, '/');

	} while (endIndex != NULL); // получаем имя файла

	if (!fileExists(startIndex, neededFolder)) // нет такого файла в нужной папке
		return -ENOENT;

	return 0;
}

static int _read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info * fi) {
    printf("read: %s\n", path);

    int neededFolder = getNeededFolderPointer(path);
	if (neededFolder == -1) // какой-то папки нет
		return -1;

	char* startIndex = NULL;
	char* endIndex;

	do
	{
		if (startIndex == NULL) 
			startIndex = strchr(path, '/') + 1;
		else
			startIndex = endIndex + 1;
		endIndex = strchr(startIndex, '/');

	} while (endIndex != NULL); // получаем имя файла

	if (!fileExists(startIndex, neededFolder)) // нет такого файла в нужной папке
		return -ENOENT;

	fat_header* header = getFileHeader(startIndex, neededFolder);

	if (offset > header->size)
		return 0;

	int startCluster = getClusterPointer(getFirstCluster(header));

	int skipClusters = offset / (CLUSTER_SIZE);
	int i;

	for (i = 0; i < skipClusters; i++)
		startCluster = getClusterPointer(startCluster);

	int clusterOffset = offset % (CLUSTER_SIZE);

	int readData = 0;
	while(readData < size)
	{		
		int readBytes = MIN(size - readData, CLUSTER_SIZE - clusterOffset);
		//printf("readData: %d, readBytes: %d, header.size: %d\n", readData, readBytes, header.size);
		if (readData + readBytes > header->size)
			readBytes = header->size - readData;

	  	int indexOfData = MAX_POINTER * POINTER_SIZE + startCluster * CLUSTER_SIZE + clusterOffset;

		fseek(fp, indexOfData, SEEK_SET);
		fread(buf + readData, readBytes, 1, fp);

		readData += readBytes;
		if (readData < size)
		{
			startCluster = getClusterPointer(startCluster);
			clusterOffset = 0;

			if (startCluster == END_CLUSTER)
				break;
		}
	}

	return readData;
}

static int _truncate(const char *path, off_t size) {
	
    printf("truncate %s to %d\n", path, (int)size);

	int neededFolder = getNeededFolderPointer(path);
	if (neededFolder == -1) // какой-то папки нет
		return -1;

	char* startIndex = NULL;
	char* endIndex;

	do
	{
		if (startIndex == NULL) 
			startIndex = strchr(path, '/') + 1;
		else
			startIndex = endIndex + 1;
		endIndex = strchr(startIndex, '/');

	} while (endIndex != NULL); // получаем имя файла

	if (!fileExists(startIndex, neededFolder)) // нет такого файла в нужной папке
		return -ENOENT;

	fat_header* header = getFileHeader(startIndex, neededFolder); 	
	header->size = size;
	setFileHeader(startIndex, neededFolder, header);

 	printf("truncate: Truncated successfully\n");
	return 0;
}

static int _create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    
    printf("create: %s\n", path);

	int neededFolder = getNeededFolderPointer(path);
	if (neededFolder == -1) // значит какой-то папки нет или наоборот есть такая же
		return -1; // не создаем ничего

	char* startIndex = NULL;
	char* endIndex;

	do
	{
		if (startIndex == NULL) 
			startIndex = strchr(path, '/') + 1;
		else
			startIndex = endIndex + 1;
		endIndex = strchr(startIndex, '/');

	} while (endIndex != NULL); // получаем имя файла

	if (fileExists(startIndex, neededFolder)) // есть уже такой файл или папка в нужной папке
		return -ENOENT;

	int freeCluster = getFreeCluster();
	if (freeCluster == -1) //нет места
		return -1;

	fat_header header;

	strcpy(header.name, startIndex);
	header.size = 0;
	header.attr = 0;
	setFirstCluster(&header, freeCluster);

	fseek(fp, freeCluster, SEEK_SET);
	printf("writing end cluster in creating, index %d, size %d\n", freeCluster, (int)sizeof(int));
	fwrite(&END_CLUSTER, sizeof(int), 1, fp);

	int record;
	for(record = sizeof(fat_header); record <= CLUSTER_SIZE; record += sizeof(fat_header))
	{ // пробегаем по всему кластеру папки, ищем пустую ячейку
		int dataIndex = MAX_POINTER * POINTER_SIZE + neededFolder * CLUSTER_SIZE + record;

		fat_header secondHeader = getClusterHeader(dataIndex);
		if (secondHeader.name[0] == '\0') // не заполнена, значит, перезаписываем
		{
			fseek(fp, dataIndex, SEEK_SET);
			printf("writing header in creating, index %d, size %d\n", dataIndex, (int)sizeof(fat_header));
			fwrite(&header, sizeof(fat_header), 1, fp);
			break;
		}
	}

	return 0;
}

static int _write(const char *path, const char *content, size_t size, off_t offset, struct fuse_file_info *fi) {
    printf("write: %s\n", path);

    int neededFolder = getNeededFolderPointer(path);
	if (neededFolder == -1) // какой-то папки нет
		return -1;

	char* startIndex = NULL;
	char* endIndex;

	do
	{
		if (startIndex == NULL) 
			startIndex = strchr(path, '/') + 1;
		else
			startIndex = endIndex + 1;
		endIndex = strchr(startIndex, '/');

	} while (endIndex != NULL); // получаем имя файла

	if (!fileExists(startIndex, neededFolder)) // нет такого файла в нужной папке
		return -ENOENT;

	fat_header* header = getFileHeader(startIndex, neededFolder);


	if (offset > header->size)
		return 0;

	int startCluster = getClusterPointer(getFirstCluster(header));

	int skipClusters = offset / (CLUSTER_SIZE);
	int i;

	for (i = 0; i < skipClusters; i++)
		startCluster = getClusterPointer(startCluster);

	int clusterOffset = offset % (CLUSTER_SIZE);

  	int writtenBytes = 0;
  	while (writtenBytes < size)
  	{
  		int readBytes = MIN(size, CLUSTER_SIZE - clusterOffset);


  		int indexOfData = MAX_POINTER * POINTER_SIZE + startCluster * CLUSTER_SIZE + clusterOffset;
		fseek(fp, indexOfData, SEEK_SET);
		printf("writing data on fs_write to index %d, size %d\n", indexOfData, readBytes);
		fwrite((char*)(content + writtenBytes), readBytes, 1, fp);


		writtenBytes += readBytes;
		if (writtenBytes < size)
		{
			//нужен новый клстер
			if (startCluster == END_CLUSTER)
			{
				int freeCluster2 = getFreeCluster();
				setClusterPointer(startCluster, freeCluster2);

				startCluster = freeCluster2;
			}
			else
				startCluster = getClusterPointer(startCluster);		

			clusterOffset = 0;

		}
  	}  	

	//изменяем размер файла
  	header->size += size;
	startCluster = getFirstCluster(header);

	setFileHeader(startIndex, neededFolder, header);

	return size;
}

static int _mkdir(const char *path, mode_t mode, struct fuse_file_info *fi)
{
	printf("mkdir: %s\n", path);

	int neededFolder = getNeededFolderPointer(path);
	if (neededFolder == -1) // значит какой-то папки нет или наоборот есть такая же
		return -1; // не создаем ничего

	char* startIndex = NULL;
	char* endIndex;

	do
	{
		if (startIndex == NULL) 
			startIndex = strchr(path, '/') + 1;
		else
			startIndex = endIndex + 1;
		endIndex = strchr(startIndex, '/');

	} while (endIndex != NULL); // получаем имя

	if (fileExists(startIndex, neededFolder)) // есть уже такое имя
		return -ENOENT;

	int freeCluster = getFreeCluster();
	if (freeCluster == -1)
		return -1;

	fat_header header;

	strcpy(header.name, startIndex);
	header.size = 0;
	header.attr = 0x10;
	setFirstCluster(&header, freeCluster);

	fseek(fp, freeCluster, SEEK_SET);
	printf("writing end cluster in mkdir, index %d, size %d\n", freeCluster, (int)sizeof(int));
	fwrite(&END_CLUSTER, sizeof(int), 1, fp);

	int record;
	for(record = sizeof(fat_header); record <= CLUSTER_SIZE; record += sizeof(fat_header))
	{ // пробегаем по всему кластеру папки, ищем пустую ячейку
		int dataIndex = MAX_POINTER * POINTER_SIZE + neededFolder * CLUSTER_SIZE + record;

		fat_header secondHeader = getClusterHeader(dataIndex);
		if (secondHeader.name[0] == '\0') // не заполнена, значит, перезаписываем
		{
			fseek(fp, dataIndex, SEEK_SET);
			printf("writing header in mkdir, index %d, size %d\n", dataIndex, (int)sizeof(fat_header));
			fwrite(&header, sizeof(fat_header), 1, fp);
			break;
		}
	}

	return 0;
}

int getClusterPointer(int index)
{
	fseek(fp, index, SEEK_SET);

	int readInfo;
	fread(&readInfo, sizeof(int), 1, fp);
	return readInfo;
}

void setClusterPointer(int index, int data)
{
	fseek(fp, index, SEEK_SET);
	fwrite(&data, sizeof(int), 1, fp);
}

fat_header getClusterHeader(int cluster)
{
	fat_header header;

	fseek(fp, MAX_POINTER * POINTER_SIZE + cluster * CLUSTER_SIZE, SEEK_SET);
	fread(&header, sizeof(fat_header), 1, fp);
	
	return header;
}

int getFreeCluster()
{
	int cluster;
	for (cluster = 0; cluster < MAX_POINTER * POINTER_SIZE; cluster+= POINTER_SIZE)
	{
		fseek(fp, cluster, SEEK_SET);

		int readInfo;
		fread(&readInfo, sizeof(int), 1, fp);

		if (readInfo == FREE_CLUSTER)
			return cluster;
	}
	return -1;
}

int getNeededFolderPointer(const char* foldername)
{
	int folderPointer = 0; // начинам с корневой папки

	char* endIndex;
	char* startIndex = NULL;
	do // если есть еще разделители, проверяется хотя бы один раз (для корневой папки)
	{
		if (startIndex == NULL) 
			startIndex = strchr(foldername, '/') + 1;
		else
			startIndex = endIndex + 1;
		endIndex = strchr(startIndex, '/');

		if (endIndex == NULL) // все ок, создаем файл или папку в корне
			return 0; // 0 - первый кластер корневой папки		

		int found = 0; // нашли ли мы папку

		int record;
		for(record = sizeof(fat_header); record <= CLUSTER_SIZE; record += sizeof(fat_header))
		{ // пробегаем по всему кластеру, ищем нужную папку
			fat_header header = getClusterHeader(MAX_POINTER * POINTER_SIZE + folderPointer * CLUSTER_SIZE + record);
			
			// получаем имя папки
			if (strncmp(startIndex, header.name, endIndex - startIndex - 1) == 0)
			{
				// если это папка, то переходим в нее, если нет, то возвращаем -1
				if (header.attr == 0x10)
				{
					printf("folder %s founded \n", header.name);
					startIndex = getFirstCluster(header);
					found = 1;
					break;
				}
				else
					return -1;
			}	
		}		

		if (found == 0) // если не нашли папку
			return -1;

	} while(endIndex != NULL);

	return folderPointer;
}

int fileExists(char* filename, int folderPointer)
{
	int record;
	for(record = sizeof(fat_header); record <= CLUSTER_SIZE; record += sizeof(fat_header))
	{ // пробегаем по всему кластеру
		int dataIndex = MAX_POINTER * POINTER_SIZE + folderPointer * CLUSTER_SIZE + record;
		fseek(fp, dataIndex, SEEK_SET);
		fat_header header = getClusterHeader(dataIndex);

		if (strcmp(filename, header.name) == 0) //если сущ-ет
		{
			return 1;
		}	
	}	
	return 0;
}

int folderExists(char* filename, int folderPointer)
{
	int record;
	for(record = sizeof(fat_header); record <= CLUSTER_SIZE; record += sizeof(fat_header))
	{ // пробегаем по всему кластеру
		int dataIndex = MAX_POINTER * POINTER_SIZE + folderPointer * CLUSTER_SIZE + record;
		fseek(fp, dataIndex, SEEK_SET);
		fat_header header = getClusterHeader(dataIndex);

		if (strcmp(filename, header.name) == 0 && header.attr == 0x10) // если сущ-ет и является папкой
		{
			return 1;
		}	
	}	
	return 0;
}

int getFirstCluster(fat_header header)
{
	int index = header.firstClusterFi << 16;
	index += header.firstClusterLa;
	return index;
}

void setFirstCluster(fat_header* header, int index)
{	
	header->firstClusterFi = index >> 16;
	header->firstClusterLa = index & 0x0000FFFF;
}

fat_header* getFileHeader(char* filename, int folderPointer)
{
	int record;
	for(record = sizeof(fat_header); record <= CLUSTER_SIZE; record += sizeof(fat_header))
	{ // пробегаем по всему кластеру
		int dataIndex = MAX_POINTER * POINTER_SIZE + folderPointer * CLUSTER_SIZE+ record;
		fseek(fp, dataIndex, SEEK_SET);
		fat_header header = getClusterHeader(dataIndex);

		if (strcmp(filename, header.name) == 0)
		{
			return &header;
		}	
	}	
	return NULL;
}

void setFileHeader(char* filename, int folderPointer, fat_header* header)
{
	int record;
	for(record = sizeof(fat_header); record <= CLUSTER_SIZE; record += sizeof(fat_header))
	{ // пробегаем по всему кластеру
		int dataIndex = MAX_POINTER * POINTER_SIZE + folderPointer * CLUSTER_SIZE + record;
		fseek(fp, dataIndex, SEEK_SET);
		fat_header secondHeader = getClusterHeader(dataIndex);

		if (strcmp(filename, secondHeader.name) == 0)
		{
			printf("writing header on setFileHeader, index %d, size %d\n", dataIndex, (int)sizeof(fat_header));
			fwrite(&header, sizeof(fat_header), 1, fp);
			return;
		}	
	}	
}