#include <stdlib.h> 
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <errno.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <semaphore.h>
 
#define NUM_ELEM 10 /* Number of elements in shared memory buffer */
#define SEM_MUTEX 0
#define SEM_EMPTY 1
#define SEM_FULL 2
 
FILE* fp; //declare filer pointer


int rc, semID, shmID, status, i, x, numCons; //declare other global vars
char elem;
union semun
{
    int val;
    struct semid_ds *buf;
    short *array;
} seminfo;
//dclare sembuffs 
struct sembuf WaitMutex={SEM_MUTEX, -1, 0};
struct sembuf SignalMutex={SEM_MUTEX, 1, 0};
struct sembuf WaitEmpty={SEM_EMPTY, -1, 0};
struct sembuf SignalEmpty={SEM_EMPTY, 1, 0};
struct sembuf WaitFull={SEM_FULL, -1, 0};
struct sembuf SignalFull={SEM_FULL, 1, 0};
struct shmid_ds shminfo;
char *shmPtr;
void *shmid;
 
void initialize(); //declare initiatlize
void producer(); //declare producer
void consumer(); //declare consumer
 
int main()
{
	printf("How many files would you like to make: ");
	scanf("%d", &numCons); //ask the user how many files to make
  /* Initialize shared memory and semaphores */
  initialize();
  pid_t pid;
  /* Start a child process and proceed accordingly*/
  pid = fork();
  if (pid==0)
  {
	consumer(numCons);
  }
  else if (pid>0)
  {
    /* Parent becomes the producer */

    producer();
    
    /* Wait for child to finish */
	printf("Producer consumer done file duplicated :)\n");
    wait(&status);
 	//printf("time to close everything\n");
    /* Remove shared memory */
    shmctl(shmID, IPC_RMID, &shminfo);
 
    /* Remove semaphores */
    semctl(semID, SEM_MUTEX, IPC_RMID, seminfo);
     
    /* Close file */
    fclose(fp);
     
    /* Parent is done cleaning up, so now quits */
    return 0;
  }
  else
   exit(1); 
}
 
void initialize()
{
 
  /* Init semaphores */
  /* Three semaphores (Empty, Full, Mutex) are created in one set */
  semID=semget(IPC_PRIVATE, 3, 0666 | IPC_CREAT);
 
  /* Init Mutex to one, allowing access to critical section */
  seminfo.val=1;
  semctl(semID, SEM_MUTEX, SETVAL, seminfo);
 
  /* Init Empty to number of elements in shared memory (circular buffer) */
  seminfo.val=NUM_ELEM;
  semctl(semID, SEM_EMPTY, SETVAL, seminfo);
 
  /* Init Full to zero, no elements are produced yet */
  seminfo.val=0;
  semctl(semID, SEM_FULL, SETVAL, seminfo);
 
  /* Init Shared memory */
  //shmID=shmget(IPC_PRIVATE, NUM_ELEM, 0666 | IPC_CREAT);
  char *filePath = "HW4";
  shmID = shm_open(filePath, O_CREAT | O_RDWR, 0666);
  ftruncate(shmID, NUM_ELEM);
  if((shmid = mmap(0, NUM_ELEM, PROT_WRITE, MAP_SHARED, shmID, 0)) == MAP_FAILED){
     perror("MMAP");
     exit(1);}
  shmPtr=(char*)shmid; 
  if (shmID == -1)
  {
    perror("shmget failed");
    exit(1);
  }
}
 
void producer()
{
     int w = 0;         
    /* Open file */
     fp= fopen("input.txt", "r"); //read from file 
     if (fp == NULL)
    {
        printf("File not found!\n");
		exit(0);
    }
    
    /* Check for error in shmptr assignment */
    if (shmPtr == (void *)(-1))
    {
       perror("shmat failed in producer");
       exit(1);
    }
  
    while(x = fgetc(fp))    
    {
		
    /* Wait(Empty) - pause if no empty spots in circular buffer (i.e. all filled) */
        
        semop(semID, &WaitEmpty, 1);
 
        elem = x;
		
 
        /* Wait(Mutex) - don't touch shared memory while consumer is using it */
        semop(semID, &WaitMutex, 1);
 
        /* Put element into shared memory buffer (circular buffer) */
        //*(shmPtr + (i%NUM_ELEM))=elem;
        shmPtr[w]=elem;
        
 
        /* Signal(Mutex) - allow consumer to access shared memory now */
        semop(semID, &SignalMutex, 1);
 
        /* Signal(Full) - record one more filled spot in circular buffer */
        semop(semID, &SignalFull, 1);
        
        w = (1+w)%NUM_ELEM;

        if(x == EOF)
            break;
    }
}
 
void consumer(int numCons)
{
    FILE *fp; //file handle 
    int r = 0, pid; //iterative variable for reading through the queue and pid
    char read; //variable to store what's being read
	char handle[12]; //variable to store the file handle string
    
    while (1){  
        /* Wait(Full) - pause if no filled spots in circular buffer (i.e. all empty) */
        semop(semID, &WaitFull, 1);
 
        /* Wait(Mutex) - don't touch shared memory while producer is using it */
        semop(semID, &WaitMutex, 1);
 
        /* Get element from the shared memory buffer (circular buffer) */

        read = shmPtr[r];
        r = (1+r)%NUM_ELEM;

    	for(int i=0;i<numCons;i++) //fork once for each file  to create
		{ 
		    if(fork() == 0) //only create a file if it's a child process. 
		    { 
				sprintf(handle, "output%d.txt", i); //stores a new file name
				fp = fopen(handle, "a"); //create file for append only  
        		if (read != EOF) 
				{        			
					fprintf(fp, "%c", read); //store the character to the file
					//printf("writing %c to file\n", read);
				}        
				
				fclose(fp);
				exit(0);
			}
			else
			{
			wait(NULL);
			}
		}

        /* Signal(Mutex) - allow producer to access shared memory now */
        semop(semID, &SignalMutex, 1);
 
        /* Signal(Empty) - record one more empty spot in circular buffer */
        semop(semID, &SignalEmpty, 1);
		if (read == EOF) //-1
        	break;
		
   } 
}

