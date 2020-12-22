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
#include <linux/list.h>
#include <linux/random.h>

#include <linux/uaccess.h>	
#include <linux/moduleparam.h>

#include "minesweeper.h"

#define DBG_BUFF_SZ 4096

MODULE_AUTHOR("Rodrigo Amaral Franceschinelli");
MODULE_LICENSE("Dual BSD/GPL");

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

char lost_str[] = "YOU LOST";
char won_str[] = "YOU WON";

// Offset to ease the adjacency calculation. The first 4 values correspond to the 
// 4-side adjacency, the next 4 the remaining to the 8-side adjacency.
int adj_cells[8][2] = {{-1, 0}, {0, -1}, {0, 1}, {1, 0}, {-1, -1}, {-1, 1}, {1, -1}, {1, 1}};

struct queue_node {
    struct list_head list;
    int value;
};

LIST_HEAD(queue);

char debug_buffer[DBG_BUFF_SZ];
char *debug_ptr;
int debug_offset=0;

int debug(const char *format, ...)
{
  int result;
  va_list args;

  va_start(args, format);
  // If the message would overflow the buffer, wraps around and overwrites it
  if(strlen(format)+debug_offset > DBG_BUFF_SZ){
    debug_offset = 0;
  }
  result = vsprintf(debug_buffer+debug_offset, format, args);
  debug_offset += result;
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
	struct minesweeper_dev *dev;

	debug("[[[[[MINESWEEPER]]]]] OPEN");
	dev = container_of(inode->i_cdev, struct minesweeper_dev, cdev);
	filp->private_data = dev; /* for other methods */
	return 0;
}

int minesweeper_release(struct inode *inode, struct file *filp)
{
	debug("[[[[[MINESWEEPER]]]]] RELEASE");
	return 0;
}

void set_lost(struct minesweeper_dev *device) 
{
	device->game_state = END_GAME;
	memset(device->board, 0, sizeof(char) * device->board_size);
	memcpy(device->board, lost_str, sizeof(lost_str));
}

void display_won(struct minesweeper_dev *device) 
{
	device->game_state = END_GAME;
	memset(devices->board, 0, sizeof(char) * device->board_size);
	memcpy(devices->board, won_str, sizeof(won_str));
}

bool has_bomb(int position, struct minesweeper_dev *device)
{
	int i;
	for (i = 0; i < device->bomb_count; ++i)
		if (device->bomb_positions[i] == position)
			return true;
	return false;
}

bool valid_coords(int row, int col, struct minesweeper_dev *device)
{
	return row >= 0 && row < device->board_h && col >= 0 && col < device->board_w;
}

int get_position_row(int position, struct minesweeper_dev *device)
{
	return position / device->board_h;
}

int get_position_col(int position, struct minesweeper_dev *device)
{
	return position % device->board_w;
}

int to_position(int row, int col, struct minesweeper_dev *device)
{
	return row * device->board_w + col;
}

int surround_bomb_count(int position, struct minesweeper_dev *device)
{
	int i, pos_i, pos_j, adj_i, adj_j, count = 0;

	pos_i = get_position_row(position, device);
	pos_j = get_position_col(position, device);

	for (i = 0; i < EIGHT_ADJ; ++i)
	{
		adj_i = pos_i + adj_cells[i][0];
		adj_j = pos_j + adj_cells[i][1];
		if (valid_coords(adj_i, adj_j, device) && has_bomb(to_position(adj_i, adj_j, device), device))
			count++;
	}
		
	return count;
}

