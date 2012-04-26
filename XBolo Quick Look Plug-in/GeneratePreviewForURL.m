#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>
#include <QuickLook/QuickLook.h>
#include <unistd.h>
#include <ApplicationServices/ApplicationServices.h>
#import <Cocoa/Cocoa.h>

#define MAPFILEIDENT        ("BMAPBOLO")
#define MAPFILEIDENTLEN     (8)
#define CURRENTMAPVERSION   (1)
#define MAXPILLS            (16)
#define MAXBASES            (16)
#define MAXSTARTS           (16)

/* terrain types */
enum {
  kWallTile         = 0,
  kRiverTile        = 1,
  kSwampTile        = 2,
  kCraterTile       = 3,
  kRoadTile         = 4,
  kForestTile       = 5,
  kRubbleTile       = 6,
  kGrassTile        = 7,
  kDamagedWallTile  = 8,
  kBoatTile         = 9,

  kMinedSwampTile   = 10,
  kMinedCraterTile  = 11,
  kMinedRoadTile    = 12,
  kMinedForestTile  = 13,
  kMinedRubbleTile  = 14,
  kMinedGrassTile   = 15,
} ;

/* file data structures */
struct BMAP_Preamble {
  uint8_t ident[8];  /* "BMAPBOLO" */
  uint8_t version;   /* currently 0 */
  uint8_t npills;    /* maximum 16 (at the moment) */
  uint8_t nbases;    /* maximum 16 (at the moment) */
  uint8_t nstarts;   /* maximum 16 (at the moment) */
} __attribute__((__packed__));

struct BMAP_PillInfo {
	uint8_t x;
	uint8_t y;
	uint8_t owner;   /* should be 0xFF except in speciality maps */
	uint8_t armour;  /* range 0-15 (dead pillbox = 0, full strength = 15) */
	uint8_t speed;   /* typically 50. Time between shots, in 20ms units */
                   /* Lower values makes the pillbox start off 'angry' */
} __attribute__((__packed__));

struct BMAP_BaseInfo {
	uint8_t x;
	uint8_t y;
	uint8_t owner;   /* should be 0xFF except in speciality maps */
	uint8_t armour;  /* initial stocks of base. Maximum value 90 */
	uint8_t shells;  /* initial stocks of base. Maximum value 90 */
	uint8_t mines;   /* initial stocks of base. Maximum value 90 */
} __attribute__((__packed__));

struct BMAP_StartInfo {
  uint8_t x;
  uint8_t y;
	uint8_t dir;  /* Direction towards land from this start. Range 0-15 */
} __attribute__((__packed__));

struct BMAP_Run {
	uint8_t datalen;  /* length of the data for this run */
                    /* INCLUDING this 4 byte header */
	uint8_t y;        /* y co-ordinate of this run. */
	uint8_t startx;   /* first square of the run */
	uint8_t endx;     /* last square of run + 1 */
                    /* (ie first deep sea square after run) */
/*	uint8_t data[0xFF];*/  /* actual length of data is always much less than 0xFF */
} __attribute__((__packed__));

static int drawrun(CGContextRef context, struct BMAP_Run run, const void *buf);
static int setterraincolor(CGContextRef context, int tile);
static int readnibble(const void *buf, size_t i);
static CGColorSpaceRef myGetGenericRGBSpace(void);
static CGColorRef myGetRedColor(void);
static CGColorRef myGetGreenColor(void);
static CGColorRef myGetDarkGreenColor(void);
static CGColorRef myGetBlueColor(void);
static CGColorRef myGetDarkBlueColor(void);
static CGColorRef myGetYellowColor(void);
static CGColorRef myGetCyanColor(void);
static CGColorRef myGetDarkCyanColor(void);
static CGColorRef myGetBrownColor(void);
static CGColorRef myGetLightBrownColor(void);
static CGColorRef myGetVeryLightBrownColor(void);
static CGColorRef myGetDarkBrownColor(void);
static CGColorRef myGetGreyColor(void);
static CGColorRef myGetBlackColor(void);

