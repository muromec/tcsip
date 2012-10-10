//
//  MProxy.h
//  Texr
//
//  Created by Ilya Petrov on 10/7/12.
//  Copyright (c) 2012 Ilya Petrov. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "MailBox.h"

@interface MProxy : NSProxy {
    id target;
    MailBox *mbox;
}

+ (id) withTarget: (id) target;
+ (id) withTargetBox: (id) pTarget box:(id)pBox;

@property (readonly) MailBox* mbox;

@end
