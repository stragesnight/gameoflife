/*
 * 	game of life implementation
 * 	by ishiki
 *
 *	controls:
 *	  r - reset game field
 *	  p - pause
 *	  x - exit
 */

#include <time.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <termios.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <stdatomic.h>

#define ITERATIONS 50000

typedef unsigned char bool;
#define true (bool)1
#define false (bool)0

static struct winsize term_size;		// terminal buffer size
static pthread_t pthread_input_id;		// input catcher thread
static volatile atomic_uint main_lock = 0; 	// lock for main thread
static volatile atomic_uint keep_running = 1; 	// controls when program should terminate

static char **prev_field; 	// previous state of the game field
static char **field; 		// current state of the game field

// initialize terminal IO
static void init_termios(void)
{
	struct termios tios;
	
	tcgetattr(0, &tios);
	tios.c_lflag &= ~ICANON; 	// don't ask for caret return for input
	tios.c_lflag &= ~ECHO; 		// don't echo input
	tcsetattr(0, TCSANOW, &tios);
}

// reset terminal IO state to default
static inline void reset_termios(void)
{
	struct termios tios;
	
	tcgetattr(0, &tios);
	tios.c_lflag |= ICANON;
	tios.c_lflag |= ECHO;
	tcsetattr(0, TCSANOW, &tios);
}

// custom exit routine
static void my_exit(void)
{
	printf("exit\n");
	// free allocated game field memory
	for (int i = 0; i < term_size.ws_row; ++i) {
		free(field[i]);
		free(prev_field[i]);
	}
	free(field);
	free(prev_field);

#ifndef SPEED_TEST
	// reset terminal IO
	reset_termios();
	// kill input catcher thread
	pthread_kill(pthread_input_id, 0);
#endif /* SPEED_TEST */
	// move cursor to [1;1]
	puts("\033[1;1H\033[J");
	// exit program
	exit(EXIT_SUCCESS);
}

// SIGINT catcher
static void catch_sigint(int signo)
{
	// terminate program
	keep_running = 0;
}

// initialize game field with random cells
static inline void init_field(void)
{
	for (ushort y = 0; y < term_size.ws_row; ++y) {
		for (ushort x = 0; x < term_size.ws_col; ++x)
			prev_field[y][x] = (rand() & 1) ? '#' : ' ';
	}
}

// input catcher thread
void *pthread_input(void *args)
{
	fflush(stdin);
	while (keep_running)
	{
		char c = getchar();

		switch (c)
		{
		case 'r':	// reset
			main_lock = 1;
			init_field();
			main_lock = 0;
			break;
		case 'p':	// pause
			main_lock ^= 1;
			printf("pause");
			break;
		case 'x':	// exit
			my_exit();
			break;
		default:
			break;
		}
	}

	return NULL;
}

// get cell state at given coordinates
static char get_cell(int x, int y)
{
	// wrap coordinates if out of bounds
	if (x >= term_size.ws_col)
		x = 0;
	else if (x < 0)
		x = term_size.ws_col - 1;
	if (y >= term_size.ws_row)
		y = 0;
	else if (y < 0)
		y = term_size.ws_row - 1;

	return prev_field[y][x];
}

// get number of alive neighbours around given coordinates
static inline ushort get_nneighbours(int x, int y)
{
	int nneighbours = 0;

	nneighbours += get_cell(x + 1, y) != ' ';
	nneighbours += get_cell(x - 1, y) != ' ';
	nneighbours += get_cell(x, y + 1) != ' ';
	nneighbours += get_cell(x, y - 1) != ' ';
	nneighbours += get_cell(x + 1, y + 1) != ' ';
	nneighbours += get_cell(x + 1, y - 1) != ' ';
	nneighbours += get_cell(x - 1, y + 1) != ' ';
	nneighbours += get_cell(x - 1, y - 1) != ' ';

	return nneighbours;
}

// iteration of game of life
static inline void iteration()
{
	// move cursor to [1;1]
	puts("\033[1;1H\033[J");

	for (ushort y = 0; y < term_size.ws_row; ++y) {
		for (ushort x = 0; x < term_size.ws_col; ++x) {
			// cell is alive if:
			// - it has 3 neighbours, or
			// - it has 2 neighbours and is currently alive
			unsigned n = get_nneighbours(x, y);
			bool alive = get_cell(x, y) != ' ';
			char c = ((n == 2 && alive) || n == 3) ? '#' : ' ';
			field[y][x] = c;
			putchar(field[y][x]);
		}
		putchar('\n');
	}
	putchar('\b');

	// copy previous state to current
	for (ushort y = 0; y < term_size.ws_row; ++y)
		memcpy(prev_field[y], field[y], term_size.ws_col);
}

int main(void)
{
	// get console buffer size
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &term_size);

#ifndef SPEED_TEST
	// initialize console
	init_termios();
	// initialize atomic variables
	main_lock = ATOMIC_VAR_INIT(0);
	keep_running = ATOMIC_VAR_INIT(1);
	// start input catcher thread
	pthread_create(&pthread_input_id, NULL, pthread_input, NULL);
	// catch SIGINT
	signal(SIGINT, catch_sigint);
#endif /* SPEED_TEST */

	srand(time(0));

	// last row is ommited to avoid jitter
	term_size.ws_row -= 1;

	// allocate game field memory
	field = malloc(term_size.ws_row * sizeof(char *));
	prev_field = malloc(term_size.ws_row * sizeof(char *));
	for (ushort i = 0; i < term_size.ws_row; ++i) {
		field[i] = malloc(term_size.ws_col);
		prev_field[i] = malloc(term_size.ws_col);

		memset(field[i], 0, term_size.ws_col);
		memset(prev_field[i], 0, term_size.ws_col);
	}

	init_field();

	// main loop
#ifdef SPEED_TEST
	for (size_t i = 0; i < ITERATIONS; ++i) {
#else /* !SPEED_TEST */
	while (keep_running) {
#endif /* SPEED_TEST */
#ifndef SPEED_TEST
		// wait for thread to be unlocked
		while (main_lock)
			usleep(10);
#endif /* SPEED_TEST */

		// iterate game state
		iteration();

#ifndef SPEED_TEST
		// sleep for 50 ms
		usleep(50000);
#endif /* SPEED_TEST */
	}

	my_exit();
}

