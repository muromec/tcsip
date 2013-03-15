#ifndef TEXR_CROSS_H
#define TEXR_CROSS_H

#if TARGET_OS_IPHONE
#define IPHONE 1
#else
#define IPHONE 0
#endif

#define on_iphone(x) {}
#define on_mac(x) {}
#define on_ios(x) {}
#define on_ipad(x) {}

extern int ui_idiom;

#if IPHONE
#undef on_iphone
#undef on_ios
#undef on_ipad
#define on_iphone(x) if(!ui_idiom) {x}
#define on_ipad(x) if(ui_idiom==1) {x}
#define on_ios(x) {x}
#define on_wide(x) if(ui_idiom==1) {x}
#else
#undef on_mac
#define on_mac(x) {x}
#define on_wide(x) {x}
#endif

#endif
