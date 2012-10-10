//
//  TXSipUser.h
//  Texr
//
//  Created by Ilya Petrov on 9/25/12.
//  Copyright (c) 2012 Ilya Petrov. All rights reserved.
//

#import <Foundation/Foundation.h>
#include <re.h>

@interface TXSipUser : NSObject {
    NSString* user;
    NSString* name;
    NSString* host;
    NSString* addr;
}
- (id) initWithName: (NSString*)user;
+ (id) withName: (NSString*)user;

+ (id) withAddr: (struct sip_taddr*)addr;
- (id) initWithAddr: (struct sip_taddr*)addr;

@property NSString* user;
@property NSString* name;
@property NSString* host;

@property (readonly) NSString* addr;

@end
