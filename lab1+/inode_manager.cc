#include "inode_manager.h"

// disk layer -----------------------------------------

disk::disk()
{
  bzero(blocks, sizeof(blocks));
}

void
disk::read_block(blockid_t id, char *buf)
{
  memcpy(buf,blocks[id],BLOCK_SIZE);
}

void
disk::write_block(blockid_t id, const char *buf)
{
  memcpy(blocks[id],buf,BLOCK_SIZE);
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
  blockid_t cur = 0;
  while (cur < sb.nblocks) {
    read_block(BBLOCK(cur), buf);
    for (int i = 0; i < BLOCK_SIZE && cur < sb.nblocks; ++i) {
      unsigned char mask = 0x80;
      while (mask > 0 && cur < sb.nblocks) {
        if ((buf[i] & mask) == 0) {
          buf[i] = buf[i] | mask;
          write_block(BBLOCK(cur), buf);
          return cur;
        }
        mask = mask >> 1;
        ++cur;
      }
    }
  }
  exit(0);
}

void
block_manager::free_block(uint32_t id)
{
  /* 
   * your code goes here.
   * note: you should unmark the corresponding bit in the block bitmap when free.
   */
  
  char buf[BLOCK_SIZE];
  read_block(BBLOCK(id), buf);

  int index = (id % BPB) >> 3;
  unsigned char mask = 0xFF ^ (1 << (7 - ((id % BPB) & 0x7)));
  buf[index] = buf[index] & mask;

  write_block(BBLOCK(id), buf);
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

  char buf[BLOCK_SIZE];
  blockid_t cur = 0;
  blockid_t ending = RESERVED_BLOCK(sb.ninodes, sb.nblocks);
  while (cur < ending) {
    read_block(BBLOCK(cur), buf);
    for (int i = 0; i < BLOCK_SIZE && cur < ending; ++i) {
      unsigned char mask = 0x80;
      while (mask > 0 && cur < ending) {
        buf[i] = buf[i] | mask;
        mask = mask >> 1;
        ++cur;
      }
    }
    write_block(BBLOCK(cur - 1), buf);
  }

  bzero(buf, sizeof(buf));
  memcpy(buf, &sb, sizeof(sb));
  write_block(1, buf);
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
    //printf("\tim: error! alloc first inode %d, should be 1\n", root_dir);
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
  char buf[BLOCK_SIZE];
  uint32_t cur = 1;
  while (cur <= bm->sb.ninodes) {
    bm->read_block(IBLOCK(cur, bm->sb.nblocks), buf);
    for (int i = 0; i < IPB && cur <= bm->sb.ninodes; ++i) {
      inode_t * ino = (inode_t *)buf + i;
      if (ino->type == 0) {
        ino->type = type;
        ino->size = 0;
        ino->atime = time(0);
        ino->mtime = time(0);
        ino->ctime = time(0);
        bm->write_block(IBLOCK(cur, bm->sb.nblocks), buf);
        return cur;
      }
      ++cur;
    }
  }
  exit(0);
}

void
inode_manager::free_inode(uint32_t inum)
{
  /* 
   * your code goes here.
   * note: you need to check if the inode is already a freed one;
   * if not, clear it, and remember to write back to disk.
   */

  char buf[BLOCK_SIZE];
  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  inode_t * ino = (inode_t *)buf + (inum - 1) % IPB;
  if (ino->type == 0) {
    exit(0);
  } else {
    ino->type = 0;
    bm->write_block(IBLOCK(inum, bm->sb.nblocks), buf);
  }
}


/* Return an inode structure by inum, NULL otherwise.
 * Caller should release the memory. */
