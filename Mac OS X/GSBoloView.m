#import "GSBoloView.h"
#import "GSXBoloController.h"

#include "tiles.h"
#include "images.h"
#include "vector.h"
#include "rect.h"
#include "list.h"
#include "bolo.h"
#include "server.h"
#include "client.h"
#include "errchk.h"

#include <Carbon/Carbon.h>
#include <pthread.h>
#include <math.h>

static NSMutableArray *boloViews = nil;
static NSImage *tiles = nil;
static NSImage *sprites = nil;
static NSCursor *cursor = nil;
static int dirtytiles(struct ListNode *list, Recti rect);

@interface GSBoloView (Private)
- (void)drawTileAtPoint:(Pointi)point;
- (void)drawTilesInRect:(NSRect)rect;
- (void)eraseSprites;
- (void)refreshTiles;
- (void)drawSprites;
- (void)drawSprite:(int)tile at:(Vec2f)point fraction:(float)fraction;
- (void)drawLabel:(char *)label at:(Vec2f)point withAttributes:(NSDictionary *)attr;
- (void)dirtyTiles:(NSRect)rect;
@end

@implementation GSBoloView

+ (void)initialize {
  if (self == [GSBoloView class]) {
    boloViews = [[NSMutableArray alloc] init];

    assert((tiles = [[NSImage imageNamed:@"Tiles"] retain]) != nil);
    assert((sprites = [[NSImage imageNamed:@"Sprites"] retain]) != nil);
    assert((cursor = [[NSCursor alloc] initWithImage:[NSImage imageNamed:@"Cursor"] hotSpot:NSMakePoint(8.0, 8.0)]) != nil);
  }
}

+ (void)refresh {
  NSEnumerator *enumerator;
  GSBoloView *view;

  /* disable flush window on a bolo view windows */
  enumerator = [boloViews objectEnumerator];
  while ((view = [enumerator nextObject]) != nil) {
    [[view window] disableFlushWindow];
  }

  /* draw */
  enumerator = [boloViews objectEnumerator];
  while ((view = [enumerator nextObject]) != nil) {  
    if ([view lockFocusIfCanDraw]) {
      [view eraseSprites];
      [view refreshTiles];
      [view drawSprites];
      [view unlockFocus];
      [[view window] flushWindow];
    }
  }

  /* enable flush window and flush if needed */
  enumerator = [boloViews objectEnumerator];
  while ((view = [enumerator nextObject]) != nil) {
    [[view window] enableFlushWindow];
    [[view window] flushWindowIfNeeded];
  }

  clearchangedtiles();
}

+ (void)removeView:(GSBoloView *)view {
  [boloViews removeObject:view];
}

- (id)initWithFrame:(NSRect)frameRect {
TRY
	if ((self = [super initWithFrame:frameRect]) != nil) {
    if (initlist(&rectlist) == -1) LOGFAIL(errno)
    [boloViews addObject:self];
	}

CLEANUP
  switch (ERROR) {
  case 0:
    RETURN(self)

  default:
    PCRIT(ERROR)
    printlineinfo();
    CLEARERRLOG
    exit(EXIT_FAILURE);
  }
END
}

- (void)drawRect:(NSRect)rect {
  int gotlock = 0;

TRY
  if (lockclient()) LOGFAIL(errno)
  gotlock = 1;

  [self drawTilesInRect:rect];
  [self drawSprites];

  if (unlockclient()) LOGFAIL(errno)
  gotlock = 0;

CLEANUP
  switch (ERROR) {
  case 0:
    return;

  default:
    if (gotlock) {
      unlockclient();
    }

    PCRIT(ERROR)
    printlineinfo();
    CLEARERRLOG
    exit(EXIT_FAILURE);
  }
END
}

