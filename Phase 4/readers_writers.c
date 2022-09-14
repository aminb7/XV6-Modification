#include "types.h"
#include "stat.h"
#include "fs.h"
#include "fcntl.h"
#include "syscall.h"
#include "user.h"
#include "spinlock.h"

void
readers_writers(){

  struct condvar rw_mutex_cv;
  struct condvar mutex_cv;

  struct spinlock rw_mutex;
  struct spinlock mutex;

  init_lock(&rw_mutex);
  init_lock(&mutex);

  rw_mutex_cv.lock = rw_mutex;
  mutex_cv.lock = mutex;

  reset_counter();
  for (int i = 0; i < 6; i++){
      int pid = fork();

      if (pid == 0){
        cv_wait(&rw_mutex_cv);
        cv_wait(&mutex_cv);
      
        // Writers
        if (i == 0 || i == 1){
            lock(&rw_mutex);
            
            printf(1, "Child %d is writing\n", i + 1);

            unlock(&rw_mutex);
        }

        // Readers
        if (i != 0 && i != 1){
            sleep(200);

            printf(1, "Child %d is waiting for reading\n", i + 1);
            lock(&mutex);
            inc_counter();
            
            if (get_counter() == 1)
              lock(&rw_mutex);

            unlock(&mutex);

            printf(1, "Child %d is reading\n", i + 1);

            lock(&mutex);
            dec_counter();
            if (get_counter() == 0)
              unlock(&rw_mutex);
            unlock(&mutex);
        }

        exit();
      }
      sleep(400);
      cv_signal(&rw_mutex_cv);
      cv_signal(&mutex_cv);
  }
  
  for (int i = 0; i < 5; i++)
    wait();
  cv_signal(&rw_mutex_cv);
  cv_signal(&mutex_cv);
  wait();
}

int
main(int argc, char *argv[]){
  readers_writers();
  exit();
}
