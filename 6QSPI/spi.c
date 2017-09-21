/*filename:spi.c
 *function:platform driver implementation for ecspi-1 soc
 *ver:1.0
 *email:shell.albert@gmail.com
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/wait.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/delay.h>

#define SPI_MAJOR  1988
#define SPI_MINOR   1

#define IOMUXC_SW_MUX_CTL_PAD_EIM_LBA_B 0x20E0108//ECSPI2_SS1
#define IOMUXC_SW_MUX_CTL_PAD_EIM_OE_B  0x20E0100//ECSPI2_MISO
#define IOMUXC_SW_MUX_CTL_PAD_EIM_CS1_B 0x20E00FC//ECSPI2_MOSI
#define IOMUXC_SW_MUX_CTL_PAD_EIM_CS0_B 0x20E00F8//ECSPI2_SCLK

//STM_RST 6Q->STM32 used to wake up STM32L433 from Stop2Mode.
#define IOMUXC_SW_MUX_CTL_PAD_EIM_EB2_B	0x20E008C

#define ECSPI2_RXDATA       0x200C000
#define ECSPI2_TXDATA       0x200C004
#define ECSPI2_CONREG       0x200C008
#define ECSPI2_CONFIGREG    0x200C00C
#define ECSPI2_INTREG       0x200C010
#define ECSPI2_DMAREG       0x200C014
#define ECSPI2_STATREG      0x200C018
#define ECSPI2_PERIODREG    0x200C01C

#define CCM_CCGR1   0x20C406C

#define GPIO2_DR	0x20A0000
#define GPIO2_GDIR	0x20A0004



#define ECSPI2_BASE     (ECSPI2_RXDATA)
#define ECSPI2_SIZE     (4*8)

#define NULL_POINTER	printk("error,file:%s,line:%d,function:%s,null pointer\n",__FILE__,__LINE__,__func__)
/**
 * ecspi device struction
 * contains all necessary resources here.
 */
typedef unsigned long ulong;
typedef struct
{
	volatile ulong*pECSPI2;
	dev_t devno;/*the device number*/
	struct cdev chardev;/*char device*/
	struct fasync_struct *fasync_queue;/*fasync notice*/
	spinlock_t lock_spi;
}ZSPIDev;

/**
 * file system interface,for ioctl() call.
 */
typedef struct
{
	unsigned char d0;
	unsigned char d1;
	unsigned char d2;
	unsigned char d3;
}ZSPIReg;


ZSPIDev *gSPIDev=NULL;
/******************************platform device here********************************/
/**
 * device release function
 * do some rollback work.
 */
	static void
spi_device_release (struct device *pdev)
{
	/**
	 * nothing to do here
	 */
	return;
}

/**
 * platform device resource.
 * define physical address & IRQ resouce here,
 * but it does not used in driver codes.
 * physical address was hard coded for easy coding.
 */
static struct resource spi_resources[]=
{
	[0]=
	{ ///<
		.start=(0x1054),/*ecspi-1:for register operate*/
		.end=(0x1068), ///<
		.flags=IORESOURCE_MEM,///<
	},
	[1]=
	{
		.start=227,/*FPGA_INT1,GPIO4_GPIO[3]=(4-1)*32+3+128*/
		.end=227,
		.flags=IORESOURCE_IRQ,
	},
};

/**
 * platform device
 */
static struct platform_device spi_device=
{
	.name="6QSPI",
	.id=-1,
	.resource=spi_resources,
	.num_resources=ARRAY_SIZE(spi_resources),
	.dev=
	{
		.release=spi_device_release,
	},
};

/*************************platform driver here******************************/
/**
 * define global board resource structure pointer.
 * only a pointer here !!!
 * will be memory allocated in initial functions.
 */
ZSPIDev *gSPIDev;

/**
 *mapping physical to virtual address
 *the global imx5_ecspi1 will hold the virtual address pointer
 *return value: 0 success,-1 failed
 */
	int 
