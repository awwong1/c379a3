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

#define	NUMSCR 10		/* limit to number of saucers	*/
#define NUMBLS 10               /* limit to number of bullets   */
#define NUMALL NUMSCR+NUMBLS+1	/* number of maximum threads 	*/
#define	TUNIT  20000       	/* timeunits in microseconds    */

/* Struct to hold saucer data */
struct	spropset {
	char	*str;	/* the saucer 	*/
	int	delay;  /* delay in time units */
	int	row;	/* the row	*/
	int     col;    /* the column */
	int	dir;	/* the direction 1 or -1 */
	int	alive;  /* 1 for alive, 0 for dead */
};

/* Struct to hold bullet data */
struct bpropset {
	char    *str;   /* the bullet */
	int	delay;  /* delay in time units */
	int	row;	/* the row    */
	int     col;    /* the column */
	int	fired;	/* 1 if fired, 0 if not */
};

/* Struct to hold cursor (player) data */
struct cpropset {
	char *str;   /* the player cursor */
	int  col;    /* the column */
};

pthread_mutex_t mx = PTHREAD_MUTEX_INITIALIZER;

int main(int ac, char *av[])
{
	int	        c;		   /* user input		*/
	pthread_t       thrds[NUMALL];     /* max amt threads		*/
	struct spropset scrprops[NUMSCR];  /* properties of saucer	*/
	struct bpropset blsprops[NUMBLS];  /* properties of bullets	*/
	struct cpropset plrprops[1];	   /* properties of player	*/
	void	        *scranimate();	   /* the saucer function	*/
	void	        *blsanimate();	   /* the bullet function	*/
	void	        *plranimate();	   /* the player function	*/
	int	        i;

	setup_player(plrprops);
	setup_bullets(blsprops);
	setup_saucers(scrprops);
	
	/* create all the threads */
	for(i=0 ; i<(NUMALL); i++)
	{
		if (i < 1) {
			if(pthread_create(&thrds[i], NULL, plranimate, 
					  &plrprops[i])){
				fprintf(stderr, "error creating thread");
				endwin();
				exit(0);
			}
		} else if (i < 1 + NUMSCR) {
			if(pthread_create(&thrds[i], NULL, scranimate, 
					  &scrprops[i-1])){
				fprintf(stderr, "error creating thread");
				endwin();
				exit(0);
			}
		} else if (i < NUMALL) {
			if(pthread_create(&thrds[i], NULL, blsanimate,
					  &blsprops[i-1-NUMSCR])){
				fprintf(stderr, "error creating thread");
				endwin();
				exit(0);
			}
		}
	}

	/* process user input and game logic */
	while(1) {
		collisiondetect(blsprops, scrprops);
		c = getch();
		if ( c == 'Q' ) 
			break;
		// move left
		if ( c == ',' && plrprops[0].col > 0)
			plrprops[0].col = plrprops[0].col-1;
		// move right
		else if ( c == '.' && plrprops[0].col+3 < COLS)
			plrprops[0].col = plrprops[0].col+1;
		// fire bullet
		if ( c == ' ' )
			firebullet(blsprops, plrprops[0].col);
	}

	/* cancel all the threads */
	pthread_mutex_lock(&mx);
	for (i=0; i<NUMALL; i++ )
		pthread_cancel(thrds[i]);
	endwin();
	return 0;
}

/* Attempt to fire a bullet */
int firebullet(struct bpropset props[], int colval)
{
	int i;
	pthread_mutex_lock(&mx);	
	for(i=0; i<NUMBLS; i++){
		if(props[i].fired == 0) {
			props[i].fired = 1;
			props[i].col = colval;
			break;
		}
	}
	pthread_mutex_unlock(&mx);
	return 0;
}

