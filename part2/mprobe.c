#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/string.h>
#include <linux/device.h>
#include <linux/jiffies.h>
#include <linux/init.h>
#include <linux/moduleparam.h>
#include <linux/kprobes.h>
#include <linux/kallsyms.h>
#include <linux/sched.h>
#include <linux/spinlock.h>

#include <linux/types.h>

#include "commonData.h"
#include "bufferStruct.h"


#define DEVICE_NAME                 "Mprobe"  // device name to be created and registered
#define RING_BUFFER_SIZE             5

/* per device structure */
struct mprobe_dev {
	struct cdev cdev;               /* The cdev structure */
	probe_arg_t kp_args;
	struct kprobe kp;
} *mprobe_devp;

static dev_t mprobe_dev_number;      /* Allotted device number */
struct class *mprobe_dev_class;          /* Tie with the device model */
static struct device *mprobe_dev_device;

inline uint64_t GetTSCTime(void)
{
	unsigned int high, low;
	asm volatile(
		"RDTSC\n\t"
		"mov %%edx, %0\n\t"
		"mov %%eax, %1\n\t": "=r" (high), "=r" (low)::"%eax","%edx");
	return ((uint64_t)high << 32) | low;
}

spinlock_t mLock;
unsigned long flags;
buff_obj_t ring_buff[RING_BUFFER_SIZE];
int buff_head,buff_tail,buff_size;

/*
* Open mprobe driver
*/

int mprobe_driver_open(struct inode *inode, struct file *file){
	struct mprobe_dev *mprobe_devp;	
	/* Get the per-device structure that contains this cdev */
	mprobe_devp = container_of(inode->i_cdev, struct mprobe_dev, cdev);
	/* Easy access to cmos_devp from rest of the entry points */
	file->private_data = mprobe_devp;
	//printk("\n%s is openning \n", mprobe_devp->name);
	return 0;
}

/*
 * Release mprobe driver
 */

int mprobe_driver_release(struct inode *inode, struct file *file){
	/*struct mprobe_dev *mprobe_devp = file->private_data;
	struct kprobe* kp = &(mprobe_devp->kp);

	if(kp->addr != 0){
		printk("Release: unregistering prev kprobe at addr: %x\n", kp->addr);
		unregister_kprobe(kp);
	}*/

	return 0;
}

static int handler_pre(struct kprobe *p, struct pt_regs *regs){

	printk(KERN_ALERT "pre_handler: p->addr = 0x%p, ip = %lx,"
                         " flags = 0x%lx\n",
                p->addr, regs->ip, regs->flags);

	printk(KERN_INFO "Process %s (pid: %d, threadinfo=%p task=%p)", 
				current->comm, current->pid, current_thread_info(), current);

        /* A dump_stack() here will give a stack backtrace */
	printk("global variable: %d  at 0x%lx\n", 
						*((int*)((mprobe_devp->kp_args).globalAddress)),
									(mprobe_devp->kp_args).globalAddress);
	printk("local variable: %d at 0x%lx\n", 
						*((int*)((regs->bp)-(mprobe_devp->kp_args).localOffset)), 
									(regs->bp)-(mprobe_devp->kp_args).localOffset);
 	
        return 0;
 }

 /* kprobe post_handler: called after the probed instruction is executed */

 static void handler_post(struct kprobe *p, struct pt_regs *regs,
                                unsigned long flags){

 	buff_obj_t buffer;
 	struct mprobe_dev* mprobe_devp;
	mprobe_devp = container_of(p, struct mprobe_dev, kp );
 	printk(KERN_ALERT "post_handler: p->addr = 0x%p, ip = %lx,"
                         " flags = 0x%lx\n",
                p->addr, regs->ip, regs->flags);

 	spin_lock_irqsave(&mLock,flags);  // disabling pre-emption for critical secton

	buffer.tid = current->pid;
	buffer.l_value = *((int*)((regs->bp)-(mprobe_devp->kp_args).localOffset));
	buffer.g_value = *((int*)((mprobe_devp->kp_args).globalAddress));
	buffer.kp_addr = (unsigned long)p->addr;
	buffer.tsc = GetTSCTime();

	ring_buff[buff_head] = buffer;
	buff_head = (buff_head+1)%RING_BUFFER_SIZE;
	buff_size = (((buff_size+1)>RING_BUFFER_SIZE)?RING_BUFFER_SIZE:(buff_size+1));
	printk("buff_head =%d\tbuff_tail=%d\n",buff_head,buff_tail);
	if(buff_head == buff_tail){
		buff_tail=(buff_tail+1)%RING_BUFFER_SIZE;
	}

 	spin_unlock_irqrestore(&mLock,flags);  //re-enabling pre-emption
	printk("global variable: %d  at 0x%lx\n", *((int*)((mprobe_devp->kp_args).globalAddress)), (mprobe_devp->kp_args).globalAddress);
	printk("local variable: %d at 0x%lx\n", *((int*)((regs->bp)-(mprobe_devp->kp_args).localOffset)),  (regs->bp)-(mprobe_devp->kp_args).localOffset);
 	

 }

 /*
  * fault_handler: this is called if an exception is generated for any
  * instruction within the pre- or post-handler, or when Kprobes
  * single-steps the probed instruction.
  */
static int handler_fault(struct kprobe *p, struct pt_regs *regs, int trapnr){
        printk(KERN_ALERT "fault_handler: p->addr = 0x%p, trap #%dn",
                p->addr, trapnr);
        /* Return 0 because we don't handle the fault. */



        printk(KERN_ALERT "FAULT");
        return 0;
}

/*
 * Write to mprobe driver
 */
