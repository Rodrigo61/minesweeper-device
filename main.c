#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/kernel.h>	
#include <linux/slab.h>		
#include <linux/fs.h>		
#include <linux/errno.h>	
#include <linux/types.h>	
#include <linux/proc_fs.h>
#include <linux/fcntl.h>	
#include <linux/seq_file.h>
#include <linux/cdev.h>

#include <linux/uaccess.h>	
#include <linux/moduleparam.h>

#include "minesweeper.h"


// TODO: Add the possibility of setting row/col/bomb count on load time.

MODULE_AUTHOR("Rodrigo Amaral Franceschinelli");
MODULE_LICENSE("Dual BSD/GPL"); // TODO: Which license?

// struct minesweeper_dev device;
struct minesweeper_dev devices[MAX_DEV_COUNT];
int minesweeper_major;
int minesweeper_minor = 0;
int device_count = 1;
static int board_w, board_h, bomb_count;

module_param(board_w, int, S_IRUGO);
module_param(board_h, int, S_IRUGO);
module_param(bomb_count, int, S_IRUGO);

MODULE_PARM_DESC(board_w, "Width of the minesweeper board");
MODULE_PARM_DESC(board_h, "Height of the minesweeper board");
MODULE_PARM_DESC(bomb_count, "Count of bombs in minesweeper board");

int minesweeper_open(struct inode *inode, struct file *filp)
{
	printk("[[[[[MINESWEEPER]]]]] OPEN");
	return 0;
}

int minesweeper_release(struct inode *inode, struct file *filp)
{
	printk("[[[[[MINESWEEPER]]]]] RELEASE");
	return 0;
}

void exec_play(int position) 
{
	// TODO: Verify if a bomb has exploded and interrupt the game loop.
	devices[0].board[position] = OPEN_CELL;
}

void create_board(struct minesweeper_dev *device)
{
	int i;
	unsigned long board_size = device->board_w * device->board_h;

	printk("[[[[[MINESWEEPER]]]]] Creating board");
	if (!device->board) 
	{
		device->board = kmalloc(sizeof(char) * board_size, GFP_KERNEL);
		if (!device->board)
		{
			printk("[[[[[MINESWEEPER]]]]] There is no memory enough to create a new board.");
			return;
		}
	}
	
	for (i = 0; i < board_size; ++i)
		device->board[i] = NOT_OPEN_CELL;
}

void generate_bomb_positions(struct minesweeper_dev *device) 
{
	int i;

	printk("[[[[[MINESWEEPER]]]]] Generating bombs");
	if (!device->bomb_positions) 
	{
		device->bomb_positions = kmalloc(sizeof(int) * bomb_count, GFP_KERNEL);
		if (!device->bomb_positions)
		{
			printk("[[[[[MINESWEEPER]]]]] There is no memory enough to create the list of bomb positions.");
			return;
		}
	}

	// TODO: Use a random function to generate the bombs.
	for (i = 1; i <= bomb_count; ++i)
	{
		device->bomb_positions[i - 1] = i * 3;
	}
}

void restart_game(struct minesweeper_dev *device)
{
	generate_bomb_positions(device);
	create_board(device);
	device->game_loop = true;
}

void prepend_board_dimensions(char *buf, struct minesweeper_dev device)
{
	int board_size = device.board_w * device.board_h;
	size_t i;
	buf[0] = device.board_w;
	buf[1] = device.board_h;
	
	for (i = 0; i < board_size; ++i)
	{
		buf[i + BOARD_DIM_COUNT] = device.board[i];
	}
}

/**
 * Read the board sequentially.
*/
ssize_t minesweeper_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
	struct minesweeper_dev device = devices[0];
	int board_size = device.board_w * device.board_h;
	int write_size = board_size + BOARD_DIM_COUNT;
	char *write_buf = kmalloc(write_size * sizeof(char), GFP_KERNEL);
	printk("[[[[[MINESWEEPER]]]]] READ");

	if (!device.game_loop)
		restart_game(&devices[0]);

	printk("[[[[[MINESWEEPER]]]]] WRITE_SIZE (%d)", write_size);
	prepend_board_dimensions(write_buf, device);
	if (copy_to_user(buf, write_buf, write_size)) {
		return -EFAULT;
	}
	kfree(write_buf);
	return write_size;
}

