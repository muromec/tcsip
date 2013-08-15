#define _str(__x, __len) ([[NSString alloc] initWithBytes:__x length:__len encoding:NSASCIIStringEncoding])
#define _pstr(__pl) (_str(__pl.p, __pl.l))

#define _byte(_x) ([_x cStringUsingEncoding:NSASCIIStringEncoding])
