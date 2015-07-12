#include <stdio.h>
#include "hash.h"
#include "log.h"
#include "http_request.h"

#define str3cmp(m, c0, c1, c2) \
    (m[0] == c0 && m[1] == c1 && m[2] == c2)

#define str3Ocmp(m, c0, c1, c2, c3) \
    m[0] == c0 && m[2] == c2 && m[3] == c3

#define str4cmp(m, c0, c1, c2, c3) \
    m[0] == c0 && m[1] == c1 && m[2] == c2 && m[3] == c3

#define str5cmp(m, c0, c1, c2, c3, c4) \
    m[0] == c0 && m[1] == c1 && m[2] == c2 && m[3] == c3 && m[4] == c4

#define str6cmp(m, c0, c1, c2, c3, c4, c5) \
    m[0] == c0 && m[1] == c1 && m[2] == c2 && m[3] == c3 \
    && m[4] == c4 && m[5] == c5

#define str7cmp(m, c0, c1, c2, c3, c4, c5, c6, c7) \
    m[0] == c0 && m[1] == c1 && m[2] == c2 && m[3] == c3 \
    && m[4] == c4 && m[5] == c5 && m[6] == c6

#define str8cmp(m, c0, c1, c2, c3, c4, c5, c6, c7) \
    m[0] == c0 && m[1] == c1 && m[2] == c2 && m[3] == c3 \
    && m[4] == c4 && m[5] == c5 && m[6] == c6 && m[7] == c7

#define str9cmp(m, c0, c1, c2, c3, c4, c5, c6, c7, c8) \
    m[0] == c0 && m[1] == c1 && m[2] == c2 && m[3] == c3 \
    && m[4] == c4 && m[5] == c5 && m[6] == c6 && m[7] == c7 && m[8] == c8

static unsigned usual[] =
{
    0xffffdbfe, /* 1111 1111 1111 1111  1101 1011 1111 1110 */
    /* ?>=< ;:98 7654 3210  /.-, +*)( '&%$ #"!  */
    0x7fff37d6, /* 0111 1111 1111 1111  0011 0111 1101 0110 */
    /* _^]\ [ZYX WVUT SRQP  ONML KJIH GFED CBA@ */
    0xffffffff, /* 1111 1111 1111 1111  1111 1111 1111 1111 */
    /*  ~}| {zyx wvut srqp  onml kjih gfed cba` */
    0xffffffff, /* 1111 1111 1111 1111  1111 1111 1111 1111 */

    0xffffffff, /* 1111 1111 1111 1111  1111 1111 1111 1111 */
    0xffffffff, /* 1111 1111 1111 1111  1111 1111 1111 1111 */
    0xffffffff, /* 1111 1111 1111 1111  1111 1111 1111 1111 */
    0xffffffff  /* 1111 1111 1111 1111  1111 1111 1111 1111 */
};


enum http_request_line_status
{
    sw_start = 0,
    sw_method,
    sw_spaces_before_uri,
    sw_schema,
    sw_schema_slash,
    sw_schema_slash_slash,
    sw_host_start,
    sw_host,
    sw_host_end,
    sw_host_ip_literal,
    sw_port,
    sw_host_http_09,
    sw_after_slash_in_uri,
    sw_check_uri,
    sw_check_uri_http_09,
    sw_uri,
    sw_http_09,
    sw_http_H,
    sw_http_HT,
    sw_http_HTT,
    sw_http_HTTP,
    sw_first_major_digit,
    sw_major_digit,
    sw_first_minor_digit,
    sw_minor_digit,
    sw_spaces_after_digit,
    sw_almost_done
};

