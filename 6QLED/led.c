/*filename:led_driver.c
 *function:led platform driver implementation
 *ver:1.0
 *date:2012/12/5
 *author:1811543668@qq.com
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/ioctl.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#ifdef IMX5_DEBUG
	#define PDEBUG(fmt,args...) printk(fmt,##args)
#else
	#define PDEBUG(fmt,args...)
#endif

#define NULL_POINTER	PDEBUG("error,file:%s,line:%d,function:%s,null pointer\n",__FILE__,__LINE__,__func__)

/*define device name*/
#define IMX5_LED_NAME		"zsy_imx5_led"	

#define IOMUXC_SW_MUX_CTL_PAD_CSI0_DATA19 0x20E02A4 //led1.
#define IOMUXC_SW_MUX_CTL_PAD_CSI0_HSYNC  0x20E025C //led2.
#define IOMUXC_SW_MUX_CTL_PAD_CSI0_DATA18 0x20E02A0 //led3.
#define IOMUXC_SW_MUX_CTL_PAD_CSI0_DATA17 0x20E029C //led4.

#define GPIO5_GDIR  0x20AC004
#define GPIO6_GDIR  0x20B0004

#define GPIO5_DR    0x20AC000
#define GPIO6_DR    0x20B0000

/*define device number*/
#define IMX5_LED_MAJOR	1987
#define IMX5_LED_MINOR_START	1
#define IMX5_LED_NUM	6
/*device release function*/
static void imx5_led_device_release(struct device *pdev){
	/*nothing to do*/
	return;
}
/*define platform_device resource*/
static struct resource imx5_led_resources[]={
	[0]={
		.start=(IOMUXC_SW_MUX_CTL_PAD_CSI0_DATA19),
		.end=(IOMUXC_SW_MUX_CTL_PAD_CSI0_DATA19), 
		.flags=IORESOURCE_MEM,
	},
	[1]={
		.start=(IOMUXC_SW_MUX_CTL_PAD_CSI0_DATA19),
		.end=(IOMUXC_SW_MUX_CTL_PAD_CSI0_DATA19),
		.flags=IORESOURCE_MEM,
	},
	[2]={
		.start=(IOMUXC_SW_MUX_CTL_PAD_CSI0_DATA19),
		.end=(IOMUXC_SW_MUX_CTL_PAD_CSI0_DATA19),
		.flags=IORESOURCE_MEM,
	},
	[3]={
		.start=(IOMUXC_SW_MUX_CTL_PAD_CSI0_DATA19),
		.end=(IOMUXC_SW_MUX_CTL_PAD_CSI0_DATA19),
		.flags=IORESOURCE_MEM,
	},
};
/*define platform device*/
static struct platform_device imx5_led_device={
	.name=IMX5_LED_NAME,
	.id=-1,
	.num_resources=ARRAY_SIZE(imx5_led_resources),
	.resource=imx5_led_resources,
	.dev={
		.release=imx5_led_device_release,
	},
};

/*led device struction*/
typedef struct{
	volatile unsigned long *pGPIO5_vir;/*virtual address of GPIO5*/
	volatile unsigned long *pGPIO6_vir;/*virtual address of GPIO6*/
	dev_t devno;/*the device number*/
	struct cdev chardev;/*chardev*/
}IMX5_PLAT_LED;
static IMX5_PLAT_LED *imx5_led;
/*call this function to set iomuxc to GPIO mode
 *return value: 0 success,-1 failed
 */
static int imx5_led_init_iomuxc(void){
	volatile unsigned long*pbase;
	int ret;
	//led1,GPIO6[5].
	pbase=(unsigned long*)ioremap(IOMUXC_SW_MUX_CTL_PAD_CSI0_DATA19,0x4);
	if(!pbase){
		NULL_POINTER;
		return -1;
	}
	ret=ioread32(pbase);
	/*[4]=0,input path determined by the selected mux mode*/
	ret&=(~(0x1<<4));
	/*[2:0]=101,GPIO6_GPIO[5]*/
	ret|=(0x1<<2);
	ret&=(~(0x1<<1));
	ret|=(0x1<<0);
	iowrite32(ret,pbase);
	iounmap(pbase);

    //led2,GPIO5[19].
    pbase=(unsigned long*)ioremap(IOMUXC_SW_MUX_CTL_PAD_CSI0_HSYNC,0x4);
	if(!pbase){
		NULL_POINTER;
		return -1;
	}
	ret=ioread32(pbase);
	/*[4]=0,input path determined by the selected mux mode*/
	ret&=(~(0x1<<4));
	/*[2:0]=101,GPIO5_GPIO[19]*/
	ret|=(0x1<<2);
	ret&=(~(0x1<<1));
	ret|=(0x1<<0);
	iowrite32(ret,pbase);
	iounmap(pbase);
    
	//led3,GPIO6[4].
	pbase=(unsigned long*)ioremap(IOMUXC_SW_MUX_CTL_PAD_CSI0_DATA18,0x4);
	if(!pbase){
		NULL_POINTER;
		return -1;
	}
	ret=ioread32(pbase);
	/*[4]=0,input path determined by the selected mux mode*/
	ret&=(~(0x1<<4));
	/*[2:0]=101,GPIO6_GPIO[4]*/
	ret|=(0x1<<2);
	ret&=(~(0x1<<1));
	ret|=(0x1<<0);
	iowrite32(ret,pbase);
	iounmap(pbase);

	//led4,GPIO6[3].
	pbase=(unsigned long*)ioremap(IOMUXC_SW_MUX_CTL_PAD_CSI0_DATA17,0x4);
	if(!pbase){
		NULL_POINTER;
		return -1;
	}
	ret=ioread32(pbase);
	/*[4]=0,input path determined by the selected mux mode*/
	ret&=(~(0x1<<4));
	/*[2:0]=101,GPIO6_GPIO[3]*/
	ret|=(0x1<<2);
	ret&=(~(0x1<<1));
	ret|=(0x1<<0);
	iowrite32(ret,pbase);
	iounmap(pbase);

	return 0;
}
/*call this function to set pin direction correctly
 *return value: 0 success, -1 failed
 */
