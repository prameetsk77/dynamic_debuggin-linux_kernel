#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/device.h>
#include <linux/hashtable.h>
#include "commonData.h"

#define DEVICE_NAME				"ht530"  // device name to be created and registered

volatile int test=10;
struct list_object{
	ht_object_t object;
	struct hlist_node list;
};
typedef struct list_object list_object_t;

/* per device structure */
struct ht_dev {
	struct cdev cdev;               /* The cdev structure */
	char name[10];
	DECLARE_HASHTABLE(ht530_tbl, 7);
} *ht_devp;

static dev_t ht_dev_number;      /* Allotted device number */
struct class *ht_dev_class;        /* Tie with the device model */
static struct device *ht_dev_device;

/*
* Open ht driver
*/
int ht_driver_open(struct inode *inode, struct file *file){
	struct ht_dev *ht_devp;
//	printk("\nopening\n");

	/* Get the per-device structure that contains this cdev */
	ht_devp = container_of(inode->i_cdev, struct ht_dev, cdev);

	/* Easy access to ht_devp from rest of the entry points */
	file->private_data = ht_devp;
	printk("\n%s is openning \n", ht_devp->name);
	return 0;
}

/*
 * Release ht driver
 */
int ht_driver_release(struct inode *inode, struct file *file){
	struct ht_dev *ht_devp = file->private_data;
	
	printk("\n%s is closing\n", ht_devp->name);
	
	return 0;
}

/*
*
* No need of mutex(threadsafe) since only read operations are performed.
*
*/
static long ht_ioctl(struct file *file, unsigned int cmd, unsigned long arg){
	int bkt,ret;
	struct ht_dev *ht_devp = file->private_data;
	ht_object_t returnVal[8];
	int currArrayPos =0;
	int hasReached = 0; 
	int bkt_no;
	list_object_t* curr;
	struct hlist_node temp;

	struct dump_arg input;

	struct hlist_node* temp_addr;
	temp_addr = &temp;

	copy_from_user(&input, (struct dump_arg*)arg, sizeof(struct dump_arg));

	switch (cmd){
		case dump:
			printk("dump recd\n");
			bkt_no = input.n;

			printk("bkt: %d", bkt_no);
			if(bkt_no > 127 || bkt_no < 0){
				printk("incorrect bucket\n");
				return -EINVAL;
			}

			hash_for_each_safe(ht_devp->ht530_tbl, bkt, temp_addr, curr, list){
				printk(KERN_INFO "bkt: %d \t data:%d \t key: %d\n", bkt, (curr->object).data, (curr->object).key);

				if(bkt != bkt_no && !hasReached)
					continue;
				else if(bkt != bkt_no && hasReached){
					printk(KERN_INFO "Reached end of array\n");
					copy_to_user(((struct dump_arg*)arg)->object_array, returnVal, sizeof(ht_object_t)*currArrayPos);
					hasReached = 0;
					return currArrayPos;
				}
				else{
					printk(KERN_INFO "\nReached: data:%d \t key: %d\n", (curr->object).data, (curr->object).key);
					hasReached =1;

					returnVal[currArrayPos].data = (curr->object).data;
					returnVal[currArrayPos++].key = (curr->object).key;
					if(currArrayPos==8){
						printk(KERN_INFO "collected 8 elements\n");
						ret = copy_to_user(((struct dump_arg*)arg)->object_array, returnVal, sizeof(ht_object_t)*currArrayPos);
						return currArrayPos;
					}
				}
			}
			break;
		default:
				printk(KERN_INFO "INVALID COMMAND\n");
				return -EINVAL;
	}
	if(currArrayPos > 0){
		printk(KERN_INFO "Last non empty bkt\n");
		ret = copy_to_user(((struct dump_arg*)arg)->object_array, returnVal, sizeof(ht_object_t)*currArrayPos);
		return currArrayPos;
	}
	return -ENOMSG;
}

/*
 * Write to ht driver
 */
ssize_t ht_driver_write(struct file *file, const char *buf,
           size_t count, loff_t *ppos){
	ht_object_t input;
	list_object_t* hash_object;
	list_object_t* curr;
	struct ht_dev* ht_devp = file->private_data;
	volatile int bkt = 10;
	test = 5;
	if(buf == NULL)
		return EFAULT;
	bkt =3;
	if(copy_from_user(&input, buf, sizeof(ht_object_t)) != 0)
		return -EFAULT;
	bkt =5;
	/* Check for same key in hash table*/
	hash_for_each_possible(ht_devp->ht530_tbl, curr, list, input.key){
		if(input.key == (curr->object).key){
			printk(KERN_INFO "data:%d \t key: %d\n", (curr->object).data, (curr->object).key);
			if(input.data == 0){
				hash_del(&(curr->list));
				kfree(curr); 
			}
			else
				(curr->object).data = input.data;
			return 0;
		}
	}
	//curr->object = NULL;

	hash_for_each(ht_devp->ht530_tbl,bkt,curr,list){
			printk(KERN_INFO "bkt:%d data:%d \t key: %d\n", bkt, (curr->object).data, (curr->object).key);
	}

	/*Corner Case: data == 0 && no object to delete */
	if(input.data == 0){
		return -EINVAL;
	}

	hash_object = kmalloc(sizeof(list_object_t), GFP_KERNEL);
	if(!hash_object){
		printk(KERN_INFO "Malloc failed\n");
		return -1;
	}

	(hash_object->object).key = input.key;
	(hash_object->object).data = input.data;
	(hash_object->list).next = NULL;

	hash_add(ht_devp->ht530_tbl, &(hash_object->list), (hash_object->object).key);

	printk(KERN_ALERT "Data Hashed\n");
	test = input.key;
	return 0;
}
/*
 * Read from ht driver
 */

