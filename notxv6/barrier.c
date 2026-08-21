#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <pthread.h>

static int nthread = 1;
static int round = 0;

struct barrier {
  pthread_mutex_t barrier_mutex;
  pthread_cond_t barrier_cond;
  int nthread;      // Number of threads that have reached this round of the barrier
  int round;     // Barrier round
} bstate;

static void
barrier_init(void)
{
  assert(pthread_mutex_init(&bstate.barrier_mutex, NULL) == 0);//初始化互斥锁
  assert(pthread_cond_init(&bstate.barrier_cond, NULL) == 0);//初始化条件变量
  bstate.nthread = 0;
}

static void
barrier(void)
{
  pthread_mutex_lock(&bstate.barrier_mutex);

  //这个线程进入barrier时的轮次
  int round = bstate.round;

  //这个线程已经到达当前轮的屏障
  bstate.nthread++;

  if (bstate.nthread == nthread) {
    //这个线程是当前轮最后一个到达的线程。重置计数器并进入下一轮，然后唤醒所有等待线程。
    bstate.nthread = 0;
    bstate.round++;

    pthread_cond_broadcast(&bstate.barrier_cond);

  } else {
    //不是最后一个线程。
    //只要 round 没有改变，就说明当前轮尚未结束
    while (round == bstate.round) {
      pthread_cond_wait(&bstate.barrier_cond, &bstate.barrier_mutex);
    }
  }

  pthread_mutex_unlock(&bstate.barrier_mutex);
}

static void *
thread(void *xa)
{
  long n = (long) xa;
  long delay;
  int i;

  for (i = 0; i < 20000; i++) {
    int t = bstate.round;
    assert (i == t);
    barrier();
    usleep(random() % 100);
  }

  return 0;
}

int
main(int argc, char *argv[])
{
  pthread_t *tha;
  void *value;
  long i;
  double t1, t0;

  if (argc < 2) {
    fprintf(stderr, "%s: %s nthread\n", argv[0], argv[0]);
    exit(-1);
  }
  nthread = atoi(argv[1]);
  tha = malloc(sizeof(pthread_t) * nthread);
  srandom(0);

  barrier_init();

  for(i = 0; i < nthread; i++) {
    assert(pthread_create(&tha[i], NULL, thread, (void *) i) == 0);
  }
  for(i = 0; i < nthread; i++) {
    assert(pthread_join(tha[i], &value) == 0);
  }
  printf("OK; passed\n");
}
