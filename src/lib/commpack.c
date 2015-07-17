#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

#include "shark.h"
#include "env.h"
#include "commpack.h"

/*
    协议格式
    unsigned char  SOH;     包头
    unsigned char  HVer;    高版本号
    unsigned char  LVer;    低版本号
    unsigned int   seq;     序列号
    unsigned int   len;     包长
    unsigned char  body[n]; 变长的包体
    unsigned char  EOT;     包尾

      1    1    1     1      4         4              n           1
    +----+----+----+----+---------+---------+-------------------+----+
    |    |    |    |    |         |         |                   |    |
    |SOH |Hver|Lver| NA |   seq   |   len   |       body        |EOT |
    |    |    |    |    |         |         |                   |    |
    +----+----+----+----+---------+---------+-------------------+----+
*/

#define SOH                 0x5A
#define EOT                 0x5A
#define COMMPACK_VER_HIGH   0x01
#define COMMPACK_VER_LOW    0x00

/*
    包中各个字段的位置
*/
#define COMMPACK_POS_SOH    0
#define COMMPACK_POS_HVER   1
#define COMMPACK_POS_LVER   2
#define COMMPACK_POS_SEQ    4
#define COMMPACK_POS_LEN    8
#define COMMPACK_POS_BODY   12

#define COMMPACK_LEN_least  13

static int commpack_seq = 0;

static int commpack_resize(struct commpack *pack, int inc_len)
{
    unsigned char *buff;

    inc_len = DIV_ROUND_UP(pack->len + inc_len, PAGE_SIZE);
    inc_len *= PAGE_SIZE;

    buff = (unsigned char *)malloc(inc_len);
    if (!buff)
        return -1;

    memcpy(buff, pack->buff, pack->pos + 1);   //尾部的EOT一并拷贝过来

    free(pack->buff);
    pack->buff = buff;
    pack->len = inc_len;

    return 0;
}

int commpack_write_data(struct commpack *pack, void *data, int len)
{
    if (pack->pos + len > pack->len - 1)
    {
        if (commpack_resize(pack, len))
            return -1;
    }

    memcpy(&pack->buff[pack->pos], data, len);
    pack->pos += len;
    pack->buff[pack->pos] = EOT;

    return 0;
}

int write_byte(struct commpack *pack, unsigned char param)
{
    return commpack_write_data(pack, &param, sizeof(unsigned char));
}

int write_short(struct commpack *pack, unsigned short param)
{
    param = htons(param);
    return commpack_write_data(pack, &param, sizeof(unsigned short));
}

int write_int(struct commpack *pack, unsigned param)
{
    param = htonl(param);
    return commpack_write_data(pack, &param, sizeof(unsigned));
}

int commpack_get_data(struct commpack *pack, void **data, int len)
{
    if (pack->pos + len > pack->len - 1)
        return -1;

    *data = &pack->buff[pack->pos];
    pack->pos += len;

    return 0;
}

int read_byte(struct commpack *pack, unsigned char *param)
{
    void *data;

    if (commpack_get_data(pack, &data, sizeof(unsigned char)))
        return -1;

    memcpy(param, data, sizeof(unsigned char));
    return 0;
}

int read_short(struct commpack *pack, unsigned short *param)
{
    void *data;

    if (commpack_get_data(pack, &data, sizeof(unsigned short)))
        return -1;

    memcpy(param, data, sizeof(unsigned char));
    *param = ntohs(*param);
    return 0;
}

int read_int(struct commpack *pack, unsigned *param)
{
    void *data;

    if (commpack_get_data(pack, &data, sizeof(unsigned short)))
        return -1;

    memcpy(param, data, sizeof(unsigned char));
    *param = ntohl(*param);
    return 0;
}

int commpack_buff_alloc(struct commpack *pack, int len)
{
    unsigned seq;

    len = DIV_ROUND_UP(len, PAGE_SIZE);
    len *= PAGE_SIZE;

    pack->buff = (unsigned char *)malloc(len);
    if (!pack->buff)
        return -1;

    pack->pos = COMMPACK_POS_BODY;
    pack->len = len;

    pack->buff[COMMPACK_POS_SOH] = SOH;
    pack->buff[COMMPACK_POS_HVER] = COMMPACK_VER_HIGH;
    pack->buff[COMMPACK_POS_LVER] = COMMPACK_VER_LOW;
    seq = htonl(commpack_seq++);
    memcpy(&pack->buff[COMMPACK_POS_SEQ], &seq, sizeof(unsigned));
    memset(&pack->buff[COMMPACK_POS_LEN], 0, sizeof(unsigned));
    pack->buff[COMMPACK_POS_BODY] = EOT;

    return 0;
}

void commpack_buff_free(struct commpack *pack)
{
    free(pack->buff);
    pack->buff = NULL;
}