/**
 * Function that conducts a BFS to reveal all the blank cells (no bombs around) up to a limit.
*/
void reveal_blank_cells(int position, int reveal_limit, struct minesweeper_dev *device)
{
    struct queue_node *entry, *new_entry;
	int curr_position, adj_position, i, pos_i, pos_j, adj_i, adj_j;

	new_entry = kmalloc(sizeof(struct queue_node), GFP_KERNEL); 
	if (!new_entry)
		return;
	new_entry->value = position;
	list_add_tail(&new_entry->list, &queue);

	while (!list_empty(&queue) && reveal_limit)
	{
		entry = list_first_entry(&queue, struct queue_node, list);
		curr_position = entry->value;
		list_del(queue.next);
		kfree(entry);

		pos_i = get_position_row(curr_position, device);
		pos_j = get_position_col(curr_position, device);

		for (i = 0; i < FOUR_ADJ && reveal_limit; ++i)
		{
			adj_i = pos_i + adj_cells[i][0];
			adj_j = pos_j + adj_cells[i][1];
			adj_position = to_position(adj_i, adj_j, device);
			if (valid_coords(adj_i, adj_j, device) && !has_bomb(adj_position, device))
			{
				device->board[adj_position] = surround_bomb_count(adj_position, device) + '0';
				if (device->board[adj_position] == '0')
				{
					--reveal_limit;
					device->board[adj_position] = OPEN_CELL;
					new_entry = kmalloc(sizeof(struct queue_node), GFP_KERNEL); 
					if (new_entry) 
					{
						new_entry->value = adj_position;
						list_add_tail(&new_entry->list, &queue);
					}
					
				}
			}
		}
	}

	// Clean the queue.
	while (!list_empty(&queue))
	{
		entry = list_first_entry(&queue, struct queue_node, list);
		list_del(queue.next);
		kfree(entry);
	}
}

void exec_play(int position, struct minesweeper_dev *device) 
{
	int surrouding_bomb_count;
	
	device->board[position] = OPEN_CELL;
	if (has_bomb(position, device))
	{
		set_lost(device);
		return;
	}

	surrouding_bomb_count = surround_bomb_count(position, device);
	if (surrouding_bomb_count > 0)
		device->board[position] = surrouding_bomb_count + '0';

	reveal_blank_cells(position, BLANK_REVEAL_LIMIT, device);
}

void create_board(struct minesweeper_dev *device)
{
	int i;
	unsigned long board_size = device->board_w * device->board_h;

	debug("[[[[[MINESWEEPER]]]]] Creating board");
	if (!device->board) 
	{
		device->board = kmalloc(sizeof(char) * board_size, GFP_KERNEL);
		if (!device->board)
		{
			debug("[[[[[MINESWEEPER]]]]] There is no memory enough to create a new board.");
      debug("Not enough memory to create board\n");
			return;
		}
	}
	
	for (i = 0; i < board_size; ++i)
		device->board[i] = NOT_OPEN_CELL;
}

void generate_bomb_positions(struct minesweeper_dev *device) 
{
	int i, j;
	unsigned int random_position;
	bool already_used;

	debug("[[[[[MINESWEEPER]]]]] Generating bombs");
	if (!device->bomb_positions) 
	{
		device->bomb_positions = kmalloc(sizeof(int) * device->bomb_count, GFP_KERNEL);
		if (!device->bomb_positions)
		{
			debug("[[[[[MINESWEEPER]]]]] There is no memory enough to create the list of bomb positions.");
			return;
		}
	}

	// TODO: Use a random function to generate the bombs.
	for (i = 1; i <= device->bomb_count; ++i)
	{
		get_random_bytes(&random_position, sizeof(random_position));
		random_position %= device->board_size;

		already_used = false;
		for (j = 0; j < i - 1; ++j)
			if (device->bomb_positions[j] == random_position)
			{
				already_used = true;
				break;
			}
		
		if (already_used)
			--i;
		else
			device->bomb_positions[i - 1] = random_position;
	}
}

void restart_game(struct minesweeper_dev *device)
{
	generate_bomb_positions(device);
	create_board(device);
	device->game_state = ONGOING_GAME;
}

