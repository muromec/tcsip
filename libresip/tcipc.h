#ifndef TCIPC_H
#define TCIPC_H
struct tcsip;
struct msgpack_object;
void tcsip_ob_cmd(struct tcsip*, struct msgpack_object ob);
#endif