ssize_t mprobe_driver_write(struct file *file, const char *buf,
           size_t count, loff_t *ppos){

	int ret;
	struct mprobe_dev* mprobe_devp = file->private_data;

	struct kprobe* kp = &(mprobe_devp->kp);
	probe_arg_t kp_args;

	printk("\n\\\\\\\\\\ ****************************   \\\\\\\\\\\\\n");

	if(copy_from_user(&kp_args, buf, sizeof(probe_arg_t)) != 0){
		printk(KERN_INFO "copy_from_user failed kp_args\n");
		return -EFAULT;
	}

	if(kp->addr != 0){
		printk("unregistering prev kprobe at addr: %x\n",(unsigned int)kp->addr);
		unregister_kprobe(kp);
	}

	kp->pre_handler = handler_pre;
	kp->post_handler = handler_post;
	kp->fault_handler = handler_fault;

	
	printk("kProbe args recd:\t funcName: %s \t function offset: 0x%lx\n", kp_args.funcName, kp_args.funcOffset);
	printk("\t\t\t\t local variable offset: 0x%lx \t global variable offset 0x%lx\n", kp_args.localOffset, kp_args.globalAddress);

	kp->addr = (kprobe_opcode_t*) kallsyms_lookup_name(kp_args.funcName);
	if(kp->addr == NULL){
		printk("Error looking up symbol name\n");
		return 1;
	}
	kp->addr += kp_args.funcOffset;
	printk("Kprobe Reg addr: 0x%x \n",(unsigned int)kp->addr);
	
	ret = register_kprobe(kp);
	if (ret < 0) {
		printk(KERN_INFO "register_kprobe failed, returned %d\n", ret);
		return ret;
	}
	
	printk(KERN_INFO "Planted kprobe at %p\n", kp->addr);
	mprobe_devp->kp_args = kp_args;
	enable_kprobe(kp);
	return 0;
}

/*
 * Read to mprobe driver
 */
ssize_t mprobe_driver_read(struct file *file, char *buf,
           size_t count, loff_t *ppos){
	int bytes_read = 0;
	//struct mprobe_dev *mprobe_devp = file->private_data;
	/* 
	 * Most read functions return the number of bytes put into the buffer
	 */
	 spin_lock_irqsave(&mLock,flags);  // disabling pre-emption for critical secton

	 if(buff_size == 0){  // buffer is empty
	 	printk("Buffer Empty");
	 	spin_unlock_irqrestore(&mLock,flags);
	 	return -EINVAL;
	 }
	 printk("buff_size =%d, tail =%d\n",buff_size,buff_tail);

	 if(copy_to_user(buf,&ring_buff[buff_tail],sizeof(buff_obj_t)) != 0){
	 	spin_unlock_irqrestore(&mLock,flags);
	 	return -ENOMEM;
	 }

	 buff_size = buff_size-1;
	 buff_tail = (buff_tail + 1)%RING_BUFFER_SIZE;

	 spin_unlock_irqrestore(&mLock,flags);  //re-enabling pre-emption

	return bytes_read;
}

/* File operations structure. Defined in linux/fs.h */
static struct file_operations mprobe_fops = {
    .owner		= THIS_MODULE,           /* Owner */
    .open		= mprobe_driver_open,        /* Open method */
    .release	= mprobe_driver_release,     /* Release method */
    .write		= mprobe_driver_write,       /* Write method */
    .read		= mprobe_driver_read,        /* Read method */
};

/*
 * Driver Initialization
 */
int __init mprobe_driver_init(void){
	int ret;
	/* Request dynamic allocation of a device major number */

	if (alloc_chrdev_region(&mprobe_dev_number, 0, 1, DEVICE_NAME) < 0){
			printk(KERN_DEBUG "Can't register device\n"); return -1;
	}

	/* Populate sysfs entries */
	mprobe_dev_class = class_create(THIS_MODULE, DEVICE_NAME);
	/* Allocate memory for the per-device structure */
	mprobe_devp = kmalloc(sizeof(struct mprobe_dev), GFP_KERNEL);
	if (!mprobe_devp) {
		printk("Bad Kmalloc\n"); return -ENOMEM;
	}

	/* Request I/O region */

	/* Connect the file operations with the cdev */
	cdev_init(&mprobe_devp->cdev, &mprobe_fops);

	mprobe_devp->cdev.owner = THIS_MODULE;

	/* Connect the major/minor number to the cdev */
	ret = cdev_add(&mprobe_devp->cdev, (mprobe_dev_number), 1);
	if (ret) {
		printk("Bad cdev\n");
		return ret;
	}

	/* Send uevents to udev, so it'll create /dev nodes */
	mprobe_dev_device = device_create(mprobe_dev_class, NULL, MKDEV(MAJOR(mprobe_dev_number), 0), NULL, DEVICE_NAME);

	(mprobe_devp->kp).addr =0;
	printk("mprobe driver initialized.\n");// '%s'\n",mprobe_devp->in_string);

	buff_head =0;
	buff_tail =0;
	buff_size =0;
	
	spin_lock_init(&mLock);

	return 0;
}

/* Driver Exit */
void __exit mprobe_driver_exit(void){
	// device_remove_file(mprobe_dev_device, &dev_attr_xxx);
	/* Release the major number */
	if((mprobe_devp->kp).addr != 0)
		unregister_kprobe(&(mprobe_devp->kp));
	
	unregister_chrdev_region((mprobe_dev_number), 1);

	/* Destroy device */
	device_destroy (mprobe_dev_class, MKDEV(MAJOR(mprobe_dev_number), 0));

	cdev_del(&mprobe_devp->cdev);

	kfree(mprobe_devp);

	/* Destroy driver_class */
	class_destroy(mprobe_dev_class);

	printk("mprobe driver removed.\n");
}

module_init(mprobe_driver_init);
module_exit(mprobe_driver_exit);
MODULE_LICENSE("GPL v2");