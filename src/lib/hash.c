/*
    Copyright (C) 2015 bo.shen. All Rights Reserved.
    Author: bo.shen <sanpoos@gmail.com>
*/
#include <stdlib.h>
#include <ctype.h>

#include "str.h"
#include "hash.h"

unsigned hash_key(unsigned char *data, size_t len)
{
    unsigned i, key = 0;

    for (i = 0; i < len; i++)
        key = HASH(key, data[i]);

    return key;
}

unsigned hash_key_lc(unsigned char *data, size_t len)
{
    unsigned i, key = 0;

    for (i = 0; i < len; i++)
        key = HASH(key, tolower(data[i]));

    return key;
}

void *hash_table_find(struct hash_table *ht, unsigned key)
{
    struct hash_element *loop;
    struct list_head *bucket = &ht->buckets[key % ht->bucket_size];

    list_for_each_entry(loop, bucket, list)
    {
        if (loop->key < key)
            continue;

        if (loop->key == key)
            return loop->object;
        else
            return NULL;
    }

    return NULL;
}

void *hash_table_remove(struct hash_table *ht, unsigned key)
{
    struct hash_element *loop;
    struct list_head *bucket = &ht->buckets[key % ht->bucket_size];

    list_for_each_entry(loop, bucket, list)
    {
        if (key == loop->key)
        {
            list_del_init(&loop->list);
            void *object = loop->object;
            free(loop);

            return object;
        }
    }

    return NULL;
}

int hash_table_add(struct hash_table *ht, unsigned key, void *object)
{
    struct hash_element *loop;
    struct list_head *bucket = &ht->buckets[key % ht->bucket_size];
    struct hash_element *elem = (struct hash_element *)malloc(sizeof(struct hash_element));
    if (!elem)
        return -1;

    elem->key = key;
    elem->object = object;

    list_for_each_entry(loop, bucket, list)
    {
        if (loop->key < key)
            continue;

        if (loop->key == key)
        {
            free(elem);
            return -2;
        }

        list_add_tail(&elem->list, &loop->list);
        return 0;
    }

    list_add_tail(&elem->list, &loop->list);
    return 0;
}

struct hash_table *hash_table_create(unsigned bucket_size)
{
    size_t size;
    struct hash_table *ht;

    if (bucket_size == 0 || (bucket_size & (bucket_size - 1)) != 0)
        return NULL;

    size = sizeof(struct hash_table) + bucket_size * sizeof(struct list_head);
    ht = (struct hash_table *)malloc(size);
    if (!ht)
        return NULL;

    ht->bucket_size = bucket_size;
    ht->buckets = (struct list_head *)((char *)ht + sizeof(struct hash_table));

    while (bucket_size--)
        INIT_LIST_HEAD(&ht->buckets[bucket_size]);

    return ht;
}

