struct buf {
  int valid;   // has data been read from disk?
  int disk;    // does disk "own" buf?
  uint dev;    //磁盘设备编号
  uint blockno;  //磁盘块编号
  struct sleeplock lock;
  uint refcnt;     //有多少使用者在使用该缓冲区
  struct buf *prev; // LRU cache list
  struct buf *next;
  uchar data[BSIZE];
  uint timestamp;   //refcnt变成0时记录最后使用时间
  int bucket;   //桶号
};