- (void)drawTileAtPoint:(Pointi)point {
  int image;
  NSRect dstRect, srcRect;

  image = client.images[point.y][point.x];
  dstRect = NSMakeRect(16.0*point.x, 16.0*(255 - point.y), 16.0, 16.0);
  srcRect = NSMakeRect((image%16)*16, (image/16)*16, 16.0, 16.0);

  /* draw tile */
  if (image == -1) {
    /* draw black */
    [[NSColor blackColor] set];
    [NSBezierPath fillRect:dstRect];
  }
  else {
    /* draw image */
    [tiles drawInRect:dstRect fromRect:srcRect operation:NSCompositeCopy fraction:1.0];

    /* draw mine */
    if (isMinedTile(client.seentiles, point.x, point.y)) {
      NSRect mineImageRect;

      mineImageRect = NSMakeRect((MINE00IMAGE%16)*16, (MINE00IMAGE/16)*16, 16.0, 16.0);
      [tiles drawInRect:dstRect fromRect:mineImageRect operation:NSCompositeSourceOver fraction:1.0];
    }

    /* draw fog */
    if (client.fog[point.y][point.x] <= 0) {
      [[[NSColor blackColor] colorWithAlphaComponent:0.5] set];
      [NSBezierPath fillRect:dstRect];
    }
  }
}

- (void)drawTilesInRect:(NSRect)rect {
  int min_i, max_i, min_j, max_j;
  int min_x, max_x, min_y, max_y;
  int y, x;
  NSRect dstRect, srcRect;

  min_i = ((int)floorf(NSMinX(rect)))/16;
  max_i = ((int)ceilf(NSMaxX(rect)))/16;

  min_j = ((int)floorf(NSMinY(rect)))/16;
  max_j = ((int)ceilf(NSMaxY(rect)))/16;

  min_x = min_i;
  max_x = max_i;

  min_y = 255 - max_j;
  max_y = 255 - min_j;

  /* draw the tiles in the rect */
  for (y = min_y; y <= max_y; y++) {
    for (x = min_x; x <= max_x; x++) {
      int image;

      image = client.images[y][x];
      dstRect = NSMakeRect(16.0*x, 16.0*(255 - y), 16.0, 16.0);
      srcRect = NSMakeRect((image%16)*16, (image/16)*16, 16.0, 16.0);

      /* draw tile */
      if (image == -1) {
        /* draw black */
        [[NSColor blackColor] set];
        [NSBezierPath fillRect:dstRect];
      }
      else {
        /* draw image */
        [tiles drawInRect:dstRect fromRect:srcRect operation:NSCompositeCopy fraction:1.0];

        /* draw mine */
        if (isMinedTile(client.seentiles, x, y)) {
          NSRect mineImageRect;

          mineImageRect = NSMakeRect((MINE00IMAGE%16)*16, (MINE00IMAGE/16)*16, 16.0, 16.0);
          [tiles drawInRect:dstRect fromRect:mineImageRect operation:NSCompositeSourceOver fraction:1.0];
        }

        /* draw fog */
        if (client.fog[y][x] <= 0) {
          [[[NSColor blackColor] colorWithAlphaComponent:0.5] set];
          [NSBezierPath fillRect:dstRect];
        }
      }
    }
  }
}

- (void)eraseSprites {
  struct ListNode *node;
  int min_x, max_x, min_y, max_y;
  int y, x;
  Recti *rect;

//  [NSGraphicsContext saveGraphicsState];
//  [[NSGraphicsContext currentContext] setShouldAntialias:NO];

  for (node = nextlist(&rectlist); node != NULL; node = nextlist(node)) {
    rect = (Recti *)ptrlist(node);

    min_x = minxrect(*rect);
    max_x = maxxrect(*rect);

    min_y = minyrect(*rect);
    max_y = maxyrect(*rect);

    for (y = min_y; y <= max_y; y++) {
      for (x = min_x; x <= max_x; x++) {
        [self drawTileAtPoint:makepoint(x, y)];
      }
    }
  }

//  [NSGraphicsContext restoreGraphicsState];

  clearlist(&rectlist, free);
}

