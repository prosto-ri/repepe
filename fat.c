#define FUSE_USE_VERSION  26

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#define FILENAME_LENGTH  10

typedef struct fat_header //структура метаданных
{
	char filename[FILENAME_LENGTH];
	int size;

} fat_header;

static int _getattr(const char *path, struct stat *stbuf); //получение атрибутов файла
static int _readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi);//чтение директории
static int _open(const char *path, struct fuse_file_info *fi); //открыть файл
static int _read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info * fi); //чтение файла
static void _destroy(void *a); //удаление файла
static int _create(const char *path, mode_t mode, struct fuse_file_info *fi); //создание пустого файла
static int _write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi); //запись в файл

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