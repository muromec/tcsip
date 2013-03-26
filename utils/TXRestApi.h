//
//  TXRestApi.h
//  Texr
//
//  Created by Ilya Petrov on 8/26/12.
//  Copyright (c) 2012 Ilya Petrov. All rights reserved.
//

#import <Foundation/Foundation.h>
struct httpc;
@class MailBox;

@interface TXRestApi : NSObject {
    struct httpc* httpc;
    MailBox* ret_box;
    NSMutableArray *rq;
}

- (id) initWithApp:(struct httpc*)app;

- (id)request: (NSString*)path cb:(id)pCb;
- (id)load: (NSString*)path cb:(id)pCb;
- (void)ready: (id)req;
- (void)kick;

- (void) cert:(NSString*)cert;

@property MailBox* ret_box;

@end

extern const NSDateFormatter *http_df;

