#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

const char sem_gen_name[] = "/sem_gen";
const char shared_mem_name[] = "/posix-shar-object";

int fd;
sem_t *sem_gen;
int sem_id;
int *mem;

int *pids;
int main_pid;
int rounds;
int place = 0;

const char *choices[] = {"rock", "scissors", "paper"};

// 0 - rock, 1 - paper, 2 - scissors
int generate_choice() { return rand() % 3; }

void handler(int signal) {
  if (signal == SIGINT) {
    if (getpid() == main_pid) {
      free(pids);
      close(fd);
      close(sem_id);
      shm_unlink(sem_gen_name);
      shm_unlink(shared_mem_name);
    }
    exit(-1);
  }
  if (signal == SIGUSR1) {
    mem[0] = generate_choice();
  }
  if (signal == SIGUSR2) {
    mem[1] = generate_choice();
    sem_post(sem_gen);
  }
}

int main(void) {
  signal(SIGINT, handler);
  signal(SIGUSR1, handler);
  signal(SIGUSR2, handler);
  int num_students;
  main_pid = getpid();
  srand(time(NULL));
  printf("Enter the number of students: ");
  scanf("%d", &num_students);

  pids = (int *)malloc(num_students * sizeof(int));

  // UNNAMED SEMAPHOR init
  sem_id = shm_open(sem_gen_name, O_CREAT | O_EXCL | O_RDWR, 0666);
  ftruncate(sem_id, sizeof(sem_t));
  sem_gen = (sem_t *)mmap(0, sizeof(sem_t), PROT_WRITE | PROT_READ, MAP_SHARED,
                          sem_id, 0);
  sem_init(sem_gen, 1, 0);
  // MEMORY
  fd = shm_open(shared_mem_name, O_CREAT | O_EXCL | O_RDWR, 0666);
  if (fd < 0) {
    fprintf(stderr, "shm_open failed: %s\n", strerror(errno));
    handler(SIGINT);
    exit(-1);
  }
  ftruncate(fd, 2 * sizeof(int));
  mem = mmap(0, 2 * sizeof(int), PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0);

  for (int i = 0; i < num_students; i++) {
    int pid = fork();
    // CHILD
    if (pid == 0) {
      // just do nothing
      srand(getpid());
      while (1) {
        sleep(1);
      }
      exit(0);
    }
    pids[i] = pid;
  }
  int round = 1;
  while (pids[1] != 0) {
    place = 0;
    printf("\n--------------ROUND %d------------\n", round);
    for (int i = 0; i < num_students; i++) {
      int first = pids[i];
      pids[i] = 0;
      if (first != 0) {
        int second = pids[i + 1];
        if (i + 1 == num_students || second == 0) {
          pids[place] = first;
          continue;
        }
        pids[i + 1] = 0;
        while (1) {
          kill(first, SIGUSR1);
          kill(second, SIGUSR2);
          sem_wait(sem_gen);
          int res1 = mem[0];
          int res2 = mem[1];
          printf("Student %d shows %s, Student %d shows %s | ", first,
                 choices[res1], second, choices[res2]);
          if (res1 == 0 && res2 == 1 || res1 == 1 && res2 == 2 ||
              res1 == 2 && res2 == 0) {
            kill(second, SIGINT);
            pids[place++] = first;
            printf("Student %d win!!!\n", first);
            break;
          } else if (res2 == 0 && res1 == 1 || res2 == 1 && res1 == 2 ||
                     res2 == 2 && res1 == 0) {
            kill(first, SIGINT);
            pids[place++] = second;
            printf("Student %d win!!!\n", second);
            break;
          } else {
            printf("Draw! Replay!\n");
          }
        }
      }
    }
    ++round;
  }

  printf("\nTHE WINNER IS STUDENT %d!!!!!\n\n", pids[0]);

  for (int i = 0; i < num_students; i++) {
    if (pids[i] != 0) {
      kill(pids[i], SIGINT);
    }
  }

  // EXIT
  handler(SIGINT);
  exit(0);
  return 0;
}
