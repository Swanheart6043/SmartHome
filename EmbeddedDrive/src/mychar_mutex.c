#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <asm/uaccess.h>

#include "mychar.h"

#define BUF_LEN 100

int major = 11;
int minor = 0;
int mychar_num  = 1;

struct mychar_dev
{
	struct cdev mydev;

	char mydev_buf[BUF_LEN];
	int curlen;
	struct mutex lock;

	wait_queue_head_t rq;
	wait_queue_head_t wq;

	struct fasync_struct *pasync_obj;
};

struct mychar_dev *pgmydev = NULL;

int mychar_open(struct inode *pnode,struct file *pfile)
{
	pfile->private_data =(void *) (container_of(pnode->i_cdev,struct mychar_dev,mydev));
	return 0;
}

int mychar_close(struct inode *pnode,struct file *pfile)
{
	struct mychar_dev *pmydev = (struct mychar_dev *)pfile->private_data;

	if(pmydev->pasync_obj != NULL)
		fasync_helper(-1,pfile,0,&pmydev->pasync_obj);
	return 0;
}

ssize_t mychar_read(struct file *pfile,char __user *puser,size_t count,loff_t *p_pos)
{
	struct mychar_dev *pmydev = (struct mychar_dev *)pfile->private_data;
	int size = 0;
	int ret = 0;

	mutex_lock(&pmydev->lock);
	if(pmydev->curlen <= 0)
	{
		if(pfile->f_flags & O_NONBLOCK)
		{//非阻塞
			mutex_unlock(&pmydev->lock);
			printk("O_NONBLOCK No Data Read\n");
			return -1;
		}
		else
		{//阻塞
			mutex_unlock(&pmydev->lock);
			ret = wait_event_interruptible(pmydev->rq,pmydev->curlen > 0);
			if(ret)
			{
				printk("Wake up by signal\n");
				return -ERESTARTSYS;
			}
			mutex_lock(&pmydev->lock);
		}
	}

	if(count > pmydev->curlen)
	{
		size = pmydev->curlen;
	}
	else
	{
		size = count;
	}

	ret = copy_to_user(puser,pmydev->mydev_buf,size);
	if(ret)
	{
		mutex_unlock(&pmydev->lock);
		printk("copy_to_user failed\n");
		return -1;
	}

	memcpy(pmydev->mydev_buf,pmydev->mydev_buf + size,pmydev->curlen - size);

	pmydev->curlen -= size;

	mutex_unlock(&pmydev->lock);

	wake_up_interruptible(&pmydev->wq);
	return size;
}

ssize_t mychar_write(struct file *pfile,const char __user *puser,size_t count,loff_t *p_pos)
{
	int size = 0;
	int ret = 0;
	struct mychar_dev *pmydev = (struct mychar_dev *)pfile->private_data;

	mutex_lock(&pmydev->lock);
	if(pmydev->curlen >= BUF_LEN)
	{
		if(pfile->f_flags & O_NONBLOCK)
		{
			mutex_unlock(&pmydev->lock);
			printk("O_NONBLOCK Can not write data\n");
			return -1;
		}
		else
		{
			mutex_unlock(&pmydev->lock);
			ret = wait_event_interruptible(pmydev->wq,pmydev->curlen < BUF_LEN);
			if(ret)
			{
				printk("wake up by signal\n");
				return -ERESTARTSYS;
			}
			mutex_lock(&pmydev->lock);
		}
	}

	if(count > BUF_LEN-pmydev->curlen)
	{
		size = BUF_LEN - pmydev->curlen;
	}
	else
	{
		size = count;
	}

	ret = copy_from_user(pmydev->mydev_buf + pmydev->curlen,puser,size);
	if(ret)
	{
		mutex_unlock(&pmydev->lock);
		printk("copy_from_user failed\n");
		return -1;
	}
	pmydev->curlen  +=  size;

	mutex_unlock(&pmydev->lock);

	wake_up_interruptible(&pmydev->rq);

	if(pmydev->pasync_obj != NULL)
	{
		kill_fasync(&pmydev->pasync_obj,SIGIO,POLL_IN);
	}

	return size;
}

