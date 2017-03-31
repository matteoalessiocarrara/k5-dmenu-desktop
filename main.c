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

regex_t header_cr, name_cr;

void find_files(char dirname[], int out_names_fd, int out_files_fd) {
	DIR *d;
	struct dirent *entry;
	struct stat entrys;
	char *entry_path = malloc(PATH_MAX);
	unsigned bufsize = 20000;
	char *filebuf = malloc(bufsize + 1);
	int fd;
	size_t header_matches = 5;
	regmatch_t *headers = malloc(sizeof(regmatch_t) * header_matches), *header = NULL;
	unsigned short headers_found;
	size_t max_names = 2;
	regmatch_t *names = malloc(sizeof(regmatch_t) * max_names), *name = NULL;
	unsigned short names_found;
	unsigned name_len;

	d = opendir(dirname);
	if (d == NULL) {
		perror(dirname);
		return;
	}

	while(1) {
		errno = 0;
		entry = readdir(d);
		if (entry == NULL) {
			if(errno != 0) perror(dirname);
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
				find_files(entry_path, out_names_fd, out_files_fd);
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

						while(1) {
							if (regexec(&header_cr, filebuf, header_matches, headers, 0) == REG_NOMATCH) {
								fprintf(stderr, "%s: Could not find any header\n", entry_path);
								goto close_desktop_file;
							}
							headers_found = 0;
							for(register unsigned short i = 0; i < header_matches; i++) {
								if(headers[i].rm_so != -1) {
									headers_found++;
									if(!strncmp(filebuf + headers[i].rm_so, "[Desktop Entry]", strlen("[Desktop Entry]")))
										header = headers + i;
								}
							}
							if (header == NULL) {
								if(headers_found == header_matches) {
									header_matches += 5;
									headers = realloc(headers, sizeof(regmatch_t) * header_matches);
									if (headers == NULL) die(strerror(errno));
								}
								else {
									fprintf(stderr, "%s: Could not find `Desktop Entry` header\n", entry_path);
									goto close_desktop_file;
								}
							}
							else break;
						}

						// Find the 'Name' key
						while(1) {
							if(regexec(&name_cr, filebuf, max_names, names, 0) == REG_NOMATCH) {
								fprintf(stderr, "%s: no 'Name' keys\n", entry_path);
								goto close_desktop_file;
							}

							names_found = 0;
							for(register unsigned short i = 0; i < max_names; i++)
								if (names[i].rm_so != -1) names_found++;

							if(headers_found == 1) {
								if (names_found == 1)
									name = names + 0;
								else {
									fprintf(stderr, "%s: Invalid syntax, found multiple `Name` keys ‎under the same header\n", entry_path);
									goto close_desktop_file;
								}
							}
							else {
								if (names_found == 1) {
									if (names[0].rm_so > header->rm_so) {
										for(register unsigned short i = 0; i < headers_found; i++) {
											if (headers + i != header) {
												if((headers[i].rm_so > header->rm_so) && (name[0].rm_so > headers[i].rm_so)) {
													fprintf(stderr, "%s: 'Name' key found but under incorrect header\n", entry_path);
													goto close_desktop_file;
												}
											}
										}
										name = names + 0;
									}
								}
								else {
									// FIXME
									fprintf(stderr, "Not implemented\n");
								}
							}

							if (name == NULL) {
								if (names_found == max_names) {
									max_names += 4;
									names = realloc(names, sizeof(regmatch_t) * max_names);
									if (names == NULL) die(strerror(errno));
								}
								else {
									fprintf(stderr, "%s: No valid 'Name' key found\n", entry_path);
									goto close_desktop_file;
								}

							}
							else break;
						}
					}

					for(name_len = 0; (filebuf[name->rm_eo + name_len] != '\n') && (filebuf[name->rm_eo + name_len] != '\0'); name_len++);
					write(out_names_fd, filebuf + name->rm_eo, name_len);
					write(out_names_fd, "\n", 1);
					write(out_files_fd, entry_path, strlen(entry_path));
					write(out_files_fd, "\n", 1);

close_desktop_file:
					if(close(fd) == -1) perror(entry_path);
				}
			}
		}
	}

	if (closedir(d) == -1) perror(dirname);
	free(entry_path);
	free(filebuf);
	free(headers);
	free(names);
}

int main () {
	char **searchdirs, *xdg_data_dirs, *testdir;
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

	if(regcomp(&header_cr, "^\\[.*\\]$", REG_NEWLINE) != 0) die("Regex compilation error\n");
	if(regcomp(&name_cr, "^Name *= *", REG_NEWLINE) != 0) die("Regex compilation error\n");

	testdir = malloc(PATH_MAX);
	for(short i = 0; i < dirs; i++) {
		strcpy(testdir, searchdirs[i]);
		strcat(testdir, "/applications");
		find_files(testdir, STDOUT_FILENO, STDERR_FILENO);
	}

	return 0;
}
