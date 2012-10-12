//
//  Callback.m
//  Texr
//
//  Created by Ilya Petrov on 9/25/12.
//  Copyright (c) 2012 Ilya Petrov. All rights reserved.
//

#import "Callback.h"
#include <objc/message.h>


@implementation Callback
+ (id) withObjSel: (id)pObj sel:(SEL)pSel {
    return [[Callback alloc] initWithObjSel: pObj sel:pSel];
}
- (id) initWithObjSel: (id)pObj sel:(SEL)pSel {
    self = [super init];
    if(!self) return self;

    obj = pObj;
    sel = pSel;

    return self;
}

- (oneway void) response: (id)ret {
    objc_msgSend(obj, sel, ret);
}
@end
