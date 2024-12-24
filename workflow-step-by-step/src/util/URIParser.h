#ifndef _URIPARSER_H_
#define _URIPARSER_H_

#include <string>
#include <vector>
#include <map>
#include <string.h>

#define URI_STATE_INIT		0
#define URI_STATE_SUCCESS	1
#define URI_STATE_INVALID	2
#define URI_STATE_ERROR		3

/**
 * @file   URIParser.h
 * @brief  URI parser
 */

// RAII: YES
class ParsedURI
{
public:
    char *scheme;
    char *userinfo;
    char *host;
    char *port;
    char *path;
    char *query;
    char *fragment;
    int state;
    int error;

    ParsedURI() { init(); }
    virtual ~ParsedURI() { deinit(); }

    // copy constructor
    ParsedURI(const ParsedURI& copy);
    // copy operator
    ParsedURI& operator= (const ParsedURI& copy);
    // move constructor
    ParsedURI(ParsedURI&& move);
    // move operator
    ParsedURI& operator= (ParsedURI&& move);

private:
    void init();
    void deinit();

    void __copy(const ParsedURI &copy);
    void __move(ParsedURI&& move);
};

class URIParser
{
public:
	// return 0 mean succ, -1 mean fail
	static int parse(const char *str, ParsedURI& uri);
	static int parse(const std::string& str, ParsedURI& uri);

	static std::map<std::string, std::vector<std::string>>
	split_query_strict(const std::string &query);

	static std::map<std::string, std::string>
	split_query(const std::string &query);

	static std::vector<std::string> split_path(const std::string &path);
};

////////////////////////

inline void ParsedURI::init()
{
	scheme = NULL;
	userinfo = NULL;
	host = NULL;
	port = NULL;
	path = NULL;
	query = NULL;
	fragment = NULL;
	state = URI_STATE_INIT;
	error = 0;
}

inline ParsedURI::ParsedURI(const ParsedURI& copy)
{
	__copy(copy);
}

inline ParsedURI::ParsedURI(ParsedURI&& move)
{
	__move(std::move(move));
}

inline ParsedURI& ParsedURI::operator= (const ParsedURI& copy)
{
	if (this != &copy)
	{
		deinit();
		__copy(copy);
	}

	return *this;
}

inline ParsedURI& ParsedURI::operator= (ParsedURI&& move)
{
	if (this != &move)
	{
		deinit();
		__move(std::move(move));
	}

	return *this;
}

inline int URIParser::parse(const std::string& str, ParsedURI& uri)
{
	return parse(str.c_str(), uri);
}

#endif