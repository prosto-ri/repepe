#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>

void* reader(void *p);
void* worker(void *p);
void* writer(void *p);

int val[10];
int summ[10];
int k = 2;
int j = 0;

struct Params{
   pthread_cond_t condvar;
   pthread_mutex_t mutex;
};

int main(){
    struct Params params;  
    pthread_cond_init(&params.condvar, NULL); 
    pthread_mutex_init(&params.mutex, NULL);

    pthread_t read;
    pthread_t write;

    pthread_create(&read, NULL, reader, &params);
    pthread_create(&write, NULL, writer, &params);

    pthread_join(read, NULL);
    pthread_join(write, NULL);

    pthread_mutex_destroy(&params.mutex); 
    pthread_cond_destroy(&params.condvar);
}

void* reader(void *p){
    struct Params* params = (struct Params*) p;
    FILE *in = fopen("file", "r");
    char c;
    int i = 0;
    while((c = fgetc(in)) != EOF)
    {
	if(c != ' ' & c != '\n')
	{
		val[i] = (int)c;
		i++;
	}
	if(c = '\n')
	{
	    pthread_t work;
	    pthread_create(&work, NULL, worker, &params);
	    i = 0;
	}	
	}
}

void* worker(void *p){
    struct Params* params = (struct Params*) p;
    int sum = 0;
    for (int i = 0; i < (sizeof(val)/sizeof(int)); i++)
    {
	sum = sum + val[i];
    }
    summ[j] = sum;
    j++;
    if (j > 2)
    {
        k = j;
    }
    pthread_cond_signal(&params->condvar);
}

void* writer(void *p){
    struct Params* params = (struct Params*) p;
    FILE *out = fopen("newFile.txt", "w");
    int sumW = 0;
    for (int i = 0; i < k; i++)
    {
	pthread_mutex_lock(&params->mutex);
        pthread_cond_wait(&params->condvar, &params->mutex);
        sumW = sumW + summ[i];
	pthread_mutex_unlock(&params->mutex);
    }
    printf("%d \n", sumW);
    fputc((char)sumW, out);
}
