/* Copyright 2017 Matteo Alessio Carrara <sw.matteoac@gmail.com> */
#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <regex.h>

#define die(...) {fprintf(stderr, __VA_ARGS__); exit(EXIT_FAILURE);}
#define pstrcpy(dest, src) {dest = malloc(PATH_MAX); strcpy(dest, src);}

regex_t rheader, rname;

void find_files(char dirname[]) {
	DIR *d;
	struct dirent *entry;
	struct stat entrys;
	char *entry_path = malloc(PATH_MAX);
	unsigned bufsize = 20000;
	char *filebuf = malloc(bufsize + 1);
	int fd;

	d = opendir(dirname);
	if (d == NULL) {
		perror(dirname);
		return;
	}

	errno = 0;
	while(1) {
		entry = readdir(d);
		if (entry == NULL) {
			if(errno != 0) perror(NULL);
			break;
		}

		strcpy(entry_path, dirname);
		strcat(entry_path, "/");
		strcat(entry_path, entry->d_name);

		if (stat(entry_path, &entrys) == -1) {
			perror(entry_path);
			break;
		}

		if (S_ISDIR(entrys.st_mode)) {
			if ((strcmp(entry->d_name, ".") != 0) && (strcmp(entry->d_name, "..") != 0)) {
				find_files(entry_path);
			}
		}
		else if(strlen(entry->d_name) >= strlen(".desktop")) {
			if (!strcmp(entry->d_name + (strlen(entry->d_name) - strlen(".desktop")), ".desktop")) {
				fd = open(entry_path, O_RDONLY);
				if (fd == -1) {
					perror(entry_path);
				}
				else {
					if(entrys.st_size > bufsize) {
						bufsize = entrys.st_size;
						filebuf = realloc(filebuf, bufsize + 1);
						if (filebuf == NULL)
							die("Memory allocation failed for bytes=%u: %s\n", bufsize + 1, strerror(errno));
					}

					if(read(fd, filebuf, bufsize) == -1) {
						perror(entry_path);
					}
					else {
						filebuf[entrys.st_size] = '\0';
					}

					if(close(fd) == -1) perror(entry_path);
				}
			}
		}
	}

	if (closedir(d) == -1) perror(dirname);
	free(entry_path);
	free(filebuf);
}

int main () {
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
		searchdirs = realloc(searchdirs, sizeof(char*) * dirs);
		pstrcpy(searchdirs[1], "/usr/local/share");
		pstrcpy(searchdirs[2], "/usr/share");
	} else {
		for(char *tok = strtok(xdg_data_dirs, ":"); tok != NULL; tok = strtok(NULL, ":")) {
			dirs++;
			searchdirs = realloc(searchdirs, sizeof(char*) * dirs);
			pstrcpy(searchdirs[dirs - 1], tok);
		}
	}

	if(regcomp(&rheader, "^\\[.*\\]$", 0) != 0) die("Regex error\n");
	if(regcomp(&rname, "^Name *=", 0) != 0) die("Regex error\n");

	for(short i = 0; i < dirs; i++) {
		char dir[PATH_MAX];
		strcpy(dir, searchdirs[i]);
		strcat(dir, "/applications");
		find_files(dir);
	}

	return 0;
}
