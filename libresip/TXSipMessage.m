//
//  TXSipMessage.m
//  Texr
//
//  Created by Ilya Petrov on 9/21/12.
//  Copyright (c) 2012 Ilya Petrov. All rights reserved.
//

#import "TXSipMessage.h"

@implementation TXSipMessage
- (void) send
{

    [self createHandle];

    mstate = MSTATE_ROUTE;
}

- (void) response
{

    switch(mstate) {
    case MSTATE_ROUTE:
        [self route_response];
        break;

    case MSTATE_TRY:
        [self send_response: 200];
    default:
        NSLog(@"message in state %d", mstate);
    }

}

- (void) route_response
{
    /*
    if(status != 200) {
        printf("route failed. what to do?\n");
        return;
    }
    */

    dest = [NSString stringWithFormat:@"<sip:%s:%s>",
         "host", "5060"];


    [self createHandle];
    mstate = MSTATE_TRY;

}

- (void) send_response: (int)status
{
    switch(status) {
    case 200:
        printf("sent succesfully\n");
        break;
    default:
        printf("send failed\n");
    }
}

- (void) hangup: (bool) remote
{
    NSLog(@"chat hangup");
}

@end
