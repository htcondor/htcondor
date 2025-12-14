#include "condor_common.h"
#include "condor_config.h"
#include "condor_io.h"
#include "condor_commands.h"
#include "condor_attributes.h"
#include "condor_classad.h"
#include "compat_classad_util.h"
#include "daemon.h"
#include "condor_daemon_core.h"
#include <string>
#include <iostream>
#include <vector>

namespace {

struct DaemonHack : public Daemon {
    using Daemon::Daemon;
    using Daemon::startCommand;
};

void usage()
{
    std::cerr << "Usage: startup_limit_ctl --create|--query [options]\n"
              << "  --schedd <name|addr>  target schedd (default: local)\n"
              << "  --tag <tag>           tag for create / filter for query\n"
              << "  --name <name>         friendly name (create)\n"
              << "  --expr <expr>         ClassAd expression (create)\n"
              << "  --count <N>           rate count (create)\n"
              << "  --window <sec>        rate window seconds (create)\n"
              << "  --cost-expr <expr>    per-job startup cost (create, default 1)\n"
              << "  --burst <N>           burst depth allowed beyond count (create, default 0)\n"
              << "  --max-burst-cost <N>  maximum cost debited in a single burst (default unlimited)\n"
              << "  --expires <sec>       expiration seconds (create, default 300)\n"
              << "  --verbose             print raw ads\n";
}

bool parse_int(const char *s, int &out)
{
    char *end = nullptr;
    long v = strtol(s, &end, 10);
    if (!s || *s == '\0' || (end && *end)) { return false; }
    out = (int)v;
    return true;
}

ReliSock *open_command_sock(DaemonHack &schedd, int cmd, CondorError &err)
{
    Sock *sock = nullptr;
    StartCommandResult res = schedd.startCommand(cmd, Stream::reli_sock, &sock, 20, &err, 0, nullptr, nullptr, false);
    if (res != StartCommandSucceeded) { return nullptr; }
    ReliSock *rsock = dynamic_cast<ReliSock *>(sock);
    if (!rsock) {
        if (sock) { delete sock; }
        return nullptr;
    }
    rsock->timeout(20);
    return rsock;
}

void print_ad(const ClassAd &ad)
{
    classad::ClassAdUnParser unp;
    std::string s;
    unp.Unparse(s, &ad);
    std::cout << s << "\n";
}

} // namespace