static inline enum http_method get_request_method(str_t *method_name)
{
    unsigned char *p = method_name->p;

    switch (method_name->len)
    {
        case 3:
            if (str3cmp(p, 'G', 'E', 'T'))  return HTTP_GET;
            if (str3cmp(p, 'P', 'U', 'T'))  return HTTP_PUT;
            return HTTP_UNKNOWN;
        case 4:
            if (p[1] == 'O')
            {
                if (str3Ocmp(p, 'P', 'O', 'S', 'T')) return HTTP_POST;
                if (str3Ocmp(p, 'C', 'O', 'P', 'Y')) return HTTP_COPY;
                if (str3Ocmp(p, 'M', 'O', 'V', 'E')) return HTTP_MOVE;
                if (str3Ocmp(p, 'L', 'O', 'C', 'K')) return HTTP_LOCK;
                return HTTP_UNKNOWN;
            }
            if (str4cmp(p, 'H', 'E', 'A', 'D')) return HTTP_HEAD;
            return HTTP_UNKNOWN;
        case 5:
            if (str5cmp(p, 'M', 'K', 'C', 'O', 'L')) return HTTP_MKCOL;
            if (str5cmp(p, 'P', 'A', 'T', 'C', 'H')) return HTTP_PATCH;
            if (str5cmp(p, 'T', 'R', 'A', 'C', 'E')) return HTTP_TRACE;
            return HTTP_UNKNOWN;
        case 6:
            if (str6cmp(p, 'D', 'E', 'L', 'E', 'T', 'E')) return HTTP_DELETE;
            if (str6cmp(p, 'U', 'N', 'L', 'O', 'C', 'K')) return HTTP_UNLOCK;
            return HTTP_UNKNOWN;
        case 7:
            if (str7cmp(p, 'O', 'P', 'T', 'I', 'O', 'N', 'S', ' ')) return HTTP_OPTIONS;
            return HTTP_UNKNOWN;
        case 8:
            if (str8cmp(p, 'P', 'R', 'O', 'P', 'F', 'I', 'N', 'D')) return HTTP_PROPFIND;
            return HTTP_UNKNOWN;
        case 9:
            if (str9cmp(p, 'P', 'R', 'O', 'P', 'P', 'A', 'T', 'C', 'H')) return HTTP_PROPPATCH;
            return HTTP_UNKNOWN;
    }

    return HTTP_UNKNOWN;
}

