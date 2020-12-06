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


// TODO: Add the possibility of setting row/col/bomb count on load time.
MODULE_AUTHOR("Rodrigo Amaral Franceschinelli");
MODULE_LICENSE("Dual BSD/GPL");

struct minesweeper_dev device;
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

void set_lost(void) 
{
	device.game_state = END_GAME;
	memset(device.board, 0, sizeof(char) * BOARD_SZ);
	memcpy(device.board, lost_str, sizeof(lost_str));
}

void display_won(void) 
{
	device.game_state = END_GAME;
	memset(device.board, 0, sizeof(char) * BOARD_SZ);
	memcpy(device.board, won_str, sizeof(won_str));
}

bool has_bomb(int position)
{
	int i;
	for (i = 0; i < device.bomb_count; ++i)
		if (device.bomb_positions[i] == position)
			return true;
	return false;
}

bool valid_coords(int row, int col)
{
	return row >= 0 && row < board_h && col >= 0 && col < board_w;
}

int get_position_row(int position)
{
	return position / board_h;
}

int get_position_col(int position)
{
	return position % board_w;
}

int to_position(int row, int col)
{
	return row * board_w + col;
}

int surround_bomb_count(int position)
{
	int i, pos_i, pos_j, adj_i, adj_j, count = 0;

	pos_i = get_position_row(position);
	pos_j = get_position_col(position);

	for (i = 0; i < EIGHT_ADJ; ++i)
	{
		adj_i = pos_i + adj_cells[i][0];
		adj_j = pos_j + adj_cells[i][1];
		if (valid_coords(adj_i, adj_j) && has_bomb(to_position(adj_i, adj_j)))
			count++;
	}
		
	return count;
}

/**
 * Function that conducts a BFS to reveal all the blank cells (no bombs around) up to a limit.
*/
void reveal_blank_cells(int position, int reveal_limit)
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

		pos_i = get_position_row(curr_position);
		pos_j = get_position_col(curr_position);

		for (i = 0; i < FOUR_ADJ && reveal_limit; ++i)
		{
			adj_i = pos_i + adj_cells[i][0];
			adj_j = pos_j + adj_cells[i][1];
			adj_position = to_position(adj_i, adj_j);
			if (valid_coords(adj_i, adj_j) && !has_bomb(adj_position))
			{
				device.board[adj_position] = surround_bomb_count(adj_position) + '0';
				if (device.board[adj_position] == '0')
				{
					--reveal_limit;
					device.board[adj_position] = OPEN_CELL;
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

void exec_play(int position) 
{
	int surrouding_bomb_count;
	
	device.board[position] = OPEN_CELL;
	if (has_bomb(position))
	{
		set_lost();
		return;
	}

	surrouding_bomb_count = surround_bomb_count(position);
	if (surrouding_bomb_count > 0)
		device.board[position] = surrouding_bomb_count + '0';

	reveal_blank_cells(position, BLANK_REVEAL_LIMIT);
}

void create_board(void)
{
	int i;
	unsigned long board_size = device.board_w * device.board_h;

	printk("[[[[[MINESWEEPER]]]]] Creating board");
	if (!device.board) 
	{
		device.board = kmalloc(sizeof(char) * board_size, GFP_KERNEL);
		if (!device.board)
		{
			printk("[[[[[MINESWEEPER]]]]] There is no memory enough to create a new board.");
			return;
		}
	}
	
	for (i = 0; i < board_size; ++i)
		device.board[i] = NOT_OPEN_CELL;
}

void generate_bomb_positions(void) 
{
	int i, j;
	unsigned int random_position;
	bool already_used;

	printk("[[[[[MINESWEEPER]]]]] Generating bombs");
	if (!device.bomb_positions) 
	{
		device.bomb_positions = kmalloc(sizeof(int) * device.bomb_count, GFP_KERNEL);
		if (!device.bomb_positions)
		{
			printk("[[[[[MINESWEEPER]]]]] There is no memory enough to create the list of bomb positions.");
			return;
		}
	}

	// TODO: Use a random function to generate the bombs.
	for (i = 1; i <= device.bomb_count; ++i)
	{
		get_random_bytes(&random_position, sizeof(random_position));
		random_position %= BOARD_SZ;

		already_used = false;
		for (j = 0; j < i - 1; ++j)
			if (device.bomb_positions[j] == random_position)
			{
				already_used = true;
				break;
			}
		
		if (already_used)
			--i;
		else
			device.bomb_positions[i - 1] = random_position;
	}
}

void restart_game(void) 
{
	create_board();
	generate_bomb_positions();
	device.game_state = ONGOING_GAME;
}

void prepend_board_dimensions(char *buf)
{
	unsigned long board_size = device.board_w * device.board_h;
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
	long board_size = device.board_w * device.board_h;
	long write_size = board_size + BOARD_DIM_COUNT;
	char *write_buf = kmalloc(board_size * sizeof(char) + BOARD_DIM_COUNT, GFP_KERNEL);
	printk("[[[[[MINESWEEPER]]]]] READ");

	if (device.game_state == NEW_GAME)
		restart_game();

	prepend_board_dimensions(write_buf);
	if (copy_to_user(buf, write_buf, write_size)) 
		return -EFAULT;

	kfree(write_buf);

	if (device.game_state == END_GAME)
		device.game_state = NEW_GAME;
	
	return write_size;
}

ssize_t minesweeper_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
	char *play_buf;
	long play;

	printk("[[[[[MINESWEEPER]]]]] WRITE (count=%ld)", count);

	play_buf = kmalloc(sizeof(char) * count, GFP_KERNEL);
	if (!play_buf)
	{
		printk("[[[[[MINESWEEPER]]]]] WRITE, failed to allocate play_buf");
		return 0;
	}

	if (device.game_state == NEW_GAME)
		restart_game();

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

	printk(KERN_INFO "[[[[[MINESWEEPER]]]]] (board_w, board_h): (%d, %d)", board_w, board_h);

	device.board_w = (char)board_w;
	device.board_h = (char)board_h;
	device.bomb_count = (char)bomb_count;
	device.game_state = NEW_GAME;
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
	cdev_del(&device.cdev);
	unregister_chrdev_region(devno, device_count);
}

module_init(minesweeper_init_module);
module_exit(minesweeper_cleanup_module);