int main(int argc, char **argv)
{
    config();

    bool do_create = false;
    bool do_query = false;
    bool verbose = false;
    std::string schedd_arg;
    std::string tag;
    std::string name;
    std::string expr = "true";
    std::string cost_expr = "1";
    int rate_count = 0;
    int rate_window = 0;
    int burst = 0;
    int max_burst_cost = 0;
    int expires = 300;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--create") { do_create = true; }
        else if (a == "--query") { do_query = true; }
        else if (a == "--schedd" && i + 1 < argc) { schedd_arg = argv[++i]; }
        else if (a == "--tag" && i + 1 < argc) { tag = argv[++i]; }
        else if (a == "--name" && i + 1 < argc) { name = argv[++i]; }
        else if (a == "--expr" && i + 1 < argc) { expr = argv[++i]; }
        else if (a == "--count" && i + 1 < argc) { if (!parse_int(argv[++i], rate_count)) { usage(); return 1; } }
        else if (a == "--window" && i + 1 < argc) { if (!parse_int(argv[++i], rate_window)) { usage(); return 1; } }
        else if (a == "--cost-expr" && i + 1 < argc) { cost_expr = argv[++i]; }
        else if (a == "--burst" && i + 1 < argc) { if (!parse_int(argv[++i], burst)) { usage(); return 1; } }
        else if (a == "--max-burst-cost" && i + 1 < argc) { if (!parse_int(argv[++i], max_burst_cost)) { usage(); return 1; } }
        else if (a == "--expires" && i + 1 < argc) { if (!parse_int(argv[++i], expires)) { usage(); return 1; } }
        else if (a == "--verbose") { verbose = true; }
        else { usage(); return 1; }
    }

    if (do_create == do_query) { usage(); return 1; }

    DaemonHack schedd(DT_SCHEDD, schedd_arg.empty() ? nullptr : schedd_arg.c_str());
    if (!schedd.locate()) {
        std::cerr << "Failed to locate schedd" << (schedd_arg.empty() ? "" : (": " + schedd_arg)) << "\n";
        return 1;
    }

    CondorError err;
    int cmd = do_create ? CREATE_STARTUP_LIMIT : QUERY_STARTUP_LIMITS;
    ReliSock *sock = open_command_sock(schedd, cmd, err);
    if (!sock) {
        std::cerr << "Failed to open command socket: " << err.getFullText() << "\n";
        return 1;
    }

    ClassAd req;
    if (do_create) {
        req.InsertAttr(ATTR_STARTUP_LIMIT_TAG, tag);
        if (!name.empty()) { req.InsertAttr(ATTR_STARTUP_LIMIT_NAME, name); }

        classad::ClassAdParser expr_parser;
        expr_parser.SetOldClassAd(true);
        classad::ExprTree *expr_tree = expr_parser.ParseExpression(expr);
        if (!expr_tree) {
            std::cerr << "Failed to parse expression: " << expr << "\n";
            sock->close();
            delete sock;
            return 1;
        }

        classad::ExprTree *cost_expr_tree = expr_parser.ParseExpression(cost_expr);
        if (!cost_expr_tree) {
            std::cerr << "Failed to parse cost expression: " << cost_expr << "\n";
            sock->close();
            delete sock;
            return 1;
        }

        req.Insert(ATTR_STARTUP_LIMIT_EXPR, expr_tree);
        req.Insert(ATTR_STARTUP_LIMIT_COST_EXPR, cost_expr_tree);
        req.InsertAttr(ATTR_STARTUP_LIMIT_RATE_COUNT, rate_count);
        req.InsertAttr(ATTR_STARTUP_LIMIT_RATE_WINDOW, rate_window);
        req.InsertAttr(ATTR_STARTUP_LIMIT_BURST, burst);
        if (max_burst_cost > 0) { req.InsertAttr(ATTR_STARTUP_LIMIT_MAX_BURST_COST, max_burst_cost); }
        req.InsertAttr(ATTR_STARTUP_LIMIT_EXPIRATION, expires);
    } else {
        if (!tag.empty()) { req.InsertAttr(ATTR_STARTUP_LIMIT_TAG, tag); }
    }

    sock->encode();
    if (!putClassAd(sock, req) || !sock->end_of_message()) {
        std::cerr << "Failed to send request\n";
        sock->close();
        delete sock;
        return 1;
    }

    sock->decode();
    if (do_create) {
        ClassAd reply;
        if (!getClassAd(sock, reply) || !sock->end_of_message()) {
            std::cerr << "Failed to receive reply\n";
            sock->close();
            delete sock;
            return 1;
        }
        if (verbose) { print_ad(reply); }
        int status = -1;
        reply.LookupInteger(ATTR_STARTUP_LIMIT_STATUS, status);
        if (status != 0) {
            std::string err_text;
            reply.LookupString(ATTR_STARTUP_LIMIT_ERROR, err_text);
            std::cerr << "Error: " << err_text << "\n";
            sock->close();
            delete sock;
            return 2;
        }
        std::string uuid;
        reply.LookupString(ATTR_STARTUP_LIMIT_UUID, uuid);
        std::cout << uuid << "\n";
    } else {
        while (true) {
            ClassAd ad;
            if (!getClassAd(sock, ad)) { break; }
            bool is_last = false;
            ad.LookupBool("Last", is_last);
            if (is_last) { break; }
            std::string ad_tag;
            ad.LookupString(ATTR_STARTUP_LIMIT_TAG, ad_tag);
            if (!tag.empty() && tag != ad_tag) { continue; }
            if (verbose) {
                print_ad(ad);
            } else {
                long long skipped = 0, ignored = 0;
                ad.LookupInteger(ATTR_STARTUP_LIMIT_JOBS_SKIPPED, skipped);
                ad.LookupInteger(ATTR_STARTUP_LIMIT_MATCHES_IGNORED, ignored);
                std::string name_out;
                ad.LookupString(ATTR_STARTUP_LIMIT_NAME, name_out);
                std::cout << ad_tag << " " << name_out << " " << skipped << " " << ignored << "\n";
            }
        }
        sock->end_of_message();
    }

    sock->close();
    delete sock;
    return 0;
}
