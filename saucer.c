/*
 *  Copyright (c) 2013 Alexander Wong <admin@alexander-wong.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

/*
 * This is a heavily modified version of tanimate.c, provided by Bob Beck,
 * originally taken from kent.edu from grant macewan
 * 
 * Compile with "make saucer"
 * Run with "./saucer"
 */

#include	<stdio.h>
#include	<curses.h>
#include	<pthread.h>
#include	<stdlib.h>
#include	<unistd.h>
#include	<string.h>

#define MAXBLS  20		/* Number of bullet threads at once */
#define MAXSCR  10		/* Number of saucer threads at once */
#define	NUMALL	MAXBLS+MAXSCR+1	/* Number of threads running */
#define	TUNIT   20000		/* timeunits in microseconds */
#define NUMLOSE	10		/* Number of escaped units until loss */
#define ROWSCRS 10		/* Number of rows saucers can occupy from top */

struct	element {
	char	*str;
	int	type;	/* 0 for player, 1 for saucer, 2 for bullet */
	int	alive;  /* bullets and saucers, is alive? 1 or 0 */
	int	respawn;/* if dead, how many cycles to stay dead for? */
	int	row;	/* row */
	int	col;	/* column */
	int	delay;	/* delay in units of TUNIT */
};

int firebullet();
int moveplayer(int);
int setup(struct element *);
void *animate(void *);
int detectcollision();
int collisionbls(int);
int collisionscr(int);
int resetscr(void*);
int rowscrinit(void*);
int resetbls(void*);
int eledraw(void*);
int scrtxtdraw();
int eleclear(void*, int);

struct element	elements[NUMALL];
int score;
int escape;
pthread_mutex_t collisionlock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t scorelock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t escapelock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t firelock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t movelock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t drawlock = PTHREAD_MUTEX_INITIALIZER;

int main(int ac, char *av[])
{
	pthread_t	thrds[NUMALL];		/* the threads		*/
	void		*animate();		/* the function		*/
	int		num_msg ;		/* number of strings	*/
	int		c;			/* user input		*/
	int		i;

	setup(elements);

	/* create all the threads */
	for(i=0 ; i<NUMALL; i++)
		if ( pthread_create(&thrds[i], NULL, animate, &elements[i])){
			fprintf(stderr,"error creating thread");
			endwin();
			exit(0);
		}
	moveplayer('.');
	/* process user input */
	while(1) {
		c = getch();
		if (c == 'Q' || escape >= NUMLOSE) break;
		if (c == ' ')
			firebullet();
		if (c == ',' || c == '.')
			moveplayer(c);
	}
	
	/* cancel all the threads */
	pthread_mutex_lock(&drawlock);
	for (i=0; i<NUMALL; i++ )
		pthread_cancel(thrds[i]);
	endwin();
	printf("Your final score was: %d\n", score);
	return 0;
}

/* Fires a bullet from the player location */
int firebullet() {
	int i;
	pthread_mutex_lock(&firelock);
	for (i=0; i<NUMALL; i++) {
		if (elements[i].type == 2 && 
		    elements[i].alive == 0) {
			elements[i].row = LINES-3;
			elements[i].col = elements[0].col;
			elements[i].alive = 1;
			break;
		}	
	}
	pthread_mutex_unlock(&firelock);
	return 0;
}

/* Moves the player according to input */
int moveplayer(int c) {
	pthread_mutex_lock(&movelock);
	elements[0].row = LINES-2;
	if (c == ',' && elements[0].col > 0)
		elements[0].col--;
	if (c == '.'&& elements[0].col+3 < COLS) 
		elements[0].col++;
	pthread_mutex_unlock(&movelock);
	return 0;
}

/* Setup all elements and undeclared globals */
int setup(struct element elmts[])
{
	int i;
	srand(getpid());

	score = 0;
	escape = 0;

	/* Setup array of elements */
	for(i=0 ; i<NUMALL; i++){
		// setup the player
		if (i == 0) {
			elmts[i].str = "|";
			elmts[i].type = 0;
			elmts[i].alive = 1;
			elmts[i].respawn = 1;
			elmts[i].row = LINES-2;
			elmts[i].col = 1;
			elmts[i].delay = 1;
		}
		// setup the saucers
		else if (i < MAXSCR+1) {
			elmts[i].str = "<--->";
			elmts[i].type = 1;
			elmts[i].alive = 0;
			elmts[i].respawn = 1+(rand()%100);
			elmts[i].row = 1+(rand()%ROWSCRS);
			elmts[i].col = 0;
			elmts[i].delay = 1+(rand()%20);
		} 
		// setup the bullets
		else {
			elmts[i].str = "^";
			elmts[i].type = 2;
			elmts[i].alive = 0;
			elmts[i].respawn = 0;
			elmts[i].row = 0;
			elmts[i].col = 0;
			elmts[i].delay = 5;
		}
	}

	/* set up curses */
	initscr();
	crmode();
	noecho();
	clear();
	return 0;
}

