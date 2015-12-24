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

static int _getattr(const char *path, struct stat * stbuf) {
    printf("getattr: %s\n", path);
    int res;

	res = lstat(path, stbuf);
	if (res == -1)
		return -errno;

	printf("getattr: %s\n", path);

	memset(stbuf, 0, sizeof(struct stat));
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} else {
		stbuf->st_mode = S_IFREG | 0444;
		stbuf->st_nlink = 1;
		stbuf->st_size = 0;
	}

	return 0;
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

    /*
      Get list of files in directory
      ...
    */
    
    fileInfo->fileInode = // set inode;
    fileInfo->fullfileName = // specify file name;
    fileInfo->isLocked = false; // set is file locked

    

    printf("open: Opened successfully\n");

    return 0;
}

static int _read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info * fi) {
    printf("read: %s\n", path);
    /*
      Read file
      ...
      byte *fileContent = readFile(fileInode);
      memcpy(buf, fileContent, size);
    */  
    return size;
}

static int _truncate(const char *path, off_t size) {
    /*
      truncate file
      ...
    */

    printf("truncate: Truncated successfully\n");
    return 0;
}

static int _create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    printf("create: %s\n", path);
    /*
      Create inode
      ...
    */

    return 0;
}

static int _write(const char *path, const char *content, size_t size, off_t offset, struct fuse_file_info *fi) {
    printf("write: %s\n", path);
    /*
      Write bytes to fs
      ...
    */
    return 0; // Num of bytes written
}