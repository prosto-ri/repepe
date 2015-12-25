#define FUSE_USE_VERSION  26

#include <fuse.h>
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
	int firstCluster;
	int size;

} fat_header;

static int _getattr(const char *path, struct stat *stbuf); //получение атрибутов файла
static int _readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi);//чтение директории
static int _open(const char *path, struct fuse_file_info *fi); //открыть файл
static int _read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info * fi); //чтение файла
static int _truncate(const char *path, off_t size); //удаление файла
static int _create(const char *path, mode_t mode, struct fuse_file_info *fi); //создание пустого файла
static int _write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi); //запись в файл

static struct fuse_operations oper = {
    .readdir = _readdir,
    .create = _create,
    .open = _open,
    .read = _read,
    .write = _write,
    .getattr = _getattr,
    .truncate = _truncate
};

fat_header getClusterHeader(int cluster);//возвращает метаданные файла из кластера с индексом cluster
int getClusterPointer(int index);//возвращает данные из указателя с индексом index
void setClusterPointer(int index, int data);//установка нового значения указателя с индексом index
int getFreeCluster();//возвращает индекс первого пустого указателя или -1

int main(int argc, char *argv[])
{
	if (argc != 3)
	{
		printf("Usage: %s [mount-point] [filesystem-file]", argv[0]);
		return 1;
	}

	return fuse_main(argc, argv, &oper, NULL);//запуск фс
}

static int fs_getattr(const char *path, struct stat *stbuf)
{
	int res = 0;

	memset(stbuf, 0, sizeof(struct stat));
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
		return res;
	}

	int cluster;
	for (cluster = 0; cluster < MAX_POINTER * POINTER_SIZE; cluster+= POINTER_SIZE)
	{
		int index = getClusterPointer(cluster);
		if (index == END_CLUSTER)
		{
			fat_header header = getClusterHeader(cluster);

			stbuf->st_mode = S_IFREG | 0444;
			stbuf->st_nlink = 1;
			stbuf->st_size = header.size;

			return 0;
		}
	}

	return -ENOENT;
}

static int _readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info * fi) {

    (void) offset;
	(void) fi;

	int cluster;
	
	for (cluster = 0; cluster < MAX_POINTER * POINTER_SIZE; cluster+= POINTER_SIZE)
	{
		int index = getClusterPointer(cluster);
		if (index == END_CLUSTER)
		{
			fat_header header = getClusterHeader(cluster);
			filler(buf, header.filename, NULL, 0);
		}
	}

	return 0;
}

static int _open(const char *path, struct fuse_file_info * fi) {
    printf("open: %s\n", path);

    int cluster;
    for (cluster = 0; cluster < MAX_POINTER * POINTER_SIZE; cluster+= POINTER_SIZE)
	{
		int clusterPointer = getClusterPointer(cluster);

		if (clusterPointer == FREE_CLUSTER)
			continue;

		fat_header header = getClusterHeader(cluster);
      		if (strcmp(path, header.filename) == 0)
      		{ 
        		fileInfo->fileInode = header.firstCluster;
          		fileInfo->fullfileName = header.filename;
          		fileInfo->isLocked = false; // set is file locked

                printf("open: Opened successfully\n");
        		return 0;
        	}
        } 
    }
  
  return -ENOENT; //если файла с данным именем не сущ-ет
}

