/* Copyright 2017 Matteo Alessio Carrara <sw.matteoac@gmail.com> */
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>

#define die(...) {fprintf(stderr, __VA_ARGS__); exit(EXIT_FAILURE);}
#define mstrcpy(dest, src) {dest = malloc(strlen(src) + 1); strcpy(dest, src);}

void find_files(char dirname[]) {
	DIR *d;

	d = opendir(dirname);
	if (d == NULL) {
		fprintf(stderr, "`%s`: %s\n", dirname, strerror(errno));
		return;
	}

	for(errno = 0;;) {
		struct dirent *entry;
		char entry_path[PATH_MAX];

		entry = readdir(d);
		if (entry == NULL) {
			if(errno != 0) perror(NULL);
			break;
		}

		strcpy(entry_path, dirname);
		strcat(entry_path, "/");
		strcat(entry_path, entry->d_name);

		// warning: d_type is non-standard
		if (entry->d_type & DT_DIR) {
			if ((strcmp(entry->d_name, ".") != 0) && (strcmp(entry->d_name, "..") != 0)) {
				find_files(entry_path);
			}
		}
		else if(strlen(entry->d_name) >= strlen(".desktop")) {
			if (!strcmp(entry->d_name + (strlen(entry->d_name) - strlen(".desktop")), ".desktop")) {
				const unsigned bufsize = 200;
				char buf[bufsize + 1];
				int fd;

				fd = open(entry_path, O_RDONLY);
				if (fd == -1) {
					fprintf(stderr, "'%s': %s\n", entry_path, strerror(errno));
				}
				else {
					for(ssize_t r; (r = read(fd, &buf, bufsize)) > 0;) {
						/*short broken_bytes = 0;

						for(; (buf[r - 1 - broken_bytes] != '\n') && (broken_bytes <r); broken_bytes++);
						buf[r - broken_bytes] = '\0';
						if(broken_bytes) lseek(fd, -broken_bytes, SEEK_CUR);*/
						}

					if(close(fd) == -1) fprintf(stderr, "'%s': %s", entry_path, strerror(errno));
				}
			}
		}
	}

	if (closedir(d) == -1) fprintf(stderr, "`%s`: %s\n", dirname, strerror(errno));
}

int main (int argc, char **argv) {
	char **searchdirs, *xdg_data_dirs;
	short dirs;
	
	searchdirs = malloc(sizeof(char*));
	dirs = 1;
	searchdirs[0] = getenv("XDG_DATA_HOME");
	if (searchdirs[0] == NULL) {
		char *home;

		home = getenv("HOME");
		if (home == NULL) die("Err: Unable to get $HOME value\n");

		searchdirs[0] = malloc(strlen(home) + strlen("/.local/share") + 1);
		strcpy(searchdirs[0], home);
		strcat(searchdirs[0], "/.local/share");
	}
	
	xdg_data_dirs = getenv("XDG_DATA_DIRS");
	if (xdg_data_dirs == NULL) {
		dirs += 2;
		realloc(searchdirs, sizeof(char*) * dirs);
		mstrcpy(searchdirs[1], "/usr/local/share/");
		mstrcpy(searchdirs[2], "/usr/share");
	} else {
		for(char *tok = strtok(xdg_data_dirs, ":"); tok != NULL; tok = strtok(NULL, ":")) {
			dirs++;
			realloc(searchdirs, sizeof(char*) * dirs);
			mstrcpy(searchdirs[dirs - 1], tok);
		}
	}

	for(short i = 0; i < dirs; i++) {
		char dir[PATH_MAX];
		strcpy(dir, searchdirs[i]);
		strcat(dir, "/applications");
		find_files(dir);
	}

	return 0;
}
