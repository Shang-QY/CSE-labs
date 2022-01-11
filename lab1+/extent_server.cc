// the extent server implementation

#include "extent_server.h"
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

extent_server::extent_server() 
{
  im = new inode_manager();
}

int extent_server::create(uint32_t type, extent_protocol::extentid_t &id)
{
  id = im->alloc_inode(type);
  return extent_protocol::OK;
}

int extent_server::put(extent_protocol::extentid_t id, std::string buf, int &)
{
  const char * cbuf = buf.c_str();
  int size = buf.size();
  im->write_file(id, cbuf, size);
  return extent_protocol::OK;
}

int extent_server::get(extent_protocol::extentid_t id, std::string &buf)
{
  int size = 0;
  char *cbuf = NULL;
  im->read_file(id, &cbuf, &size);
  if (size == 0)
    buf = "";
  else {
    buf.assign(cbuf, size);
    free(cbuf);
  }
  return extent_protocol::OK;
}

int extent_server::getattr(extent_protocol::extentid_t id, extent_protocol::attr &a)
{
  extent_protocol::attr attr;
  memset(&attr, 0, 20);
  im->getattr(id, attr);
  a = attr;
  return extent_protocol::OK;
}

int extent_server::remove(extent_protocol::extentid_t id, int &)
{
  im->remove_file(id);
  return extent_protocol::OK;
}