- (void)refreshTiles {
  struct ListNode *node;

//  [NSGraphicsContext saveGraphicsState];
//  [[NSGraphicsContext currentContext] setShouldAntialias:NO];

  for (node = nextlist(&client.changedtiles); node != NULL; node = nextlist(node)) {
    Pointi *p;
    int image;
    NSRect dstRect;
    NSRect srcRect;

    p = (Pointi *)ptrlist(node);
    image = client.images[p->y][p->x];
    dstRect = NSMakeRect(16.0*p->x, 16.0*(255 - p->y), 16.0, 16.0);
    srcRect = NSMakeRect((image%16)*16, (image/16)*16, 16.0, 16.0);

    /* draw tile */
    if (client.images[p->y][p->x] == -1) {
      /* draw black */
      [[NSColor blackColor] set];
      [NSBezierPath fillRect:dstRect];
    }
    else {
      /* draw image */
      [tiles drawInRect:dstRect fromRect:srcRect operation:NSCompositeCopy fraction:1.0];

      /* draw mine */
      if (isMinedTile(client.seentiles, p->x, p->y)) {
        NSRect mineImageRect;

        mineImageRect = NSMakeRect((MINE00IMAGE%16)*16, (MINE00IMAGE/16)*16, 16.0, 16.0);
        [tiles drawInRect:dstRect fromRect:mineImageRect operation:NSCompositeSourceOver fraction:1.0];
      }

      /* draw fog */
      if (client.fog[p->y][p->x] == 0) {
        [[[NSColor blackColor] colorWithAlphaComponent:0.5] set];
        [NSBezierPath fillRect:dstRect];
      }
    }
  }

//  [NSGraphicsContext restoreGraphicsState];
}

- (void)drawSprites {
  int i;
  struct ListNode *node;
  char *string = NULL;

TRY

  /* draw builders */
  for (i = 0; i < MAXPLAYERS; i++) {
    if (client.players[i].connected) {
      switch (client.players[i].builderstatus) {
      case kBuilderGoto:
      case kBuilderWork:
      case kBuilderWait:
      case kBuilderReturn:
        [self drawSprite:(client.players[client.player].seq/5)%2 ? BUILD0IMAGE : BUILD1IMAGE at:client.players[i].builder fraction:calcvis(client.players[i].builder)];
        break;

      default:
        break;
      }
    }
  }

  /* draw other players */
  for (i = 0; i < MAXPLAYERS; i++) {
    if (client.players[i].connected && i != client.player && !client.players[i].dead) {
      float vis;

      vis = calcvis(client.players[i].tank);

      if
      (
        (client.players[client.player].alliance & (1 << i)) &&
        (client.players[i].alliance & (1 << client.player))
      ) {
        [self drawSprite:(client.players[i].boat ? FTKB00IMAGE : FTNK00IMAGE) + (((int)(client.players[i].dir/(kPif/8.0) + 0.5))%16) at:client.players[i].tank fraction:vis];
      }
      else {
        [self drawSprite:(client.players[i].boat ? ETKB00IMAGE : ETNK00IMAGE) + (((int)(client.players[i].dir/(kPif/8.0) + 0.5))%16) at:client.players[i].tank fraction:vis];
      }

      if (vis > 0.90) {
        [self drawLabel:client.players[i].name at:client.players[i].tank withAttributes:[NSDictionary dictionaryWithObjectsAndKeys:[NSColor whiteColor], NSForegroundColorAttributeName, nil]];
      }
    }
  }

  /* draw player */
  if (!client.players[client.player].dead) {
    /* draw tank */
    [self drawSprite:(client.players[client.player].boat ? PTKB00IMAGE : PTNK00IMAGE) + (((int)(client.players[client.player].dir/(kPif/8.0) + 0.5))%16) at:client.players[client.player].tank fraction:1.0];
  }

  /* draw shells */
  for (i = 0; i < MAXPLAYERS; i++) {
    if (client.players[i].connected) {
      struct ListNode *node;

      for (node = nextlist(&client.players[i].shells); node != NULL; node = nextlist(node)) {
        struct Shell *shell;

        shell = ptrlist(node);
        [self drawSprite:SHELL0IMAGE + (((int)(shell->dir/(kPif/8.0) + 0.5))%16) at:shell->point fraction:fogvis(shell->point)];
      }
    }
  }

  /* draw explosions */
  node = nextlist(&client.explosions);

  while (node != NULL) {
    struct Explosion *explosion;
    float f;

    explosion = ptrlist(node);
    f = ((float)explosion->counter)/EXPLOSIONTICKS;
    [self drawSprite:EXPLO0IMAGE + ((EXPLO5IMAGE - EXPLO0IMAGE)*f) at:explosion->point fraction:fogvis(explosion->point)];

    node = nextlist(node);
  }

  for (i = 0; i < MAXPLAYERS; i++) {
    if (client.players[i].connected) {
      node = nextlist(&client.players[i].explosions);

      while (node != NULL) {
        struct Explosion *explosion;
        float f;

        explosion = ptrlist(node);
        f = ((float)explosion->counter)/EXPLOSIONTICKS;
        [self drawSprite:EXPLO0IMAGE + ((EXPLO5IMAGE - EXPLO0IMAGE)*f) at:explosion->point fraction:fogvis(explosion->point)];

        node = nextlist(node);
      }
    }
  }

  /* draw parachuting builders */
  for (i = 0; i < MAXPLAYERS; i++) {
    if (client.players[i].connected) {
      if (client.players[i].builderstatus == kBuilderParachute) {
        [self drawSprite:BUILD2IMAGE at:client.players[i].builder fraction:fogvis(client.players[i].builder)];
      }
    }
  }

  /* draw selector */
  {
    NSPoint aPoint;
    aPoint = [self convertPoint:[[self window] convertScreenToBase:[NSEvent mouseLocation]] fromView:nil];
    if ([self mouse:aPoint inRect:[self visibleRect]]) {
      [self drawSprite:SELETRIMAGE at:make2f(floorf(aPoint.x/16.0) + 0.5, floorf(FWIDTH - ((aPoint.y + 0.5)/16.0)) + 0.5) fraction:1.0];
    }
  }

  /* draw crosshair */
  if (!client.players[client.player].dead) {
    [self drawSprite:CROSSHIMAGE at:add2f(client.players[client.player].tank, mul2f(dir2vec(client.players[client.player].dir), client.range)) fraction:1.0];
  }

  if (client.pause) {
    NSRect rect;
    rect = [self visibleRect];

    if (client.pause == -1) {
      [self drawLabel:"Paused" at:make2f((rect.origin.x + rect.size.width*0.5)/16.0, (256.0*16.0 - (rect.origin.y + rect.size.height*0.5))/16.0) withAttributes:[NSDictionary dictionaryWithObjectsAndKeys:[NSFont fontWithName:@"Helvetica" size:90], NSFontAttributeName, [NSColor whiteColor], NSForegroundColorAttributeName, nil]];
    }
    else {
      if (asprintf(&string, "Resume in %d", client.pause) == -1) LOGFAIL(errno)
      [self drawLabel:string at:make2f((rect.origin.x + rect.size.width*0.5)/16.0, (256.0*16.0 - (rect.origin.y + rect.size.height*0.5))/16.0) withAttributes:[NSDictionary dictionaryWithObjectsAndKeys:[NSFont fontWithName:@"Helvetica" size:90], NSFontAttributeName, [NSColor whiteColor], NSForegroundColorAttributeName, nil]];
      free(string);
      string = NULL;
    }
  }

CLEANUP
  switch (ERROR) {
  case 0:
    return;

  default:
    if (string) {
      free(string);
    }

    PCRIT(ERROR);
    printlineinfo();
    CLEARERRLOG
    exit(EXIT_FAILURE);
  }
END
}

