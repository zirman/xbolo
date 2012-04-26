#ifndef __TRACKER__
#define __TRACKER__

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define TRACKERPORT       (40000)
#define TRACKERIDENT      ("XBOLOTRK")
#define TRACKERVERSION    (0)
#define TRKPLYRNAMELEN    (16)
#define TRKMAPNAMELEN     (32)

/* tracker server msgs */
enum {
  kTrackerVersionOK,
  kTrackerVersionErr,
} ;

enum {
  kTrackerTCPPortOK,
  kTrackerTCPPortClosed,
} ;

enum {
  kTrackerUDPPortOK,
  kTrackerUDPPortClosed,
} ;

/* tracker client msgs */
enum {
  kTrackerHost,
  kTrackerList,
} ;

struct TRACKER_Preamble {
  uint8_t ident[8];  /* "XBOLOTRK" */
  uint8_t version;   /* currently 0 */
} ;

struct TrackerHost {
  uint8_t playername[TRKPLYRNAMELEN];
  uint8_t mapname[TRKMAPNAMELEN];
  uint16_t port;
  uint8_t gametype;
  uint32_t timelimit;
  uint8_t passreq;
  uint8_t nplayers;
  uint8_t allowjoin;
  uint8_t pause;
} ;

struct TrackerHostList {
  struct in_addr addr;
  struct TrackerHost game;
} ;

#endif  /* __TRACKER__ */
