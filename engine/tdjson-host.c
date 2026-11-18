/* A local TDLib process, separate from the Cascades C++ runtime.
 * stdin: one TDLib JSON request per line. stdout: TDLib JSON updates/responses.
 * No account credentials are accepted on the command line or logged here.
 */
#define _POSIX_C_SOURCE 200809L
#define JSMN_STATIC
#define JSMN_STRICT
#include "vendor/jsmn.h"

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <time.h>
#include <unistd.h>

#define MAX_REQUEST (1024 * 1024)
#define CLOSE_TIMEOUT 10.0

typedef int (*CreateClient)(void);
typedef void (*SendRequest)(int, const char *);
typedef const char *(*ReceiveUpdate)(double);
typedef const char *(*ExecuteRequest)(const char *);

static volatile sig_atomic_t stop_signal;
static FILE *startup_trace;
static unsigned int traced_requests[64];
static size_t traced_request_count;

static void request_stop(int signum)
{
    stop_signal = signum;
}

static double monotonic_seconds(void)
{
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        return -1.0;
    }
    return (double)now.tv_sec + (double)now.tv_nsec / 1000000000.0;
}

static int token_is(const char *json, const jsmntok_t *token, const char *text)
{
    return token->type == JSMN_STRING &&
        (size_t)(token->end - token->start) == strlen(text) &&
        memcmp(json + token->start, text, strlen(text)) == 0;
}

/* Only inspect the small authorization update, never search message text for
 * a state name. Token ranges keep nested @type fields separate.
 */
static int object_field(const char *json, const jsmntok_t *tokens,
                        int count, int object, const char *name)
{
    int i = object + 1;
    while (i < count && tokens[i].start < tokens[object].end) {
        int value = i + 1;
        if (value >= count) {
            return -1;
        }
        if (token_is(json, &tokens[i], name)) {
            return value;
        }
        i = value + 1;
        while (i < count && tokens[i].start < tokens[value].end) {
            ++i;
        }
    }
    return -1;
}

/* Opt-in device profiling. Never record payloads, even for errors. */
static unsigned int trace_id(const char *json)
{
    const char prefix[] = "\"@extra\":\"berrygram.";
    const char *field = strstr(json, prefix);
    if (!field) return 0;
    field += sizeof(prefix) - 1;
    unsigned int id = 0;
    while (*field >= '0' && *field <= '9' && id < 10000000U)
        id = id * 10U + (unsigned int)(*field++ - '0');
    return *field == '"' ? id : 0;
}

static void trace_request(const char *json)
{
    if (!startup_trace || traced_request_count == sizeof(traced_requests) / sizeof(*traced_requests)) return;
    const char *method = strstr(json, "\"@type\":\"");
    if (!method) return;
    method += sizeof("\"@type\":\"") - 1;
    const char *label = NULL;
    if (strncmp(method, "getChatHistory\"", sizeof("getChatHistory\"") - 1) == 0) label = "history";
    else if (strncmp(method, "loadChats\"", sizeof("loadChats\"") - 1) == 0) label = "list";
    else if (strncmp(method, "testSquareInt\"", sizeof("testSquareInt\"") - 1) == 0) label = "control";
    if (!label) return;
    unsigned int id = trace_id(json);
    if (!id) return;
    traced_requests[traced_request_count++] = id;
    fprintf(startup_trace, "%.3f %u %s requested\n", monotonic_seconds(), id, label);
    fflush(startup_trace);
}

static unsigned int trace_response(const char *json)
{
    if (!startup_trace) return 0;
    unsigned int id = trace_id(json);
    if (!id) return 0;
    for (size_t i = 0; i < traced_request_count; ++i) {
        if (traced_requests[i] == id) {
            fprintf(startup_trace, "%.3f %u reply received\n", monotonic_seconds(), id);
            fflush(startup_trace);
            return id;
        }
    }
    return 0;
}

