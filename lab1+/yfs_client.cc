// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

yfs_client::yfs_client()
{
    ec = new extent_client();

}

yfs_client::yfs_client(std::string extent_dst, std::string lock_dst)
{
    ec = new extent_client();
    if (ec->put(1, "") != extent_protocol::OK)
        printf("error init root dir\n"); // XYB: init root dir
}

yfs_client::inum
yfs_client::n2i(std::string n)
{
    std::istringstream ist(n);
    unsigned long long finum;
    ist >> finum;
    return finum;
}

std::string
yfs_client::filename(inum inum)
{
    std::ostringstream ost;
    ost << inum;
    return ost.str();
}

int
yfs_client::inodetype(inum inum)
{
    extent_protocol::attr a;

    if (ec->getattr(inum, a) != extent_protocol::OK) {
        //printf("error getting attr\n");
        return extent_protocol::T_FILE;
    }

    return a.type;
}

bool
yfs_client::isfile(inum inum)
{
    extent_protocol::attr a;

    if (ec->getattr(inum, a) != extent_protocol::OK) {
        //printf("error getting attr\n");
        return false;
    }

    if (a.type == extent_protocol::T_FILE) {
        //printf("isfile: %lld is a file\n", inum);
        return true;
    } 
    //printf("isfile: %lld is a dir\n", inum);
    return false;
}
/** Your code here for Lab...
 * You may need to add routines such as
 * readlink, issymlink here to implement symbolic link.
 * 
 * */

bool
yfs_client::isdir(inum inum)
{
    // Oops! is this still correct when you implement symlink?
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        //printf("error getting attr\n");
        return false;
    }

    if (a.type == extent_protocol::T_DIR) {
        //printf("isdir: %lld is a dir\n", inum);
        return true;
    }
    //printf("isdir: %lld is not a dir\n", inum);
    return false;
}

bool
yfs_client::issymlink(inum inum) {
    extent_protocol::attr a;

    if (ec->getattr(inum, a) != extent_protocol::OK) {
        //printf("error getting attr\n");
        return false;
    }

    if (a.type == extent_protocol::T_SYMLINK) {
        //printf("issymlink: %lld is a symlink\n", inum);
        return true;
    }
    //printf("issymlink: %lld is not a symlink\n", inum);
    return false;
}

int
yfs_client::getfile(inum inum, fileinfo &fin)
{
    int r = OK;

    //printf("getfile %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
	return r;
    }

    fin.atime = a.atime;
    fin.mtime = a.mtime;
    fin.ctime = a.ctime;
    fin.size = a.size;
    //printf("getfile %016llx -> sz %llu\n", inum, fin.size);
    return r;
}

int
yfs_client::getdir(inum inum, dirinfo &din)
{
    int r = OK;

    //printf("getdir %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        return r;
    }
    din.atime = a.atime;
    din.mtime = a.mtime;
    din.ctime = a.ctime;

    return r;
}


#define EXT_RPC(xx) do { \
    if ((xx) != extent_protocol::OK) { \
        printf("EXT_RPC Error: %s:%d \n", __FILE__, __LINE__); \
        r = IOERR; \
        goto release; \
    } \
} while (0)

// Only support set size of attr
int
yfs_client::setattr(inum ino, size_t size)
{
    int r = OK;
    std::string buf;

    /*
     * your code goes here.
     * note: get the content of inode ino, and modify its content
     * according to the size (<, =, or >) content length.
     */

    if (ec->get(ino, buf) != extent_protocol::OK) {
        r = IOERR;
        return r;
    }
    buf.resize(size);

    if (ec->put(ino, buf) != extent_protocol::OK)
        r = IOERR;
    return r;
}

int
yfs_client::create(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    int r = OK;

    /*
     * your code goes here.
     * note: lookup is what you need to check if file exist;
     * after create file or dir, you must remember to modify the parent infomation.
     */

    bool found;
    std::string buf;
    struct dirent_n ent;
    // if(lookup(parent, name, found, ino_out) == extent_protocol::OK && found){
    //     r = IOERR;
    //     return r;
    // }
    if (ec->create(extent_protocol::T_FILE, ino_out) != extent_protocol::OK) {
        r = IOERR;
        return r;
    }
    if (ec->get(parent, buf) != extent_protocol::OK) {
        r = IOERR;
        return r;
    }

    ent.inum = ino_out;
    ent.len = strlen(name);
    memcpy(ent.name, name, ent.len);
    buf.append((char *)(&ent), sizeof(dirent_n));

    if (ec->put(parent, buf) != extent_protocol::OK)
        r = IOERR;
    return r;
}

int
yfs_client::mkdir(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    int r = OK;
    bool found;
    std::string buf, append;
    struct dirent_n ent;

    /*
     * your code goes here.
     * note: lookup is what you need to check if directory exist;
     * after create file or dir, you must remember to modify the parent infomation.
     */
    // if(lookup(parent, name, found, ino_out) == extent_protocol::OK && found){
    //     r = IOERR;
    //     return r;
    // }
    if (ec->get(parent, buf) != extent_protocol::OK) {
        r = IOERR;
        return r;
    }
    if (ec->create(extent_protocol::T_DIR, ino_out) != extent_protocol::OK) {
        r = IOERR;
	return r;
    }

    ent.inum = ino_out;
    ent.len = strlen(name);
    memcpy(ent.name, name, ent.len);
    buf.append((char *)(&ent), sizeof(dirent_n));

    if (ec->put(parent, buf) != extent_protocol::OK)
        r = IOERR;
    return r;
}

