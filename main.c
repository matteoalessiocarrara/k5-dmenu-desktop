/* Copyright 2017 Matteo Alessio Carrara <sw.matteoac@gmail.com> */
#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <regex.h>

#define die(...) {fprintf(stderr, __VA_ARGS__); exit(EXIT_FAILURE);}

// TODO Cache
// TODO Avoid using regex

static void find_files(const char *scandir, off_t **fsize, char ***fpath, unsigned *fnum)
{
	DIR *d;
	struct dirent *entry;
	struct stat estat;
	char *efullpath = malloc(PATH_MAX);

	static unsigned out_bufsiz = 200;
	static bool out_init = false;

	// initialize output pointers
	if(!out_init)
	{
		out_init = true;
		*fnum = 0;
		*fsize = malloc(sizeof(off_t) * out_bufsiz);
		*fpath = malloc(sizeof(char*) * out_bufsiz);

		if((*fsize == NULL) || (*fpath == NULL))
			die(strerror(errno));

		for(register unsigned i = 0; i < out_bufsiz; i++)
		{
			(*fpath)[i] = malloc(PATH_MAX);
			if ((*fpath)[i] == NULL) die(strerror(errno));
		}
	}

	// function begin
	d = opendir(scandir);
	if (d == NULL)
	{
		perror(scandir);
		return;
	}

	while(1)
	{
ls_start:
		errno = 0;
		entry = readdir(d);
		if (entry == NULL)
		{
			if(errno == 0) // end of direcory
				break;
			else // reading error
			{
				perror(scandir);
				goto ls_start;
			}
		}

		strcpy(efullpath, scandir);
		strcat(efullpath, "/");
		strcat(efullpath, entry->d_name);

		if (stat(efullpath, &estat) == -1)
		{
			perror(efullpath);
			goto ls_start;
		}

		if (S_ISDIR(estat.st_mode))
		{
			if ((strcmp(entry->d_name, ".") != 0) && (strcmp(entry->d_name, "..") != 0))
				find_files(efullpath, fsize, fpath, fnum);
		}
		else if(strlen(entry->d_name) >= strlen(".desktop"))
		{
			if (!strcmp(entry->d_name + (strlen(entry->d_name) - strlen(".desktop")), ".desktop"))
			{
				(*fsize)[*fnum] = estat.st_size;
				strcpy((*fpath)[*fnum], efullpath);
				(*fnum)++;
				if (*fnum == out_bufsiz)
				{
					out_bufsiz += 200;
					*fsize = realloc(*fsize, sizeof(off_t) * out_bufsiz);
					*fpath = realloc(*fpath, sizeof(char*) * out_bufsiz);

					if((*fsize == NULL) || (*fpath == NULL))
						die(strerror(errno));

					for(register unsigned i = (*fnum) - 1; i < out_bufsiz; i++)
					{
						(*fpath)[i] = malloc(PATH_MAX);
						if ((*fpath)[i] == NULL) die(strerror(errno));
					}
				}
			}
		}
	}

	if (closedir(d) == -1) perror(scandir);
	free(efullpath);
}

