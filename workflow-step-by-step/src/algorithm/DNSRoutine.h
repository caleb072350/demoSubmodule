#ifndef _DNSROUTINE_H_
#define _DNSROUTINE_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string>

class DNSInput
{
public:
	DNSInput():
		port_(0)
	{}

    //move constructor
	DNSInput(DNSInput&& move) = default;
	//move operator
	DNSInput& operator= (DNSInput &&move) = default;

	void reset(const std::string& host, unsigned short port)
	{
		host_.assign(host);
		port_ = port;
	}

	const std::string& get_host() const { return host_; }
	unsigned short get_port() const { return port_; }

protected:
    std::string host_;
    unsigned short port_;

    friend class DNSRoutine;
};

class DNSOutput
{
public:
	DNSOutput():
		error_(0),
		addrinfo_(NULL)
	{}

    ~DNSOutput()
	{
		if (addrinfo_)
			freeaddrinfo(addrinfo_);
	}

    //move constructor
	DNSOutput(DNSOutput&& move);
	//move operator
	DNSOutput& operator= (DNSOutput&& move);

	int get_error() const { return error_; }
	const struct addrinfo *get_addrinfo() const { return addrinfo_; }

	//if DONOT want DNSOutput release addrinfo, use move_addrinfo in callback
	struct addrinfo *move_addrinfo()
	{
		struct addrinfo *p = addrinfo_;
		addrinfo_ = NULL;
		return p;
	}

protected:
	int error_;
	struct addrinfo *addrinfo_;

	friend class DNSRoutine;
};

class DNSRoutine
{
public:
	static void run(const DNSInput *in, DNSOutput *out);
};

#endif