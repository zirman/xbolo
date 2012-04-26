//
//  GSStatusBar.m
//  XBolo
//
//  Created by Robert Chrzanowski on 11/12/04.
//  Copyright 2004 Robert Chrzanowski. All rights reserved.
//

#import "GSStatusBar.h"


@implementation GSStatusBar

- (id)initWithFrame:(NSRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
      type = GSHorizontalBar;
      value = 0.0f;
    }
    return self;
}

- (void)drawRect:(NSRect)rect {
  switch (type) {
  case GSHorizontalBar:
    [[NSColor greenColor] set];
    [NSBezierPath fillRect:NSMakeRect(0.0f, 0.0f, NSWidth([self frame])*value, NSHeight([self frame]))];
    break;

  case GSVerticalBar:
    [[NSColor greenColor] set];
    [NSBezierPath fillRect:NSMakeRect(0.0f, 0.0f, NSWidth([self frame]), NSHeight([self frame])*value)];
    break;

  default:
    break;
  }
}

- (GSStatusBarType)type {
  return type;
}

- (void)setType:(GSStatusBarType)aType {
  switch (aType) {
  case GSHorizontalBar:
  case GSVerticalBar:
    if (type != aType) {
      type = aType;
      [self setNeedsDisplay:YES];
    }

    break;

  default:
    assert(0);
    break;
  }
}

- (float)value {
  return value;
}

- (void)setValue:(float)aValue {
  if (value != aValue) {
    value = aValue;
    [self setNeedsDisplay:YES];
  }
}

@end
