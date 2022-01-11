#ifndef yfs_client_h
#define yfs_client_h

#include <string>
//#include "yfs_protocol.h"
#include "extent_client.h"

#include <vector>

struct cache_node{
  char directory_entry[64];
  int inode_number;
  cache_node *next;
};

struct inode_list{
  int inode_number;
  inode_list * next;
};

class yfs_client {
  extent_client *ec;
 public:

  typedef unsigned long long inum;
  enum xxstatus { OK, RPCERR, NOENT, IOERR, EXIST };
  typedef int status;

  struct fileinfo {
    unsigned long long size;
    unsigned long atime;
    unsigned long mtime;
    unsigned long ctime;
  };
  struct dirinfo {
    unsigned long atime;
    unsigned long mtime;
    unsigned long ctime;
  };
  struct dirent {
    std::string name;
    yfs_client::inum inum;
  };

  struct dirent_n {
    char name[BLOCK_SIZE/4];
    yfs_client::inum inum;
    size_t len;
  };

 private:
  static std::string filename(inum);
  static inum n2i(std::string);

 public:
  yfs_client();
  yfs_client(std::string, std::string);

  int inodetype(inum);
  bool isfile(inum);
  bool isdir(inum);
  bool issymlink(inum);

  int getfile(inum, fileinfo &);
  int getdir(inum, dirinfo &);

  int setattr(inum, size_t);
  int lookup(inum, const char *, bool &, inum &);
  int create(inum, const char *, mode_t, inum &);
  int readdir(inum, std::list<dirent> &);
  int write(inum, size_t, off_t, const char *, size_t &);
  int read(inum, size_t, off_t, std::string &);
  int unlink(inum,const char *);
  int mkdir(inum , const char *, mode_t , inum &);
  int readlink(inum , std::string &);
  int symlink(inum , const char *, const char *, inum &);

  /** you may need to add symbolic link related methods here.*/
};

#endif 