spi_mapping (ZSPIDev*p)
{
	p->pECSPI2 = (ulong*) ioremap (ECSPI2_BASE,ECSPI2_SIZE);
	if (!p->pECSPI2)
	{
		NULL_POINTER;
		return -1;
	}
	return 0;
}
int gpio_set_stm_rst_pin(unsigned char level)
{
	volatile ulong *pbase;
	unsigned int ret;
        //high level:1.
        pbase = (ulong*) ioremap (GPIO2_DR, 0x4);
        if (!pbase)
        {
                NULL_POINTER;
                return -1;
        }
        ret = ioread32 (pbase);
	if(!level)
	{
		ret &= (~(0x1<<30));//[30]=0,low level.
	}else{
	        ret |= (0x1 << 30);//[30]=1,high level.
	}
        iowrite32 (ret, pbase);
        iounmap (pbase);
	return 0;
}
/*iomuxc change mux mode
 *pad connected to ecspi blocks
 *return value:0 success,-1 failed
 */
	int
spi_init_iomuxc (void)
{
	volatile ulong *pbase;
	unsigned int ret;
	//#define IOMUXC_SW_MUX_CTL_PAD_EIM_LBA_B 0x20E0108//ECSPI2_SS1
	pbase = (ulong*) ioremap (IOMUXC_SW_MUX_CTL_PAD_EIM_LBA_B, 0x4);
	if (!pbase)
	{
		NULL_POINTER;
		return -1;
	}
	ret = ioread32 (pbase);
	/*[4]=0,input path determined by selected mux mode*/
	ret &= (~(0x1 << 4));
	/*[2:0]=010,ALT2,ECSPI2_SS1*/
	ret &= (~(0x1 << 2));
	ret |= (0x1 << 1);
	ret &= (~(0x1 << 0));
	iowrite32 (ret, pbase);
	iounmap (pbase);
	//#define IOMUXC_SW_MUX_CTL_PAD_EIM_OE_B  0x20E0100//ECSPI2_MISO
	pbase = (ulong*) ioremap (IOMUXC_SW_MUX_CTL_PAD_EIM_OE_B,0x4);
	if (!pbase)
	{
		NULL_POINTER;
		return -1;
	}
	ret = ioread32 (pbase);
	/*[4]=0,input path determined by selected mux mode*/
	ret &= (~(0x1 << 4));
	/*[2:0]=010,ALT2,ECSPI2_MISO*/
	ret &= (~(0x1 << 2));
	ret |= (0x1 << 1);
	ret &= (~(0x1 << 0));
	iowrite32 (ret, pbase);
	iounmap (pbase);


	//#define IOMUXC_SW_MUX_CTL_PAD_EIM_CS1_B 0x20E00FC//ECSPI2_MOSI
	pbase = (ulong*) ioremap (IOMUXC_SW_MUX_CTL_PAD_EIM_CS1_B, 0x4);
	if (!pbase)
	{
		NULL_POINTER;
		return -1;
	}
	ret = ioread32 (pbase);
	/*[4]=0,input path determined by selected mux mode*/
	ret &= (~(0x1 << 4));
	/*[2:0]=010,ALT2,ECSPI2_MOSI*/
	ret &= (~(0x1 << 2));
	ret |= (0x1 << 1);
	ret &= (~(0x1 << 0));
	iowrite32 (ret, pbase);
	iounmap (pbase);

	//#define IOMUXC_SW_MUX_CTL_PAD_EIM_CS0_B 0x20E00F8//ECSPI2_SCLK
	pbase = (ulong*) ioremap (IOMUXC_SW_MUX_CTL_PAD_EIM_CS0_B, 0x4);
	if (!pbase)
	{
		NULL_POINTER;
		return -1;
	}
	ret = ioread32 (pbase);
	/*[4]=0,input path determined by selected mux mode*/
	ret &= (~(0x1 << 4));
	/*[2:0]=010,ALT2,ECSPI2_SCLK*/
	ret &= (~(0x1 << 2));
	ret |= (0x1 << 1);
	ret &= (~(0x1 << 0));
	iowrite32 (ret, pbase);
	iounmap (pbase);

	//#define IOMUXC_SW_MUX_CTL_PAD_EIM_EB2_B	0x20E008C
	pbase = (ulong*) ioremap (IOMUXC_SW_MUX_CTL_PAD_EIM_EB2_B, 0x4);
	if (!pbase)
	{
		NULL_POINTER;
		return -1;
	}
	ret = ioread32 (pbase);
	/*[4]=0,input path determined by selected mux mode*/
	ret &= (~(0x1 << 4));
	/*[2:0]=101,ALT5,GPIO2[30]*/
	ret |= (0x1 << 2);
	ret &= (~(0x1 << 1));
	ret |= (0x1 << 0);
	iowrite32 (ret, pbase);
	iounmap (pbase);

	//#define GPIO2_GDIR	0x20A0004
	pbase = (ulong*) ioremap (GPIO2_GDIR, 0x4);
	if (!pbase)
	{
		NULL_POINTER;
		return -1;
	}
	ret = ioread32 (pbase);
	ret |= (0x1 << 30);//[30]=1,output.
	iowrite32 (ret, pbase);
	iounmap (pbase);
	//high level:1.
	gpio_set_stm_rst_pin(1);

	/*success*/
	return 0;
}