struct inode* 
inode_manager::get_inode(uint32_t inum)
{
  struct inode *ino, *ino_disk;
  char buf[BLOCK_SIZE];

  //printf("\tim: get_inode %d\n", inum);

  if (inum < 0 || inum >= INODE_NUM) {
    //printf("\tim: inum out of range\n");
    return NULL;
  }

  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  // printf("%s:%d\n", __FILE__, __LINE__);

  ino_disk = (struct inode*)buf + inum%IPB;
  if (ino_disk->type == 0) {
    //printf("\tim: inode not exist\n");
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

  //printf("\tim: put_inode %d\n", inum);
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
  char block[BLOCK_SIZE];
  inode_t * ino = get_inode(inum);
  char * buf = (char *)malloc(ino->size);
  unsigned int cur = 0;
  for (int i = 0; i < NDIRECT && cur < ino->size; ++i) {
    if (ino->size - cur > BLOCK_SIZE) {
      bm->read_block(ino->blocks[i], buf + cur);
      cur += BLOCK_SIZE;
    } else {
      int len = ino->size - cur;
      bm->read_block(ino->blocks[i], block);
      memcpy(buf + cur, block, len);
      cur += len;
    }
  }

  if (cur < ino->size) {
    char indirect[BLOCK_SIZE];
    bm->read_block(ino->blocks[NDIRECT], indirect);
    for (unsigned int i = 0; i < NINDIRECT && cur < ino->size; ++i) {
      blockid_t ix = *((blockid_t *)indirect + i);
      if (ino->size - cur > BLOCK_SIZE) {
        bm->read_block(ix, buf + cur);
        cur += BLOCK_SIZE;
      } else {
        int len = ino->size - cur;
        bm->read_block(ix, block);
        memcpy(buf + cur, block, len);
        cur += len;
      } 
    }
  }

  *buf_out = buf;
  *size = ino->size;
  ino->atime = time(0);
  ino->ctime = time(0);
  put_inode(inum, ino);
  free(ino);
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
  char block[BLOCK_SIZE];
  char indirect[BLOCK_SIZE];
  inode_t * ino = get_inode(inum);
  unsigned int old_block_num = (ino->size + BLOCK_SIZE - 1) / BLOCK_SIZE;
  unsigned int new_block_num = (size + BLOCK_SIZE - 1) / BLOCK_SIZE;

  if (old_block_num > new_block_num) {
    if (new_block_num > NDIRECT) {
      bm->read_block(ino->blocks[NDIRECT], indirect);
      for (unsigned int i = new_block_num; i < old_block_num; ++i) {
        bm->free_block(*((blockid_t *)indirect + (i - NDIRECT)));
      }
    } else {
      if (old_block_num > NDIRECT) {
        bm->read_block(ino->blocks[NDIRECT], indirect);
        for (unsigned int i = NDIRECT; i < old_block_num; ++i) {
          bm->free_block(*((blockid_t *)indirect + (i - NDIRECT)));
        }
        bm->free_block(ino->blocks[NDIRECT]);
        for (unsigned int i = new_block_num; i < NDIRECT; ++i) {
          bm->free_block(ino->blocks[i]);
        }
      } else {
        for (unsigned int i = new_block_num; i < old_block_num; ++i) {
          bm->free_block(ino->blocks[i]);
        }
      }
    }
  }

  if (new_block_num > old_block_num) {
    if (new_block_num <= NDIRECT) {
      for (unsigned int i = old_block_num; i < new_block_num; ++i) {
        ino->blocks[i] = bm->alloc_block();
      }
    } else {
      if (old_block_num <= NDIRECT) {
        for (unsigned int i = old_block_num; i < NDIRECT; ++i) {
          ino->blocks[i] = bm->alloc_block();
        }
        ino->blocks[NDIRECT] = bm->alloc_block();

        bzero(indirect, BLOCK_SIZE);
        for (unsigned int i = NDIRECT; i < new_block_num; ++i) {
          *((blockid_t *)indirect + (i - NDIRECT)) = bm->alloc_block();
        }
        bm->write_block(ino->blocks[NDIRECT], indirect);
      } else {
        bm->read_block(ino->blocks[NDIRECT], indirect);
        for (unsigned int i = old_block_num; i < new_block_num; ++i) {
          *((blockid_t *)indirect + (i - NDIRECT)) = bm->alloc_block();
        }
        bm->write_block(ino->blocks[NDIRECT], indirect);
      }
    }
  }

  int cur = 0;
  for (int i = 0; i < NDIRECT && cur < size; ++i) {
    if (size - cur > BLOCK_SIZE) {
      bm->write_block(ino->blocks[i], buf + cur);
      cur += BLOCK_SIZE;
    } else {
      int len = size - cur;
      memcpy(block, buf + cur, len);
      bm->write_block(ino->blocks[i], block);
      cur += len;
    }
  }

  if (cur < size) {
    bm->read_block(ino->blocks[NDIRECT], indirect);
    for (unsigned int i = 0; i < NINDIRECT && cur < size; ++i) {
      blockid_t ix = *((blockid_t *)indirect + i);
      if (size - cur > BLOCK_SIZE) {
        bm->write_block(ix, buf + cur);
        cur += BLOCK_SIZE;
      } else {
        int len = size - cur;
        memcpy(block, buf + cur, len);
        bm->write_block(ix, block);
        cur += len;
      }
    }
  }

  ino->size = size;
  ino->mtime = time(0);
  ino->ctime = time(0);
  put_inode(inum, ino);
  free(ino);
}

void
inode_manager::getattr(uint32_t inum, extent_protocol::attr &a)
{
  /*
   * your code goes here.
   * note: get the attributes of inode inum.
   * you can refer to "struct attr" in extent_protocol.h
   */
  
  struct inode *ino;
  ino = get_inode(inum);
  if(!ino) return;
  a.type=ino->type;
  a.size=ino->size;
  a.atime=ino->atime;
  a.mtime=ino->mtime;
  a.ctime=ino->ctime;
  free(ino);
}

void
inode_manager::remove_file(uint32_t inum)
{
  /*
   * your code goes here
   * note: you need to consider about both the data block and inode of the file
   */
  
  inode_t * ino = get_inode(inum);
  unsigned int block_num = (ino->size + BLOCK_SIZE - 1) / BLOCK_SIZE;
  if (block_num <= NDIRECT) {
    for (unsigned int i = 0; i < block_num; ++i) {
      bm->free_block(ino->blocks[i]);
    }
  } else {
    for (int i = 0; i < NDIRECT; ++i) {
      bm->free_block(ino->blocks[i]);
    }
    char indirect[BLOCK_SIZE];
    bm->read_block(ino->blocks[NDIRECT], indirect);
    for (unsigned int i = 0; i < block_num - NDIRECT; ++i) {
      bm->free_block(*((blockid_t *)indirect + i));
    }
    bm->free_block(ino->blocks[NDIRECT]);
  }
  free_inode(inum);
  free(ino);
}
