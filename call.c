#include "call.h"
const char* HD = "HD";

inode* read_inode(int fd, int inode_number) {
  inode* ip = malloc(sizeof(inode));
  lseek(fd, I_OFFSET + inode_number * sizeof(inode), SEEK_SET);
  read(fd, ip, sizeof(inode));
  return ip;
}
void* read_block(int fd, int block_number) {
  void* block = malloc(BLK_SIZE);
  lseek(fd, D_OFFSET + block_number * BLK_SIZE, SEEK_SET);
  read(fd, block, BLK_SIZE);
  return block;
}

int open_t(char* pathname) {
  int fd = open(HD, O_RDONLY);
  if (fd < 0)
    return -1;

  char* token = strtok(pathname, "/");
  int inode_number = 0; // Start from root directory

  while (token != NULL) {
    inode* ip = read_inode(fd, inode_number);
    if (ip->f_type != DIR) {
      close(fd);
      return -1; // Not a directory
    }

    int found = 0;
    for (int i = 0; i < ip->blk_number; i++) {
      DIR_NODE* block;
      if (i >= 2) {
        int* indirect_block = read_block(fd, ip->indirect_blk);
        block = read_block(fd, indirect_block[i - 2]);
        free(indirect_block);
      } else {
        block = read_block(fd, ip->direct_blk[i]);
      }
      for (int j = 0; j < BLK_SIZE / sizeof(DIR_NODE); j++) {
        if (strcmp(block[j].f_name, token) == 0) {
          inode_number = block[j].i_number;
          found = 1;
          break;
        }
      }
      free(block);
      if (found)
        break;
    }
    free(ip);
    if (!found) {
      close(fd);
      return -1; // File not found
    }
    token = strtok(NULL, "/");
  }

  close(fd);
  return inode_number;
}

int read_t(int i_number, int offset, void* buf, int count) {
  // printf("buf: %p    ", buf);
  // printf("i_number: %d, offset: %d, count: %d\n", i_number, offset, count);
  int fd = open(HD, O_RDONLY);
  if (fd < 0)
    return -1;

  inode* ip = read_inode(fd, i_number);
  if (ip->f_type != FILE) {
    close(fd);
    return -1; // Not a file
  }
  int file_size = ip->f_size;
  if (offset >= file_size) {
    free(ip);
    close(fd);
    return 0; // Offset is greater than file size
  }
  if (offset + count > file_size)
    count = file_size - offset; // Read only available bytes

  int start_block = offset / BLK_SIZE;
  int end_block = (offset + count) / BLK_SIZE;
  if ((offset + count) % BLK_SIZE > 0)
    end_block++;

  // printf("start_block: %d, end_block: %d\n", start_block, end_block);
  int read_bytes = 0;
  for (int i = start_block; i < end_block; i++) {
    void* block;
    if (i >= 2) { // Indirect block
      int* indirect_block = read_block(fd, ip->indirect_blk);
      block = read_block(fd, indirect_block[i - 2]);
      free(indirect_block);
    } else { // Direct block
      block = read_block(fd, ip->direct_blk[i]);
    }
    int block_offset = (i == start_block) ? offset % BLK_SIZE : 0;
    int bytes_to_read;
    if (start_block == end_block - 1) {
      bytes_to_read = count;
    } else if (i == end_block - 1) {
      bytes_to_read = (offset + count) % BLK_SIZE;
    } else {
      bytes_to_read = BLK_SIZE - block_offset;
    }
    memcpy(buf + read_bytes, block + block_offset, bytes_to_read);
    read_bytes += bytes_to_read;
    free(block);
  }

  free(ip);
  close(fd);
  return read_bytes;
}

// you are allowed to create any auxiliary functions that can help your
// implementation. But only "open_t()" and "read_t()" are allowed to call these
// auxiliary functions.