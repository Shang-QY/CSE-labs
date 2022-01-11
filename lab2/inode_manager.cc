#include "inode_manager.h"

// disk layer -----------------------------------------

disk::disk()
{
  bzero(blocks, sizeof(blocks));
}

void
disk::read_block(blockid_t id, char *buf)
{
  if (id < 0 || id >= BLOCK_NUM || buf == NULL)
    return;

  memcpy(buf, blocks[id], BLOCK_SIZE);
}

void
disk::write_block(blockid_t id, const char *buf)
{
  if (id < 0 || id >= BLOCK_NUM || buf == NULL)
    return;

  memcpy(blocks[id], buf, BLOCK_SIZE);
}

// block layer -----------------------------------------

// Allocate a free disk block.
blockid_t
block_manager::alloc_block()
{
  /*
   * your code goes here.
   * note: you should mark the corresponding bit in block bitmap when alloc.
   * you need to think about which block you can start to be allocated.
   */
  char buf[BLOCK_SIZE];
  int i = BLOCK_NUM/BPB + 3 + INODE_NUM/IPB + 1;
  int lastBlock = 0;
  for(; i < BLOCK_NUM; ++i){
    if(lastBlock != BBLOCK(i)){
      d->read_block(BBLOCK(i), buf);
      lastBlock = BBLOCK(i);
    }
    if((buf[(i % BPB) / 8] & (1 << (i % 8))) == 0) {
      // printf("找到\n");
      // printf(" %X\n",buf[(i % BPB) / 8]);
      buf[(i % BPB) / 8] |= (1 << (i % 8));
      d->write_block(BBLOCK(i), buf);
      break;
    }
  }

  // printf("找 %d\n", i);
  return i;
}

void
block_manager::free_block(uint32_t id)
{
  /* 
   * your code goes here.
   * note: you should unmark the corresponding bit in the block bitmap when free.
   */
  char buf[BLOCK_SIZE];
  d->read_block(BBLOCK(id), buf);
  buf[(id % BPB) / 8] ^= (1 << (id % 8));
  d->write_block(BBLOCK(id), buf);
  return;
}

// The layout of disk should be like this:
// |<-sb->|<-free block bitmap->|<-inode table->|<-data->|
block_manager::block_manager()
{
  d = new disk();

  // format the disk
  sb.size = BLOCK_SIZE * BLOCK_NUM;
  sb.nblocks = BLOCK_NUM;
  sb.ninodes = INODE_NUM;

}

void
block_manager::read_block(uint32_t id, char *buf)
{
  d->read_block(id, buf);
}

void
block_manager::write_block(uint32_t id, const char *buf)
{
  d->write_block(id, buf);
}

// inode layer -----------------------------------------

inode_manager::inode_manager()
{
  bm = new block_manager();
  uint32_t root_dir = alloc_inode(extent_protocol::T_DIR);
  if (root_dir != 1) {
    printf("\tim: error! alloc first inode %d, should be 1\n", root_dir);
    exit(0);
  }
}

/* Create a new file.
 * Return its inum. */
uint32_t
inode_manager::alloc_inode(uint32_t type)
{
  /* 
   * your code goes here.
   * note: the normal inode block should begin from the 2nd inode block.
   * the 1st is used for root_dir, see inode_manager::inode_manager().
   */
  inode_t inode;
  inode.type = type;
  inode.size = 0;
  inode.atime = time(NULL);
  inode.ctime = time(NULL);
  inode.mtime = time(NULL);
  int i;
  for(i = 1; i < INODE_NUM; ++i){
    inode_t *ino = get_inode(i);
    if(!ino) break;
    free(ino);
  }
  put_inode(i, &inode);
  return i;
}

void
inode_manager::free_inode(uint32_t inum)
{
  /* 
   * your code goes here.
   * note: you need to check if the inode is already a freed one;
   * if not, clear it, and remember to write back to disk.
   */
  inode_t *inode = get_inode(inum);
  inode->type = 0;
  put_inode(inum, inode);
  free(inode);
  // char buf[BLOCK_SIZE];
  // bzero(buf, sizeof(buf));
  // bm->write_block(IBLOCK(inum, BLOCK_NUM), buf);
  return;
}


