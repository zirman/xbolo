//
//  XBoloAppDelegate.m
//  XBolo
//
//  Created by Robert Chrzanowski on 6/28/09.
//  Copyright Robert Chrzanowski 2009. All rights reserved.
//

#import "XBoloAppDelegate.h"
#import "XBoloViewController.h"

@implementation XBoloAppDelegate

@synthesize window;
@synthesize viewController;


- (void)applicationDidFinishLaunching:(UIApplication *)application {    
    
    // Override point for customization after app launch    
    [window addSubview:viewController.view];
    [window makeKeyAndVisible];
}


- (void)dealloc {
    [viewController release];
    [window release];
    [super dealloc];
}


@end
