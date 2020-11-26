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

#include "minesweeper.h"


// TODO: Add the possibility of setting row/col/bomb count on load time.

MODULE_AUTHOR("Rodrigo Amaral Franceschinelli");
MODULE_LICENSE("Dual BSD/GPL"); // TODO: Which license?

struct minesweeper_dev device;
int minesweeper_major;
int minesweeper_minor = 0;
int device_count = 1;
char debug_buffer[80];
char *debug_ptr;


int debug(const char *format, ...)
{
  int result;
  va_list args;

  va_start(args, format);
  result = vsprintf(debug_buffer, format, args);
  va_end(args);
  return result;
}

ssize_t minesweeper_read_procmem(struct file *filp, char *buff, size_t size, loff_t *offset)
{
  int written = 0;

  if (*debug_ptr == 0) {return 0;}

  while (size && *debug_ptr) {
    put_user(*(debug_ptr++), buff++);

    size--;
    written++;
  }

  return written;
}

static int minesweeper_open_procmem(struct inode *nodep, struct file *filp)
{
  debug("Testando %s", "teste \n");
  debug_ptr = debug_buffer;
  return 0;
}

static const struct file_operations minesweeper_proc_fops =
{
  .owner = THIS_MODULE,
  .open = minesweeper_open_procmem,
  .read = minesweeper_read_procmem,
};

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
	device.board[position] = OPEN_CELL;
}

void create_board(void)
{
	int i;

	printk("[[[[[MINESWEEPER]]]]] Creating board");
	if (!device.board)
	{
		device.board = kmalloc(sizeof(char) * BOARD_SZ, GFP_KERNEL);
		if (!device.board)
		{
			printk("[[[[[MINESWEEPER]]]]] There is no memory enough to create a new board.");
			return;
		}
	}

	for (i = 0; i < BOARD_SZ; ++i)
		device.board[i] = NOT_OPEN_CELL;
}

void generate_bomb_positions(void)
{
	int i;

	printk("[[[[[MINESWEEPER]]]]] Generating bombs");
	if (!device.bomb_positions)
	{
		device.bomb_positions = kmalloc(sizeof(int) * BOMB_COUNT, GFP_KERNEL);
		if (!device.bomb_positions)
		{
			printk("[[[[[MINESWEEPER]]]]] There is no memory enough to create the list of bomb positions.");
			return;
		}
	}

	// TODO: Use a random function to generate the bombs.
	for (i = 1; i <= BOMB_COUNT; ++i)
	{
		device.bomb_positions[i - 1] = i * 3;
	}
}

void restart_game(void)
{
	generate_bomb_positions();
	create_board();
	device.game_loop = true;
}

/**
 * Read the board sequentially.
*/
ssize_t minesweeper_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
	printk("[[[[[MINESWEEPER]]]]] READ");

	if (!device.game_loop)
		restart_game();

	//TODO: Add a \n at the end of each row.
	if (copy_to_user(buf, device.board, BOARD_SZ)) {
		return -EFAULT;
	}

	return count;
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
		return -EFAULT;
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
		return -EINVAL;
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

static void minesweeper_setup_cdev(void)
{
	int err, devno = MKDEV(minesweeper_major, minesweeper_minor);

	cdev_init(&device.cdev, &minesweeper_fops);
	device.cdev.owner = THIS_MODULE;

	err = cdev_add(&device.cdev, devno, device_count);
	if (err)
		printk("[[[[[MINESWEEPER]]]]] Error %d adding minesweeper", err);
}

int minesweeper_init_module(void)
{
	int result;
	dev_t dev = 0;

	result = alloc_chrdev_region(&dev, minesweeper_minor, device_count, "minesweeper");
	minesweeper_major = MAJOR(dev);
	if (result < 0) {
		printk(KERN_WARNING "minesweeper: can't get major %d\n", minesweeper_major);
		return result;
	}

  proc_create("minesweepermem", 0 /* default mode */, NULL /* parent dir */, &minesweeper_proc_fops);
	device.game_loop = false;
	minesweeper_setup_cdev();

	return result;
}

void minesweeper_cleanup_module(void)
{
	dev_t devno = MKDEV(minesweeper_major, minesweeper_minor);

	printk("[[[[[MINESWEEPER]]]]] CLEAN UP");

	if (device.board)
		kfree(device.board);
	if (device.bomb_positions)
		kfree(device.bomb_positions);
  remove_proc_entry("minesweepermem", NULL);
	cdev_del(&device.cdev);
	unregister_chrdev_region(devno, device_count);
}

module_init(minesweeper_init_module);
module_exit(minesweeper_cleanup_module);
