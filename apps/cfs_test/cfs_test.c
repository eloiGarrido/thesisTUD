#include "../cfs/cfs.h" 
#include "../contiki.h"
#include <stdio.h>
#include <string.h>

#define MESSAGE_SIZE 30
void clean_message(char message[MESSAGE_SIZE])
{
  uint8_t idx;
  for (idx=0; idx<MESSAGE_SIZE; idx++){message[idx] = '\0';}
}


int read_lines(int fd_read, char message[MESSAGE_SIZE], uint16_t lines)
{
  
  char char_buffer[2] = "\0\0";
  char string_buffer[MESSAGE_SIZE];
  uint8_t idx=0;
  uint16_t lines_read = 0;
  int bytes_read = 0;
  clean_message(string_buffer);
  clean_message(message);
  if(fd_read!=-1) 
  {
    do
    {
      bytes_read = cfs_read(fd_read, char_buffer, sizeof(char));
      if (bytes_read > 0) 
      {
        string_buffer[idx] = char_buffer[0];
        idx += 1;
        if ( char_buffer[0] == '\n')
        {
          lines_read += 1;
        }
      }
      else
      {
        return 0;
      }
    }while (lines_read < lines);
    strncpy(message, string_buffer, idx);
    // cfs_close(fd_read);
  } 
  else 
  {
    printf("ERROR: read_lines.\n");
  }
  return bytes_read;
}


PROCESS(coffee_test_process, "Coffee test process");
AUTOSTART_PROCESSES(&coffee_test_process);
PROCESS_THREAD(coffee_test_process, ev, data)
{
  PROCESS_BEGIN();

  char message[MESSAGE_SIZE];
  char buf[100];
  printf("step 1: %s\n", buf );
  int fd_read;
  int bytes_read = 0;
  int idx = 0;

  fd_read = cfs_open("cfs_file.txt", CFS_READ);
  if(fd_read!=-1) 
  {
    for ( idx = 0; idx < 6; idx++ )
    {
      bytes_read = read_lines(fd_read, message, 1 );
      if (bytes_read > 0)
      {
        printf("step %d: %s\n",idx, message);
      }
      else
      {
        printf("Error, 0 bytes read\n");
        cfs_seek(fd_read, 0, CFS_SEEK_SET);
        idx--;
      }
    }
    cfs_close(fd_read);
  } 
  else 
  {
    printf("ERROR: could not read from memory in step 3.\n");
  }

  PROCESS_END();
}