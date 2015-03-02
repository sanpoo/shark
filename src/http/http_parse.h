#ifndef __HTTP_PARSE_H__
#define __HTTP_PARSE_H__

#include "http_request.h"

int http_parse_request_line(struct http_request *request, struct buffer *b);
int http_parse_request_header(struct http_request *request, struct buffer *b);

#endif
