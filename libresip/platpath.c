#include "re.h"
#include <stdlib.h>

#if __APPLE__
#define PLATPATH
#include <CoreFoundation/CoreFoundation.h>

int platpath(struct pl *login, char **certpath, char**capath) {

    char *home = getenv("HOME"), *bundle = NULL;
    char path[2048];

    CFBundleRef mbdl = CFBundleGetMainBundle();
    if(!mbdl) goto afail;
    CFURLRef url = CFBundleCopyBundleURL(mbdl);
    if (!CFURLGetFileSystemRepresentation(url, TRUE, (UInt8 *)path, sizeof(path)))
    {
        goto afail1;
    }
    bundle = path;

#if TARGET_OS_IPHONE
    re_sdprintf(capath, "%s/CA.cert", bundle);
#else
    re_sdprintf(capath, "%s/Contents/Resources/CA.cert", bundle);
#endif

    if(home) {
        re_sdprintf(certpath, "%s/Library/Texr/%r.cert", home, login);
    } else {
        re_sdprintf(certpath, "./%r.cert", login);
    }

afail1:
    CFRelease(url);

    return 0;
afail:
    return -1;
}

int platpath_db(struct pl *login, char **dbpath) {
    int err;
    char *home = getenv("HOME");

    if(home) {
        re_sdprintf(dbpath, "%s/Library/Texr/%r.db", home, login);
    } else {
        re_sdprintf(dbpath, "./%r.db", login);
    }

    return 0;
}

#endif

#if ANDROID
#define PLATPATH
int platpath(struct pl *login, char **certpath, char**capath) {
    // assisted with utter hach
    char *home = getenv("HOME");

    if(home) {
        re_sdprintf(capath, "%s/CA.cert", home);
        re_sdprintf(certpath, "%s/%r.cert", home, login);
    } else {
        re_sdprintf(capath, "./CA.cert", home);
        re_sdprintf(certpath, "./%r.cert", login);
    }
}
#endif

#ifndef PLATPATH
int platpath(struct pl *login, char **certpath, char**capath) {
    // assisted with utter hach
    char *home = getenv("HOME");

    if(home) {
        re_sdprintf(certpath, "%s/.texr.cert", home);
    } else {
        re_sdprintf(certpath, "./%r.cert", login);
    }

    re_sdprintf(capath, "./CA.cert", home);
}
#endif