static int write_output(const char *data, size_t size)
{
    while (size) {
        const size_t chunk = size < 4096 ? size : 4096;
        const ssize_t sent = write(STDOUT_FILENO, data, chunk);
        if (sent > 0) {
            data += sent;
            size -= (size_t)sent;
        } else if (sent == -1 && errno == EINTR && !stop_signal) {
            continue;
        } else if (sent == -1 && (errno == EAGAIN || errno == EWOULDBLOCK) && !stop_signal) {
            struct pollfd output = { STDOUT_FILENO, POLLOUT, 0 };
            if (poll(&output, 1, 100) == -1 && errno != EINTR) return -1;
        } else {
            return -1;
        }
    }
    return 0;
}

static int is_closed_update(const char *json)
{
    jsmn_parser parser;
    jsmntok_t tokens[48];
    int count, type, state, state_type;
    jsmn_init(&parser);
    count = jsmn_parse(&parser, json, strlen(json), tokens, 48);
    if (count < 1 || tokens[0].type != JSMN_OBJECT) {
        return 0;
    }
    type = object_field(json, tokens, count, 0, "@type");
    state = object_field(json, tokens, count, 0, "authorization_state");
    if (type < 0 || state < 0 ||
        !token_is(json, &tokens[type], "updateAuthorizationState") ||
        tokens[state].type != JSMN_OBJECT) {
        return 0;
    }
    state_type = object_field(json, tokens, count, state, "@type");
    return state_type >= 0 &&
        token_is(json, &tokens[state_type], "authorizationStateClosed");
}

static int forward_request(SendRequest send_request, int client,
                           char *request, size_t length)
{
    if (length && request[length - 1] == '\r') {
        --length;
    }
    if (!length) {
        return 0;
    }
    if (memchr(request, '\0', length)) {
        fprintf(stderr, "Request contains a NUL byte.\n");
        return -1;
    }
    request[length] = '\0';
    trace_request(request);
    send_request(client, request);
    return 0;
}