int main ()
{
	char **searchdirs, *xdg_data_dirs, *home, *testdir;
	short ndirs;
	char **files;
	off_t *fsize;
	unsigned files_found;
	regex_t header_cr, name_cr;
	unsigned fbufsize = 20000;
	char *fbuf = malloc(fbufsize + 1);
	int fd;
	short headers_max = 5;
	regmatch_t *headers = malloc(sizeof(regmatch_t) * headers_max);
	regmatch_t *header;
	short headers_found;
	short max_names = 2;
	regmatch_t *names = malloc(sizeof(regmatch_t) * max_names);
	regmatch_t *name;
	unsigned short names_found;
	unsigned name_len;

	// get paths

	ndirs = 1;
	searchdirs = malloc(sizeof(char*) * ndirs);
	searchdirs[0] = getenv("XDG_DATA_HOME");
	if (searchdirs[0] == NULL)
	{
		home = getenv("HOME");
		if (home == NULL) die("Err: Neither $XDG_DATA_HOME nor $HOME are set\n");

		searchdirs[0] = malloc(PATH_MAX);
		strcpy(searchdirs[0], home);
		strcat(searchdirs[0], "/.local/share");
	}

	xdg_data_dirs = getenv("XDG_DATA_DIRS");
	if (xdg_data_dirs == NULL)
	{
		ndirs += 2;
		searchdirs = realloc(searchdirs, sizeof(char*) * ndirs);
		searchdirs[1] = "/usr/local/share";
		searchdirs[2] = "/usr/share";
	}
	else
	{
		char *tok = strtok(xdg_data_dirs, ":");
		while(tok != NULL)
		{
			ndirs++;
			searchdirs = realloc(searchdirs, sizeof(char*) * ndirs);
			searchdirs[ndirs - 1] = tok;
			tok = strtok(NULL, ":");
		}
	}

	// find .desktop files

	testdir = malloc(PATH_MAX);
	for(short i = 0; i < ndirs; i++)
	{
		strcpy(testdir, searchdirs[i]);
		strcat(testdir, "/applications");
		find_files(testdir, &fsize, &files, &files_found);
	}

	// compile regex

	if(regcomp(&header_cr, "^\\[.*\\]$", REG_NEWLINE) != 0) die("Regex compilation error\n");
	if(regcomp(&name_cr, "^Name *= *", REG_NEWLINE) != 0) die("Regex compilation error\n");

	// print application names

	for(unsigned fn = 0; fn < files_found; fn++)
	{

		fd = open(files[fn], O_RDONLY);
		if (fd == -1)
		{
			perror(files[fn]);
			goto next_file;
		}

		if(fsize[fn] > fbufsize)
		{
			fbufsize = fsize[fn];
			fbuf = realloc(fbuf, fbufsize + 1);
			if (fbuf == NULL)
				die("Memory allocation failed for bytes=%u: %s\n", fbufsize + 1, strerror(errno));
		}

		if(read(fd, fbuf, fbufsize) == -1)
		{
			perror(files[fn]);
			goto next_file;
		}

		fbuf[fsize[fn]] = '\0';

		// Find the 'Desktop Entry' header
		while(1)
		{
			if (regexec(&header_cr, fbuf, headers_max, headers, 0) == REG_NOMATCH)
			{
				fprintf(stderr, "%s: Could not find any header\n", files[fn]);
				goto close_desktop_file;
			}

			header = NULL;
			headers_found = 0;
			for(short i = 0; i < headers_max; i++)
			{
				if(headers[i].rm_so != -1)
				{
					headers_found++;
					if(!strncmp(fbuf + headers[i].rm_so, "[Desktop Entry]", strlen("[Desktop Entry]")))
						header = headers + i;
				}
			}

			if (header == NULL)
			{
				if(headers_found == headers_max)
				{
					headers_max += 5;
					headers = realloc(headers, sizeof(regmatch_t) * headers_max);
					if (headers == NULL) die(strerror(errno));
				}
				else
				{
					fprintf(stderr, "%s: Could not find `Desktop Entry` header\n", files[fn]);
					goto close_desktop_file;
				}
			}
			else break;
		}

		// Find the 'Name' key
		while(1)
		{
			if(regexec(&name_cr, fbuf, max_names, names, 0) == REG_NOMATCH)
			{
				fprintf(stderr, "%s: no 'Name' keys\n", files[fn]);
				goto close_desktop_file;
			}

			names_found = 0;
			for(short i = 0; i < max_names; i++)
				if (names[i].rm_so != -1) names_found++;

			if (names_found == max_names)
			{
				max_names += 4;
				names = realloc(names, sizeof(regmatch_t) * max_names);
				if (names == NULL) die(strerror(errno));
			}
			else break;
		}

		// If we are here we found the header and at least one 'Name' key
		if(headers_found == 1)
		{
			if (names_found == 1)
				name = names + 0;
			else
			{
				fprintf(stderr, "%s: Invalid syntax, found multiple `Name` keys ‎under the same header\n", files[fn]);
				goto close_desktop_file;
			}
		}
		else if(headers_found > 1)
		{
			if (names_found == 1)
			{
				if (names[0].rm_so > header->rm_so)
				{
					// Verify that the key is under the correct header
					for(short i = 0; i < headers_found; i++)
					{
						if ((headers + i) != header)
						{
							if((headers[i].rm_so > header->rm_so) && (name[0].rm_so > headers[i].rm_so))
							{
								fprintf(stderr, "%s: 'Name' key found but under incorrect header\n", files[fn]);
								goto close_desktop_file;
							}
						}
					}

					name = names + 0;
				}
				else
				{
					fprintf(stderr, "%s: 'Name' key found but under incorrect header\n", files[fn]);
					goto close_desktop_file;
				}
			}
			else if (names_found > 1)
			{
				// FIXME
				fprintf(stderr, "Not implemented\n");
				goto close_desktop_file;
			}
		}

		// Key found
		for(name_len = 0; (fbuf[name->rm_eo + name_len] != '\n') && (fbuf[name->rm_eo + name_len] != '\0'); name_len++);
		write(STDOUT_FILENO, fbuf + name->rm_eo, name_len);
		write(STDOUT_FILENO, "\n", 1);
		write(STDOUT_FILENO, files[fn], strlen(files[fn]));
		write(STDOUT_FILENO, "\n", 1);

close_desktop_file:
		if(close(fd) == -1) perror(files[fn]);
next_file:;
	}

	return 0;
}
