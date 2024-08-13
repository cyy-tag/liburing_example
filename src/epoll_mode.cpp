#include <liburing.h>
#include <sys/eventfd.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "file_verify.h"

int main() {
    const char *filename = "test.txt";

    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("Failed to open file");
        return 1;
    }

    struct io_uring ring;
    if (io_uring_queue_init(1024, &ring, 0) < 0) {
        perror("Failed to initialize io_uring queue");
        close(fd);
        return 1;
    }
    
    int block_size = 64;
    int write_cnt = 64;
    char* data = new char[block_size * write_cnt];

    for(int i = 0; i < block_size*write_cnt; ++i) {
        data[i] = rand() % 26 + 'a';
    }

    //register eventfd
    int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    io_uring_register_eventfd(&ring, evtfd);
    int epfd = epoll_create1(0);
    struct epoll_event event = {0};
    event.data.fd = evtfd;
    event.events = EPOLLIN | EPOLLET;

    if (epoll_ctl(epfd, EPOLL_CTL_ADD, evtfd, &event) == -1) {
        perror("epoll_ctl");
        exit(EXIT_FAILURE);
    }

    off_t write_off = 0;
    for(int i = 0; i < write_cnt; ++i) {
      struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
      io_uring_prep_write(sqe, fd, data+write_off, block_size, write_off);
        
      if (io_uring_submit(&ring) < 0) {
          perror("Failed to submit IO request");
          io_uring_queue_exit(&ring);
          close(fd);
          return 1;
      }
      write_off += block_size;
    }

    int cnt = 0;
    while(true) {
      int n = epoll_wait(epfd, &event, 1, -1);
      if (n == -1) {
          perror("epoll_wait");
          break;
      }
      struct io_uring_cqe * cqe{nullptr};
      while (io_uring_peek_cqe(&ring, &cqe) == 0) {
          if (cqe->res < 0) {
              fprintf(stderr, "IO error: %s\n", strerror(-cqe->res));
          } else {
              printf("Write successful, bytes written: %d\n", cqe->res);
          }
          io_uring_cq_advance(&ring, 1);
          cnt++;
      }
      //complete quit
      if(cnt == write_cnt) {
        break;
      }
    }

    //verify data
    if(int ret = VerifyFile(filename, data, block_size * write_cnt); ret != 0) {
        fprintf(stderr, "verify file data err %d\n", ret);
    }
    io_uring_queue_exit(&ring);
    close(fd);

    return 0;
}