ssize_t minesweeper_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
	char *play_buf;
	long play;

	printk("[[[[[MINESWEEPER]]]]] WRITE");

	play_buf = kmalloc(sizeof(char) * count, GFP_KERNEL);
	if (!play_buf)
	{
		printk("[[[[[MINESWEEPER]]]]] WRITE, failed to allocate play_buf");
		return 0;
	}

	if (copy_from_user(play_buf, buf, count)) 
	{
		printk("[[[[[MINESWEEPER]]]]] WRITE, failed to copy from user");
		kfree(play_buf);
		return -EFAULT;
	} 

	play_buf[count] = '\0';
	printk("[[[[[MINESWEEPER]]]]] WRITE, play_buf = %s", play_buf);	

	if (kstrtol(play_buf, 10, &play) != 0)
	{
		printk("[[[[[MINESWEEPER]]]]] WRITE, failed to convert play_buf");
		kfree(play_buf);
		return 0; // TODO: Search what would be the correct way of reporting this error.
	}

	printk("[[[[[MINESWEEPER]]]]] WRITE, play = %ld\n", play);

	exec_play(play);
	kfree(play_buf);

	printk("[[[[[MINESWEEPER]]]]] SUCCESSFUL WRITE\n");
	return count;
}

struct file_operations minesweeper_fops = {
	.owner =    THIS_MODULE,
	.read =     minesweeper_read,
	.write =    minesweeper_write,
	.open =     minesweeper_open,
	.release =  minesweeper_release,
};

void init_device_board(struct minesweeper_dev *dev)
{
	dev->game_loop = false;
	dev->board_w = (char)board_w;
	dev->board_h = (char)board_h;
	dev->bomb_count = (char)bomb_count;
}

static void minesweeper_setup_cdev(struct minesweeper_dev *dev, int minor)
{
	int err, devno = MKDEV(minesweeper_major, minor);
	cdev_init(&dev->cdev, &minesweeper_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->devno = devno;
	err = cdev_add(&dev->cdev, devno, 1);
	init_device_board(dev);
	if (err)
		printk("[[[[[MINESWEEPER]]]]] Error %d adding minesweeper", err);
	else
		printk("[[[[[MINESWEEPER]]]]] device (%d, %d) added", minesweeper_major, minor);
}

int minesweeper_init_module(void)
{
	int result;
	int i_minor;
	dev_t dev = 0;

	result = alloc_chrdev_region(&dev, minesweeper_minor, MAX_DEV_COUNT, "minesweeper");
	minesweeper_major = MAJOR(dev);
	if (result < 0) {
		printk(KERN_WARNING "minesweeper: can't get major %d\n", minesweeper_major);
		return result;
	}
	
	for (i_minor = 0; i_minor < MAX_DEV_COUNT; i_minor++) {
		minesweeper_setup_cdev(&devices[i_minor], i_minor);
	}
	printk("[[[[[MINESWEEPER]]]]] board (%d, %d)", (int)devices[0].board_w, (int)devices[0].board_h);
	return result;
}

void minesweeper_cleanup_module(void)
{
	int minor;
	struct minesweeper_dev target_dev;
	for (minor = 0; minor < MAX_DEV_COUNT; minor++) {
		target_dev = devices[minor];
		if (target_dev.board)
			kfree(target_dev.board);
		if (target_dev.bomb_positions)
			kfree(target_dev.bomb_positions);
		cdev_del(&devices[0].cdev);
		printk("[[[[[MINESWEEPER]]]]] device (%d, %d) unregistered", minesweeper_major, minor);
	}
	unregister_chrdev_region(devices[0].devno, MAX_DEV_COUNT);
	printk("[[[[[MINESWEEPER]]]]] CLEAN UP");
}

module_init(minesweeper_init_module);
module_exit(minesweeper_cleanup_module);
