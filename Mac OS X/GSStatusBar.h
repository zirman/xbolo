//
//  GSStatusBar.h
//  XBolo
//
//  Created by Robert Chrzanowski on 11/12/04.
//  Copyright 2004 Robert Chrzanowski. All rights reserved.
//

#import <Cocoa/Cocoa.h>

typedef enum {
  GSHorizontalBar,
  GSVerticalBar,
} GSStatusBarType;

@interface GSStatusBar : NSView {
  GSStatusBarType type;
  float value;
}
- (GSStatusBarType)type;
- (void)setType:(GSStatusBarType)aType;
- (float)value;
- (void)setValue:(float)aValue;
@end
