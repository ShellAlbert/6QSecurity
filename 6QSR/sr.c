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
#include <linux/workqueue.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <asm/irq.h>
#include <linux/fsl_devices.h>
#include <asm/gpio.h>

#define IOMUXC_SW_MUX_CTL_PAD_CSI0_DATA12   0x20E0288
#define GPIO5_GDIR  0x20AC004
#define GPIO5_ICR2  0x20AC010
#define GPIO5_IMR   0x20AC014

typedef struct
{
  dev_t devno;/*the device number*/
  struct cdev chardev;/*char device*/
  struct fasync_struct *fasync_queue;/*fasync notice*/
} SRDevice;

SRDevice *srDev=NULL;

//soft-reset:sr.
/******************************platform device here********************************/
/**
 * device release function
 * do some rollback work.
 */
static void
imx6_sr_release (struct device *pdev)
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
static struct resource imx6_sr_resources[]=
  {
    [0]=
      {
	.start=286,/*EXT_INT,GPIO5_GPIO[30]=(5-1)*32+30+128*/
	.end=286,
	.flags=IORESOURCE_IRQ,
      },
  };

/**
 * platform device
 */
static struct platform_device imx6_sr_device=
  {
    .name="6QSR",
    .id=-1,
    .resource=imx6_sr_resources,
    .num_resources=ARRAY_SIZE(imx6_sr_resources),
    .dev=
      {
	.release=imx6_sr_release,
      },
  };

/*iomuxc change mux mode
 *pad connected to ecspi blocks
 *return value:0 success,-1 failed
 */
int
imx6_sr_init (void)
{
  volatile unsigned long *pbase;
  int ret;
  /******************ecspi-1****************************/
  /*CSI0_DATA12*/
  pbase = (unsigned long*) ioremap (IOMUXC_SW_MUX_CTL_PAD_CSI0_DATA12, 0x4);
  if (!pbase)
    {
      printk("ioremap() faild!\n");
      return -1;
    }

  ret = ioread32 (pbase);
  /*[4]=0,input path determined by selected mux mode*/
  ret &= (~(0x1 << 4));
  /*[2:0]=101,ALT5,GPIO5_IO30*/
  ret |= (0x1 << 2);
  ret &= (~(0x1 << 1));
  ret |= (0x1 << 0);
  iowrite32 (ret, pbase);
  iounmap (pbase);
  
  //GPIO5[30]:input.
  pbase = (unsigned long*) ioremap (GPIO5_GDIR, 0x4);
  if (!pbase)
    {
      printk("ioremap() faild!\n");
      return -1;
    }
  ret = ioread32 (pbase);
  ret &= (~(0x1 << 30));
  iowrite32 (ret, pbase);
  iounmap (pbase);
  
  //falling-edge.
  pbase = (unsigned long*) ioremap (GPIO5_ICR2, 0x4);
  if (!pbase)
    {
      printk("ioremap() faild!\n");
      return -1;
    }
  ret = ioread32 (pbase);
  ret |= (0x1 << 29);
  ret |= (0x1 << 28);
  iowrite32 (ret, pbase);
  iounmap (pbase);
  return 0;
} 
int
imx6_en_isr(int en)
{
  volatile unsigned long *pbase;
  int ret;
  //enable irq.
  pbase = (unsigned long*) ioremap (GPIO5_IMR, 0x4);
  if (!pbase)
    {
      printk("ioremap() faild!\n");
      return -1;
    }
  ret = ioread32 (pbase);
  if(en)
  {
  	ret |= (0x1 << 30);
  }else
  {
        ret&=~(0x1<<30);
  }
  iowrite32 (ret, pbase);
  iounmap (pbase);
  return 0;
}


/**
 *ISR for FPGA_INT1
 *FPGA send INT1 signal to ARM
 *using fasync signal to tell application layer to read data
 */
irqreturn_t
imx6_sr_isr (int irq, void*dev)
{
  SRDevice *p=(SRDevice*)dev;
  if(p==NULL)
  {
     printk("null dev in imx6_sr_isr!\n");
     return IRQ_NONE;
  }
  printk("6QSR:soft-reset,restore manufacture default.\n");
  disable_irq_nosync(gpio_to_irq(IMX_GPIO_NR(5,30)));
  /*tell the application layer to read&write register*/
  if (p->fasync_queue)
   {
     kill_fasync(&p->fasync_queue, SIGIO, POLL_IN);
   }
  enable_irq(gpio_to_irq(IMX_GPIO_NR(5,30)));
  return IRQ_HANDLED;
}

/**
 *char device interface:open()
 */
