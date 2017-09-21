#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#define SRW_VERSION     "0.1"
#define SRW_DEVICE_NODE  "/dev/spi"
#define OP_DELAY_US     (1000*500) //us.
typedef struct
{
    unsigned char d0;
    unsigned char d1;
    unsigned char d2;
    unsigned char d3;
}ZSPIReg;
typedef struct
{
    unsigned char d0;
    unsigned char d1;
    unsigned char d2;
    unsigned char d3;
}ZSPIData;
//register address define here.
#define ADDR_R_RTC_DATE 0x30000000
#define ADDR_R_RTC_TIME 0x30000004

//global variables defines here.
int gExitFlag=0;
int gVerboseEnabled=0;

void gSigInt(int signo)
{
    if(signo==SIGINT)
    {
        printf("capture SIGINT,exit.\n");
        gExitFlag=1;
    }
}
//basic read/write.
int gDoSPIRead(ZSPIReg reg,ZSPIData *data);
int gDoSPIWrite(ZSPIReg reg,ZSPIData data);
//cmd handler.
int gCmdReadRegister(int addr);
int gCmdWriteRegister(int addr,int value);
int gCmdReadRTCInitialOSTime(void);
int gCmdPrintHelp(char **argv);
int main(int argc,char **argv)
{
    int opCode=-1;
    int regAddr;
    int regValue;
    int ret;
    signal(SIGINT,gSigInt);
    //parse out command line.
    while((ret=getopt(argc,argv,"r:w:ivh"))!=-1)
    {
        switch(ret)
        {
        case 'r':
            //printf("r:%c,%s\n",optopt,optarg);
            opCode=0;
            sscanf(optarg,"%x",&regAddr);
            break;
        case 'w':
            //printf("w:%c,%s\n",optopt,optarg);
            opCode=1;
            sscanf(optarg,"%x=%x",&regAddr,&regValue);
            break;
        case 'i':
            opCode=2;
            break;
        case 'v':
            gVerboseEnabled=1;
            break;
        case 'h':
            //printf("h:%c,%s\n",optopt,optarg);
            opCode=3;
            break;
        default:
            break;
        }
    }
    switch(opCode)
    {
    case 0:
        ret=gCmdReadRegister(regAddr);
        break;
    case 1:
        ret=gCmdWriteRegister(regAddr,regValue);
        break;
    case 2:
        ret=gCmdReadRTCInitialOSTime();
        break;
    case 3:
        ret=gCmdPrintHelp(argv);
        break;
    default:
        ret=-1;
        printf("err:invalid command line!\n");
        break;
    }
    return ret;
}
int gDoSPIRead(ZSPIReg reg,ZSPIData *data)
{
    int fd;
    //open to wake up STM32L431.
    fd=open(SRW_DEVICE_NODE,O_RDWR);
    if(fd<0)
    {
        printf("open dev:%s failed!\n",SRW_DEVICE_NODE);
        return -1;
    }
    usleep(OP_DELAY_US);

    reg.d0&=(~0x80);//the MSB bit=1:write,0:read.
    //tx read register address.
    write(fd,&reg,sizeof(reg));
    if(gVerboseEnabled)
    {
        printf("R1:[%02x %02x %02x %02x]\n",reg.d0,reg.d1,reg.d2,reg.d3);
    }
    usleep(OP_DELAY_US);
    //tx invalid data to generate SCLK.
    reg.d0=0x19;
    reg.d1=0x87;
    reg.d2=0x09;
    reg.d3=0x01;
    read(fd,(char*)&reg,sizeof(reg));
    if(gVerboseEnabled)
    {
        printf("R2:[%02x %02x %02x %02x]\n",reg.d0,reg.d1,reg.d2,reg.d3);
    }
    usleep(OP_DELAY_US);
    //copy reg to data to return.
    data->d0=reg.d0;
    data->d1=reg.d1;
    data->d2=reg.d2;
    data->d3=reg.d3;
    //now STM32 enter stop2 mode.
    close(fd);
    return 0;
}
int gDoSPIWrite(ZSPIReg reg,ZSPIData data)
{
    int fd;
    //open to wake up STM32L431.
    fd=open(SRW_DEVICE_NODE,O_RDWR);
    if(fd<0)
    {
        printf("open dev:%s failed!\n",SRW_DEVICE_NODE);
        return -1;
    }
    usleep(OP_DELAY_US);

    //tx write register address.
    reg.d0|=0x80;
    write(fd,&reg,sizeof(reg));
    if(gVerboseEnabled)
    {
        printf("W1:[%02x %02x %02x %02x]\n",reg.d0,reg.d1,reg.d2,reg.d3);
    }
    usleep(OP_DELAY_US);
    //tx data.
    reg.d0=data.d0;
    reg.d1=data.d1;
    reg.d2=data.d2;
    reg.d3=data.d3;
    write(fd,&reg,sizeof(reg));
    if(gVerboseEnabled)
    {
        printf("W2:[%02x %02x %02x %02x]\n",reg.d0,reg.d1,reg.d2,reg.d3);
    }
    usleep(OP_DELAY_US);

    //now STM32 enter stop2 mode.
    close(fd);
    return 0;
}
int gCmdReadRegister(int addr)
{
    ZSPIReg reg;
    ZSPIData data;
    reg.d0=(unsigned char)((addr>>24)&0xFF);
    reg.d1=(unsigned char)((addr>>16)&0xFF);
    reg.d2=(unsigned char)((addr>>8)&0xFF);
    reg.d3=(unsigned char)((addr>>0)&0xFF);
    if(gDoSPIRead(reg,&data)<0)
    {
        return -1;
    }
    if(gVerboseEnabled)
    {
        printf("Read Back:%02x%02x%02x%02x\n",data.d0,data.d1,data.d2,data.d3);
    }
    return 0;
}
int gCmdWriteRegister(int addr,int value)
{
    ZSPIReg reg;
    ZSPIData data;
    reg.d0=(unsigned char)((addr>>24)&0xFF);
    reg.d1=(unsigned char)((addr>>16)&0xFF);
    reg.d2=(unsigned char)((addr>>8)&0xFF);
    reg.d3=(unsigned char)((addr>>0)&0xFF);
    data.d0=(unsigned char)((value>>24)&0xFF);
    data.d1=(unsigned char)((value>>16)&0xFF);
    data.d2=(unsigned char)((value>>8)&0xFF);
    data.d3=(unsigned char)((value>>0)&0xFF);
    if(gDoSPIWrite(reg,data)<0)
    {
        return -1;
    }
    return 0;
}
int gCmdReadRTCInitialOSTime(void)
{
    int regAddr;
    ZSPIReg reg;
    ZSPIData data;
    char tTmpBuffer[64];
    int year,month,day,hour,minute,second;
    time_t tTimeT;
    struct tm tTM;
    struct timeval tTV;

    //read Date.
    regAddr=ADDR_R_RTC_DATE;
    reg.d0=(unsigned char)((regAddr>>24)&0xFF);
    reg.d1=(unsigned char)((regAddr>>16)&0xFF);
    reg.d2=(unsigned char)((regAddr>>8)&0xFF);
    reg.d3=(unsigned char)((regAddr>>0)&0xFF);
    if(gDoSPIRead(reg,&data)<0)
    {
        return -1;
    }
    year=data.d1;
    month=data.d2;
    day=data.d3;
    if(gVerboseEnabled)
    {
        printf("Get Date:%02x%02x%02x%02x\n",data.d0,data.d1,data.d2,data.d3);
    }
    //read Time.
    regAddr=ADDR_R_RTC_TIME;
    reg.d0=(unsigned char)((regAddr>>24)&0xFF);
    reg.d1=(unsigned char)((regAddr>>16)&0xFF);
    reg.d2=(unsigned char)((regAddr>>8)&0xFF);
    reg.d3=(unsigned char)((regAddr>>0)&0xFF);
    if(gDoSPIRead(reg,&data)<0)
    {
        return -1;
    }
    hour=data.d1;
    minute=data.d2;
    second=data.d3;
    if(gVerboseEnabled)
    {
        printf("Get Time:%02x%02x%02x%02x\n",data.d0,data.d1,data.d2,data.d3);
    }

    //BCD format.
    sprintf(tTmpBuffer,"20%02x-%02x-%02x %02x:%02x:%02x",year,month,day,hour,minute,second);
    if(gVerboseEnabled)
    {
        printf("1.%s\n",tTmpBuffer);
    }
    sscanf(tTmpBuffer,"%d-%d-%d %d:%d:%d",&year,&month,&day,&hour,&minute,&second);
    if(gVerboseEnabled)
    {
        printf("2.%d-%d-%d %d:%d:%d\n",year,month,day,hour,minute,second);
    }

    tTM.tm_year=year-1900;
    tTM.tm_mon=month-1;
    tTM.tm_mday=day;
    tTM.tm_hour=hour;
    tTM.tm_min=minute;
    tTM.tm_sec=second;
    tTM.tm_isdst=-1;
    //build time_t.
    if((tTimeT=mktime(&tTM))==((time_t)-1))
    {
        if(gVerboseEnabled)
        {
            printf("mktime failed!\n");
        }
       return -1;
    }
    //build struct timeval.
    tTV.tv_sec=tTimeT;
    tTV.tv_usec=0;
    if(settimeofday(&tTV,(struct timezone*)0)<0)
    {
        if(gVerboseEnabled)
        {
            printf("setOSTime failed!\n");
        }
        return -1;
    }
    if(gVerboseEnabled)
    {
        printf("setOSTime okay:%d-%d-%d %d:%d:%d\n",year,month,day,hour,minute,second);
    }
    return 0;
}
int gCmdPrintHelp(char **argv)
{
    printf("SRW(Security Read/Write) tool binary V%s\n",SRW_VERSION);
    printf("Build on %s %s\n",__DATE__,__TIME__);
    printf("Usage:%s [options]\n",argv[0]);
    printf("-r 0xXXXXXXXX:            read from register\n");
    printf("-w 0xXXXXXXXX=0xXXXXXXXX: write value to register\n");
    printf("-i :                      read RTC and initial OS time\n");
    printf("-v :                      show verbose debug messages\n");
    printf("-h :                      show this help tips\n");
    printf("Bugs report to shell.albert@gmail.com\n");
    printf("Copyright(C) Oristar R&D Insititute,2017~2027.\n");
    return 0;
}
//the end of file.
