// RPC stubs for clients to talk to extent_server

#include "extent_client.h"
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

extent_client::extent_client()
{
  im = new inode_manager();
}

extent_protocol::status
extent_client::create(uint32_t type, extent_protocol::extentid_t &id)
{
  id = im->alloc_inode(type);
  return extent_protocol::OK;
}

extent_protocol::status
extent_client::get(extent_protocol::extentid_t eid, std::string &buf)
{
  int size = 0;
  char *cbuf = NULL;
  im->read_file(eid, &cbuf, &size);
  if (size == 0)
    buf = "";
  else {
    buf.assign(cbuf, size);
    free(cbuf);
  }
  return extent_protocol::OK;
}

extent_protocol::status
extent_client::getattr(extent_protocol::extentid_t eid, 
		       extent_protocol::attr &a)
{
  extent_protocol::attr attr;
  memset(&attr, 0, 20);
  im->getattr(eid, attr);
  a = attr;
  return extent_protocol::OK;
}

extent_protocol::status
extent_client::put(extent_protocol::extentid_t eid, std::string buf)
{
  const char * cbuf = buf.c_str();
  int size = buf.size();
  im->write_file(eid, cbuf, size);
  return extent_protocol::OK;
}

extent_protocol::status
extent_client::remove(extent_protocol::extentid_t eid)
{
  im->remove_file(eid);
  return extent_protocol::OK;
}


