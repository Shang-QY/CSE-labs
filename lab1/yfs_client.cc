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

bool
yfs_client::isfile(inum inum)
{
    extent_protocol::attr a;

    if (ec->getattr(inum, a) != extent_protocol::OK) {
        printf("error getting attr\n");
        return false;
    }

    if (a.type == extent_protocol::T_FILE) {
        printf("isfile: %lld is a file\n", inum);
        return true;
    } 
    
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
        printf("error getting attr\n");
        return false;
    }

    if (a.type == extent_protocol::T_DIR) {
        printf("isfile: %lld is a file\n", inum);
        return true;
    } 
    
    return false;

}

int
yfs_client::getfile(inum inum, fileinfo &fin)
{
    int r = OK;

    printf("getfile %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }

    fin.atime = a.atime;
    fin.mtime = a.mtime;
    fin.ctime = a.ctime;
    fin.size = a.size;
    printf("getfile %016llx -> sz %llu\n", inum, fin.size);

release:
    return r;
}

int
yfs_client::getdir(inum inum, dirinfo &din)
{
    int r = OK;

    printf("getdir %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }
    din.atime = a.atime;
    din.mtime = a.mtime;
    din.ctime = a.ctime;

release:
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
    printf("setattr %d size to %d\n", ino, size);
    if (ec->get(ino, buf) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }
    buf.resize(size);

    if (ec->put(ino, buf) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }
release:
    return r;
}

int
yfs_client::create(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    int r = OK;
    bool found;
    std::string buf;
    struct dirent_n ent;
    /*
     * your code goes here.
     * note: lookup is what you need to check if file exist;
     * after create file or dir, you must remember to modify the parent infomation.
     */
    
    printf("create in dir %016llx %s\n", parent, name);

    if(lookup(parent, name, found, ino_out) == extent_protocol::OK && found){
        printf("create dup name\n");
        r = IOERR;
        goto release;
    }

    if (ec->create(extent_protocol::T_FILE, ino_out) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }

    if (ec->get(parent, buf) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }

    ent.inum = ino_out;
    ent.len = strlen(name);
    memcpy(ent.name, name, ent.len);
    buf.append((char *)(&ent), sizeof(dirent_n));
    // buf.append(ino_out, 8);
    // buf.append(strlen(name), 8);
    // buf.append(name, strlen(name));

    if (ec->put(parent, buf) != extent_protocol::OK) {
        printf("can't put dir %016llx\n", parent);
        r = IOERR;
        goto release;
    }

    printf("create finished -----------------------\n");
release:
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

    printf("mkdir in dir %016llx %s\n", parent, name);
    
    if(lookup(parent, name, found, ino_out) == extent_protocol::OK && found){
        printf("create dup name\n");
        r = IOERR;
        goto release;
    }

    if (ec->get(parent, buf) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }

    if (ec->create(extent_protocol::T_DIR, ino_out) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }


    ent.inum = ino_out;
    ent.len = strlen(name);
    memcpy(ent.name, name, ent.len);
    buf.append((char *)(&ent), sizeof(dirent_n));
    // buf.append(ino_out, 8);
    // buf.append(strlen(name), 8);
    // buf.append(name, strlen(name));

    if (ec->put(parent, buf) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }

release:
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
    printf("lookup in dir %016llx %s\n", parent, name);
    if (ec->getattr(parent, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }
    assert(a.type == extent_protocol::T_DIR);

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
release:
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
    printf("readdir in dir %016llx\n", dir);
    if (ec->getattr(dir, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }
    assert(a.type == extent_protocol::T_DIR);
    if (ec->get(dir, buf) != extent_protocol::OK) {
        r = IOERR;
        goto release;
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
release:
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
    printf("setattr %d from offset: %d and size: %d\n", ino, off, size);
    if (ec->get(ino, buf) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }

    if((uint32_t)off >= buf.size())
        goto release;

    if(size > buf.size())
        size = buf.size();
    data.assign(buf, off, size);
release:
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
    assert(size);
    printf("write %016llx sz %ld off %ld\n", ino, size, off);
    if (ec->get(ino, buf) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }
    
    temp.assign(data, size);
    if((uint32_t)off <= buf.size())
        bytes_written = size;
    else{
        bytes_written = off + size - buf.size();
        buf.resize(off+size, '\0');
    }
    buf.replace(off, size, temp);
        
    if (ec->put(ino, buf) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }
release:
    return r;
}

int yfs_client::unlink(inum parent,const char *name)
{
    int r = OK;
    bool found;
    inum child;
    std::list<dirent> ents;
    std::string buf;
    /*
     * your code goes here.
     * note: you should remove the file using ec->remove,
     * and update the parent directory content.
     */
    printf("unlink in dir %016llx %s\n", parent, name);
    if(lookup(parent, name, found, child) != extent_protocol::OK || !found){
        printf("file %s is not found\n", name);
        r = IOERR;
        goto release;
    }
    if (ec->remove(child) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }
    if(readdir(parent, ents) != extent_protocol::OK){
        r = IOERR;
        goto release;
    }
    for (std::list<dirent>::iterator it = ents.begin(); it != ents.end(); ++it){
        if(it->inum != child){
            dirent_n ent_disk;
            ent_disk.inum = it->inum;
            ent_disk.len = it->name.size();
            memcpy(ent_disk.name, it->name.data(), ent_disk.len);
            buf.append((char *) (&ent_disk), sizeof(struct dirent_n));
        }
    }
    if (ec->put(parent, buf) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }
release:
    return r;
}

int yfs_client::readlink(inum ino, std::string &buf) 
{
    int r = OK;

    if(ec->get(ino, buf) != extent_protocol::OK){
        r = IOERR;
        goto release;
    }
release:
    return r;
}

int
yfs_client::symlink(inum parent, const char *name, const char *link, inum &ino_out) {
    int r = OK;
    bool found;
    inum id;
    struct dirent_n ent;
    std::string buf;

    if(ec->get(parent, buf) != extent_protocol::OK){
        r = IOERR;
        goto release;
    }
    if(lookup(parent, name, found, id) != extent_protocol::OK && found){
        r = EXIST;
        goto release;
    }

    if(ec->create(extent_protocol::T_SYMLINK, ino_out) != extent_protocol::OK){
        r = IOERR;
        goto release;
    }
    if(ec->put(ino_out, std::string(link)) != extent_protocol::OK){
        r = IOERR;
        goto release;
    }

    ent.inum = ino_out;
    ent.len = strlen(name);
    memcpy(ent.name, name, ent.len);
    buf.append((char *) (&ent), sizeof(struct dirent_n));

    if(ec->put(parent, buf) != extent_protocol::OK){
        r = IOERR;
        goto release;
    }
release:
    return r;
}