long mychar_ioctl(struct file *pfile,unsigned int cmd,unsigned long arg)
{
	int __user *pret = (int *)arg;
	int maxlen = BUF_LEN;
	int ret = 0;
	struct mychar_dev *pmydev = (struct mychar_dev *)pfile->private_data;

	switch(cmd)
	{
		case MYCHAR_IOCTL_GET_MAXLEN:
			ret = copy_to_user(pret,&maxlen,sizeof(int));
			if(ret)
			{
				printk("copy_to_user MAXLEN failed\n");
				return -1;
			}
			break;
		case MYCHAR_IOCTL_GET_CURLEN:
			mutex_lock(&pmydev->lock);
			ret = copy_to_user(pret,&pmydev->curlen,sizeof(int));
			mutex_unlock(&pmydev->lock);
			if(ret)
			{
				printk("copy_to_user CURLEN failed\n");
				return -1;
			}
			break;
		default:
			printk("The cmd is unknow\n");
			return -1;

	}

	return 0;
}

unsigned int mychar_poll(struct file *pfile,poll_table *ptb)
{
	struct mychar_dev *pmydev = (struct mychar_dev *)pfile->private_data;
	unsigned int mask = 0;

	poll_wait(pfile,&pmydev->rq,ptb);
	poll_wait(pfile,&pmydev->wq,ptb);

	mutex_lock(&pmydev->lock);
	if(pmydev->curlen > 0)
	{
		mask |= POLLIN | POLLRDNORM;
	}
	if(pmydev->curlen < BUF_LEN)
	{
		mask |= POLLOUT | POLLWRNORM;
	}
	mutex_unlock(&pmydev->lock);

	return mask;
}

int mychar_fasync(int fd,struct file *pfile,int mode)
{
	struct mychar_dev *pmydev = (struct mychar_dev *)pfile->private_data;

	return fasync_helper(fd,pfile,mode,&pmydev->pasync_obj);
}

struct file_operations myops = {
	.owner = THIS_MODULE,
	.open = mychar_open,
	.release = mychar_close,
	.read = mychar_read,
	.write = mychar_write,
	.unlocked_ioctl = mychar_ioctl,
	.poll = mychar_poll,
	.fasync = mychar_fasync,
};

int __init mychar_init(void)
{
	int ret = 0;
	dev_t devno = MKDEV(major,minor);

	/*申请设备号*/
	ret = register_chrdev_region(devno,mychar_num,"mychar");
	if(ret)
	{
		ret = alloc_chrdev_region(&devno,minor,mychar_num,"mychar");
		if(ret)
		{
			printk("get devno failed\n");
			return -1;
		}
		major = MAJOR(devno);//容易遗漏，注意
	}

	pgmydev = (struct mychar_dev *)kmalloc(sizeof(struct mychar_dev),GFP_KERNEL);
	if(NULL == pgmydev)
	{
		unregister_chrdev_region(devno,mychar_num);
		printk("kmallc for struct mychar_dev failed\n");
		return -1;
	}

	/*给struct cdev对象指定操作函数集*/	
	cdev_init(&pgmydev->mydev,&myops);

	/*将struct cdev对象添加到内核对应的数据结构里*/
	pgmydev->mydev.owner = THIS_MODULE;
	cdev_add(&pgmydev->mydev,devno,mychar_num);


	init_waitqueue_head(&pgmydev->rq);
	init_waitqueue_head(&pgmydev->wq);

	mutex_init(&pgmydev->lock);
	return 0;
}

void __exit mychar_exit(void)
{
	dev_t devno = MKDEV(major,minor);

	cdev_del(&pgmydev->mydev);

	unregister_chrdev_region(devno,mychar_num);

	kfree(pgmydev);
	pgmydev = NULL;
}


MODULE_LICENSE("GPL");

module_init(mychar_init);
module_exit(mychar_exit);
