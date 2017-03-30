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

void find_files(int namesfd, int filesfd, char dirname[]) {
	DIR *d = opendir(dirname);
	if (d == NULL) {
		fprintf(stderr, "`%s`: %s\n", dirname, strerror(errno));
		return;
	}

	errno = 0;
	while (1) {
		struct dirent *entry = readdir(d);
		char fullpath[PATH_MAX];

		if (entry == NULL) {
			if(errno) perror(NULL);
			break;
		}

		strcpy(fullpath, dirname);
		strcat(fullpath, "/");
		strcat(fullpath, entry->d_name);

		// warning: d_type is non-standard
		if (entry->d_type & DT_DIR) {
			if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) {
				find_files(namesfd, filesfd, fullpath);
			}
		}
		else if(strlen(entry->d_name) >= (sizeof(".desktop") - 1)) {
			if (!strncmp(entry->d_name + (strlen(entry->d_name) - sizeof(".desktop") + 1), ".desktop", sizeof(".desktop"))) {
				const unsigned bufsize = 200;
				char buf[bufsize + 1];
				int fd;

				if ((fd = open(fullpath, O_RDONLY)) == -1)
					fprintf(stderr, "'%s': %s\n", fullpath, strerror(errno));
				else {
					ssize_t r;
					while((r = read(fd, &buf, bufsize)) > 0) {
						unsigned broken_bytes = 0;
						for(; buf[r - 1 - broken_bytes] != '\n'; broken_bytes++);
						buf[r - broken_bytes] = '\0';
					}

					if(close(fd) == -1) fprintf(stderr, "'%s': %s", fullpath, strerror(errno));
				}
			}
		}
	}

	if (closedir(d) == -1) fprintf(stderr, "`%s`: %s\n", dirname, strerror(errno));
}

int main (int argc, char **argv) {
	char **data_dirs = malloc(sizeof(char*));
	short dir = 1;
	
	if ((data_dirs[0] = getenv("XDG_DATA_HOME")) == NULL) {
		char *home = getenv("HOME");
		if (home == NULL) die("Err: Unable to get $HOME value\n");
		data_dirs[0] = malloc(strlen(home) + sizeof("/.local/share") + 1);
		strcpy(data_dirs[0], home);
		strcat(data_dirs[0], "/.local/share");
	}
	
	char *tmp = getenv("XDG_DATA_DIRS");
	if (tmp == NULL) {
		realloc(data_dirs, sizeof(char*) * (dir += 2));
		mstrcpy(data_dirs[1], "/usr/local/share/");
		mstrcpy(data_dirs[2], "/usr/share");
	} else {
		for(char *tok = strtok(tmp, ":"); tok != NULL; tok = strtok(NULL, ":")) {
			realloc(data_dirs, sizeof(char*) * ++dir);
			mstrcpy(data_dirs[dir - 1], tok);
		}
	}

	for(short i = 0; i < dir; i++) {
		char dirname[PATH_MAX];
		strcpy(dirname, data_dirs[i]);
		strcat(dirname, "/applications");
		find_files(1, 1, dirname);
	}

	return 0;
}
