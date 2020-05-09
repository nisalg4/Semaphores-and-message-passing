// gcc -o "swim_mill" swim_mill.c

// for solution to no space left on device: bash kill_ipcs.sh

#include <stdio.h>
#include <stdlib.h>

#include <time.h>
#include <unistd.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

//semaphores
#include <sys/sem.h>
#define KEY 0x1111

#define SHM_SIZE 60     // shared memory segment size 15  
                        // fish[0] 
                        // pellets[1-10] 
                        // numPellets[11]
                        // start[12] & endwait[13]
                        // numPelletsCaught[14]

static volatile int keepRunning = 1;


union semun{
int val;
struct semid_ds *buf;
unsigned short *array;
};

struct sembuf p={ 0, -1, SEM_UNDO};//semwait
struct sembuf v={ 0, +1, SEM_UNDO};//semsignal


void intHandler(int dummy) {
  keepRunning = 0;
  printf("\nProcess killed.\n");
}
  
int main(void){

  int *shmem;               // shared memory array denoting location
  int shmid;
  int key = 5678;           // declares key 
  int shmdtOut;             // holds shmdt output
  int id;	            //  holds semaphore
  char swimMill[99];        // declare visible swim mill
  
  // file to write swim mill output to
  FILE *f = fopen("output.txt", "w");

  // pids for swim mill parent and fish/pellet child processes
  pid_t parentId = getpid();
  pid_t fishId, pelletId;

 
  // measure length of computation
  clock_t endwait;             
  clock_t start = clock();  // returns start time
  clock_t duration = 30;     // process ends after 30 seconds
  endwait = start + duration;

 //semaphores
  if((id=semget(KEY,1,0666|IPC_CREAT)<0)){
    perror("semget");
    exit(1);
  }
  //initialize semaphore
  union semun u;
  u.val=1;
  if(semctl(id,0,SETVAL,u)<0){
   perror("semctl");
    exit(1);
  }
  
  // allocates shared memory segment
  shmid = shmget(key, SHM_SIZE, IPC_CREAT | 0666);
  if( shmid < 0 ){
    perror("shmget");
    exit(1);
  }

  // attaches shared memory segment identified by shmid
  // to the address space of the calling process
  shmem = shmat(shmid, NULL, 0);
  if( shmem == (int*) -1 ){
    perror("shmat");
    exit(1);
  } 
  

  // ensures that all processes only run for 30 second duration
  shmem[12] = start;
  shmem[13] = endwait;
  
  // sets pellets caught count to 0
  shmem[14] = 0;

  signal(SIGINT, intHandler);

  // fork child process for fish
  fishId = fork();

  if( fishId < 0 ){
    perror("cannot fork\n");
    exit(1);
  }
  
  else if( fishId > 0 ){

    // fork child process for pellet
    pelletId = fork();
  
    if( pelletId < 0 ){
      perror("cannot fork\n");
      exit(1);
    }
  
    else if( pelletId > 0 ){
      
      sleep(2);

      // 30 second computation
      while( (start < endwait) && keepRunning ){ 
      //decrementing sem
	if(semop(id,&p,1)<0){
	 perror("cannot semop decrement\n");
      exit(1);
	}
        /* Print result to terminal */

        // fills in or clears swim mill
        int i;
        for( i = 0; i < 100; i++ ){
          swimMill[i] = '~';
        }

        // assign pellet location based on shared mem
        for( i = 1; i <= shmem[11]; i++ ){
          swimMill[shmem[i]] = 'o';
        }

        // assign fish location based on shared mem
        swimMill[shmem[0]] =(char) 0x80;

        // print swim mill && write result to file
        for( i = 0; i < 100; i++ ){  
          
          if( i%10 == 0 ){
            printf("\n%c", swimMill[i]);
            fprintf(f, "\n%c", swimMill[i]);
          }
  
          else{
            printf("%c", swimMill[i]);
            fprintf(f, "%c", swimMill[i]);
          }

          if(i == 99){
            printf("\n\n");
            fprintf(f, "\n\n");
          }
        }

        sleep(2);
        start++;
        shmem[12] = start;   

        //printf("start: %Lf\n", (long double) start);
        //printf("endwait: %Lf\n", (long double) endwait);
        
     //incrementing sem
	if(semop(id,&v,1)<0){
	 perror("cannot semop increment\n");
      exit(1);
	}
      }


    }

    else{
  
      // executes and leaves pellet process
      int pelletOut = execl("./pellet", "./pellet", NULL); 
      if( pelletOut == -1 ){
        perror("execl pellet");
        exit(1);
      }

      _exit(EXIT_FAILURE);
    }

  }

  else{

    // executes and leaves fish process
    int fishOut = execl("./fish", "./fish", NULL);
    if( fishOut == -1 ){
      perror("execl fish");
      exit(1);
    }

    _exit(EXIT_FAILURE);
  }

  fclose(f);

  // all children of coordinator killed
  kill(fishId, SIGKILL);
  printf("fish process exited\n");

  kill(pelletId, SIGKILL);
  printf("pellet process exited\n");

  printf("swim mill exited\n");

  shmdtOut = shmdt(shmem);
  if( shmdtOut == -1 ){
    perror("shmdt");
    exit(1);
  }  

  exit(0);

}

