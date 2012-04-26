//
//  XBoloView.m
//  XBolo
//
//  Created by Robert Chrzanowski on 6/28/09.
//  Copyright 2009 Robert Chrzanowski. All rights reserved.
//

#import "GSBoloView.h"


@implementation GSBoloView

- (void)drawRect:(CGRect)rect {
  UIImage *image;
  [super drawRect:rect];
  [[UIColor blueColor] set];
  image = [UIImage imageNamed:@"BaseStatDead.tiff"];
  [image drawAtPoint:CGPointMake(0.0, 0.0)];
}

@end