- (void)drawSprite:(int)tile at:(Vec2f)point fraction:(float)fraction {
  NSRect srcRect;
  NSRect dstRect;

  if (fraction > 0.00001) {
    srcRect = NSMakeRect((tile%16)*16, (tile/16)*16, 16.0, 16.0);
    dstRect = NSMakeRect(floorf(point.x*16.0 - 8.0), floorf((FWIDTH - point.y)*16.0 - 8.0), 16.0, 16.0);
    [sprites drawInRect:dstRect fromRect:srcRect operation:NSCompositeSourceOver fraction:fraction];
    [self dirtyTiles:dstRect];
  }
}

- (void)drawLabel:(char *)label at:(Vec2f)point withAttributes:(NSDictionary *)attr {
  NSString *string;
  NSRect rect;

  string = [NSString stringWithUTF8String:label];
  rect.size = [string sizeWithAttributes:attr];
  rect.origin.x = point.x*16.0 - rect.size.width*0.5;
  rect.origin.y = FWIDTH*16.0 - point.y*16.0 + 8.0;
  [string drawInRect:rect withAttributes:attr];
  [self dirtyTiles:rect];
}

- (BOOL)becomeFirstResponder {
  BOOL okToChange;
  if ((okToChange = [super becomeFirstResponder])) {
    UInt32 carbonModifiers;
    carbonModifiers = GetCurrentKeyModifiers();
    modifiers =
      (carbonModifiers & alphaLock ? NSAlphaShiftKeyMask : 0) |
      (carbonModifiers & shiftKey || carbonModifiers & rightShiftKey ? NSShiftKeyMask : 0) |
      (carbonModifiers & controlKey || carbonModifiers & rightControlKey ? NSControlKeyMask : 0) |
      (carbonModifiers & optionKey || carbonModifiers & rightOptionKey ? NSAlternateKeyMask : 0) |
      (carbonModifiers & cmdKey ? NSCommandKeyMask : 0);
//    (carbonModifiers &  ? NSNumericPadKeyMask : 0) |
//    (carbonModifiers &  ? NSHelpKeyMask : 0) |
//    (carbonModifiers &  ? NSFunctionKeyMask : 0);
  }
  return okToChange;
}

