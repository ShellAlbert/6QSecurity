#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
void gLEDOn(int on)
{
  FILE *fp;
  int i;
  char cmdBuffer[128];
  if(on)
  { 
    for(i=0;i<4;i++)
    { 
    	sprintf(cmdBuffer,"/bin/echo -n %d1 > /dev/led_sr",i+1);
	fp=popen(cmdBuffer,"r");
        if(fp)
	{
		pclose(fp);
	}
    }
  }else{
    for(i=0;i<4;i++)
    {
	sprintf(cmdBuffer,"/bin/echo -n %d0 > /dev/led_sr",i+1);
	fp=popen(cmdBuffer,"r");
        if(fp)
	{
		pclose(fp);
	}
    }
  }
}
void gSigHandler(int signo)
{
 FILE *fp0=NULL;
 FILE *fp1=NULL;
 FILE *fp2=NULL;
 int i;
 printf("6Q soft-reset:%d\n",signo);
 fp0=popen("/bin/rm -rf /dev/led ; /bin/mknod /dev/led_sr c 1987 1","r");
 if(fp0)
 {
   pclose(fp0);
 }
 for(i=0;i<10;i++)
 {
   gLEDOn(0);
   usleep(1000*100);
   gLEDOn(1);
   usleep(1000*100);
 }
 fp1=popen("/bin/rm -rf /home/CEAMS/6QWeb/cgi-bin/ip.manual","r");
 if(fp1)
 {
    pclose(fp1);
 }
 fp2=popen("/bin/sync;/sbin/reboot","r");
 if(fp2)
 {
   pclose(fp2);
 }
}
int main(void)
{
  int flags;
  int fd=open("/dev/sr",O_RDWR);
  int fd2;
  char buffer[32];
  pid_t pid;
  if(fd<0)
  {
   printf("failed to open /dev/sr!");
   return -1;
  }
  signal(SIGIO,gSigHandler);
  fcntl(fd,F_SETOWN,getpid());
  flags=fcntl(fd,F_GETFL);
  fcntl(fd,F_SETFL,flags|FASYNC); 
  //write pid to file.
  pid=getpid();
  sprintf(buffer,"%d",pid);
  fd2=open("/tmp/srapp.pid",O_RDWR|O_CREAT|O_TRUNC);
  if(fd2<0)
  {
     printf("failed to open /tmp/srapp.pid\n");
     return -1;
  }
  write(fd2,buffer,strlen(buffer));
  close(fd2);	
  while(1)
  {
    sleep(100);
  }
  return 0;
}