ssize_t ht_driver_read(struct file *file, char *buf,
           size_t count, loff_t *ppos){
	struct ht_dev *ht_devp = file->private_data;
	volatile int bkt;
	list_object_t* curr;
	ht_object_t object;
	bkt = 0;
	test = 1;

	if(copy_from_user(&object, buf, sizeof(ht_object_t)) != 0)
		return -EFAULT;
	test = 10;
	bkt = 7;

	printk(KERN_INFO "test: %d\t bkt: %d\n", test, bkt);
	hash_for_each_possible(ht_devp->ht530_tbl, curr, list, object.key){
		if(object.key == (curr->object).key){
			printk(KERN_INFO "data:%d \t key: %d\n", (curr->object).data, (curr->object).key);
			object.data = (curr->object).data;

			//hash_del(&(curr->list));
			//kfree(curr);

			if(copy_to_user(buf, &object, sizeof(ht_object_t)) != 0)
				return -EFAULT;
			return 0;
		}
	}
	return -EINVAL;
}

/* File operations structure. Defined in linux/fs.h */
static struct file_operations ht_fops = {
    .owner		= THIS_MODULE,           /* Owner */
    .open		= ht_driver_open,        /* Open method */
    .release	= ht_driver_release,     /* Release method */
    .write		= ht_driver_write,       /* Write method */
    .read		= ht_driver_read,        /* Read method */
    .unlocked_ioctl = ht_ioctl,
};

/*
 * Driver Initialization
 */
int __init ht_driver_init(void){
	int ret;

	/* Request dynamic allocation of a device major number */
	if (alloc_chrdev_region(&ht_dev_number, 0, 1, DEVICE_NAME) < 0) {
			printk(KERN_DEBUG "Can't register device\n"); return -1;
	}

	/* Populate sysfs entries */
	ht_dev_class = class_create(THIS_MODULE, DEVICE_NAME);

	/* Allocate memory for the per-device structure */
	ht_devp = kmalloc(sizeof(struct ht_dev), GFP_KERNEL);
		
	if (!ht_devp) {
		printk("Bad Kmalloc\n"); return -ENOMEM;
	}

	/* Request I/O region */
	sprintf(ht_devp->name, DEVICE_NAME);

	/* Connect the file operations with the cdev */
	cdev_init(&ht_devp->cdev, &ht_fops);
	ht_devp->cdev.owner = THIS_MODULE;

	/* Connect the major/minor number to the cdev */
	ret = cdev_add(&ht_devp->cdev, (ht_dev_number), 1);

	if (ret) {
		printk("Bad cdev\n");
		return ret;
	}

	hash_init(ht_devp->ht530_tbl);

	/* Send uevents to udev, so it'll create /dev nodes */
	ht_dev_device = device_create(ht_dev_class, NULL, MKDEV(MAJOR(ht_dev_number), 0), NULL, DEVICE_NAME);		
	// device_create_file(ht_dev_device, &dev_attr_xxx);

	printk("ht530 driver initialized.\n");
	return 0;
}
/* Driver Exit */
void __exit ht_driver_exit(void){
	// device_remove_file(ht_dev_device, &dev_attr_xxx);
	/* Release the major number */

	int bkt;
	list_object_t* curr;
	struct hlist_node temp;

	struct hlist_node* temp_addr;
	temp_addr = &temp;

	hash_for_each_safe(ht_devp->ht530_tbl, bkt, temp_addr, curr, list){
		hash_del(&(curr->list));
		kfree(curr);
	}
	unregister_chrdev_region((ht_dev_number), 1);

	/* Destroy device */
	device_destroy (ht_dev_class, MKDEV(MAJOR(ht_dev_number), 0));
	cdev_del(&ht_devp->cdev);
	kfree(ht_devp);
	
	/* Destroy driver_class */
	class_destroy(ht_dev_class);

	printk("ht530 driver removed.\n");
}

module_init(ht_driver_init);
module_exit(ht_driver_exit);
MODULE_LICENSE("GPL v2");