- (BOOL)acceptsFirstResponder {
  return YES;
}

- (void)keyDown:(NSEvent *)theEvent {
  if (![theEvent isARepeat]) {
    [boloController keyEvent:YES forKey:[theEvent keyCode]];
  }
}

- (void)keyUp:(NSEvent *)theEvent {
  if (![theEvent isARepeat]) {
    [boloController keyEvent:NO forKey:[theEvent keyCode]];
  }
}

- (void)flagsChanged:(NSEvent *)theEvent {
  unsigned int oldModifiers;
  oldModifiers = modifiers;
  modifiers = [theEvent modifierFlags] & (NSAlphaShiftKeyMask | NSShiftKeyMask | NSControlKeyMask | NSAlternateKeyMask | NSCommandKeyMask | NSNumericPadKeyMask | NSHelpKeyMask | NSFunctionKeyMask);
  if (modifiers & (oldModifiers ^ modifiers)) {
    [boloController keyEvent:YES forKey:[theEvent keyCode]];
  }
  else {
    [boloController keyEvent:NO forKey:[theEvent keyCode]];
  }
}

- (void)mouseUp:(NSEvent *)theEvent {
  if ([theEvent type] == NSLeftMouseUp) {
    NSPoint point;

    point = [self convertPoint:[theEvent locationInWindow] fromView:nil];
    [boloController mouseEvent:makepoint(point.x/16.0, 255 - (int)(point.y/16.0))];
  }
}

- (BOOL)isOpaque {
  return YES;
}

- (void)dirtyTiles:(NSRect)rect {
  Recti recti;

TRY
  recti.origin.x = NSMinX(rect)/16.0;
  recti.origin.y = NSMinY(rect)/16.0;
  recti.size.width = ((int)((NSMaxX(rect) + 16.0)/16.0)) - recti.origin.x;
  recti.size.height = ((int)((NSMaxY(rect) + 16.0)/16.0)) - recti.origin.y;
  recti.origin.y = WIDTH - (recti.origin.y + recti.size.height);

  if (dirtytiles(&rectlist, recti)) LOGFAIL(errno)

CLEANUP
  switch (ERROR) {
  case 0:
    return;

  default:
    PCRIT(ERROR);
    printlineinfo();
    CLEARERRLOG
    exit(EXIT_FAILURE);
  }
END
}

- (void)resetCursorRects {
	[self addCursorRect:[self visibleRect] cursor:cursor];
  [cursor setOnMouseEntered:YES];
}

@end

int dirtytiles(struct ListNode *list, Recti rect) {
  Recti *rectptr = NULL;
  struct ListNode *node = NULL;

TRY
  if (isemptyrect(rect)) {
    SUCCESS
  }

  for (node = nextlist(list); node != NULL; node = nextlist(node)) {
    Recti *oldrect;

    oldrect = ptrlist(node);

    if (intersectsrect(*oldrect, rect)) {
      if (containsrect(rect, *oldrect)) {
        *oldrect = rect;
      }
      else if (!containsrect(*oldrect, rect)) {
        Recti rectsub[4];
        int i;

        subtractrect(rect, *oldrect, rectsub);

        for (i = 0; i < 4; i++) {
          if (dirtytiles(node, rectsub[i])) LOGFAIL(errno)
        }
      }

      break;
    }
  }

  if (node == NULL) {
    if ((rectptr = (Recti *)malloc(sizeof(Recti))) == NULL) LOGFAIL(errno)
    *rectptr = rect;
    if (addlist(list, rectptr) == -1) LOGFAIL(errno)
    rectptr = NULL;
  }

CLEANUP
  switch (ERROR) {
  case 0:
    RETURN(0)

  default:
    if (rectptr) {
      free(rectptr);
    }

    RETERR(-1)
  }
END
}