OSStatus GeneratePreviewForURL(void *thisInterface, QLPreviewRequestRef preview, CFURLRef url, CFStringRef contentTypeUTI, CFDictionaryRef options) {
  NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
  NSData *data;
  const void *buf;
  size_t nbytes;

  data = [NSData dataWithContentsOfURL:(NSURL *)url];
  buf = [data bytes];
  nbytes = [data length];

  CGContextRef context = QLPreviewRequestCreateContext(preview, CGSizeMake(256, 256), false, NULL);

  if(context) {
    int i;
    const struct BMAP_Preamble *preamble;
    const struct BMAP_PillInfo *pillInfos;
    const struct BMAP_BaseInfo *baseInfos;
    const struct BMAP_StartInfo *startInfos;
    const void *runData;
    int runDataLen;
    int offset;
    int minx = 256, maxx = 0, miny = 256, maxy = 0;

    /* turn of antialiasing */
    CGContextSetShouldAntialias(context, 0);

    /* draw deep sea */
    CGContextSetFillColorWithColor(context, myGetBlueColor());
    CGContextFillRect(context, CGRectMake(0, 0, 256, 256));

    /* invert y axis */
    CGContextTranslateCTM(context, 0, 256);
    CGContextScaleCTM(context, 1, -1);

    if (nbytes < sizeof(struct BMAP_Preamble)) {
      QLPreviewRequestFlushContext(preview, context);
      CFRelease(context);
      return noErr;
    }

    preamble = buf;

    if (strncmp((char *)preamble->ident, MAPFILEIDENT, MAPFILEIDENTLEN) != 0) {
      QLPreviewRequestFlushContext(preview, context);
      CFRelease(context);
      return noErr;
    }

    if (preamble->version != CURRENTMAPVERSION) {
      QLPreviewRequestFlushContext(preview, context);
      CFRelease(context);
      return noErr;
    }

    if (preamble->npills > MAXPILLS) {
      QLPreviewRequestFlushContext(preview, context);
      CFRelease(context);
      return noErr;
    }

    if (preamble->nbases > MAXBASES) {
      QLPreviewRequestFlushContext(preview, context);
      CFRelease(context);
      return noErr;
    }

    if (preamble->nstarts > MAXSTARTS) {
      QLPreviewRequestFlushContext(preview, context);
      CFRelease(context);
      return noErr;
    }

    if (nbytes <
        sizeof(struct BMAP_Preamble) +
        preamble->npills*sizeof(struct BMAP_PillInfo) +
        preamble->nbases*sizeof(struct BMAP_BaseInfo) +
        preamble->nstarts*sizeof(struct BMAP_StartInfo)) {
      QLPreviewRequestFlushContext(preview, context);
      CFRelease(context);
      return noErr;
    }

    pillInfos = (struct BMAP_PillInfo *)(preamble + 1);
    baseInfos = (struct BMAP_BaseInfo *)(pillInfos + preamble->npills);
    startInfos = (struct BMAP_StartInfo *)(baseInfos + preamble->nbases);
    runData = (void *)(startInfos + preamble->nstarts);
    runDataLen =
      nbytes - (sizeof(struct BMAP_Preamble) +
                preamble->npills*sizeof(struct BMAP_PillInfo) +
                preamble->nbases*sizeof(struct BMAP_BaseInfo) +
                preamble->nstarts*sizeof(struct BMAP_StartInfo));

    offset = 0;

    for (;;) {
      struct BMAP_Run run;

      if (offset + sizeof(struct BMAP_Run) > runDataLen) {
        break;
      }

      run = *(struct BMAP_Run *)(runData + offset);

      if (run.datalen == 4 && run.y == 0xff && run.startx == 0xff && run.endx == 0xff) {
        break;
      }

      if (offset + run.datalen > runDataLen) {
        break;
      }

      if (run.y < miny) {
        miny = run.y;
      }
      if (run.y > maxy) {
        maxy = run.y;
      }

      if (run.startx < minx) {
        minx = run.startx;
      }
      if (run.endx > maxx) {
        maxx = run.endx;
      }

      offset += run.datalen;
    }

    minx -= 3;

    if (minx < 0) {
      minx = 0;
    }

    maxx += 3;

    if (maxx > 256) {
      maxx = 256;
    }

    miny -= 3;

    if (miny < 0) {
      miny = 0;
    }

    maxy += 3;

    if (maxy > 256) {
      maxy = 256;
    }

    if (maxx - minx > maxy - miny) {
      CGContextScaleCTM(context, 256.0/(maxx - minx), 256.0/(maxx - minx));
      CGContextTranslateCTM(context, -minx, -miny + ((maxx - minx) - (maxy - miny))/2);
    }
    else {
      CGContextScaleCTM(context, 256.0/(maxy - miny), 256.0/(maxy - miny));
      CGContextTranslateCTM(context, -minx + ((maxy - miny) - (maxx - minx))/2, -miny);
    }

    /* begin drawing terrain */

    offset = 0;

    for (;;) {
      struct BMAP_Run run;

      if (offset + sizeof(struct BMAP_Run) > runDataLen) {
        break;
      }

      run = *(struct BMAP_Run *)(runData + offset);

      if (run.datalen == 4 && run.y == 0xff && run.startx == 0xff && run.endx == 0xff) {
        break;
      }

      if (offset + run.datalen > runDataLen) {
        break;
      }

      if (drawrun(context, run, runData + offset + sizeof(struct BMAP_Run)) == -1) {
        QLPreviewRequestFlushContext(preview, context);
        CFRelease(context);
        return noErr;
      }

      offset += run.datalen;
    }

    CGContextSetFillColorWithColor(context, myGetYellowColor());

    for (i = 0; i < preamble->nbases; i++) {
      CGContextFillRect(context, CGRectMake(baseInfos[i].x, baseInfos[i].y, 1, 1));
    }

    CGContextSetFillColorWithColor(context, myGetRedColor());

    for (i = 0; i < preamble->npills; i++) {
      CGContextFillRect(context, CGRectMake(pillInfos[i].x, pillInfos[i].y, 1, 1));
    }

    CGContextSetFillColorWithColor(context, myGetGreyColor());

    for (i = 0; i < preamble->nstarts; i++) {
      CGContextFillRect(context, CGRectMake(startInfos[i].x, startInfos[i].y, 1, 1));
    }

    QLPreviewRequestFlushContext(preview, context);
    CFRelease(context);
  }

  [pool release];

  return noErr;
}

