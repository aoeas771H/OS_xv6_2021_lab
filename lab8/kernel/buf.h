struct buf {
  int valid;   // has data been read from disk?
  int disk;    // does disk "own" buf?
  uint dev;
  uint blockno;
  struct sleeplock lock;
  uint refcnt; // reference count
  struct buf *prev; // LRU cache list
  struct buf *next;
  uchar data[BSIZE];
  uint timestamp;  // 时间戳，由于不用原始方案的双向链表实验LRU
  // 这里用时间戳的方式实现LRU
};

