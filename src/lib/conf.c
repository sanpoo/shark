/*
    Copyright (C) 2014 bo.shen. All Rights Reserved.
    Author: bo.shen <sanpoos@gmail.com>
*/
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>

#include "str.h"

struct conf
{
    struct conf *next;

    char key[128];
    char value[128];
};

static struct conf *g_conf = NULL;

static int parse_conf(unsigned char *str, str_t *key, str_t *value)
{
    register unsigned char *loop = str;

    while ((isspace(*loop) || !isprint(*loop)) && *loop != '\0')
        loop++;

    if (*loop == '#' || *loop == '\0')
        return -1;

    key->p = loop;
    while (*loop != '#' && *loop != '\0' && *loop != '=' && !isspace(*loop) && isprint(*loop))
        loop++;

    key->len = loop - key->p;

    while (*loop != '#' && *loop != '\0' && (*loop == '=' || isspace(*loop) || !isprint(*loop)))
        loop++;

    value->p = loop;
    while (*loop != '#' && *loop != '\0' && *loop != ';' && !isspace(*loop) && isprint(*loop))
        loop++;

    value->len = loop - value->p;

    return 0;
}

static void free_old_conf(struct conf *c)
{
    struct conf *next;

    while (c)
    {
        next = c->next;
        free(c);
        c = next;
    }
}

void print_raw_conf()
{
    struct conf *c = g_conf;

    printf("system conf:\n");

    while (c)
    {
        printf("\t%s\t[%s]\n", c->key, c->value);
        c = c->next;
    }
}

char *get_raw_conf(const char *key)
{
    struct conf *c = g_conf;

    while (c)
    {
        if (str_equal(c->key, key))
            return c->value;

        c = c->next;
    }

    printf("Failed to find configuration item : %s\n", key);
    exit(0);
}

/*
    load config from file. support load from multi file, all config will insert
    into g_conf
*/
void load_raw_conf(const char *filename)
{
    FILE *fp;
    char buff[512];
    struct conf *head = NULL;
    struct conf *c;

    fp = fopen(filename, "r");
    if (NULL == fp)
    {
        printf("%s :%s\n", filename, strerror(errno));
        exit(0);
    }

    while (fgets(buff, sizeof(buff), fp))
    {
        str_t key, value;
        if (parse_conf((unsigned char *)buff, &key, &value))
            continue;

        c = (struct conf *)malloc(sizeof(struct conf));
        if (!c)
        {
            printf("Failed to alloc mem for conf\n");
            exit(0);
        }

        memcpy(c->key, key.p, key.len);
        c->key[key.len] = 0;
        memcpy(c->value, value.p, value.len);
        c->value[value.len] = 0;

        if (head)
            c->next = head;
        else
            c->next = NULL;
        head = c;
    }

    free_old_conf(g_conf);
    g_conf = head;
    fclose(fp);
}

