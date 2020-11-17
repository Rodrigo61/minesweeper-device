#ifndef _MINESWEEPER_H_
#define _MINESWEEPER_H_

/*
 * Board dimensions.
 */
#ifndef ROW_COUNT
#define ROW_COUNT 10
#endif
#ifndef COL_COUNT
#define COL_COUNT 10
#endif
#ifndef BOARD_SZ
#define BOARD_SZ ROW_COUNT * COL_COUNT
#endif
#ifndef BOMB_COUNT
#define BOMB_COUNT 30
#endif
#ifndef NOT_OPEN_CELL
#define NOT_OPEN_CELL 'X'
#endif
#ifndef OPEN_CELL
#define OPEN_CELL ' '
#endif

struct minesweeper_dev {
	struct cdev cdev;
    char *board;
    int *bomb_positions;
    bool game_loop;
};

/*
 * Prototypes for shared functions
 */
int minesweeper_p_init(dev_t dev);
void minesweeper_p_cleanup(void);
ssize_t minesweeper_read(struct file *filp, char __user *buf, size_t count,
                   loff_t *f_pos);
ssize_t minesweeper_write(struct file *filp, const char __user *buf, size_t count,
                    loff_t *f_pos);

#endif /* _MINESWEEPER_H_ */
