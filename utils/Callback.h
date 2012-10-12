//
//  Callback.h
//  Texr
//
//  Created by Ilya Petrov on 9/25/12.
//  Copyright (c) 2012 Ilya Petrov. All rights reserved.
//

#import <Foundation/Foundation.h>

#define CB(_ob, _sel) ([Callback withObjSel:_ob sel:@selector(_sel)])
@interface Callback : NSObject {
    id obj;
    SEL sel;
}
+ (id) withObjSel: (id)pObj sel:(SEL)pSel;
- (id) initWithObjSel: (id)pObj sel:(SEL)pSel;
- (oneway void) response: (id)ret;
@end
