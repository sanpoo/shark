/*
    Copyright (C) 2014 bo.shen. All Rights Reserved.
    Author: bo.shen <sanpoos@gmail.com>
*/
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "strop.h"
#include "refstr.h"

struct conf
{
    struct conf *next;

    char key[128];
    char value[128];
};

static struct conf *g_conf = NULL;

static int parse_conf(const char *str, struct refstr *key, struct refstr *value)
{
    register const char *loop = str;

    while ((isspace(*loop) || !isprint(*loop)) && *loop != '\0')
        loop++;

    if (*loop == '#' || *loop == '\0')
        return -1;

    key->p = loop;
    while (*loop != '#' && *loop != '\0' && *loop != '=' && !isspace(*loop) && isprint(*loop))
        loop++;

    if (loop == str)
        return -1;

    key->len = loop - key->p;

    while ((*loop == '=' || isspace(*loop) || !isprint(*loop)) && *loop != '\0' && *loop != '#')
        loop++;

    value->p = loop;
    while (*loop != '\0' && *loop != ';' && *loop != '#' && !isspace(*loop) && isprint(*loop))
        loop++;

    value->len = loop - value->p;

    return 0;
}

void print_conf()
{
    struct conf *c = g_conf;

    printf("system conf:\n");

    while (c)
    {
        printf("\t*%s\t[%s]\n", c->key, c->value);
        c = c->next;
    }
}

char *get_conf(const char *key)
{
    struct conf *c = g_conf;

    while (c)
    {
        if (str_equal(c->key, key))
            return c->value;

        c = c->next;
    }

    printf("Failed to find config :%s\n", key);
    exit(0);
}

/*
    load config from file. support load from muti file, all config will insert
    into g_conf
 */
void load_conf(const char *filename)
{
    FILE *fp;
    char buff[512];
    struct conf *c;

    fp = fopen(filename, "r");
    if (NULL == fp)
    {
        printf("Failed to open config file:%s, err:%s\n", filename, strerror(errno));
        exit(0);
    }

    while (fgets(buff, sizeof(buff), fp))
    {
        struct refstr key, value;
        if (parse_conf(buff, &key, &value))
            continue;

        c = (struct conf *)malloc(sizeof(struct conf));
        if (!c)
        {
            printf("Failed to alloc mem for conf");
            exit(0);
        }

        memcpy(c->key, key.p, key.len);
        c->key[key.len] = 0;
        memcpy(c->value, value.p, value.len);
        c->value[value.len] = 0;

        if (g_conf)
            c->next = g_conf;
        g_conf = c;
    }

    fclose(fp);
}

