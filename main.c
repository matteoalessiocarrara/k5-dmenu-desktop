/* Copyright 2017 Matteo Alessio Carrara <sw.matteoac@gmail.com> */
# include <stdio.h>
# include <stdlib.h>
# include <string.h>

# define err(...) {fprintf(stderr, __VA_ARGS__); exit(EXIT_FAILURE);}
# define mstrcpy(dest, src) {dest = malloc(strlen(src) + 1); strcpy(dest, src);}

int main (int argc, char **argv) {
	// parse commandline
	// get directories

	short dirn = 0;
	char **dirs = malloc(sizeof(char*) * ++dirn);
	
	if ((dirs[0] = getenv("XDG_DATA_HOME")) == NULL) {
		char *home = getenv("HOME"); if (home == NULL) err("Err: Unable to get $HOME value\n"); 
		dirs[0] = malloc(strlen(home) + strlen("/.local/share") + 1);
		strcpy(dirs[0], home);
		strcat(dirs[0], "/.local/share");
	}
	
	char *tmp = getenv("XDG_DATA_DIRS");
	if (tmp == NULL) {
		realloc(dirs, sizeof(char*) * (dirn += 2));
		mstrcpy(dirs[1], "/usr/local/share/");
		mstrcpy(dirs[2], "/usr/share");
	} else {
		char *tok = strtok(tmp, ":");
		for(; tok != NULL; tok = strtok(NULL, ":")) {
			realloc(dirs, sizeof(char*) * ++dirn);
			mstrcpy(dirs[dirn - 1], tok);
		}
	}

	for(short i = 0; i < dirn; i++) {

	}

	// find desktop files
	// start dmenu
	// send names
	// get choice
	// run
	// free

	return 0;
}
