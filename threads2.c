#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>

void* reader(void *p);
void* worker(void *p);
void* writer(void *p);

int k = 1;

struct Params{
   pthread_cond_t condvar;
   pthread_mutex_t mutex;
   int numbers[10];
};

typedef struct node_t
{
   int value;
   struct node_t* next;
}  TNode;
 
typedef struct list_t
{
   TNode* head;
   TNode* tail;
}  TList;

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
		params->numbers[i] = (int)c;
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
    TList list;
    int sum = 0;
    for (int i = 0; i < (sizeof(params->numbers)/sizeof(int)); i++)
    {
	sum = sum + params->numbers[i];
    }
    push(&list, sum);
    k++;
    pthread_cond_signal(&params->condvar);
}

void* writer(void *p){
    struct Params* params = (struct Params*) p;
    TList list = {NULL, NULL};
    FILE *out = fopen("newFile2.txt", "w");
    int sumW = 0;
    for (int i = 0; i < k; i++)
    {
//	pthread_mutex_lock(&params->mutex);
        pthread_cond_wait(&params->condvar, &params->mutex);
        sumW = sumW + pop(&list);
//	pthread_mutex_unlock(&params->mutex);
    }
    printf("%d \n", sumW);
    fprintf(out, "%d", sumW);
}

TList* push(TList* list, int value)
{
   TNode* node = (TNode*) malloc(sizeof(TNode));
   node->value = value;
   node->next = NULL;
 
   if (list->head && list->tail)
   {
      list->tail->next = node;
      list->tail = node;
   }
   else
   {
      list->head = list->tail = node;
   }
 
   return list;
}

int pop(TList* list)
{
   TNode* node = list->head;
   list->head = node->next;
   if (list->head == NULL)
   {
      list->tail = NULL;
   }
   int value = node->value;
   free(node);
 
   return value;
}
