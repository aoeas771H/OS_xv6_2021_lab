// On-disk file system format.
// Both the kernel and user programs use this header file.


#define ROOTINO  1   // root i-number
#define BSIZE 1024  // block size

// Disk layout:
// [ boot block | super block | log | inode blocks |
//                                          free bit map | data blocks]
//
// mkfs computes the super block and builds an initial file system. The
// super block describes the disk layout:
struct superblock {
  uint magic;        // Must be FSMAGIC
  uint size;         // Size of file system image (blocks)
  uint nblocks;      // Number of data blocks
  uint ninodes;      // Number of inodes.
  uint nlog;         // Number of log blocks
  uint logstart;     // Block number of first log block
  uint inodestart;   // Block number of first inode block
  uint bmapstart;    // Block number of first free map block
};

#define FSMAGIC 0x10203040

#define NDIRECT 11 // 直接块的数量，直接块：存放数据的块
#define NINDIRECT (BSIZE / sizeof(uint)) // 一级间接块的数量：256
#define NDINDIRECT ((BSIZE / sizeof(uint)) * (BSIZE / sizeof(uint))) // 二级间接块的数量：256*256
#define MAXFILE (NDIRECT + NINDIRECT + NDINDIRECT) // 一个文件所占的最大块数：11+256+256*256
#define NADDR_PER_BLOCK (BSIZE / sizeof(uint))  // 一个块中的地址数量

// On-disk inode structure
struct dinode {
  short type;           // File type
  short major;          // Major device number (T_DEVICE only)
  short minor;          // Minor device number (T_DEVICE only)
  short nlink;          // Number of links to inode in file system
  uint size;            // Size of file (bytes)
  uint addrs[NDIRECT+2];   // Data block addresses
}; // NDIRECT改变成11，那么这里要改成+2

/*字段type区分文件、目录和特殊文件（设备）。type为零表示磁盘inode是空闲的。
字段nlink统计引用此inode的目录条目数，以便识别何时应释放磁盘上的inode及其数据块。
字段size记录文件中内容的字节数。
addrs数组记录保存文件内容的磁盘块的块号。*/

// Inodes per block.
#define IPB           (BSIZE / sizeof(struct dinode))

// Block containing inode i
#define IBLOCK(i, sb)     ((i) / IPB + sb.inodestart)

// Bitmap bits per block
#define BPB           (BSIZE*8)

// Block of free map containing bit for block b
#define BBLOCK(b, sb) ((b)/BPB + sb.bmapstart)

// Directory is a file containing a sequence of dirent structures.
#define DIRSIZ 14

struct dirent {
  ushort inum;
  char name[DIRSIZ];
};