void CancelPreviewGeneration(void* thisInterface, QLPreviewRequestRef preview) {
  // implement only if supported
}

static int drawrun(CGContextRef context, struct BMAP_Run run, const void *buf) {
  int i;
  int x;
  int offset;

  x = run.startx;
  offset = 0;

  while (x < run.endx) {
    int len;

    if (sizeof(struct BMAP_Run) + (offset + 2)/2 > run.datalen) {
      return -1;
    }

    len = readnibble(buf, offset++);

    if (len >= 0 && len <= 7) {  /* this is a sequence of different tiles */
      len += 1;

      if (sizeof(struct BMAP_Run) + (offset + len + 1)/2 > run.datalen) {
        return -1;
      }

      for (i = 0; i < len; i++) {
        /* draw terrain */
        setterraincolor(context, readnibble(buf, offset++));
        CGContextFillRect(context, CGRectMake(x++, run.y, 1, 1));
      }
    }
    else if (len >= 8 && len <= 15) {  /* this is a sequence of like tiles */
      len -= 6;

      if (sizeof(struct BMAP_Run) + (offset + 2)/2 > run.datalen) {
        return -1;
      }

      /* draw terrain */
      setterraincolor(context, readnibble(buf, offset++));
      CGContextFillRect(context, CGRectMake(x, run.y, len, 1));
      x += len;
    }
    else {
      return -1;
    }
  }

  if (sizeof(struct BMAP_Run) + (offset + 1)/2 != run.datalen) {
    return -1;
  }

  return 0;
}

