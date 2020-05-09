// gcc -pthread -o "pellet" pellet.c

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/syscall.h>

#define PELLET_MIN 5    // min number pellets
#define PELLET_MAX 10   // max number pellets
#define SHM_SIZE 60     // size 15 shmem
//semaphores
#include <sys/sem.h>
#define KEY 0x1111 //sem
// global vars accessible by both parent process and threads

int pellet;
int *shmem;         // shared memory array denoting location
int shmid;
int key;
int shmdtOut;       // holds shmdt output

//FILE * file;        // file to log pellet information
union semun{
int val;
struct semid_ds *buf;
unsigned short *array;
};

struct sembuf p={ 0, -1, SEM_UNDO};//semwait
struct sembuf v={ 0, +1, SEM_UNDO};//semsignal

void *pelletChild(void *ignored){
 
  int pos;
  int thisPellet = pellet;              // denotes unique pellet
  long pid = syscall(SYS_gettid);       // denotes thread id
int id;    //sem id
  // generate random pellet location in first 5 rows
  srand(time(NULL));
  int r = rand() % 50;
  
  //semaphores
  if((id=semget(KEY,1,0666|IPC_CREAT)<0)){
    perror("semget");
    exit(1);
  }
  
  // ensure pellet does not overlap with another
  int i;
  for( i = 0; i < pellet; i++ ){
    while( r == shmem[i] ){
      r = rand() % 50;
    }
  }

  shmem[thisPellet] = r;
  shmem[11] = thisPellet;   // tracks number of pellets

  // keep thread running until cancelled
  while( shmem[12] < shmem[13] ){
    sleep(2);
    //sem should be here
    //decrementing sem
	if(semop(id,&p,1)<0){
	 perror("cannot semop decrement\n");
      exit(1);
	}
    
    // if pellet has left range of mill
    if( shmem[thisPellet] > 99 ){
      
      // Each pellet's status printed before exiting
      if( shmem[thisPellet] < 110 ){
        printf("pellet ID: %Lf\n", (long double) pid);
        printf("pellet position: %d\n", shmem[thisPellet]-10);
        printf("pellet status: MISSED\n");

        //fprintf(file, "pellet ID: %Lf\n", (long double) pid);
        //fprintf(file, "pellet position: %d\n", shmem[thisPellet]-10);
        //fprintf(file, "pellet status: MISSED\n");

      }
      else{
        printf("pellet ID: %Lf\n", (long double) pid);
        printf("pellet position: %d\n", shmem[0]);
        printf("pellet status: EATEN\n");

        //fprintf(file, "pellet ID: %Lf\n", (long double) pid);
        //fprintf(file, "pellet position: %d\n", shmem[0]);
        //fprintf(file, "pellet status: EATEN\n");
      }

      // move back to random position
      srand(time(NULL));
      for( i = 0; i < pellet; i++ ){
        while( r == shmem[i] ){
          r = rand() % 50;
        }
      }

      shmem[thisPellet] = r;

    }

    // if next position doesnt overlap with fish
    pos = shmem[thisPellet];
    if( (pos += 10) != shmem[0] ){  
      // move pellet one row down 
      shmem[thisPellet] += 10;
    }
    
    else {
      // or move pellet out of range of mill
      shmem[thisPellet] = 110;
      
      //increment pelletsCaughtCount
      shmem[14]++;
      //printf("pellet caught!: %d\n", shmem[14]);
    }
    
    //printf("location[%d]: %d\n", thisPellet, shmem[thisPellet]);


//sem inc
//incrementing sem
	if(semop(id,&v,1)<0){
	 perror("cannot semop increment\n");
      exit(1);
	}
  } 

  //fclose(file);
}

int main(){
  
  pthread_t child_thread;   // instantiation of thread
  int code;                 // holds any error code that may occur

  key = 5678;               // declares key
  pellet = 1;               // tracks shmem locations of pellet (1-10)
  
  //file = fopen("pelletlog.txt", "w+");


  // allocates shared memory segment
  if( (shmid = shmget(key, SHM_SIZE, 0666)) < 0 ){
    perror("shmget");
    exit(1);
  }   

  // attach shared memory segment to address space
  if( (shmem = shmat(shmid, NULL, 0)) == (int *) -1){
    perror("shmat");
    exit(1);
  }

  // parent thread creates up to 10 child threads
  while( (pellet <= PELLET_MAX) && (shmem[12] < shmem[13])){
    code = pthread_create(&child_thread, NULL, pelletChild, NULL);
    if(code){
      fprintf(stderr, "pthread_create failed with code %d\n", code);
    }
    
    sleep(7);   // allow first pellet to run before creating second
                // 30 should divide by this number evenly
    pellet++;

  }

  printf("# pellets caught: %d\n", shmem[14]);

  // shared memory deallocated
  shmdtOut = shmdt(shmem);
  if( shmdtOut == -1 ){
    perror("shmdt");
    exit(1);
  }  
  
  exit(0);
}


