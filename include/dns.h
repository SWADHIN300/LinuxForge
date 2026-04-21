#ifndef DNS_H
#define DNS_H

#include "commit.h"

int dns_update_hosts(const char *container_id);
int dns_add_entry(const char *name, const char *ip);
int dns_remove_entry(const char *name);
int dns_list(int json_output);
int dns_cmd(int argc, char *argv[]);

#endif
