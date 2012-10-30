//
//  MailBox.h
//  ActorKit
//
//  Created by Ilya Petrov on 10/7/12.
//
//

#import <Foundation/Foundation.h>

@interface MailBox : NSObject {
    id queue;
    NSInteger kickFd;
}
- (id) qpop;
- (void) qput: (id) data;

@property NSInteger kickFd;

@end