static int setterraincolor(CGContextRef context, int tile) {
  switch (tile) {
  case kWallTile:  /* wall */
    CGContextSetFillColorWithColor(context, myGetBrownColor());
    return 0;

  case kRiverTile:  /* river */
    CGContextSetFillColorWithColor(context, myGetCyanColor());
    return 0;

  case kSwampTile:  /* swamp */
  case kMinedSwampTile:  /* mined swamp */
    CGContextSetFillColorWithColor(context, myGetDarkCyanColor());
    return 0;

  case kCraterTile:  /* crater */
  case kMinedCraterTile:  /* mined crater */
    CGContextSetFillColorWithColor(context, myGetDarkBrownColor());
    return 0;

  case kRoadTile:  /* road */
  case kMinedRoadTile:  /* mined road */
    CGContextSetFillColorWithColor(context, myGetBlackColor());
    return 0;

  case kForestTile:  /* forest */
  case kMinedForestTile:  /* mined forest */
    CGContextSetFillColorWithColor(context, myGetDarkGreenColor());
    return 0;

  case kRubbleTile:  /* rubble */
  case kMinedRubbleTile:  /* mined rubble */
    CGContextSetFillColorWithColor(context, myGetVeryLightBrownColor());
    return 0;

  case kGrassTile:  /* grass */
  case kMinedGrassTile:  /* mined grass */
    CGContextSetFillColorWithColor(context, myGetGreenColor());
    return 0;

  case kDamagedWallTile:  /* damaged wall */
    CGContextSetFillColorWithColor(context, myGetLightBrownColor());
    return 0;

  case kBoatTile:  /* river w/boat */
    CGContextSetFillColorWithColor(context, myGetDarkBlueColor());
    return 0;

  default:
    return -1;
  }
}

static int readnibble(const void *buf, size_t i) {
  return i%2 ? *(uint8_t *)(buf + i/2) & 0x0f : (*(uint8_t *)(buf + i/2) & 0xf0) >> 4;
}

static CGColorSpaceRef myGetGenericRGBSpace(void) {
  // Only create the color space once.
  static CGColorSpaceRef colorSpace = NULL;

  if (colorSpace == NULL) {
    colorSpace = CGColorSpaceCreateWithName(kCGColorSpaceGenericRGB);
  }

  return colorSpace;
}

static CGColorRef myGetRedColor(void) {
  // Only create the CGColor object once.
  static CGColorRef red = NULL;

  if (red == NULL) {
    // R,G,B,A
    CGFloat opaqueRed[4] = { 1, 0, 0, 1 };
    red = CGColorCreate(myGetGenericRGBSpace(), opaqueRed);
  }

  return red;
}

static CGColorRef myGetGreenColor(void) {
  // Only create the CGColor object once.
  static CGColorRef green = NULL;

  if (green == NULL) {
    // R,G,B,A
    CGFloat opaqueGreen[4] = { 0, 1, 0, 1 };
    green = CGColorCreate(myGetGenericRGBSpace(), (const CGFloat *)opaqueGreen);
  }

  return green;
}

static CGColorRef myGetDarkGreenColor(void) {
  // Only create the CGColor object once.
  static CGColorRef darkGreen = NULL;

  if (darkGreen == NULL) {
    // R,G,B,A
    CGFloat opaqueDarkGreen[4] = { 0, 0.502, 0, 1 };
    darkGreen = CGColorCreate(myGetGenericRGBSpace(), opaqueDarkGreen);
  }

  return darkGreen;
}

static CGColorRef myGetBlueColor(void) {
  // Only create the CGColor object once.
  static CGColorRef blue = NULL;
  if (blue == NULL) {
    // R,G,B,A
    CGFloat opaqueBlue[4] = { 0, 0, 1, 1 };
    blue = CGColorCreate(myGetGenericRGBSpace(), opaqueBlue);
  }

  return blue;
}

