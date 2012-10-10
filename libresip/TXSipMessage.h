//
//  TXSipMessage.h
//  Texr
//
//  Created by Ilya Petrov on 9/21/12.
//  Copyright (c) 2012 Ilya Petrov. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "TXSip.h"
#import "TXBaseOp.h"

typedef enum {
    MSTATE_ROUTE,
    MSTATE_TRY
} mstate_t;

@interface TXSipMessage : TXBaseOp {
    mstate_t mstate;
}

- (void) response;
- (void) route_response;

- (void) send;

@end
