#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "list.h"
#include "http_parser.h"

#define MIN(x, y)	((x) <= (y) ? (x) : (y))
#define MAX(x, y)	((x) >= (y) ? (x) : (y))

#define HTTP_START_LINE_MAX		8192
#define HTTP_HEADER_VALUE_MAX	8192
#define HTTP_CHUNK_LINE_MAX		1024
#define HTTP_TRAILER_LINE_MAX	8192
#define HTTP_MSGBUF_INIT_SIZE	2048

enum
{
	HPS_START_LINE,
	HPS_HEADER_NAME,
	HPS_HEADER_VALUE,
	HPS_HEADER_COMPLETE
};

enum
{
	CPS_CHUNK_DATA,
	CPS_TRAILER_PART,
	CPS_CHUNK_COMPLETE
};

struct __header_line
{
	struct list_head list;
	int name_len;
	int value_len;
	char *buf;
};

int http_parser_add_header(const void *name, size_t name_len,
						   const void *value, size_t value_len,
						   http_parser_t *parser)
{
	size_t size = sizeof (struct __header_line) + name_len + value_len + 4;
	struct __header_line *line;

	line = (struct __header_line *)malloc(size);
	if (line)
	{
		line->buf = (char *)(line + 1);
		memcpy(line->buf, name, name_len);
		line->buf[name_len] = ':';
		line->buf[name_len + 1] = ' ';
		memcpy(line->buf + name_len + 2, value, value_len);
		line->buf[name_len + 2 + value_len] = '\r';
		line->buf[name_len + 2 + value_len + 1] = '\n';
		line->name_len = name_len;
		line->value_len = value_len;
		list_add_tail(&line->list, &parser->header_list);
		return 0;
	}

	return -1;
}

int http_parser_set_header(const void *name, size_t name_len,
						   const void *value, size_t value_len,
						   http_parser_t *parser)
{
	struct __header_line *line;
	struct list_head *pos;
	char *buf;

	list_for_each(pos, &parser->header_list)
	{
		line = list_entry(pos, struct __header_line, list);
		if (line->name_len == name_len &&
			strncasecmp(line->buf, name, name_len) == 0)
		{
			if (value_len > line->value_len)
			{
				buf = (char *)malloc(name_len + value_len + 4);
				if (!buf)
					return -1;

				if (line->buf != (char *)(line + 1))
					free(line->buf);

				line->buf = buf;
				memcpy(buf, name, name_len);
				buf[name_len] = ':';
				buf[name_len + 1] = ' ';
			}

			memcpy(line->buf + name_len + 2, value, value_len);
			line->buf[name_len + 2 + value_len] = '\r';
			line->buf[name_len + 2 + value_len + 1] = '\n';
			line->value_len = value_len;
			return 0;
		}
	}

	return http_parser_add_header(name, name_len, value, value_len, parser);
}

static int __match_request_line(const char *method,
								const char *uri,
								const char *version,
								http_parser_t *parser)
{
	if (strcmp(version, "HTTP/1.0") == 0 || strncmp(version, "HTTP/0", 6) == 0)
		parser->keep_alive = 0;

	method = strdup(method);
	if (method)
	{
		uri = strdup(uri);
		if (uri)
		{
			version = strdup(version);
			if (version)
			{
				free(parser->method);
				free(parser->uri);
				free(parser->version);
				parser->method = (char *)method;
				parser->uri = (char *)uri;
				parser->version = (char *)version;
				return 0;
			}

			free((char *)uri);
		}

		free((char *)method);
	}

	return -1;
}