static CGColorRef myGetDarkBlueColor(void) {
  // Only create the CGColor object once.
  static CGColorRef darkBlue = NULL;
  if (darkBlue == NULL) {
    // R,G,B,A
    CGFloat opaqueDarkBlue[4] = { 0, 0, 0.475, 1 };
    darkBlue = CGColorCreate(myGetGenericRGBSpace(), opaqueDarkBlue);
  }

  return darkBlue;
}

static CGColorRef myGetYellowColor(void) {
  // Only create the CGColor object once.
  static CGColorRef yellow = NULL;

  if (yellow == NULL) {
    // R,G,B,A
    CGFloat opaqueYellow[4] = { 1, 1, 0, 1 };
    yellow = CGColorCreate(myGetGenericRGBSpace(), opaqueYellow);
  }

  return yellow;
}

static CGColorRef myGetCyanColor(void) {
  // Only create the CGColor object once.
  static CGColorRef cyan = NULL;

  if (cyan == NULL) {
    // R,G,B,A
    CGFloat opaqueCyan[4] = { 0, 1, 1, 1 };
    cyan = CGColorCreate(myGetGenericRGBSpace(), opaqueCyan);
  }

  return cyan;
}

static CGColorRef myGetDarkCyanColor(void) {
  // Only create the CGColor object once.
  static CGColorRef darkCyan = NULL;

  if (darkCyan == NULL) {
    // R,G,B,A
    CGFloat opaqueDarkCyan[4] = { 0, 0.502, 0.502, 1 };
    darkCyan = CGColorCreate(myGetGenericRGBSpace(), opaqueDarkCyan);
  }

  return darkCyan;
}

static CGColorRef myGetBrownColor(void) {
  // Only create the CGColor object once.
  static CGColorRef brown = NULL;

  if (brown == NULL) {
    // R,G,B,A
    CGFloat opaqueBrown[4] = { 0.702, 0.475, 0.059, 1 };
    brown = CGColorCreate(myGetGenericRGBSpace(), opaqueBrown);
  }

  return brown;
}

static CGColorRef myGetLightBrownColor(void) {
  // Only create the CGColor object once.
  static CGColorRef lightBrown = NULL;

  if (lightBrown == NULL) {
    // R,G,B,A
    CGFloat opaqueLightBrown[4] = { 0.875, 0.596, 0.075, 1 };
    lightBrown = CGColorCreate(myGetGenericRGBSpace(), opaqueLightBrown);
  }

  return lightBrown;
}

static CGColorRef myGetVeryLightBrownColor(void) {
  // Only create the CGColor object once.
  static CGColorRef veryLightBrown = NULL;

  if (veryLightBrown == NULL) {
    // R,G,B,A
    CGFloat opaqueVeryLightBrown[4] = { 0.918, 0.647, 0.482, 1 };
    veryLightBrown = CGColorCreate(myGetGenericRGBSpace(), opaqueVeryLightBrown);
  }

  return veryLightBrown;
}

static CGColorRef myGetDarkBrownColor(void) {
  // Only create the CGColor object once.
  static CGColorRef darkBrown = NULL;

  if (darkBrown == NULL) {
    // R,G,B,A
    CGFloat opaqueDarkBrown[4] = { 0.502, 0.251, 0.0, 1 };
    darkBrown = CGColorCreate(myGetGenericRGBSpace(), opaqueDarkBrown);
  }

  return darkBrown;
}

static CGColorRef myGetGreyColor(void) {
  // Only create the CGColor object once.
  static CGColorRef grey = NULL;

  if (grey == NULL) {
    // R,G,B,A
    CGFloat opaqueGrey[4] = { 0.502, 0.502, 0.502, 1 };
    grey = CGColorCreate(myGetGenericRGBSpace(), opaqueGrey);
  }

  return grey;
}

static CGColorRef myGetBlackColor(void) {
  // Only create the CGColor object once.
  static CGColorRef black = NULL;

  if (black == NULL) {
    // R,G,B,A
    CGFloat opaqueBlack[4] = { 0, 0, 0, 1 };
    black = CGColorCreate(myGetGenericRGBSpace(), opaqueBlack);
  }

  return black;
}
