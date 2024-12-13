#include <signal.h>
#include "workflow/WFTaskFactory.h"
#include "workflow/WFFacilities.h"
#include "workflow/RedisMessage.h"

#define RETRY_MAX 2

struct task_data {
    std::string url;
    std::string key;
};

static WFFacilities::WaitGroup wait_group(1);

void sig_handler(int signo) {
    wait_group.done();
}

void redis_callback(WFRedisTask* task) {
    protocol::RedisRequest* req = task->get_req();
    protocol::RedisResponse* resp = task->get_resp();
    int state = task->get_state();
    int error = task->get_error();
    protocol::RedisValue val;

    switch (state)
    {
    case WFT_STATE_SYS_ERROR:
        fprintf(stderr, "system error: %s\n", strerror(error));
        break;
    case WFT_STATE_DNS_ERROR:
		fprintf(stderr, "DNS error: %s\n", gai_strerror(error));
		break;
	case WFT_STATE_SSL_ERROR:
		fprintf(stderr, "SSL error: %d\n", error);
		break;
	case WFT_STATE_TASK_ERROR:
		fprintf(stderr, "Task error: %d\n", error);
		break;
	case WFT_STATE_SUCCESS:
        resp->get_result(val);
        if (val.is_error()) {
            fprintf(stderr, "%*s\n", (int)val.string_view()->size(), val.string_view()->c_str());
            state = WFT_STATE_TASK_ERROR;
        }
        break;
    }
    
    if (state != WFT_STATE_SUCCESS) {
        fprintf(stderr, "Failed. Press Ctrl-C to exit.\n");
        return;
    }

    std::string cmd;
    req->get_command(cmd);
    if (cmd == "SET") {
        task_data* data = (task_data*)task->user_data;
        WFRedisTask* next = WFTaskFactory::create_redis_task(data->url, RETRY_MAX, redis_callback);
        next->get_req()->set_request("GET", {data->key});
        series_of(task)->push_back(next);
        fprintf(stderr, "Redis SET request success. Trying to GET...\n");
    } else { // cmd == "GET"
        if (val.is_string()) {
            fprintf(stderr, "Redis GET success. value = %s\n", val.string_value().c_str());
        } else {
            fprintf(stderr, "Error: Not a string value.\n");
        }
        fprintf(stderr, "Finished. Press Ctrl-C to exit.\n");
    }
}

int main(int argc, char* argv[]) {
    WFRedisTask *task;
    if (argc != 4) {
        fprintf(stderr, "USAGE: %s <redis URL> <key> <val>\n", argv[0]);
        exit(1);
    }

    signal(SIGINT, sig_handler);

    struct task_data data;
    data.url = argv[1];
    if (strncasecmp(argv[1], "redis://", 8) != 0 && strncasecmp(argv[1], "rediss://", 9) != 0) {
        data.url = "redis://" + data.url;
    }

    data.key = argv[2];

    task = WFTaskFactory::create_redis_task(data.url, RETRY_MAX, redis_callback);

    protocol::RedisRequest* req = task->get_req();
    req->set_request("SET", {data.key, argv[3]});

    task->user_data = &data;
    task->start();

    wait_group.wait();
    return 0;
}