/* Return an inode structure by inum, NULL otherwise.
 * Caller should release the memory. */
struct inode* 
inode_manager::get_inode(uint32_t inum)
{
  struct inode *ino, *ino_disk;
  char buf[BLOCK_SIZE];

  printf("\tim: get_inode %d\n", inum);

  if (inum < 0 || inum >= INODE_NUM) {
    printf("\tim: inum out of range\n");
    return NULL;
  }

  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  // printf("%s:%d\n", __FILE__, __LINE__);

  ino_disk = (struct inode*)buf + inum%IPB;
  if (ino_disk->type == 0) {
    printf("\tim: inode not exist\n");
    return NULL;
  }
  ino = (struct inode*)malloc(sizeof(struct inode));
  *ino = *ino_disk;

  return ino;
}

void
inode_manager::put_inode(uint32_t inum, struct inode *ino)
{
  char buf[BLOCK_SIZE];
  struct inode *ino_disk;

  printf("\tim: put_inode %d\n", inum);
  if (ino == NULL)
    return;

  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  ino_disk = (struct inode*)buf + inum%IPB;
  *ino_disk = *ino;
  bm->write_block(IBLOCK(inum, bm->sb.nblocks), buf);
}

#define MIN(a,b) ((a)<(b) ? (a) : (b))

/* Get all the data of a file by inum. 
 * Return alloced data, should be freed by caller. */
void
inode_manager::read_file(uint32_t inum, char **buf_out, int *size)
{
  /*
   * your code goes here.
   * note: read blocks related to inode number inum,
   * and copy them to buf_Out
   */
  // printf("file______\n");
  printf("~~~~~~~~~~~~~~~~~~READ BEGIN\n");
  inode_t *inode = get_inode(inum);
  int s = inode->size;
  // printf("file size is %d\n", s);
  *size = s;
  char *buffer = (char*)malloc(s);
  char buf[BLOCK_SIZE];
  int i = 0;
  while(s > 0 && i < 100){
    int readLength = MIN(s, BLOCK_SIZE);
    bm->read_block(inode->blocks[i], buf);
    for(int j = 0; j < readLength; ++j){
      buffer[i * BLOCK_SIZE + j] = buf[j];
    }
    s -= readLength;
    ++i;
  }
  if(s > 0){
    char IDEbuffer[BLOCK_SIZE];
    bm->read_block(inode->blocks[i], IDEbuffer);
    int *block_number = (int*)IDEbuffer;
    while (s > 0)
    {
      int readLength = MIN(s, BLOCK_SIZE);
      bm->read_block(*block_number, buf);
      for(int j = 0; j < readLength; ++j){
        buffer[i * BLOCK_SIZE + j] = buf[j];
      }
      printf("%d\n", *block_number);
      ++block_number;
      s -= readLength;
      ++i;
    }
  }
  // bm->read_block(inode->blocks[i], buf);
  // for(int j = 0; j < s; ++j){
  //   buffer[i * BLOCK_SIZE + j] = buf[j];
  // }
  *buf_out = buffer;
  printf("~~~~~~~~~~~~~~~~~~READ END\n");
  return;
}