/*
    < 0 error
    = 0 解析完成
    > 0 还需要解析
*/
int http_parse_request_line(struct http_request *r, struct buffer *b)
{
    unsigned char *p;
    int state = r->state;

    for (p = b->pos; p != b->last; p++)
    {
        unsigned char ch = *p;
        switch (state)
        {
            case sw_start:
                r->request_line.p = p;
                if (ch == CR || ch == LF)
                    break;
                if ((ch < 'A' || ch > 'Z') && ch != '_')
                    return -1;
                r->method_name.p = p;
                state = sw_method;
                break;

            case sw_method:
                if (ch == ' ')
                {
                    r->method_name.len = p - r->method_name.p;
                    r->method = get_request_method(&r->method_name);
                    if (HTTP_UNKNOWN & r->method)
                        return -2;
                    state = sw_spaces_before_uri;
                    break;
                }

                if ((ch < 'A' || ch > 'Z') && ch != '_')
                    return -3;
                break;

            case sw_spaces_before_uri:
                if (ch == '/')
                {
                    r->uri.p = p;
                    str_init(&r->schema); //no schema host and port, maybe in host header
                    str_init(&r->host);
                    str_init(&r->port);
                    state = sw_after_slash_in_uri;
                    break;
                }

                unsigned char c = (unsigned char)(ch | 0x20);
                if (c >= 'a' && c <= 'z')
                {
                    r->schema.p = p;
                    state = sw_schema;
                    break;
                }
                if (ch == ' ')
                    break;
                return -4;
            case sw_schema:
                c = (unsigned char)(ch | 0x20);
                if (c >= 'a' && c <= 'z')
                    break;
                if (ch == ':')
                {
                    r->schema.len = p - r->schema.p;
                    state = sw_schema_slash;
                    break;
                }
                return -5;
            case sw_schema_slash:
                if (ch == '/')
                {
                    state = sw_schema_slash_slash;
                    break;
                }
                return -6;
            case sw_schema_slash_slash:
                if (ch == '/')
                {
                    state = sw_host_start;
                    break;
                }
                return -7;
            case sw_host_start:
                r->host.p = p;
                if (ch == '[')
                {
                    state = sw_host_ip_literal;
                    break;
                }
                state = sw_host;
            case sw_host:
                c = (unsigned char)(ch | 0x20);
                if (c >= 'a' && c <= 'z')
                    break;
                if ((ch >= '0' && ch <= '9') || ch == '.' || ch == '-')
                    break;
            case sw_host_end:
                r->host.len = p - r->host.p;
                switch (ch)
                {
                    case ':':
                        r->port.p = p + 1;
                        state = sw_port;
                        break;
                    case '/':
                        str_init(&r->port);
                        r->uri.p = p;
                        state = sw_after_slash_in_uri;
                        break;
                    case ' ':
                        /*
                            GET http://www.qq.com HTTP1.1
                            使用一个/来作为uri
                        */
                        str_init(&r->port);
                        r->uri.p = r->schema.p + r->schema.len + 1;
                        r->uri.len = 1;
                        state = sw_host_http_09;
                        break;
                    default:
                        return -8;
                }
                break;
            case sw_host_ip_literal:
                if (ch >= '0' && ch <= '9')
                    break;
                c = (unsigned char)(ch | 0x20);
                if (c >= 'a' && c <= 'z')
                    break;
                switch (ch)
                {
                    case ':':
                        break;
                    case ']':
                        state = sw_host_end;
                        //r->host.len in sw_host_end set, not here
                        break;
                    case '-':
                    case '.':
                    case '_':
                    case '~':
                        /* unreserved */
                        break;
                    case '!':
                    case '$':
                    case '&':
                    case '\'':
                    case '(':
                    case ')':
                    case '*':
                    case '+':
                    case ',':
                    case ';':
                    case '=':
                        /* sub-delims */
                        break;
                    default:
                        return -9;
                }
                break;
            case sw_port:
                if (ch >= '0' && ch <= '9')
                    break;
                r->port.len = p - r->port.p;
                switch (ch)
                {
                    case '/':
                        r->uri.p = p;
                        state = sw_after_slash_in_uri;
                        break;
                    case ' ':
                        /*
                            GET http://host:port HTTP1.1
                            没有/, 那么使用一个/来作为uri
                        */
                        r->uri.p = r->schema.p + r->schema.len + 1;
                        r->uri.len = 1;
                        state = sw_host_http_09;
                        break;
                    default:
                        return -10;
                }
                break;
            /* space+ after "http://host[:port] " */
            case sw_host_http_09:
                switch (ch)
                {
                    case ' ':
                        break;
                    case CR:
                        r->http_minor = 9;
                        state = sw_almost_done;
                        break;
                    case LF:
                        r->http_minor = 9;
                        r->request_line.len = p - r->request_line.p;
                        goto done;
                    case 'H':
                        r->http_protocol.p = p;
                        state = sw_http_H;
                        break;
                    default:
                        return -11;
                }
                break;
            //检查/后面是什么字符, 如果是switch的10种特殊字符, 则单独处理
            case sw_after_slash_in_uri:
                if (usual[ch >> 5] & (1 << (ch & 0x1f)))
                {
                    state = sw_check_uri;
                    break;
                }
                switch (ch)
                {
                    case ' ':   //斜杠后面跟空格, 判断是不是0.9版本(也有可能是1.0)
                        r->uri.len = p - r->uri.p;
                        state = sw_check_uri_http_09;
                        break;
                    case CR:    //斜杠后面直接跟CR, 当0.9版本
                        r->uri.len = p - r->uri.p;
                        r->http_minor = 9;
                        state = sw_almost_done;
                        break;
                    case LF:    //斜杠后面直接跟LF, 当0.9版本
                        r->uri.len = p - r->uri.p;
                        r->http_minor = 9;
                        r->request_line.len = p - r->request_line.p;
                        goto done;
                    case '.':
                        r->complex_uri = 1;
                        state = sw_uri;
                        break;
                    case '%':
                        r->quoted_uri = 1;
                        state = sw_uri;
                        break;
                    case '/':
                        r->complex_uri = 1;
                        state = sw_uri;
                        break;
                    case '?':
                        r->args = p + 1;
                        state = sw_uri;
                        break;
                    case '#':
                        r->complex_uri = 1;
                        state = sw_uri;
                        break;
                    case '+':
                        r->plus_in_uri = 1;
                        break;
                    case '\0':
                        return -12;
                    default:
                        state = sw_check_uri;
                        break;
                }
                break;
            /* check "/", "%" and "\" (Win32) in URI */
            //确认斜杠/后的字符, 请求行中可能有多个/
            case sw_check_uri:  //check2个斜杠之间的内容
                if (usual[ch >> 5] & (1 << (ch & 0x1f)))
                    break;
                switch (ch)
                {
                    case '/':
                        str_init(&r->exten);  //此前出现的.有可能是参数里的0.12
                        state = sw_after_slash_in_uri;
                        break;
                    case '.':
                        r->exten.p = p + 1;   //exten与uri结束相同
                        break;
                    case ' ':
                        r->uri.len = p - r->uri.p;
                        state = sw_check_uri_http_09;
                        break;
                    case CR:
                        r->uri.len = p - r->uri.p;
                        r->http_minor = 9;

                        state = sw_almost_done;
                        break;
                    case LF:
                        r->uri.len = p - r->uri.p;
                        r->http_minor = 9;
                        r->request_line.len = p - r->request_line.p;
                        goto done;
                    case '%':
                        r->quoted_uri = 1;
                        state = sw_uri;
                        break;
                    case '?':
                        r->args = p + 1;
                        state = sw_uri;
                        break;
                    case '#':
                        r->complex_uri = 1;
                        state = sw_uri;
                        break;
                    case '+':
                        r->plus_in_uri = 1;
                        break;
                    case '\0':
                        return -13;
                }
                break;
            //走到这里, 表示其前面一定是空格
            case sw_check_uri_http_09:
                switch (ch)
                {
                    case ' ':
                        break;
                    case CR:
                        r->http_minor = 9;
                        state = sw_almost_done;
                        break;
                    case LF:
                        r->http_minor = 9;
                        r->request_line.len = p - r->request_line.p;
                        goto done;
                    case 'H':
                        r->http_protocol.p = p;
                        state = sw_http_H;
                        break;
                    default:
                        r->space_in_uri = 1;
                        state = sw_check_uri;
                        p--;
                        break;
                }
                break;
            /* URI */
            case sw_uri:    //在特殊字符(%?#/)后面的字符, 这些都当做uri
                if (usual[ch >> 5] & (1 << (ch & 0x1f)))
                    break;
                switch (ch)
                {
                    case ' ':
                        r->uri.len = p - r->uri.p;
                        state = sw_http_09;
                        break;
                    case CR:
                        r->uri.len = p - r->uri.p;
                        r->http_minor = 9;
                        state = sw_almost_done;
                        break;
                    case LF:
                        r->uri.len = p - r->uri.p;
                        r->http_minor = 9;
                        r->request_line.len = p - r->request_line.p;
                        goto done;
                    case '#':
                        r->complex_uri = 1;
                        break;
                    case '\0':
                        return -14;
                }
                break;
            /* space+ after URI */
            case sw_http_09:
                switch (ch)
                {
                    case ' ':
                        break;
                    case CR:
                        r->http_minor = 9;
                        state = sw_almost_done;
                        break;
                    case LF:
                        r->http_minor = 9;
                        r->request_line.len = p - r->request_line.p;
                        goto done;
                    case 'H':
                        r->http_protocol.p = p;
                        state = sw_http_H;
                        break;
                    default:
                        r->space_in_uri = 1;
                        state = sw_uri;
                        p--;
                        break;
                }
                break;
            case sw_http_H:
                switch (ch)
                {
                    case 'T':
                        state = sw_http_HT;
                        break;
                    default:
                        return -15;
                }
                break;
            case sw_http_HT:
                switch (ch)
                {
                    case 'T':
                        state = sw_http_HTT;
                        break;
                    default:
                        return -16;
                }
                break;
            case sw_http_HTT:
                switch (ch)
                {
                    case 'P':
                        state = sw_http_HTTP;
                        break;
                    default:
                        return -17;
                }
                break;
            case sw_http_HTTP:
                switch (ch)
                {
                    case '/':
                        state = sw_first_major_digit;
                        break;
                    default:
                        return -18;
                }
                break;
            /* first digit of major HTTP version */
            case sw_first_major_digit:
                if (ch < '1' || ch > '9')
                    return -19;
                r->http_major = ch - '0';
                state = sw_major_digit;
                break;
            /* major HTTP version or dot */
            case sw_major_digit:
                if (ch == '.')
                {
                    state = sw_first_minor_digit;
                    break;
                }
                if (ch < '0' || ch > '9')
                    return -20;
                r->http_major = r->http_major * 10 + ch - '0';
                break;
            /* first digit of minor HTTP version */
            case sw_first_minor_digit:
                if (ch < '0' || ch > '9')
                    return -21;
                r->http_minor = ch - '0';
                state = sw_minor_digit;
                break;
            /* minor HTTP version or end of r line */
            case sw_minor_digit:
                if (ch == CR)
                {
                    r->http_protocol.len = p - r->http_protocol.p;
                    state = sw_almost_done;
                    break;
                }

                if (ch == LF)
                {
                    r->request_line.len = p - r->request_line.p;
                    r->http_protocol.len = p - r->http_protocol.p;
                    goto done;
                }

                if (ch == ' ')
                {
                    r->http_protocol.len = p - r->http_protocol.p;
                    state = sw_spaces_after_digit;
                    break;
                }

                if (ch < '0' || ch > '9')
                    return -22;
                r->http_minor = r->http_minor * 10 + ch - '0';
                break;
            case sw_spaces_after_digit:
                switch (ch)
                {
                    case ' ':
                        break;
                    case CR:
                        state = sw_almost_done;
                        break;
                    case LF:
                        r->request_line.len = p - r->request_line.p;
                        goto done;
                    default:
                        return -23;
                }
                break;
            /* end of r line */
            case sw_almost_done:
                r->request_line.len = p - r->request_line.p - 1;
                switch (ch)
                {
                    case LF:
                        goto done;
                    default:
                        return -24;
                }
        }
    }

    b->pos = p;
    r->state = state;

    return 1;
done:
    if (r->exten.p)
        r->exten.len = r->uri.p + r->uri.len - r->exten.p;
    b->pos = p + 1;
    if (r->http_minor == 9)
        r->http_major = 0;
    r->http_version = r->http_major * 1000 + r->http_minor;
    r->state = sw_start;
    if (r->http_version == 9 && r->method != HTTP_GET)
        return -25;

    return 0;
}