/**
 *initial ecspi block
 *init flow according to the mx53 datasheet
 *must call imx5_ecspi1_mapping() before this function
 *return value:0 success,-1 failed
 */
	int
spi_init (ZSPIDev *p)
{
	int ret;
	volatile ulong*pbase;
	/*************************ECSPI1_CONREG*****************************************/
	/*1.clear the EN bit in ECSPI_CONREG to reset the block*/
	ret = ioread32 (p->pECSPI2 + 0x2);/*ECSPI2_CONREG*/
	/*[0]=0,disable the block*/
	ret &= (~(0x1 << 0));
	iowrite32 (ret, p->pECSPI2 + 0x2);/*ECSPI2_CONREG*/

	/*2.enable the clocks for ECSPI2*/
	pbase = (ulong*) ioremap (CCM_CCGR1, 0x4);
	if(!pbase)
	{
		NULL_POINTER;
		return -1;
	}
	ret = ioread32 (pbase);
	/*[3:2]=11,ecspi2_clk_enable*/
	ret |= (0x1 << 3);
	ret |= (0x1 << 2);
	iowrite32 (ret, pbase);
	iounmap (pbase);

	/*3.set EN bit in ECSPI_CONREG to put ECSPI out of reset*/
	ret = ioread32 (p->pECSPI2 + 0x2);/*ECSPI2_CONREG*/
	/*[0]=1,enable the block*/
	ret |= (0x1 << 0);
	iowrite32 (ret, p->pECSPI2 + 0x2);/*ECSPI2_CONREG*/

	/*4.configure corresponding IOMUXC for ECSPI external signals
	 */
	ret = spi_init_iomuxc ();
	if (ret)
	{
		return -1;
	}

	/*5.configure registers of ECSPI properly according to the specifications of the external SPI devices*/
	ret = ioread32 (p->pECSPI2 + 0x2);/*ECSPI2_CONREG*/
	/*[31:20]=0x007,burst length,a SPI burst contains 8LSB in a word*/
	ret &= (~(0x1 << 31));
	ret &= (~(0x1 << 30));
	ret &= (~(0x1 << 29));
	ret &= (~(0x1 << 28));

	ret &= (~(0x1 << 27));
	ret &= (~(0x1 << 26));
	ret &= (~(0x1 << 25));
	ret &= (~(0x1 << 24));

	ret &= (~(0x1 << 23));
	ret |= (0x1 << 22);
	ret |= (0x1 << 21);
	ret |= (0x1 << 20);

	/*[19:18]=01,channel select,SS1*/
	ret &= (~(0x1 << 19));
	ret |= (0x1 << 18);

	/*[17:16]=00,SPI_RDY signal is a don't care*/
	ret &= (~(0x1 << 17));
	ret &= (~(0x1 << 16));

	/*ECSPI uses a two-stage divider to generate the SPI clock*/
	/*[15:12]=1111,60MHz/16=3.9MHz,SPI Pre Divider*/
	ret |= (0x1 << 15);
	ret |= (0x1 << 14);
	ret |= (0x1 << 13);
	ret |= (0x1 << 12);

	/*[11:8]=0001,divide by 2,SPI Post Divider*/
	ret &= (~(0x1 << 11));
	ret &= (~(0x1 << 10));
	ret &= (~(0x1 << 9));
	ret |= (0x1 << 8);

	/*[7:4]=xx1x,SPI Channel Mode,SS1=Master Mode*/
	ret &= (~(0x1 << 7));
	ret &= (~(0x1 << 6));
	ret |= (0x1 << 5);
	ret &= (~(0x1 << 4));

	/*[3]=0,Start Mode Control,set XCH to start*/
	ret &= (~(0x1 << 3));
	/**
	 * bit[3]==1,immediately start SPI transfer,
	 * when data is written into TXFIFO.
	 * modified by zhangshaoyan at August 1,2014.
	 */
	//ret |= (0x1 << 3);

	/*[1]=0,disable hardware triggered mode*/
	ret &= (~(0x1 << 1));

	iowrite32 (ret, p->pECSPI2 + 0x2);/*ECSPI1_CONREG*/

	/********************ECSPI2_CONFIGREG*****************/
	ret = ioread32 (p->pECSPI2 + 0x3);/*ECSPI2_CONFIGREG*/
	/*[23:20]=xx1x,inactive state of SCLK,SPI channel1 = 1 stay high*/
	ret |= (0x1 << 21);

	/*[19:16]=xx1x,inactive state of data line,spi channel=1 stay high*/
	/*error datasheet here,the bit should be zero for stay high*/
	ret &= (~(0x1 << 17));

	/*[15:12]=xx0x,chip select,0=acitve low*/
	ret &= (~(0x1 << 13));

	/*[11:8]=xx1x,Negate Chip Select(SS) signal between SPI bursts*/
	/*multiple SPI bursts will be transmitted*/
	/*the SPI transfer will automatically stop when the TxFIFO is empty*/
	ret |= (0x1 << 9);

	/*[7:4]=xxx1,SCLK polarity,(Active low polarity(1=idle)*/
	ret |= (0x1 << 5);

	/*[3:0]=xx0x,spi channel 1,phase 1 operation,sample data at second edge of sclk*/
	ret |= (0x1 << 1);

	iowrite32 (ret, p->pECSPI2 + 0x3);/*ECSPI1_CONFIGREG*/

	/*success*/
	return 0;
}