int main(int argc, char **argv)
{
    void *library;
    CreateClient create_client;
    SendRequest send_request;
    ReceiveUpdate receive_update;
    ExecuteRequest execute_request;
    char *request;
    size_t length = 0;
    int client, closing = 0, result = 0, socket_output = 0;
    double close_deadline = 0;
    struct sigaction action;

    if (argc == 2 && strcmp(argv[1], "--help") == 0) {
        puts("Usage: berrygram-tdjson --library /absolute/path/to/libtdjson [--output-fd fd]");
        puts("Pass TDLib JSON requests on stdin; read updates from stdout.");
        return 0;
    }
    if ((argc != 3 && argc != 5) || strcmp(argv[1], "--library") != 0 || argv[2][0] != '/') {
        fprintf(stderr, "Expected --library with an absolute path.\n");
        return 64;
    }
    if (argc == 5) {
        char *end = NULL;
        errno = 0;
        long fd = strtol(argv[4], &end, 10);
        if (strcmp(argv[3], "--output-fd") != 0 || errno || !end || *end || fd < 3 || fd > INT_MAX ||
            dup2((int)fd, STDOUT_FILENO) == -1) {
            fprintf(stderr, "Cannot attach the inherited output channel.\n");
            return 64;
        }
        close((int)fd);
        int flags = fcntl(STDOUT_FILENO, F_GETFL);
        if (flags < 0 || fcntl(STDOUT_FILENO, F_SETFL, flags | O_NONBLOCK) < 0) {
            fprintf(stderr, "Cannot set up the inherited output channel.\n");
            return 64;
        }
        socket_output = 1;
    }
    if (monotonic_seconds() < 0) {
        fprintf(stderr, "Monotonic clock is unavailable.\n");
        return 70;
    }
    library = dlopen(argv[2], RTLD_NOW | RTLD_LOCAL);
    if (!library) {
        fprintf(stderr, "Cannot load TDLib: %s\n", dlerror());
        return 69;
    }
    create_client = (CreateClient)dlsym(library, "td_create_client_id");
    send_request = (SendRequest)dlsym(library, "td_send");
    receive_update = (ReceiveUpdate)dlsym(library, "td_receive");
    execute_request = (ExecuteRequest)dlsym(library, "td_execute");
    if (!create_client || !send_request || !receive_update || !execute_request) {
        fprintf(stderr, "TDLib does not export the required JSON C interface.\n");
        dlclose(library);
        return 69;
    }
    request = (char *)malloc(MAX_REQUEST + 1);
    if (!request) {
        fprintf(stderr, "Cannot allocate request buffer.\n");
        dlclose(library);
        return 70;
    }
    memset(&action, 0, sizeof(action));
    action.sa_handler = request_stop;
    sigemptyset(&action.sa_mask);
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);
    signal(SIGPIPE, SIG_IGN);
    setvbuf(stdout, NULL, _IOLBF, 0);
    if (access("data/ui-check/measure-startup", F_OK) == 0)
        startup_trace = fopen("data/ui-check/bridge.trace", "w");
    execute_request("{\"@type\":\"setLogVerbosityLevel\",\"new_verbosity_level\":0}");
    client = create_client();
    send_request(client, "{\"@type\":\"getAuthorizationState\"}");

    for (;;) {
        const char *update;
        double receive_timeout = socket_output ? 0 : 0.1;
        if (stop_signal && !closing) {
            result = 128 + stop_signal;
            closing = 1;
        }
        if (!closing) {
            fd_set read_set;
            struct timeval timeout = {0, 0};
            int ready;
            FD_ZERO(&read_set);
            FD_SET(STDIN_FILENO, &read_set);
            ready = select(STDIN_FILENO + 1, &read_set, NULL, NULL, &timeout);
            if (ready < 0 && errno != EINTR) {
                fprintf(stderr, "Cannot read request stream.\n");
                result = 74;
                closing = 1;
            } else if (ready > 0) {
                char chunk[8192];
                ssize_t size = read(STDIN_FILENO, chunk, sizeof(chunk));
                ssize_t i;
                receive_timeout = 0;
                if (size == 0) {
                    if (forward_request(send_request, client, request, length) < 0) {
                        result = 65;
                    }
                    closing = 1;
                } else if (size < 0 && errno != EINTR) {
                    fprintf(stderr, "Cannot read request stream.\n");
                    result = 74;
                    closing = 1;
                }
                for (i = 0; i < size && !closing; ++i) {
                    if (chunk[i] == '\n') {
                        if (forward_request(send_request, client, request, length) < 0) {
                            result = 65;
                            closing = 1;
                        }
                        length = 0;
                    } else if (length == MAX_REQUEST) {
                        fprintf(stderr, "Request exceeds the 1 MiB limit.\n");
                        result = 65;
                        closing = 1;
                    } else {
                        request[length++] = chunk[i];
                    }
                }
            }
        }
        if (closing && close_deadline == 0) {
            send_request(client, "{\"@type\":\"close\"}");
            close_deadline = monotonic_seconds() + CLOSE_TIMEOUT;
        }
        update = receive_update(receive_timeout);
        if (update) {
            /* TDLib owns this pointer until the next receive/execute. Consume
             * it completely here, before making either call again.
             */
            const unsigned int traced_response = trace_response(update);
            const int written = socket_output ?
                (write_output(update, strlen(update)) == 0 && write_output("\n", 1) == 0) : puts(update) != EOF;
            if (!written) {
                result = 74;
                closing = 1;
            }
            if (traced_response) {
                fprintf(startup_trace, "%.3f %u reply forwarded\n", monotonic_seconds(), traced_response);
                fflush(startup_trace);
            }
            if (is_closed_update(update)) {
                break;
            }
        } else if (socket_output) {
            /* The BB10 build cannot depend on TDLib's socket-based wakeup
             * reaching the JSON host promptly. Check the queues directly. */
            const struct timespec idle = { 0, 20000000L };
            nanosleep(&idle, NULL);
        }
        if (close_deadline && monotonic_seconds() >= close_deadline) {
            fprintf(stderr, "TDLib did not close within 10 seconds.\n");
            result = 70;
            break;
        }
    }
    free(request);
    if (startup_trace) fclose(startup_trace);
    /* Keep the module mapped until process exit: TDLib owns process-wide
     * worker infrastructure even after authorizationStateClosed.
     */
    return result;
}