static int imx5_led_set_pin_direction(void){
	volatile unsigned long*pbase;
	int ret;
	pbase=(unsigned long*)ioremap(GPIO5_GDIR,0x4);
	if(!pbase){
		NULL_POINTER;
		return -1;
	}
	ret=ioread32(pbase);
	ret|=(0x1<<19);/*output*/
	iowrite32(ret,pbase);
	iounmap(pbase);

	pbase=(unsigned long*)ioremap(GPIO6_GDIR,0x4);
	if(!pbase){
		NULL_POINTER;
		return -1;
	}
	ret=ioread32(pbase);
	ret|=(0x1<<3);/*output*/
    ret|=(0x1<<4);/*output*/
    ret|=(0x1<<5);/*output*/
	iowrite32(ret,pbase);
	iounmap(pbase);

	return 0;
}

/*mapping physical address to virtual address
 *the global imx5_led will hold the virtual address pointer
 *return value:0 success, -1 failed
 */
static int imx5_led_mapping(IMX5_PLAT_LED *p){
	p->pGPIO5_vir=(unsigned long*)ioremap(GPIO5_DR,0x4);
	if(!p->pGPIO5_vir){
		NULL_POINTER;
		return -1;
	}
	p->pGPIO6_vir=(unsigned long*)ioremap(GPIO6_DR,0x4);
	if(!p->pGPIO6_vir){
		NULL_POINTER;
		return -1;
	}
	return 0;
}
/*char device interface:open()*/
static int imx5_led_driver_open(struct inode *inode,struct file*file){
	IMX5_PLAT_LED *p;
	p=container_of(inode->i_cdev,IMX5_PLAT_LED,chardev);
	/*store pointer,avoid many times searching*/
	file->private_data=p;
	return 0;
}
/*char device interface:ioctl()*/
static int imx5_led_driver_ioctl(struct inode*inode,struct file*file,unsigned int cmd,unsigned long arg){

	return 0;
}
static int imx5_led_driver_write(struct file *filp,const char __user *buf,size_t count,loff_t *fpos)
{
    IMX5_PLAT_LED*p;
	int reg;
    char swBuf[2];
	/*get resource pointer*/
    p=filp->private_data;
    if(copy_from_user(swBuf,buf,2))
    {
        return -1;
    }
    //two character.
    //1/2/3/4, 0/1.
    //led1,GPIO6[5].
    //led2,GPIO5[19].
    //led3,GPIO6[4].
    //led4,GPIO6[3].
    if(swBuf[1]=='0')//off.
    {
        if(swBuf[0]=='1')
        {
            reg=ioread32(p->pGPIO6_vir);
            reg&=~(0x1<<5);
            iowrite32(reg,p->pGPIO6_vir);
        }else if(swBuf[0]=='2')
        {
            reg=ioread32(p->pGPIO5_vir);
            reg&=~(0x1<<19);
            iowrite32(reg,p->pGPIO5_vir);
        }else if(swBuf[0]=='3')
        {
            reg=ioread32(p->pGPIO6_vir);
            reg&=~(0x1<<4);
            iowrite32(reg,p->pGPIO6_vir);
        }else if(swBuf[0]=='4')
        {
            reg=ioread32(p->pGPIO6_vir);
            reg&=~(0x1<<3);
            iowrite32(reg,p->pGPIO6_vir); 
        }
    }else if(swBuf[1]=='1')//on.
    {
        if(swBuf[0]=='1')
        {
            reg=ioread32(p->pGPIO6_vir);
            reg|=(0x1<<5);
            iowrite32(reg,p->pGPIO6_vir);
        }else if(swBuf[0]=='2')
        {
            reg=ioread32(p->pGPIO5_vir);
            reg|=(0x1<<19);
            iowrite32(reg,p->pGPIO5_vir);
        }else if(swBuf[0]=='3')
        {
            reg=ioread32(p->pGPIO6_vir);
            reg|=(0x1<<4);
            iowrite32(reg,p->pGPIO6_vir);
        }else if(swBuf[0]=='4')
        {
            reg=ioread32(p->pGPIO6_vir);
            reg|=(0x1<<3);
            iowrite32(reg,p->pGPIO6_vir); 
        }
    }
    //printk("led write done\n");
    //here must return 2 to avoid echo -n xx>/dev/led block.
    //led1 :echo -n 10/11 > /dev/led
    //led2 :echo -n 20/21 > /dev/led
    //led3 :echo -n 30/31 > /dev/led
    //led4 :echo -n 40/41 > /dev/led
    return 2;
}
/*char device interface:release()*/
static int imx5_led_driver_release(struct inode*inode,struct file*file){
	return 0;
}
/*chardev interface*/
static struct file_operations imx5_led_fops={
	.owner=THIS_MODULE,
	.open=imx5_led_driver_open,
    .write=imx5_led_driver_write,
	//.ioctl=imx5_led_driver_ioctl,
	.release=imx5_led_driver_release,
};
/*platform driver section*/
static int imx5_led_driver_probe(struct platform_device *pdev){
	int ret;int reg;
	/*malloc memory for device struct*/
	imx5_led=kmalloc(sizeof(IMX5_PLAT_LED),GFP_KERNEL);
	if(!imx5_led){
		NULL_POINTER;
		return -ENOMEM;
	}
	memset(imx5_led,0,sizeof(IMX5_PLAT_LED));
	/*register chrdev*/
	imx5_led->devno=MKDEV(IMX5_LED_MAJOR,IMX5_LED_MINOR_START);
	ret=register_chrdev_region(imx5_led->devno,IMX5_LED_NUM,IMX5_LED_NAME);	
	if(ret){
		printk("led:register_chrdev_region() failed:%d\n",ret);
		goto err_out1;
	}else{
		printk("led:register_chrdev_region() ok:%d\n",ret);
	}
	/*add chrdev to kernel*/	
	cdev_init(&imx5_led->chardev,&imx5_led_fops);
	imx5_led->chardev.owner=THIS_MODULE;
	ret=cdev_add(&imx5_led->chardev,imx5_led->devno,IMX5_LED_NUM);
	if(ret){
		printk("led:cdev_add() failed:%d\n",ret);
		goto err_out2;
	}else{
		printk("led:cdev_add() ok:%d\n",ret);
	}	
	/*call sub initial functions*/
	ret=imx5_led_init_iomuxc();
	if(ret){
		goto err_out3;
	}
	ret=imx5_led_set_pin_direction();
	if(ret){
		goto err_out3;
	}
	ret=imx5_led_mapping(imx5_led);
	if(ret){
		goto err_out3;
	}
	printk("probe() finish\n");
        reg=ioread32(imx5_led->pGPIO6_vir);
        reg|=(0x1<<5);
        iowrite32(reg,imx5_led->pGPIO6_vir);
        reg=ioread32(imx5_led->pGPIO5_vir);
        reg|=(0x1<<19);
        iowrite32(reg,imx5_led->pGPIO5_vir);
        reg=ioread32(imx5_led->pGPIO6_vir);
        reg|=(0x1<<4);
        iowrite32(reg,imx5_led->pGPIO6_vir);
        reg=ioread32(imx5_led->pGPIO6_vir);
        reg|=(0x1<<3);
        iowrite32(reg,imx5_led->pGPIO6_vir);
	return 0;
err_out3:
	cdev_del(&imx5_led->chardev);	
err_out2:
	unregister_chrdev_region(imx5_led->devno,IMX5_LED_NUM);
err_out1:
	kfree(imx5_led);
	return -1;
}
static int imx5_led_driver_remove(struct platform_device *pdev){
	/*iounmap*/
	iounmap(imx5_led->pGPIO5_vir);
	iounmap(imx5_led->pGPIO6_vir);
	/*unregister chardev*/
	cdev_del(&imx5_led->chardev);
	unregister_chrdev_region(imx5_led->devno,IMX5_LED_NUM);
	kfree(imx5_led);
	return 0;
}
/*platform driver*/
static struct platform_driver imx5_led_driver={
	.probe=imx5_led_driver_probe,
	.remove=imx5_led_driver_remove,
	.driver={
		.name=IMX5_LED_NAME, /*must be same as platform device*/
	},
};
/*for called by module_init()*/
static int __init imx5_led_driver_init(void){
	int ret;
	ret=platform_device_register(&imx5_led_device);
	ret=platform_driver_register(&imx5_led_driver);
	if(ret){
		printk("led:platform_driver_register() failed:%d\n",ret);
	}else{
		printk("led:platform_driver_register() ok:%d\n",ret);
	}
	return ret;
}
/*for called by module_exit()*/
static void __exit imx5_led_driver_exit(void){
  	platform_device_unregister(&imx5_led_device);
	platform_driver_unregister(&imx5_led_driver);
	printk("led:platform_driver_unregister() ok\n");
	return;
}
/*for called by insmod&rmmod command*/
module_init(imx5_led_driver_init);
module_exit(imx5_led_driver_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("1811543668@qq.com");
/*the end of file*/
