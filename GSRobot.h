//
//  GSRobot.h
//  XBolo
//
//  Created by Michael Ash on 9/7/09.
//

#import <Cocoa/Cocoa.h>


@protocol GSRobot;

@interface GSRobot : NSObject
{
    NSBundle *_bundle;
    id <GSRobot> _robot;
    
    NSConditionLock *_condLock;
    NSData *_gamestateData;
    NSMutableArray *_messages;
    BOOL _halt;
}

+ (NSArray *)availableRobots;

- (NSString *)name;

- (NSError *)load;
- (void)unload; // just cleans up memory, does not actually unload code (which is enormously dangerous)
- (void)step; // call with client locked
- (void)receivedMessage: (NSString *)message;

@end
