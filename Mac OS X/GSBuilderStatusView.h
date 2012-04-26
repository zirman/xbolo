//
//  GSBuilderStatusView.h
//  XBolo
//
//  Created by Robert Chrzanowski on 11/12/04.
//  Copyright 2004 Robert Chrzanowski. All rights reserved.
//

#import <Cocoa/Cocoa.h>

typedef enum {
  GSBuilderStatusViewStateReady,
  GSBuilderStatusViewStateDirection,
  GSBuilderStatusViewStateDead,
} GSBuilderStatusViewState;

@interface GSBuilderStatusView : NSView {
  GSBuilderStatusViewState state;
  float dir;
}

- (GSBuilderStatusViewState)state;
- (void)setState:(GSBuilderStatusViewState)newState;
- (float)dir;
- (void)setDir:(float)newDir;

@end