int
imx6_sr_driver_open (struct inode*inode, struct file*file)
{
  imx6_en_isr(1);
  return 0;
}

/**
 *char device interface:read()
 */
int
imx6_sr_driver_read (struct file*file, char *buffer, size_t size, loff_t *pos)
{
    return 0;
}

/**
 *char device interface:write()
 */
int
imx6_sr_driver_write (struct file*file, const char *buffer, size_t size, loff_t *pos)
{
    return 0;
}

int
imx6_sr_driver_fasync (int fd, struct file*file, int mode)
{
  return fasync_helper (fd, file, mode, &srDev->fasync_queue);
}

int
imx6_sr_driver_release (struct inode*inode, struct file*file)
{
  imx6_en_isr(0);
  return 0;
}

/**
 *char device interface structure.
 */
struct file_operations imx6_sr_fops=
  { ///<
    .owner=THIS_MODULE,///<
    .open=imx6_sr_driver_open,///<
    .read=imx6_sr_driver_read,///<
    .write=imx6_sr_driver_write,///<
    .fasync=imx6_sr_driver_fasync,///<
    .release=imx6_sr_driver_release,///<
  };

/**
 *platform driver section.
 *when device and driver match,call this.
 */
int
imx6_sr_driver_probe (struct platform_device *pdev)
{
  int ret;
  srDev=(SRDevice*)kmalloc(sizeof(SRDevice),GFP_KERNEL);
  if(srDev==NULL)
  {
    printk("kmalloc() failed!\n");
    return -1;
  }
  memset(srDev,0,sizeof(SRDevice));
 
  /*register chrdev*/
  srDev->devno = MKDEV (2017, 1);
  ret = register_chrdev_region (srDev->devno, 1, "6QSR");
  if (ret)
    {
      printk ("6QSR:register_chrdev_region() failed:%d\n", ret);
      goto err_out;
    }
  cdev_init (&srDev->chardev, &imx6_sr_fops);
  srDev->chardev.owner = THIS_MODULE;
  ret = cdev_add (&srDev->chardev, srDev->devno, 1);
  if (ret)
    {
      printk ("6QSR:cdev_add() failed:%d\n", ret);
      goto err_out;
    }

  /*call initilize sub-functions*/
  ret = imx6_sr_init();
  if (ret)
    {
      goto err_out;
    }
    
  ret = request_irq (gpio_to_irq(IMX_GPIO_NR(5,30)),imx6_sr_isr,IRQF_DISABLED,"6QSR", (void*)srDev);
  if (ret)
    {
      printk ("request_irq() failed\n");
      goto err_out;
    }
  printk ("6QSR:probe finish\n");

  return 0;

  err_out: ///<label 
  cdev_del (&srDev->chardev);
  unregister_chrdev_region (srDev->devno, 1);
  return -1;
}

int
imx6_sr_driver_remove (struct platform_device *pdev)
{
  /*unregister chardev*/
  cdev_del (&srDev->chardev);
  unregister_chrdev_region (srDev->devno, 1);
  return 0;
}

/**
 * platform driver
 */
struct platform_driver imx6_sr_driver=
  {
    .probe=imx6_sr_driver_probe,
    .remove=imx6_sr_driver_remove,
    .driver=
      {
	.name="6QSR",/*must be same with platform device*/
      },
  };

/**
 *called by module_init()
 */
int
imx6_sr_driver_init (void)
{
  int ret;
  ret = platform_device_register (&imx6_sr_device);
  if (ret)
    {
      printk("6QSR:platform_device_register() failed:%d\n", ret);
      return -1;
    }
  else
    {
      printk ("6QSR:platform_device_register() ok:%d\n", ret);
    }

  ret = platform_driver_register (&imx6_sr_driver);
  if (ret)
    {
      printk ("6QSR:platform_driver_register() failed:%d\n", ret);
      /**
       * failed,roll-back.
       */
      platform_device_unregister (&imx6_sr_device);
      return -1;
    }
  else
    {
      printk ("6QSR:platform_driver_register() ok:%d\n", ret);
    }
   printk("imx6_sr_driver_init\n");
  return 0;
}

/**
 * called by module_exit()
 */
void
imx6_sr_driver_exit (void)
{
  platform_device_unregister (&imx6_sr_device);
  platform_driver_unregister (&imx6_sr_driver);
  printk ("6QSR:imx6_sr_driver_exit() ok\n");
  return;
}
MODULE_LICENSE("GPL");
module_init(imx6_sr_driver_init);
module_exit(imx6_sr_driver_exit);
/**
 *the end of file,for reading convenience
 *shell.albert@gmail.com
 *tagged by zhangshaoyan.
 */