static int _read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info * fi) {
    printf("read: %s\n", path);

    int cluster;
	int clusterPointer;
	int startCluster = -1;

	fat_header header;
	for (cluster = 0; cluster < MAX_POINTER * POINTER_SIZE; cluster+= POINTER_SIZE)
	{
		clusterPointer = getClusterPointer(cluster);
		if (clusterPointer == FREE_CLUSTER)
			continue;
	
		header = getClusterHeader(cluster);
		if (strcmp(path, header.filename) == 0 && startCluster == -1)
		{
			startCluster = cluster;
			break;
		}
	}

	if (startCluster == -1)
		return 0;

	if (offset > header.size)
		return 0;

	startCluster = getClusterPointer(header.firstCluster);

	int skipClusters = offset / (CLUSTER_SIZE - sizeof(fat_header));//кол-во кластеров, которые нужно пропустить, чтобы найти нужную инфу
	int i;

	for (i = 0; i < skipClusters; i++)
		startCluster = getClusterPointer(startCluster);

	int clusterOffset = offset % (CLUSTER_SIZE - sizeof(fat_header));//смещение внутри данного кластера

	int readData = 0;
	while(readData < size)
	{		
		int readBytes = MIN(size - readData, CLUSTER_SIZE - sizeof(fat_header) - clusterOffset);
		/
		if (readData + readBytes > header.size)
			readBytes = header.size - readData;

		//индекс нужной инфы в файле
	  	int indexOfData = MAX_POINTER * POINTER_SIZE + startCluster * CLUSTER_SIZE + sizeof(fat_header) + clusterOffset;

		fseek(fp, indexOfData, SEEK_SET);
		fread(buf + readData, readBytes, 1, fp);//считывание с определенного индекса

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
	
    int cluster;
	for (cluster = 0; cluster < MAX_POINTER * POINTER_SIZE; cluster+= POINTER_SIZE)
	{
		int clusterPointer = getClusterPointer(cluster);

		if (clusterPointer == FREE_CLUSTER)
			continue;

		fat_header header = getClusterHeader(cluster);
		if (strcmp(path, header.filename) == 0)
		{
			setClusterPointer(cluster, FREE_CLUSTER);
			if (startCluster == END_CLUSTER)
			{
				printf("truncate: Truncated successfully\n");
   				return 0;
			}
		}
	}
	return -ENOENT;
}

static int _create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    printf("create: %s\n", path);

    int cluster;
	for (cluster = 0; cluster < MAX_POINTER * POINTER_SIZE; cluster+= POINTER_SIZE)
	{
		int clusterPointer = getClusterPointer(cluster);
		if (clusterPointer == END_CLUSTER)
		{			
			fat_header header = getClusterHeader(cluster);
			if (strcmp(path, header.filename) == 0)
				return -1;
		}
	}

	int freeCluster = getFreeCluster();
	if (freeCluster == -1)
		return -1;

	fat_header header;

	strcpy(header.filename, path);
	header.size = 0;
	header.firstCluster = freeCluster;

	fseek(fp, freeCluster, SEEK_SET);
	fwrite(&END_CLUSTER, sizeof(int), 1, fp);

	fseek(fp, MAX_POINTER * POINTER_SIZE + freeCluster * CLUSTER_SIZE, SEEK_SET);
	fwrite(&header, sizeof(fat_header), 1, fp);

	return 0;
}

static int _write(const char *path, const char *content, size_t size, off_t offset, struct fuse_file_info *fi) {
    printf("write: %s\n", path);
    int cluster;
	int clusterPointer;
	int startCluster = -1;

	fat_header header;
	for (cluster = 0; cluster < MAX_POINTER * POINTER_SIZE; cluster+= POINTER_SIZE)
	{
		clusterPointer = getClusterPointer(cluster);
		if (clusterPointer == FREE_CLUSTER)
			continue;
	
		header = getClusterHeader(cluster);
		if (strcmp(name, header.filename) == 0)
		{
			startCluster = cluster;
			break;
		}
	}

	if (startCluster == -1)
		return 0;

	if (offset > header.size)
		return 0;

	startCluster = getClusterPointer(header.firstCluster);

	int skipClusters = offset / (CLUSTER_SIZE - sizeof(fat_header));
	int i;

	for (i = 0; i < skipClusters; i++)
		startCluster = getClusterPointer(startCluster);

	int clusterOffset = offset % (CLUSTER_SIZE - sizeof(fat_header));

  	int writtenBytes = 0;
  	while (writtenBytes < size)
  	{
  		int readBytes = MIN(size, CLUSTER_SIZE - sizeof(fat_header) - clusterOffset);

  		int indexOfData = MAX_POINTER * POINTER_SIZE + startCluster * CLUSTER_SIZE + sizeof(fat_header) + clusterOffset;
		fseek(fp, indexOfData, SEEK_SET);
		fwrite((char*)(buf + writtenBytes), readBytes, 1, fp);

		writtenBytes += readBytes;
		if (writtenBytes < size)
		{
			//нужен доп.кластер
			if (startCluster == END_CLUSTER)
			{
				int freeCluster2 = getFreeCluster();
				setClusterPointer(startCluster, freeCluster2);

				int indexOfData = MAX_POINTER * POINTER_SIZE + freeCluster2 * CLUSTER_SIZE;

				fseek(fp, indexOfData, SEEK_SET);
				fwrite(&header, sizeof(fat_header), 1, fp);

				startCluster = freeCluster2;
			}
			else
				startCluster = getClusterPointer(startCluster);		

			clusterOffset = 0;

		}
  	}  	

  	//изменяем размер файла
  	header.size += size;
	startCluster = header.firstCluster;

	while(1)
	{
		fseek(fp, MAX_POINTER * POINTER_SIZE + startCluster * CLUSTER_SIZE, SEEK_SET);
		fwrite(&header, sizeof(fat_header), 1, fp);
		if (getClusterPointer(startCluster) == END_CLUSTER)
			break;

		startCluster = getClusterPointer(startCluster);
	}

	return size;
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
	fwrite(&data, sizeof(int32_t), 1, fp);
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