void prepend_board_dimensions(char *buf, struct minesweeper_dev *device)
{
	int board_size = device->board_w * device->board_h;
	size_t i;
	buf[0] = device->board_w;
	buf[1] = device->board_h;
	
	for (i = 0; i < board_size; ++i)
	{
		buf[i + BOARD_DIM_COUNT] = device->board[i];
	}
}

/**
 * Read the board sequentially.
*/
ssize_t minesweeper_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
	struct minesweeper_dev *device = filp->private_data;
	int board_size = device->board_w * device->board_h;
	int write_size = board_size + BOARD_DIM_COUNT;
	char *write_buf = kmalloc(write_size * sizeof(char), GFP_KERNEL);

	debug("[[[[[MINESWEEPER]]]]] READ");

	if (device->game_state == NEW_GAME)
		restart_game(device);

	debug("[[[[[MINESWEEPER]]]]] READ (write_size = %d)", write_size);
	prepend_board_dimensions(write_buf, device);
	if (copy_to_user(buf, write_buf, write_size))
		return -EFAULT;

	kfree(write_buf);

	if (device->game_state == END_GAME)
		device->game_state = NEW_GAME;
	
	return write_size;
}

ssize_t minesweeper_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
	char *play_buf;
	long play;
	struct minesweeper_dev *device = filp->private_data;

	debug("[[[[[MINESWEEPER]]]]] WRITE (count=%ld)", count);

	play_buf = kmalloc(sizeof(char) * count, GFP_KERNEL);
	if (!play_buf)
	{
		debug("[[[[[MINESWEEPER]]]]] WRITE, failed to allocate play_buf");
    debug("Failed to allocated play_buf");
		return -EFAULT;
	}

	if (device->game_state == NEW_GAME)
		restart_game(device);

	if (copy_from_user(play_buf, buf, count)) 
	{
		debug("[[[[[MINESWEEPER]]]]] WRITE, failed to copy from user");
    debug("Failed to copy data from user space\n");
		kfree(play_buf);
		return -EFAULT;
	}

	play_buf[count] = '\0';
	debug("[[[[[MINESWEEPER]]]]] WRITE, play_buf = %s", play_buf);

	if (kstrtol(play_buf, 10, &play) != 0)
	{
		debug("[[[[[MINESWEEPER]]]]] WRITE, failed to convert play_buf");
		kfree(play_buf);
		return -EINVAL;
	}

	debug("[[[[[MINESWEEPER]]]]] WRITE, play = %ld\n", play);

	exec_play(play, device);
	kfree(play_buf);

	debug("[[[[[MINESWEEPER]]]]] SUCCESSFUL WRITE\n");
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
	dev->board_w = (char)board_w;
	dev->board_h = (char)board_h;
	dev->board_size = board_w * board_h;
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
		debug("[[[[[MINESWEEPER]]]]] Error %d adding minesweeper", err);
	else
		debug("[[[[[MINESWEEPER]]]]] device (%d, %d) added", minesweeper_major, minor);
}

int minesweeper_init_module(void)
{
	int result;
	int i_minor;
	dev_t dev = 0;

	result = alloc_chrdev_region(&dev, minesweeper_minor, MAX_DEV_COUNT, "minesweeper");
	minesweeper_major = MAJOR(dev);
	if (result < 0) {
		debug(KERN_WARNING "minesweeper: can't get major %d\n", minesweeper_major);
		return result;
	}
	
	for (i_minor = 0; i_minor < MAX_DEV_COUNT; i_minor++) {
		minesweeper_setup_cdev(&devices[i_minor], i_minor);
	}
	debug("[[[[[MINESWEEPER]]]]] INITIATED");
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
		debug("[[[[[MINESWEEPER]]]]] device (%d, %d) unregistered", minesweeper_major, minor);
	}
	unregister_chrdev_region(devices[0].devno, MAX_DEV_COUNT);
	debug("[[[[[MINESWEEPER]]]]] CLEAN UP");
}

module_init(minesweeper_init_module);
module_exit(minesweeper_cleanup_module);
