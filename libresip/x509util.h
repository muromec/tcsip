#ifndef X509_UTIL
#define X509_UTIL
int x509_info(char *path, int *rafter, int *rbefore, char **cname);
int x509_pub_priv(struct mbuf **rpriv, struct mbuf **rpub);
#endif
