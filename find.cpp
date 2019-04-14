//
// Created by vikac on 11.04.19.
//

#include <iostream>
#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <dirent.h>
#include <vector>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

struct filter {
    filter() {
    }

    virtual bool check(struct dirent *dir, struct stat buf, char *path, const char *item) = 0;
};

struct filter_inum : filter {
    filter_inum(char *value)
            : filter(), num(atoi(value)) {}

    bool check(struct dirent *dir, struct stat buf, char *path, const char *item) {
        return dir->d_ino == num;
    }

private:
    int num;
};


struct filter_size : filter {
    filter_size(char *value)
            : filter(), status(value[0]), num(atoi(value + 1)) {
        if (status != '-' || status != '=' || status != '+') {
            std::cout << "Invalid command for -size: '" << status << "'\n";
            exit(1);
        }
    }

    bool check(struct dirent *dir, struct stat buf, char *path, const char *item) {
        if (status == '=') {
            return num == buf.st_size;
        } else if (status == '+') {
            return num < buf.st_size;
        } else {
            return num > buf.st_size;
        }
    }

private:
    char status;
    int num;
};

struct filter_name : filter {
    filter_name(char *value)
            : filter(), name(value) {}

    bool check(struct dirent *dir, struct stat buf, char *path, const char *item) {
        return strcmp(item, name) == 0;
    }

private:
    char *name;
};

struct filter_link : filter {
    filter_link(char *value)
            : filter(), count(atoi(value)) {}

    bool check(struct dirent *dir, struct stat buf, char *path, const char *item) {
        return buf.st_nlink == count;
    }

private:
    int count;
};

struct filter_exec : filter {
    filter_exec(char *value)
            : filter(), command(value) {}

    bool check(struct dirent *dir, struct stat buf, char *path, const char *item) {
        pid_t pid = fork();
        if (pid == 0) {
            if (execl(command, path) == -1) {
                perror("Wrong input!\n");
            }
        } else if (pid > 0) {
            pid_t wpid;
            int status;
            waitpid(pid, &status, WUNTRACED);
        }
    }

private:
    char *command;
};


std::vector<filter *> filters;

void init(ssize_t count, char **args) {
    if (count > 2) {
        for (int i = 2; i < count; i += 2) {
            if (i == count - 1) {
                printf("Usage: -<command> <value>\n");
                exit(1);
            }

            char *s_key = args[i];
            char *value = args[i + 1];

            if (strcmp(s_key, "-inum") == 0) {
                filters.push_back(static_cast<filter *>(new filter_inum(value)));
            } else if (strcmp(s_key, "-name") == 0) {
                filters.push_back(static_cast<filter *>(new filter_name(value)));
            } else if (strcmp(s_key, "-size") == 0) {
                filters.push_back(static_cast<filter *>(new filter_size(value)));
            } else if (strcmp(s_key, "-nlinks") == 0) {
                filters.push_back(static_cast<filter *>(new filter_link(value)));
            } else if (strcmp(s_key, "-check") == 0) {
                filters.push_back(static_cast<filter *>(new filter_exec(value)));
            } else {
                printf("can't find command: '%s'\n", s_key);
                exit(1);
            }
        }
    }
}

void check(struct dirent *dir, char *path, const char *item) {
    struct stat buf{};
    stat(path, &buf);

    for (filter *filter: filters) {
        if (!filter->check(dir, buf, path, item)) {
            return;
        }
    }

    std::cout << path << std::endl;
}

int walk(const char *path) {
    DIR *dir = opendir(path);
    if (!dir) {
        printf("Can't open directory: '%s'\n", path);
        return -1;
    }

    struct dirent *pd;
    while ((pd = readdir(dir)) != nullptr) {
        const char *curr = pd->d_name;
        if (strcmp(curr, ".") == 0 || strcmp(curr, "..") == 0) {
            continue;
        }

        char *new_path = new char[2048];
        strcpy(new_path, path);
        strcat(new_path, "/");
        strcat(new_path, curr);

        if (pd->d_type == DT_DIR) {
            walk(new_path);
        } else if (pd->d_type == DT_REG) {
            check(pd, new_path, curr);
        }
        delete[] new_path;
    }

    closedir(dir);
    return 0;
}


int main(int argc, char **argv) {
    init(argc, argv);
    if (argc < 2) {
        walk(".");
    } else {
        walk(argv[1]);
    }
    return 0;
}