/**
 *call this function to send data
 *MOSI:32-bit address,32-bit valid data
 *MISO: do not care
 *return value: 0 success,-1 failed
 *attention:because MOSI&MISO will work sync,so when we send 2 data out,will lock 2 data in
 *so we should read 2 data from RxFIFO and trash this 2 data
 */
	int
spi_txrx(ZSPIDev *p, unsigned int *txBuffer, unsigned int txLen,unsigned int *rxCache,unsigned int rxLen)
{
	int ret;
	unsigned int index_x, index_y;

	/**
	 * check valid of parameters.
	 * buffer_len must less than 64.
	 */
	if (txBuffer == NULL || txLen <= 0 || txLen > 64 || rxCache==NULL || rxLen!=txLen)
	{
		printk("spi_txrx:invalid parameters.quit!\n");
		return -1;
	}
	//printk("txLen:%d,rxLen:%d\n",txLen,rxLen);
	/**
	 * only one signal can pass through SPI at once time.
	 * added by zhangshaoyan,2014/7/28,for mutex solve. 
	 */
	//spin_lock_irqsave (&p->lock_spi, flags);

	/**
	 * step1:
	 * write data into TXFIFO at once when TXFIFO is not full.
	 * 64 numbers at once time maximum because the size of TXFIFO is 64x32.
	 */
	for (index_x = 0; index_x < txLen; index_x++)
	{
		/**
		 * write data into TXFIFO at once when TXFIFO is not full.
		 */
		while (1)
		{
			ret = ioread32 (p->pECSPI2 + 0x6);/*ECSPI1_STATREG*/
			/**
			 * bit[2]==0,TXFIFO is not full.
			 */
			if (!(ret & (0x1 << 2)))
			{
				break;
			}
		}
		iowrite32 (txBuffer[index_x], p->pECSPI2 + 0x1);/*ECSPI1_TXDATA*/
		//printk("Tx: %02x\n",txBuffer[index_x]);
	}

	//printk("set start\n");
	/**
	 * step2:
	 * set SPI Exchange Bit in CONREG to start transfer
	 */
	ret = ioread32 (p->pECSPI2 + 0x2);/*ECSPI1_CONREG*/
	ret |= (0x1 << 2);/*start burst*/
	iowrite32 (ret, p->pECSPI2 + 0x2);/*ECSPI1_CONREG*/

	/**
	 * step3:
	 * poll&wait for tx finish.
	 * next version here will be changed into interrupt method.
	 */
	//printk("wait for tx finish..\n");
	while (1)
	{
		ret = ioread32 (p->pECSPI2 + 0x2);/*ECSPI1_CONREG*/
		/**
		 * bit[2]==0,idle.
		 */
		if (!(ret & (0x1 << 2)))
		{
			break;
		}
	}

	/**
	 * step4:
	 * trash data from RxFIFO that we do not care.
	 * because SPI is synchronous, data will be locked in while data is being send out.
	 */
	for (index_y = 0; index_y < rxLen; index_y++)
	{
	    while (1)
	    {
		ret = ioread32 (p->pECSPI2 + 0x6);/*ECSPI1_STATREG*/
		/**
		 * bit[3]==1,more than 1 words in RxFIFO.
		 */
		if (ret & (0x1 << 3))
		{
			ret = ioread32 (p->pECSPI2 + 0x0);/*ECSPI1_RXDATA*/
			rxCache[index_y]=ret;
			//printk("Rx %02x\n",ret);
			break;
		}
	    } 
	}

	/**
	 * spi communication finish here.
	 */

	/**
	 * only one signal can pass through SPI at once time.
	 * added by zhangshaoyan,2014/7/28,for mutex solve. 
	 */
	//spin_unlock_irqrestore (&p->lock_spi, flags);
	msleep(1);
	return 0;
}


