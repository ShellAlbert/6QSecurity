#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
int main(void)
{
 int fd=open("/dev/led",O_RDWR);
 if(fd<0)
 {
  return -1;
 }
 write(fd,"20",2);
 close(fd);
 return 0;
}