/* alloc/free blocks if needed */
void
inode_manager::write_file(uint32_t inum, const char *buf, int size)
{
  /*
   * your code goes here.
   * note: write buf to blocks of inode inum.
   * you need to consider the situation when the size of buf 
   * is larger or smaller than the size of original inode
   */
  char buffer[BLOCK_SIZE];
  inode_t *inode = get_inode(inum);

  int block_num = inode->size == 0? 0 : (inode->size - 1)/BLOCK_SIZE + 1;
  int freeBlocks = MIN(block_num, NDIRECT);
  for(int i = 0; i < freeBlocks; ++i){
    bm->free_block(inode->blocks[i]);
    block_num--;
  }
  if(block_num > 0){
    char IDEbuffer[BLOCK_SIZE];
    bm->read_block(inode->blocks[NDIRECT], IDEbuffer);
    int *block_number = (int*)IDEbuffer;
    while (block_num > 0)
    {
      bm->free_block(*block_number++);
      block_num--;
    }
    bm->free_block(inode->blocks[NDIRECT]);
  }
  inode->size = 0;
  
  int blockIndex = 0;
  while (size > 0 && blockIndex < 100)
  {
    inode->blocks[blockIndex] = bm->alloc_block();
    ++blockIndex;
    int fillLength = MIN(size, BLOCK_SIZE);
    // printf("file size: %d\n", fillLength);
    for(int i = 0; i < fillLength; ++i){
      buffer[i] = *buf;
      ++buf;
    }
    bm->write_block(inode->blocks[inode->size / BLOCK_SIZE], buffer);
    // printf("block number: %d\n", inode->blocks[inode->size / BLOCK_SIZE]);
    inode->size += fillLength;
    size -= fillLength;
  }
  if(size > 0){
    // printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~INDIRECT\n");
    char IDEbuffer[BLOCK_SIZE];
    int *block_number = (int*)IDEbuffer;
    uint IDEnumber = 0;
    while (size > 0 && IDEnumber < NINDIRECT)
    {
      IDEnumber++;
      int bnumber = bm->alloc_block();
      // printf("new block number: %d\n", bnumber);
      int fillLength = MIN(size, BLOCK_SIZE);
      for(int i = 0; i < fillLength; ++i){
        buffer[i] = *buf;
        ++buf;
      }
      bm->write_block(bnumber, buffer);
      *block_number++ = bnumber;
      inode->size += fillLength;
      size -= fillLength;
    }
    // printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~INDIRECT END\n");
    // int *bn = (int*)IDEbuffer;
    // for(int i = 0; i < BLOCK_SIZE/4; ++i){
    //   printf("%d\t", *bn);
    //   bn++;
    // }
    // printf("\n");

    if(size > 0) printf("file is too big.");
    inode->blocks[blockIndex] = bm->alloc_block();
    bm->write_block(inode->blocks[blockIndex], IDEbuffer);
  }
  inode->atime = time(NULL);
  inode->mtime = time(NULL);
  inode->ctime = time(NULL);
  put_inode(inum, inode);
  return;
}

void
inode_manager::getattr(uint32_t inum, extent_protocol::attr &a)
{
  /*
   * your code goes here.
   * note: get the attributes of inode inum.
   * you can refer to "struct attr" in extent_protocol.h
   */
  inode_t *inode = get_inode(inum);
  if(inode == NULL){
    a.type = 0;
    return;
  }
  a.type = inode->type;
  a.size = inode->size;
  a.atime = inode->atime;
  a.mtime = inode->mtime;
  a.ctime = inode->ctime;
  free(inode);
  return;
}

void
inode_manager::remove_file(uint32_t inum)
{
  /*
   * your code goes here
   * note: you need to consider about both the data block and inode of the file
   */
  inode_t *inode = get_inode(inum);
  int block_num = inode->size == 0? 0 : (inode->size - 1)/BLOCK_SIZE + 1;
  int freeBlocks = MIN(block_num, NDIRECT);
  for(int i = 0; i < freeBlocks; ++i){
    bm->free_block(inode->blocks[i]);
    block_num--;
  }
  if(block_num > 0){
    char IDEbuffer[BLOCK_SIZE];
    bm->read_block(inode->blocks[NDIRECT], IDEbuffer);
    int *block_number = (int*)IDEbuffer;
    while (block_num > 0)
    {
      bm->free_block(*block_number++);
      block_num--;
    }
    bm->free_block(inode->blocks[NDIRECT]);
  }
  free_inode(inum);
  free(inode);
  return;
}
