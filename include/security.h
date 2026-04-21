#ifndef SECURITY_H
#define SECURITY_H

#include "utils.h"

#define SECURITY_DIR "security"

typedef struct {
    char profile[32];
    char blocked[64][64];
    int blocked_count;
} SecurityProfile;

int seccomp_load_profile(const char *profile_name, SecurityProfile *profile);
int seccomp_apply(const char *profile_name);
int security_list_profiles(int json_output);
int security_cmd(int argc, char *argv[]);

#endif