/**
 *char device interface:open()
 */
	int
spi_driver_open (struct inode*inode, struct file*file)
{
	/*hold the global pointer*/
	ZSPIDev *p;
	p = container_of (inode->i_cdev, ZSPIDev, chardev);
	file->private_data = p;

	//generate rising edge to wakeup STM32.1-0-1.
	gpio_set_stm_rst_pin(0);
	msleep(1);
	gpio_set_stm_rst_pin(1);
	/*success*/
	return 0;
}

/**
 *char device interface:ioctl()
 */
	int
spi_driver_ioctl (struct inode*inode, struct file*file, unsigned int cmd, unsigned long arg)
{
	/**
	 * do not know how to write here.
	 *  keep it empty.
	 */
	return 0;
}

/**
 *char device interface:read()
 */
	int
spi_driver_read (struct file*file, char *buffer, size_t size, loff_t *pos)
{
	int ret;
	ZSPIDev *spiDev;
	ZSPIReg tmp;
	unsigned int txBuffer[4];
	unsigned int rxCache[4];
	spiDev= file->private_data;

	/**
	 * normal operatation
	 */
	ret = copy_from_user (&tmp, (ZSPIReg*) buffer, sizeof(ZSPIReg));
	if (ret)
	{
		printk ("ecspi-1:copy_from_user() at read() failed:%d\n", ret);
		return -1;
	}

        txBuffer[0] = tmp.d0;
        txBuffer[1] = tmp.d1;
        txBuffer[2] = tmp.d2;
        txBuffer[3] = tmp.d3;
        ret = spi_txrx(spiDev,txBuffer,sizeof(txBuffer)/sizeof(txBuffer[0]),rxCache,sizeof(rxCache)/sizeof(rxCache[0]));
	tmp.d0=(unsigned char)(rxCache[0]&0xFF);
	tmp.d1=(unsigned char)(rxCache[1]&0xFF);
	tmp.d2=(unsigned char)(rxCache[2]&0xFF);
	tmp.d3=(unsigned char)(rxCache[3]&0xFF);
        ret = copy_to_user (buffer, (char*) &tmp, sizeof(ZSPIReg));
	return ret;
}

/**
 *char device interface:write()
 */
	int
spi_driver_write (struct file*file, const char *buffer, size_t size, loff_t *pos)
{
	int ret;
	ZSPIDev *spiDev;
	ZSPIReg tmp;
	unsigned int txBuffer[4];
	unsigned int rxCache[4];

	/**
	 * check valid of parameters.
	 */
	if (size <= 0)
	{
		return -1;
	}

	/**
	 * get global resource pointer.
	 */
	spiDev = file->private_data;

	/**
	 * normal operatation
	 */
	ret = copy_from_user (&tmp, (ZSPIReg*) buffer, sizeof(ZSPIReg));
	if (ret)
	{
		printk ("ecspi-1:copy_from_user() at read() failed:%d\n", ret);
		return -1;
	}
	txBuffer[0] = tmp.d0;
	txBuffer[1] = tmp.d1;
	txBuffer[2] = tmp.d2;
	txBuffer[3] = tmp.d3;
	ret = spi_txrx(spiDev,txBuffer,sizeof(txBuffer)/sizeof(txBuffer[0]),rxCache,sizeof(rxCache)/sizeof(rxCache[0]));
	return ret;
}


	int