/*
    < 0 error
    = 0 解析全部完成
    1   没有读完, 需要继续读
    100 已经成功解析完一行
*/
int http_parse_request_header(struct http_request *r, struct buffer *b)
{
    enum
    {
        sw_start = 0,
        sw_name,
        sw_space_before_value,
        sw_value,
        sw_space_after_value,
        //sw_ignore_line,
        sw_almost_done,
        sw_header_almost_done
    } state;

    static unsigned char lowcase[] =
        "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
        "\0\0\0\0\0\0\0\0\0\0\0\0\0-\0\0" "0123456789\0\0\0\0\0\0"
        "\0abcdefghijklmnopqrstuvwxyz\0\0\0\0\0"
        "\0abcdefghijklmnopqrstuvwxyz\0\0\0\0\0"
        "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
        "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
        "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
        "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0";

    unsigned char c, ch, *p;
    state = r->state;
    unsigned hash = r->header_hash;
    unsigned i = r->lowcase_index;

    for (p = b->pos; p < b->last; p++)
    {
        ch = *p;
        switch (state)
        {
            case sw_start:
                r->header_name.p = p;
                r->invalid_header = 0;
                switch (ch)
                {
                    case CR:
                        r->header_name.len = p - r->header_name.p;
                        state = sw_header_almost_done;
                        break;
                    case LF:
                        r->header_name.len = p - r->header_name.p;
                        goto header_done;
                    default:
                        state = sw_name;
                        c = lowcase[ch];
                        if (c)
                        {
                            hash = HASH(0, c);
                            r->lowcase_header[0] = c;
                            i = 1;
                            break;
                        }

                        if (ch == '\0')
                            return -1;
                        r->invalid_header = 1;
                        break;
                }
                break;
            /* header name */
            case sw_name:
                c = lowcase[ch];    //只计算0-9, -, A-Z, a-z
                if (c)
                {
                    hash = HASH(hash, c);
                    r->lowcase_header[i++] = c;
                    i &= (HTTP_LC_HEADER_LEN - 1);
                    break;
                }
                if (ch == ':')
                {
                    r->header_name.len = p - r->header_name.p;
                    state = sw_space_before_value;
                    break;
                }
                if (ch == CR)
                {
                    r->header_name.len = p - r->header_name.p;
                    str_init(&r->header_value);
                    state = sw_almost_done;
                    break;
                }
                if (ch == LF)
                {
                    r->header_name.len = p - r->header_name.p;
                    str_init(&r->header_value);
                    goto done;
                }
                if (ch == '\0')
                    return -2;
                r->invalid_header = 1;
                break;
            /* space* before header value */
            case sw_space_before_value:
                switch (ch)
                {
                    case ' ':
                        break;
                    case CR:
                        str_init(&r->header_value);
                        state = sw_almost_done;
                        break;
                    case LF:
                        str_init(&r->header_value);
                        goto done;
                    case '\0':
                        return -3;
                    default:
                        r->header_value.p = p;
                        state = sw_value;
                        break;
                }
                break;
            /* header value */
            case sw_value:
                switch (ch)
                {
                    case ' ':
                        r->header_value.len = p - r->header_value.p;
                        state = sw_space_after_value;
                        break;
                    case CR:
                        r->header_value.len = p - r->header_value.p;
                        state = sw_almost_done;
                        break;
                    case LF:
                        r->header_value.len = p - r->header_value.p;
                        goto done;
                    case '\0':
                        return -4;
                }
                break;
            /* space* before end of header line */
            case sw_space_after_value:
                switch (ch)
                {
                    case ' ':
                        break;
                    case CR:
                        state = sw_almost_done;
                        break;
                    case LF:
                        goto done;
                    case '\0':
                        return -5;
                    default:
                        state = sw_value;
                        break;
                }
                break;
            /* end of header line */
            case sw_almost_done:
                switch (ch)
                {
                    case LF:
                        goto done;
                    case CR:
                        break;
                    default:
                        return -6;
                }
                break;
            /* end of header */
            case sw_header_almost_done:
                switch (ch)
                {
                    case LF:
                        goto header_done;
                    default:
                        return -7;
                }
        }
    }
    b->pos = p;
    r->state = state;
    r->header_hash = hash;
    r->lowcase_index = i;
    return 1;   //还要继续读

done:
    b->pos = p + 1;
    r->state = sw_start;
    r->header_hash = hash;
    r->lowcase_index = i;
    return 0;   //读到一行请求头

header_done:
    b->pos = p + 1;
    r->state = sw_start;
    return 100; //全部的请求头处理完
}

