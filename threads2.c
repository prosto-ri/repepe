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
   int count;
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

TList* push(TList* list, int value);
int pop(TList* list);

int main(){
    struct Params params;  
    pthread_cond_init(&params.condvar, NULL); 
    pthread_mutex_init(&params.mutex, NULL);

    pthread_t read;
    pthread_t write;

    pthread_create(&read, NULL, reader, &params);
    pthread_create(&write, NULL, writer, &params);
printf("create \n");

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
		(params->count)++;
		i++;
	}
	if(c == '\n')
	{printf("line %d \n", k);
	    printf("count = %d \n", params->count);
	    pthread_t work;
	    pthread_create(&work, NULL, worker, &params);
	    i = 0;
	    params->count = 0;
	    k++;
	}	
	}
}

void* worker(void *p){
    struct Params* params = (struct Params*) p;
    printf("worker started, count = %d \n", params->count);
    TList list;
    int sum = 0;
    for (int i = 0; i < (params->count); i++)
    {
	printf("workerC %d \n", i+1);
	sum = sum + params->numbers[i];
    }
    push(&list, sum);
    pthread_cond_signal(&params->condvar);
    printf("worker finished \n");
}

void* writer(void *p){
    printf("writer\n");
    struct Params* params = (struct Params*) p;
    TList list = {NULL, NULL};
    FILE *out = fopen("newFile2.txt", "w");
    int sumW = 0;
    for (int i = 0; i < k; i++)
    {
printf("writerC %d \n", i+1);
//	pthread_mutex_lock(&params->mutex);
        pthread_cond_wait(&params->condvar, &params->mutex);
printf("writerCC %d \n", i+1);
        sumW = sumW + pop(&list);
//	pthread_mutex_unlock(&params->mutex);
    }
    printf("%d \n", sumW);
    fprintf(out, "%d", sumW);
}

TList* push(TList* list, int value)
{printf("push on \n");
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
 printf("push off \n");
   return list;
}

int pop(TList* list)
{printf("pop on \n");
   TNode* node = list->head;
   list->head = node->next;
   if (list->head == NULL)
   {
      list->tail = NULL;
   }
   int value = node->value;
   free(node);
 printf("pop off \n");
   return value;
}