/* the code that runs in each thread */
void *animate(void *arg)
{
	struct element *info = arg;		/* point to info block	*/
	int	len = strlen(info->str)+2;	/* +2 for padding	*/
	int	i;
	while( 1 )
	{
		detectcollision();
		usleep(info->delay*TUNIT);
		// Handle alive code
		if (info->alive) {
			// Handle player specific code, always alive
			if (info->type == 0) {
				scrtxtdraw();
				pthread_mutex_lock(&movelock);
				eledraw(arg);
				pthread_mutex_unlock(&movelock);
			}
			// Handle saucer code
			else if (info->type == 1) {
				eledraw(arg);
				info->col++;
				// end of screen, escape and die
				if (info->col+len >= COLS) {
					pthread_mutex_lock(&escapelock);
					escape++;
					pthread_mutex_unlock(&escapelock);
					eleclear(arg, len);
					resetscr(arg);
				}
			}
			// Handle bullet code
			else {
				eleclear(arg,len);
				pthread_mutex_lock(&firelock);
				info->row--;
				eledraw(arg);
				// top of screen, escape and die
				if (info->row <=0) {
					eleclear(arg,len);
					resetbls(arg);
				}
				pthread_mutex_unlock(&firelock);
			}
		}
		// Handle not alive code, player is never here
		else {
			// Handle saucer code
			if (info->type == 1) {
				eleclear(arg,len);
				info->respawn--;
				if (!info->respawn) {
					info->alive=1;
					rowscrinit(arg);
				}
			}
			// Handle bullet code
			else if (info->type == 2) {
				eleclear(arg, len);
			}
		}
	}
}

/* Handles checking if a bullet touched a saucer */
int detectcollision() {
	int saucer, bullet;
	pthread_mutex_lock(&collisionlock);
	for (saucer=1; saucer<MAXSCR+1; saucer++) {
		for (bullet=1+MAXSCR; bullet<NUMALL; bullet++) {
			if ((elements[saucer].alive == 1) &&
			    (elements[bullet].alive == 1) &&
			    (elements[saucer].row == elements[bullet].row) &&
			    (elements[bullet].col >= elements[saucer].col-1) &&
			    (elements[bullet].col <= 
			     (elements[saucer].col + 
			      strlen(elements[saucer].str)-1))) {
				collisionbls(bullet);
				collisionscr(saucer);
				pthread_mutex_lock(&scorelock);
				score++;
				pthread_mutex_unlock(&scorelock);
				pthread_mutex_unlock(&collisionlock);
				return 0;
			}
		}		
	}
	pthread_mutex_unlock(&collisionlock);
	return 0;
}

/* collision bullet handler */
int collisionbls(int index) {
	eleclear(&elements[index], strlen(elements[index].str)+2);
	resetbls(&elements[index]);
}

/* collision saucer handler */
int collisionscr(int index) {
	eleclear(&elements[index], strlen(elements[index].str)+2);
	resetscr(&elements[index]);
}

/* saucer reset */
int resetscr(void *arg) {
	struct element *info = arg;
	info->alive=0;
	info->respawn=1+(rand()%100);
	info->col=0;
	info->row=0;
	info->delay=1+(rand()%20);
}

/* saucer row re-init */
int rowscrinit(void *arg) {
	struct element *info = arg;
	info->row=1+(rand()%ROWSCRS);
}

/* bullet reset */
int resetbls(void *arg) {
	struct element *info = arg;
	info->alive = 0;
	info->col=0;
	info->row=0;
}

/* draws the element */
int eledraw(void *arg) {
	struct element *info = arg;
	pthread_mutex_lock(&drawlock);
	move( info->row, info->col );
	addch(' ');
	addstr( info->str );
	addch(' ');
	move(LINES-1,COLS-1);
	refresh();
	pthread_mutex_unlock(&drawlock);
}

/* score text draw */
int scrtxtdraw() {
	pthread_mutex_lock(&drawlock);
	mvprintw(LINES-1,0,"'Q' to quit ',' moves left "
		 "'.' moves right SPACE fires, "
		 "Missed: %d/%d, Score: %d",escape,NUMLOSE,score);
	if (escape >= NUMLOSE) {
		mvprintw(LINES-2,0,"Game over! Anykey to exit");
	}
	pthread_mutex_unlock(&drawlock);
}

/* clears the element */
int eleclear(void *arg, int len) {
	struct element *info = arg;
	int i;
	pthread_mutex_lock(&drawlock);
	move(info->row, info->col);
	for (i=0; i<len; i++)
		addch(' ');
	move(LINES-1, COLS-1);
	refresh();
	pthread_mutex_unlock(&drawlock);				
}
