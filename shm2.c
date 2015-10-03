#include <stdlib.h>
#include <stdio.h>
#include <sys/shm.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/wait.h>

int strl;

int main(int args, char** argv){
   int memsize = 20;
   int sizeOfShm = sizeof(char) * memsize;
   int shmid = shmget(IPC_PRIVATE, sizeOfShm, SHM_W | SHM_R | IPC_CREAT);
   if(shmid < 0){
      perror("Impossible...");
      exit(EXIT_FAILURE);
   }
   int* sharedMem = shmat(shmid, NULL, 0);
   if(!fork()){
      printf("Enter your Name: ");
      char str1[10]; 
      scanf("%s", str1);
      for(int i = 0; str1[i] != 0; ++i){
         sharedMem[i] = str1[i];
         strl = i;
      }
      sharedMem[strl+1] = ' ';
      shmdt(&shmid);
   }
   else
   {
      wait(NULL);
      if(!fork())
      {
         printf("Enter your Surname: ");
         char str2[10]; 
         scanf("%s", str2);
         int j = 0;
      int i = 0;
      while(sharedMem[i] != ' ') i++;
      for(++i; str2[j] != 0; ++i){
         sharedMem[i] = str2[j];
         j++;}
         shmdt(&shmid);
      }
      else
      {
         wait(NULL);
         for(int i = 0; i < sharedMem[i] != 0; ++i)
            putchar(sharedMem[i]);
         printf("\n");
         shmdt(&shmid);
         shmctl(shmid, IPC_RMID, NULL);
      }
   }
}
