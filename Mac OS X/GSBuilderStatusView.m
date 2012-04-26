//
//  GSBuilderStatusView.m
//  XBolo
//
//  Created by Robert Chrzanowski on 11/12/04.
//  Copyright 2004 Robert Chrzanowski. All rights reserved.
//

#import "GSBuilderStatusView.h"
#include "bolo.h"
#include "server.h"
#include "client.h"
#include "vector.h"
#include "errchk.h"

@implementation GSBuilderStatusView

- (id)initWithFrame:(NSRect)frame {
  self = [super initWithFrame:frame];

  if (self) {
    state = GSBuilderStatusViewStateReady;
    dir = 0.0;
    [self setNeedsDisplay:YES];
  }

  return self;
}

- (GSBuilderStatusViewState)state {
  return state;
}

- (void)setState:(GSBuilderStatusViewState)aState {
  switch (aState) {
  case GSBuilderStatusViewStateReady:
  case GSBuilderStatusViewStateDirection:
  case GSBuilderStatusViewStateDead:
    if (state != aState) {
      state = aState;
      [self setNeedsDisplay:YES];
    }

    break;

  default:
    assert(0);
    break;
  }
}

- (float)dir {
  return dir;
}

- (void)setDir:(float)aDir {
  if (dir != aDir) {
    dir = aDir;

    if (state == GSBuilderStatusViewStateDirection) {
      [self setNeedsDisplay:YES];
    }
  }
}

- (void)drawRect:(NSRect)rect {
  switch (state) {
  case GSBuilderStatusViewStateDirection:
    {
      NSBezierPath *p;
      NSAffineTransform *t;
      NSRect bounds;

      bounds = [self bounds];
      [[NSColor whiteColor] set];
      p = [NSBezierPath bezierPathWithOvalInRect:NSMakeRect(-2.0, -2.0, 4.0, 4.0)];
      [p moveToPoint:NSMakePoint(bounds.size.width*0.25, bounds.size.height*0.125)];
      [p lineToPoint:NSMakePoint(bounds.size.width*0.5, 0.0)];
      [p lineToPoint:NSMakePoint(bounds.size.width*0.25, -bounds.size.height*0.125)];
      [p moveToPoint:NSMakePoint(0.0, 0.0)];
      [p lineToPoint:NSMakePoint(bounds.size.width*0.5, 0.0)];

      t = [NSAffineTransform transform];
      [t translateXBy:bounds.size.width*0.5 yBy:bounds.size.height*0.5];
      [t rotateByRadians:dir];
      [p transformUsingAffineTransform:t];
      [p stroke];
      [p fill];
      break;
    }
 
  case GSBuilderStatusViewStateDead:
    [[NSColor colorWithPatternImage:[NSImage imageNamed:@"StipplePattern"]] set];
    [[NSBezierPath bezierPathWithOvalInRect:[self bounds]] fill];
    break;

  case GSBuilderStatusViewStateReady:
  default:
    break;
  }
}

@end
