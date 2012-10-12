//
//  TXRestApi.m
//  Texr
//
//  Created by Ilya Petrov on 8/26/12.
//  Copyright (c) 2012 Ilya Petrov. All rights reserved.
//

#import "TXRestApi.h"
#import "JSONKit.h"
#import "Callback.h"

@implementation TXRestApi
+ (void)r: (NSString*)path cb:(id)cb
{
    TXRestApi *api = [[TXRestApi alloc] init];
    [api rload: path cb: cb];
}

- (void)rload: (NSString*)path cb:(id)pCb {
    NSURL *myURL = [NSURL URLWithString:[NSString stringWithFormat:@"http://texr.enodev.org/api/%@",path]];
    
    NSURLRequest *request = [NSURLRequest requestWithURL:myURL
                                             cachePolicy:NSURLRequestReloadIgnoringLocalCacheData
                                         timeoutInterval:60];
    
    self->cb = pCb;

    url_con = [[NSURLConnection alloc] initWithRequest:request delegate:self];
}

- (void)connection:(NSURLConnection *)connection didReceiveResponse:(NSURLResponse *)response {
    responseData = [[NSMutableData alloc] init];
    NSHTTPURLResponse* httpResponse = (NSHTTPURLResponse*)response;
    status_code = (int)[httpResponse statusCode];
}
- (void)connection:(NSURLConnection *)connection didReceiveData:(NSData *)data {
    [responseData appendData:data];
}

- (void)connection:(NSURLConnection *)connection didFailWithError:(NSError *)error {
    // Show error message
    [cb response: nil];
}

- (void)connectionDidFinishLoading:(NSURLConnection *)connection {
    // Use responseData

    JSONDecoder* decoder = [JSONDecoder decoder];
    id ret = [decoder objectWithData: responseData];
    //NSString *status = [ret objectForKey:@"status"];
    NSArray *payload = [ret objectForKey:@"data"];

    [cb response: payload];
}

- (void)connection:(NSURLConnection *)connection willSendRequestForAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge
{
    NSLog(@"http auth challenge %@", challenge);

    if (!username || [challenge previousFailureCount] > 0) {
        [[challenge sender] cancelAuthenticationChallenge:challenge];
        NSLog(@"Bad Username Or Password");
        return;
    }

    NSURLCredential *newCredential = [NSURLCredential credentialWithUser:username password:password persistence:NSURLCredentialPersistenceNone];
    [[challenge sender] useCredential:newCredential forAuthenticationChallenge:challenge];
}

- (void) setAuth:(NSString*)pU password:(NSString*)pW
{
    username = pU;
    password = pW;
}

@end