spi_driver_release (struct inode*inode, struct file*file)
{
	return 0;
}

/**
 *char device interface structure.
 */
struct file_operations spi_fops=
{ ///<
	.owner=THIS_MODULE,///<
	.open=spi_driver_open,///<
	.read=spi_driver_read,///<
	.write=spi_driver_write,///<
	//.ioctl=spi_driver_ioctl,///<
	.release=spi_driver_release,///<
};

/**
 *platform driver section.
 *when device and driver match,call this.
 */
	int
spi_driver_probe (struct platform_device *pdev)
{
	int ret;
	/*register chrdev*/
	gSPIDev->devno = MKDEV (SPI_MAJOR, SPI_MINOR);
	ret = register_chrdev_region (gSPIDev->devno, 1, "6QSPI4CEAMS");
	if (ret)
	{
		printk ("ecspi-1:register_chrdev_region() failed:%d\n", ret);
		goto err_out1;
	}
	cdev_init (&gSPIDev->chardev, &spi_fops);
	gSPIDev->chardev.owner = THIS_MODULE;
	ret = cdev_add (&gSPIDev->chardev, gSPIDev->devno, 1);
	if (ret)
	{
		printk ("ecspi-1:cdev_add() failed:%d\n", ret);
		goto err_out2;
	}

	/*call initilize sub-functions*/
	ret = spi_mapping (gSPIDev);
	if (ret)
	{
		goto err_out3;
	}
	ret = spi_init (gSPIDev);
	if (ret)
	{
		goto err_out4;
	}

	printk ("ecspi:probe finish\n");

	/**
	 * initial lock for spi read & write.
	 */
	//spin_lock_init (&gSPIDev->lock_spi);
	return 0;

err_out4: ///<label 
	iounmap (gSPIDev->pECSPI2);
err_out3: ///<label 
	cdev_del (&gSPIDev->chardev);
err_out2: ///<label
	unregister_chrdev_region (gSPIDev->devno, 1);
err_out1: ///<label 
	return -1;
}

	int
spi_driver_remove (struct platform_device *pdev)
{
	/*iounmap()*/
	iounmap (gSPIDev->pECSPI2);
	/*unregister chardev*/
	cdev_del (&gSPIDev->chardev);
	unregister_chrdev_region (gSPIDev->devno, 1);
	return 0;
}

/**
 * platform driver
 */
struct platform_driver spi_driver=
{
	.probe=spi_driver_probe,
	.remove=spi_driver_remove,
	.driver=
	{
		.name="6QSPI",/*must be same with platform device*/
	},
};

/**
 *called by module_init()
 */
	int
spi_driver_init (void)
{
	int ret;
	/**
	 * allocate  memory for global structure
	 */
	gSPIDev = kmalloc (sizeof(ZSPIDev), GFP_KERNEL);
	if (gSPIDev == NULL)
	{
		printk ("kmalloc() failed\n");
		return -ENOMEM;
	}
	memset (gSPIDev, 0, sizeof(ZSPIDev));

	ret = platform_device_register (&spi_device);
	if (ret)
	{
		printk ("6QSPI:platform_device_register() failed:%d\n", ret);
		return -1;
	}
	else
	{
		printk ("6QSPI:platform_device_register() ok:%d\n", ret);
	}

	ret = platform_driver_register (&spi_driver);
	if (ret)
	{
		printk ("6QSPI:platform_driver_register() failed:%d\n", ret);
		/**
		 * failed,roll-back.
		 */
		platform_device_unregister (&spi_device);
		return -1;
	}
	else
	{
		printk ("6QSPI:platform_driver_register() ok:%d\n", ret);
	}
	return 0;
}

/**
 * called by module_exit()
 */
	void
spi_driver_exit (void)
{
	platform_device_unregister (&spi_device);
	platform_driver_unregister (&spi_driver);
	kfree(gSPIDev);
	printk ("spi:platform_driver_unregister() ok\n");
	return;
}

MODULE_LICENSE("GPL");
module_init(spi_driver_init);
module_exit(spi_driver_exit);
/**
 *the end of file,for reading convenience
 *shell.albert@gmail.com
 *tagged by zhangshaoyan.
 */