static int __match_status_line(const char *version,
							   const char *code,
							   const char *phrase,
							   http_parser_t *parser)
{
	if (strcmp(version, "HTTP/1.0") == 0 || strncmp(version, "HTTP/0", 6) == 0)
		parser->keep_alive = 0;

	if (*code == '1' || strcmp(code, "204") == 0 || strcmp(code, "304") == 0)
		parser->transfer_length = 0;

	version = strdup(version);
	if (version)
	{
		code = strdup(code);
		if (code)
		{
			phrase = strdup(phrase);
			if (phrase)
			{
				free(parser->version);
				free(parser->code);
				free(parser->phrase);
				parser->version = (char *)version;
				parser->code = (char *)code;
				parser->phrase = (char *)phrase;
				return 0;
			}

			free((char *)code);
		}

		free((char *)version);
	}

	return -1;
}

static int __match_message_header(const char *name, size_t name_len,
								  const char *value, size_t value_len,
								  http_parser_t *parser)
{
	switch (name_len)
	{
	case 6:
		if (strcasecmp(name, "Expect") == 0)
		{
			if (strcasecmp(value, "100-continue") == 0)
				parser->expect_continue = 1;
		}

	case 10:
		if (strcasecmp(name, "Connection") == 0)
		{
			if (strcasecmp(value, "Keep-Alive") == 0)
				parser->keep_alive = 1;
			else if (strcasecmp(value, "close") == 0)
				parser->keep_alive = 0;
		}

		break;

	case 14:
		if (strcasecmp(name, "Content-Length") == 0)
		{
			if (*value >= '0' && *value <= '9')
				parser->content_length = atoi(value);
		}

		break;

	case 17:
		if (strcasecmp(name, "Transfer-Encoding") == 0)
		{
			if (value_len != 8 || strcasecmp(value, "identity") != 0)
				parser->chunked = 1;
		}

		break;
	}

	return http_parser_add_header(name, name_len, value, value_len, parser);
}

static int __parse_start_line(const char *ptr, size_t len,
							  http_parser_t *parser)
{
	char start_line[HTTP_START_LINE_MAX];
	size_t min = MIN(HTTP_START_LINE_MAX, len);
	char *p1, *p2, *p3;
	size_t i;
	int ret;

	if (len >= 2 && ptr[0] == '\r' && ptr[1] == '\n')
	{
		parser->header_offset += 2;
		return 1;
	}

	for (i = 0; i < min; i++)
	{
		start_line[i] = ptr[i];
		if (start_line[i] == '\r')
		{
			if (i == len - 1)
				return 0;

			if (ptr[i + 1] != '\n')
				return -2;

			start_line[i] = '\0';
			p1 = start_line;
			p2 = strchr(p1, ' ');
			if (p2)
				*p2++ = '\0';
			else
				return -2;

			p3 = strchr(p2, ' ');
			if (p3)
				*p3++ = '\0';
			else
				return -2;

			if (parser->is_resp)
				ret = __match_status_line(p1, p2, p3, parser);
			else
				ret = __match_request_line(p1, p2, p3, parser);

			if (ret < 0)
				return -1;

			parser->header_offset += i + 2;
			parser->header_state = HPS_HEADER_NAME;
			return 1;
		}

		if (start_line[i] == '\0')
			return -2;
	}

	if (i == HTTP_START_LINE_MAX)
		return -2;

	return 0;
}

static int __parse_header_name(const char *ptr, size_t len,
							   http_parser_t *parser)
{
	size_t min = MIN(HTTP_HEADER_NAME_MAX, len);
	size_t i;

	if (len >= 2 && ptr[0] == '\r' && ptr[1] == '\n')
	{
		parser->header_offset += 2;
		parser->header_state = HPS_HEADER_COMPLETE;
		return 1;
	}

	for (i = 0; i < min; i++)
	{
		if (ptr[i] == ':')
		{
			parser->namebuf[i] = '\0';
			parser->header_offset += i + 1;
			parser->header_state = HPS_HEADER_VALUE;
			return 1;
		}

		if (ptr[i] == '\0')
			return -2;

		parser->namebuf[i] = ptr[i];
	}

	if (i == HTTP_HEADER_NAME_MAX)
		return -2;

	return 0;
}