/* Handle collisions between saucers and bullets */
int collisiondetect(struct bpropset bprops[], struct spropset sprops[])
{
	int i;
	int j;
	pthread_mutex_lock(&mx);
	for(i=0; i<NUMBLS; i++) {
		if (bprops[i].fired == 0)
			continue;
		for(j=0; j<NUMSCR; j++) {
			if (bprops[i].row >= sprops[j].row-1 &&
			    bprops[i].row <= sprops[j].row+1 &&
			    bprops[i].col >= sprops[j].col &&
			    bprops[i].col <= sprops[j].col+5) {
				bprops[i].fired=0;
				sprops[j].alive=0;
			}
		}
	}
	pthread_mutex_unlock(&mx);
	return 0;
}

/* Initialize the values for each saucer */
int setup_saucers(struct spropset props[])
{
	int i;
	/* assign rows and velocities to each string */
	srand(getpid());
	for(i=0; i<NUMSCR; i++){
		props[i].str = "<--->";      		/* the message	*/
		props[i].row = i+1;		        /* the row	*/
		props[i].delay = 1+(rand()%15);	        /* a speed	*/
		props[i].dir = ((rand()%2)?1:-1);	/* +1 or -1	*/
		props[i].col = rand()%(COLS-8);
		props[i].alive = 1;
	}

	return 0;
}

/* Initialize the bullets in the propset's string value */
int setup_bullets(struct bpropset props[])
{
	int i;
	for(i=0; i<NUMBLS; i++){
		props[i].str = "^";
		props[i].row = LINES-3;
		props[i].col = 0;
		props[i].delay = 5;
		props[i].fired = 0;
	}
	return 0;
}

/* Initialize the player cursor */
int setup_player(struct cpropset props[])
{
	props[0].str = "|";
	props[0].col = COLS/2;

	/* set up curses */
	initscr();
	crmode();
	noecho();
	clear();
	mvprintw(LINES-1,0,"'Q' to quit ',' moves left '.' moves "  
		 "right SPACE fires");
	return 0;
}


/* the code that runs in each saucer thread */
void *scranimate(void *arg)
{
	struct 	spropset *info = arg;
	int	len = strlen(info->str)+2;

	while( 1 )
	{
		usleep(info->delay*TUNIT);
		pthread_mutex_lock(&mx);
		if (info->alive) {
			move(info->row, info->col);
			addch(' ');
			addstr( info->str );
			addch(' ');
		} else {
			move(info->row, info->col);
			addstr("       ");
			info->row = LINES;
			info->col = COLS;
		}

		/* move item to next column and check for bouncing */
		info->col += info->dir;
		if ( info->col <= 0 && info->dir == -1 )
			info->dir = 1;
		else if (  info->col+len >= COLS && info->dir == 1 )
			info->dir = -1;

		move(LINES-1,COLS-1);
		refresh();
		pthread_mutex_unlock(&mx);
	}
}

/* the code that runs in each bullet thread */
void *blsanimate(void *arg)
{
	struct 	bpropset *info = arg;

	while( 1 )
	{
		usleep(info->delay*TUNIT);
		pthread_mutex_lock(&mx);
		if(info->fired) {
			info->row -= 1;
			move(info->row+1, info->col);
			addstr("   ");
			move(info->row, info->col);
			addch(' ');
			addstr(info->str);
			addch(' ');
			if (info->row == 0) {
				move(info->row, info->col);
				addstr("   ");
				info->fired = 0;
				info->row = LINES-3;
			}	
		} else {
			move(info->row, info->col);
			addstr("   ");
			info->row = LINES-3;
		}
		move(LINES-1, COLS-1);
		refresh();
		pthread_mutex_unlock(&mx);
	}
}

/* the code that runs for the player cursor */
void *plranimate(void *arg)
{
	struct cpropset *info = arg;
	while( 1 )
	{
		pthread_mutex_lock(&mx);
		move(LINES-2, info->col);
		addch(' ');
		addstr( info->str );
		addch(' ');
		move(LINES-1, COLS-1);
		refresh();
		pthread_mutex_unlock(&mx);
	}
}