int
yfs_client::lookup(inum parent, const char *name, bool &found, inum &ino_out)
{
    int r = OK;
    std::list<dirent> entries;
    extent_protocol::attr a;
    std::string string_name;

    /*
     * your code goes here.
     * note: lookup file from parent dir according to name;
     * you should design the format of directory content.
     */
    // if (ec->getattr(parent, a) != extent_protocol::OK) {
    //     r = IOERR;
    //     return r;
    // }
    readdir(parent, entries);
    while (entries.size() != 0) {
        dirent dir_ent = entries.front();
        entries.pop_front();
        if (dir_ent.name == std::string(name)) {
            found = true;
            ino_out = dir_ent.inum;
            return r;
        }
    }

    found = false;
    return r;
}

int
yfs_client::readdir(inum dir, std::list<dirent> &list)
{
    int r = OK;
    std::string buf;
    extent_protocol::attr a;
    const char *ptr;

    /*
     * your code goes here.
     * note: you should parse the dirctory content using your defined format,
     * and push the dirents to the list.
     */
    // if (ec->getattr(dir, a) != extent_protocol::OK) {
    //     r = IOERR;
    //     return r;
    // }

    if (ec->get(dir, buf) != extent_protocol::OK) {
        r = IOERR;
        return r;
    }

    ptr = buf.c_str();
    for (uint32_t i = 0; i < buf.size() / sizeof(dirent_n); i++) {
        struct dirent_n ent_n;
        memcpy(&ent_n, ptr + i * sizeof(struct dirent_n), sizeof(struct dirent_n));

        struct dirent ent;
        ent.inum = ent_n.inum;
        ent.name.assign(ent_n.name, ent_n.len);
        list.push_back(ent);
    }
    return r;
}

int
yfs_client::read(inum ino, size_t size, off_t off, std::string &data)
{
    int r = OK;
    std::string buf;

    /*
     * your code goes here.
     * note: read using ec->get().
     */
    if (ec->get(ino, buf) != extent_protocol::OK) {
        r = IOERR;
        return r;
    }
    if((uint32_t)off >= buf.size())
        return r;

    if(size > buf.size())
        size = buf.size();
    data.assign(buf, off, size);
    return r;
}

int
yfs_client::write(inum ino, size_t size, off_t off, const char *data,
        size_t &bytes_written)
{
    int r = OK;
    std::string buf, temp;

    /*
     * your code goes here.
     * note: write using ec->put().
     * when off > length of original file, fill the holes with '\0'.
     */
    if (ec->get(ino, buf) != extent_protocol::OK) {
        r = IOERR;
        return r;
    }

    temp.assign(data, size);
    if((uint32_t)off <= buf.size())
        bytes_written = size;
    else{
        bytes_written = off + size - buf.size();
        buf.resize(off+size, '\0');
    }
    buf.replace(off, size, temp);

    if (ec->put(ino, buf) != extent_protocol::OK)
        r = IOERR;
    return r;
}

int yfs_client::unlink(inum parent,const char *name)
{
    int r = OK;
    std::list<dirent> ents;
    std::string buf;
    bool found = false;
    inum id;

    /*
     * your code goes here.
     * note: you should remove the file using ec->remove,
     * and update the parent directory content.
     */
    if(lookup(parent, name, found, id) != extent_protocol::OK || !found){
        r = NOENT;
        return r;
    }
    if (ec->remove(id) != extent_protocol::OK) {
        r = IOERR;
        return r;
    }
    if(readdir(parent, ents) != extent_protocol::OK){
        r = IOERR;
        return r;
    }
    for (std::list<dirent>::iterator it = ents.begin(); it != ents.end(); ++it)
        if(it->inum != id){
            dirent_n ent_disk;
            ent_disk.inum = it->inum;
            ent_disk.len = it->name.size();
            memcpy(ent_disk.name, it->name.data(), ent_disk.len);
            buf.append((char *) (&ent_disk), sizeof(struct dirent_n));
        }

    if (ec->put(parent, buf) != extent_protocol::OK) 
        r = IOERR;
    return r;
}

int yfs_client::readlink(inum ino, std::string &buf) 
{
    int r = OK;

    if(ec->get(ino, buf) != extent_protocol::OK)
        r = IOERR;
    return r;
}

int yfs_client::symlink(inum parent, const char *name, const char *link, inum &ino_out) {
    int r = OK;
    bool found;
    inum id;
    struct dirent_n ent;
    std::string buf;

    if(ec->get(parent, buf) != extent_protocol::OK){
        r = IOERR;
        return r;
    }
    // if(lookup(parent, name, found, id) != extent_protocol::OK && found){
    //     r = EXIST;
    //     return r;
    // }
    if(ec->create(extent_protocol::T_SYMLINK, ino_out) != extent_protocol::OK){
        r = IOERR;
        return r;
    }
    if(ec->put(ino_out, std::string(link)) != extent_protocol::OK){
        r = IOERR;
        return r;
    }

    ent.inum = ino_out;
    ent.len = strlen(name);
    memcpy(ent.name, name, ent.len);
    buf.append((char *) (&ent), sizeof(struct dirent_n));

    if(ec->put(parent, buf) != extent_protocol::OK)
        r = IOERR;
    return r;
}

