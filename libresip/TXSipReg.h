//
//  TXSipReg.h
//  Texr
//
//  Created by Ilya Petrov on 8/26/12.
//  Copyright (c) 2012 Ilya Petrov. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "TXBaseOp.h"

typedef enum {
    REG_NONE,
    REG_START=1,
    REG_AUTH=2,
    REG_ONLINE=4
} reg_state_t;

@interface TXSipReg : TXBaseOp {
    struct sipreg *reg;
    reg_state_t rstate;
}

- (void) setup;

- (void) send;

- (void) response: (int) status phrase:(const char*)phrase;

@end
