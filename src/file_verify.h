#pragma once
#include <sys/types.h>
#include <unistd.h> //close
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <string_view>

bool VerifyFile(const char* file_path, char* target_data, size_t size)
{
  int fd = -1;
  if ( (fd = open(file_path, O_RDWR)) < 0) {
    return -1;
  }
  struct stat st;
  if(fstat(fd, &st) == -1) {
    close(fd);
    return -1;
  }
  char *buf = nullptr;
  if((buf = (char*)mmap(nullptr, st.st_size, PROT_READ, MAP_PRIVATE|MAP_POPULATE, fd, 0)) == (void*)-1) {
    close(fd);
    return -2;
  }
  if(size != (size_t)st.st_size) {
    return -3;
  }
  return memcmp(target_data, buf, size);
}
