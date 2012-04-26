/*
 *  main.c
 *  XBolo
 *
 *  Created by Robert Chrzanowski on 10/10/09.
 *  Copyright 2009 Robert Chrzanowski. All rights reserved.
 *
 */

#include "bolo.h"
#include "server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <regex.h>
#include <netdb.h>
#include <signal.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUFBLOCKSIZE (1024)
#define NMATCH       (4)

/* defaults */
#define DEFAULT_MAPPATH                (NULL)
#define DEFAULT_MAPDIRECTORY           (NULL)
#define DEFAULT_TIMELIMIT              (0)
#define DEFAULT_HIDDENMINES            (0)
#define DEFAULT_STARTPAUSED            (0)
#define DEFAULT_GAMETYPE               (kDominationGameType)
#define DEFAULT_DOMINATION_TYPE        (kOpenGame)
#define DEFAULT_DOMINATION_BASECONTROL (30)
#define DEFAULT_TRACKER                (NULL)
#define DEFAULT_PAUSEONEXIT            (0)
#define DEFAULT_HOSTPORT               (0)
#define DEFAULT_HOSTPLAYER             ("Dedicated")
#define DEFAULT_PASSWORD               (NULL)

static void parsecommandline(int argc, const char *argv[]);
static void usage(const char *arg);
static ssize_t readfile(const char *path, void **buf);
static void registercallback(int status);
static int commandloop();

/* command line options */
const char *mappath = DEFAULT_MAPPATH;
uint16_t hostport = DEFAULT_HOSTPORT;
const char *hostplayer = DEFAULT_HOSTPLAYER;
const char *password = DEFAULT_PASSWORD;
int timelimit = DEFAULT_TIMELIMIT;
int hiddenmines = DEFAULT_HIDDENMINES;
int startpaused = DEFAULT_STARTPAUSED;
int gametype = DEFAULT_GAMETYPE;
struct Domination gamedata = { DEFAULT_DOMINATION_TYPE, DEFAULT_DOMINATION_BASECONTROL};
const char *tracker = DEFAULT_TRACKER;
int pauseonexit = DEFAULT_PAUSEONEXIT;
  
char mapname[TRKMAPNAMELEN];
void *mapdata = NULL;
ssize_t mapdatasize = 0;

int main(int argc, const char *argv[]) {
  regex_t mapname_preg;
  regmatch_t pmatch[NMATCH];
  int len;
  int err;

  parsecommandline(argc, argv);

TRY
  /* ignore SIGPIPE */
  if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
    fprintf(stderr, "Error Ignoring SIGPIPE\n");
  }

  /* initialize the random number generator */
#if __APPLE__
  srandomdev();
#else
  srandom(time(0));
#endif

  /* initialize server */
  if (initserver()) LOGFAIL(errno)

  /* get the map name */
  if ((err = regcomp(&mapname_preg, "([^/.]*)[^/]*$", REG_EXTENDED | REG_ICASE))) LOGFAIL(EREG_OFFSET + err)
  if ((err = regexec(&mapname_preg, mappath, NMATCH, pmatch, 0))) LOGFAIL(EREG_OFFSET + err)
  regfree(&mapname_preg);
  len = MIN(pmatch[1].rm_eo - pmatch[1].rm_so, TRKMAPNAMELEN - 1);
  bcopy(mappath + pmatch[1].rm_so, mapname, len);
  mapname[len] = '\0';

  /* read map into buffer */
  if ((mapdatasize = readfile(mappath, &mapdata)) == -1) LOGFAIL(errno)

  /* setup server */
  if (setupserver(startpaused, mapdata, mapdatasize, hostport, password, timelimit, hiddenmines, pauseonexit, gametype, &gamedata)) LOGFAIL(errno)

  /* join tracker and start */
  if (tracker) {
    printf("Starting Server\n");
    if (
        startserverthreadwithtracker(
          tracker, getservertcpport(), hostplayer,
          mapname, registercallback
        )
      ) LOGFAIL(errno)
  }
  else {
    printf("Starting Server\n");
    if (startserverthread()) LOGFAIL(errno)
  }

  /* parse commands */
  if (commandloop()) LOGFAIL(errno)
  if (stopserver()) LOGFAIL(errno)

CLEANUP
  switch (ERROR) {
  case 0:
    exit(EXIT_SUCCESS);

  default:
    stopserver();

    PCRIT(ERROR)
    printlineinfo();
    CLEARERRLOG
    exit(EXIT_FAILURE);
  }
END
}

void parsecommandline(int argc, const char *argv[]) {

  int ch;

  while ((ch = getopt(argc, (void *)argv, "?otseShn:p:P:l:T:")) != -1) {
    switch (ch) {
    case 'o':
      gamedata.type = kOpenGame;
      break;

    case 't':
      gamedata.type = kTournamentGame;
      break;

    case 's':
      gamedata.type = kStrictGame;
      break;

    case 'p':
      if ((hostport = strtol(optarg, NULL, 10)) == 0) {
        usage(argv[0]);
      }

      break;

    case 'P':
      password = optarg;
      break;

    case 'T':
      tracker = optarg;
      break;

    case 'n':
      hostplayer = optarg;
      break;

    case 'e':
      pauseonexit = 1;
      break;

    case 'S':
      startpaused = 1;
      break;

    case 'l':
      if ((timelimit = strtol(optarg, NULL, 10)) == 0) {
        usage(argv[0]);
      }

      break;

    case 'h':
      hiddenmines = 1;
      break;

    case '?':
    default:
      usage(argv[0]);
      break;
    }
  }

  if (optind != argc - 1) {
    usage(argv[0]);
  }

  mappath = argv[optind];
}

void usage(const char *arg) {
  regex_t name_preg;
  regmatch_t pmatch[NMATCH];
  const char *progname;

  /* get the map name */
  if (regcomp(&name_preg, "([^/.]*)$", REG_EXTENDED | REG_ICASE) == 0) {
    if (regexec(&name_preg, arg, NMATCH, pmatch, 0) == 0) {
      progname = arg + pmatch[1].rm_so;
    }
    else {
      progname = arg;
    }

    regfree(&name_preg);
  }
  else {
    progname = arg;
  }

  fprintf(
      stderr,
      "Usage:\n"
      "\t%s [flags] map\n"

      "Flags:\n"
      "  -o\n"
      "\tOpen Game (Default).  All tanks come preloaded with ammo.\n\n"

      "  -t\n"
      "\tTournament Game.  All tanks start with ammo propotional with the\n"
      "\tnumber of neutral bases.\n\n"

      "  -s\n"
      "\tStrict Tournament Game.  All tanks start without ammo.\n\n"

      "  -p port\n\n"
      "\tThe port to host the game on.  Default chooses a random port.\n\n"

      "  -P password\n"
      "\tPassword to join the game.  By default there is no password.\n\n"

      "  -T tracker\n"
      "\tRegister with a tracker.\n\n"

      "  -n HostPlayerName\n"
      "\tSpecifies the host player name to send to the tracker.\n\n"

      "  -e\n"
      "\tPauses the game on player exit.  Default off.\n\n"

      "  -S\n"
      "\tStart the game paused.  Default off.\n\n"

      "  -l seconds\n"
      "\tSpecifies the time limit.  Default off.\n\n"

      "  -h\n"
      "\tEnables hidden mines.  Default off.\n\n",

      progname
    );

  exit(EXIT_FAILURE);
}

ssize_t readfile(const char *path, void **buf) {
  int fd = -1;
  *buf = NULL;
  size_t bufsize = 0;
  size_t nbytes = 0;

TRY
  /* open map file for reading */
  if ((fd = open(path, O_RDONLY)) == -1) LOGFAIL(errno)

  /* read the file one block at a time */
  for (;;) {
    ssize_t r;

    if (bufsize - nbytes == 0) {
      void *tmp;

      if ((tmp = realloc(*buf, bufsize += BUFBLOCKSIZE)) == NULL) LOGFAIL(errno)
      *buf = tmp;
    }

    r = read(fd, *buf + nbytes, bufsize - nbytes);

    if (r == -1) {
      if (errno != EINTR) {
        LOGFAIL(errno)
      }
    }
    else if (r != 0) {
      nbytes += r;
    }
    else {  /* eof */
      break;
    }
  }

CLEANUP
  while (close(fd) == -1 && errno == EINTR);
  fd = -1;

ERRHANDLER(nbytes, -1)
END
}

void registercallback(int status) {
  switch (status) {
  case kRegisterRESOLVING:
    printf("\nResolving Tracker...");
    fflush(stdout);
    break;

  case kRegisterCONNECTING:
    printf("Done\nConnecting to Tracker...");
    fflush(stdout);
    break;

  case kRegisterSENDINGDATA:
    printf("Done\nSending Data...");
    fflush(stdout);
    break;

  case kRegisterTESTINGTCP:
    printf("Done\nTesting TCP Port...");
    fflush(stdout);
    break;

  case kRegisterTESTINGUDP:
    printf("Done\nTesting UDP Port...");
    fflush(stdout);
    break;

  case kRegisterSUCCESS:
    printf("Done\nServer Running\n");
    fflush(stdout);
    break;

  case kRegisterEHOSTNORECOVERY:
    fprintf(stderr, "Failed\nDNS Error: %s", hstrerror(NO_RECOVERY));
    break;
  
  case kRegisterEHOSTNOTFOUND:
    fprintf(stderr, "Failed\nDNS Error: %s", hstrerror(HOST_NOT_FOUND));
    break;

  case kRegisterEHOSTNODATA:
    fprintf(stderr, "\nDNS Error: %s", hstrerror(NO_DATA));
    break;

  case kRegisterETIMEOUT:
    fprintf(stderr, "Failed\nTracker Error: Timed Out");
    break;

  case kRegisterECONNREFUSED:
    fprintf(stderr, "Failed\nTracker Error: Connection Refused");
    break;

  case kRegisterEHOSTDOWN:
    fprintf(stderr, "Failed\nTracker Error: Host Down");
    break;

  case kRegisterEHOSTUNREACH:
    fprintf(stderr, "Failed\nTracker Error: Host Unreachable");
    break;

  case kRegisterEBADVERSION:
    fprintf(stderr, "Failed\nTracker Error: Incompatible Version");
    break;

  case kRegisterETCPCLOSED:
    fprintf(stderr, "Failed\nTracker Error: Failed to Verify TCP Port Forwarded");
    break;

  case kRegisterEUDPCLOSED:
    fprintf(stderr, "Failed\nTracker Error: Failed to Verify UDP Port Forwarded");
    break;

  case kRegisterECONNRESET:
    fprintf(stderr, "Failed\nTracker Error: Connection Reset by Peer");
    break;

  default:
    assert(0);
    break;
  }
}

int commandloop() {
  regex_t quit_preg;
  regex_t status_preg;
  regex_t pause_preg;
  regex_t resume_preg;
  regex_t allowjoin_preg;
  regex_t disallowjoin_preg;
  regex_t kick_preg;
  regex_t ban_preg;
  regex_t unban_preg;
  regmatch_t pmatch[NMATCH];
  char buf[20];
  int gotlock = 0;
  int err;

TRY
  if ((err = regcomp(&quit_preg, "^[[:space:]]*quit[[:space:]]*$", REG_EXTENDED | REG_ICASE))) LOGFAIL(EREG_OFFSET + err)
  if ((err = regcomp(&status_preg, "^[[:space:]]*status[[:space:]]*$", REG_EXTENDED | REG_ICASE))) LOGFAIL(EREG_OFFSET + err)
  if ((err = regcomp(&pause_preg, "^[[:space:]]*pause[[:space:]]*$", REG_EXTENDED | REG_ICASE))) LOGFAIL(EREG_OFFSET + err)
  if ((err = regcomp(&resume_preg, "^[[:space:]]*resume[[:space:]]*$", REG_EXTENDED | REG_ICASE))) LOGFAIL(EREG_OFFSET + err)
  if ((err = regcomp(&allowjoin_preg, "^[[:space:]]*allowjoin[[:space:]]*$", REG_EXTENDED | REG_ICASE))) LOGFAIL(EREG_OFFSET + err)
  if ((err = regcomp(&disallowjoin_preg, "^[[:space:]]*disallowjoin[[:space:]]*$", REG_EXTENDED | REG_ICASE))) LOGFAIL(EREG_OFFSET + err)
  if ((err = regcomp(&kick_preg, "^[[:space:]]*kick[[:space:]]+([[:digit:]]+)[[:space:]]*$", REG_EXTENDED | REG_ICASE))) LOGFAIL(EREG_OFFSET + err)
  if ((err = regcomp(&ban_preg, "^[[:space:]]*ban[[:space:]](.*)$", REG_EXTENDED | REG_ICASE))) LOGFAIL(EREG_OFFSET + err)
  if ((err = regcomp(&unban_preg, "^[[:space:]]*unban[[:space:]](.*)$", REG_EXTENDED | REG_ICASE))) LOGFAIL(EREG_OFFSET + err)

  for (;;) {
    int nbytes;
    printf("> ");

    /* read input one char at a time can process cooked and uncooked data I think */
    nbytes = 0;
    for (;;) {
      int ch;

      ch = getchar();

      if (ch == '\n') {
        buf[nbytes++] = '\0';
        break;
      }
      else if (ch == '\b') {
        if (nbytes > 0) {
          nbytes--;
        }
      }
      else {
        if (nbytes < sizeof(buf) - 1) {
          buf[nbytes++] = ch;
        }
        else {
          nbytes++;
        }
      }
    }

    /* quitting */
    if ((err = regexec(&quit_preg, buf, NMATCH, pmatch, 0))) {
      if (err != REG_NOMATCH) {
        LOGFAIL(EREG_OFFSET + err)
      }
    }
    else {
      printf("quitting\n");
      SUCCESS
    }

    /* display status */
    if ((err = regexec(&status_preg, buf, NMATCH, pmatch, 0))) {
      if (err != REG_NOMATCH) {
        LOGFAIL(EREG_OFFSET + err)
      }
    }
    else {
      int i;

      if (lockserver()) LOGFAIL(errno)
      gotlock = 1;

      printf(
          "Host Player Name: %s\n"
          "Map Name: %s\n"
          "Host Port: %d\n"
          "Tracker: %s\n",
          hostplayer,
          mapname,
          getservertcpport(),
          tracker ? tracker : "N/A"
        );

      for (i = 0; i < MAXPLAYERS; i++) {
        if (server.players[i].used) {
          int j;

          printf(
              "Player %d (%s):\n"
              "\tName: %s\n",
              i,
              server.players[i].cntlsock == -1 ? "Disconnected" : inet_ntoa(server.players[i].addr.sin_addr),
              server.players[i].name
            );

          printf("\tBases:");

          for (j = 0; j < MAXBASES; j++) {
            if (server.bases[j].owner == i) {
              printf(" %d", j);
            }
          }

          printf("\n\tPills:");

          for (j = 0; j < MAXPILLS; j++) {
            if (server.pills[j].owner == i) {
              printf(" %d", j);
            }
          }

          printf("\n");
        }
      }

      struct ListNode *node;

      printf("Banned List:");

      for (node = nextlist(&server.bannedplayers), i = 0; node; node = nextlist(node), i++) {
        struct BannedPlayer *bannedplayer;

        bannedplayer = ptrlist(node);
        printf(
            "\nName (id %d): %s@%s",
            i,
            bannedplayer->name,
            inet_ntoa(bannedplayer->sin_addr)
          );
      }

      printf("\n");

      if (unlockserver()) LOGFAIL(errno)
      gotlock = 0;
      continue;
    }

    /* pausing */
    if ((err = regexec(&pause_preg, buf, NMATCH, pmatch, 0))) {
      if (err != REG_NOMATCH) {
        LOGFAIL(EREG_OFFSET + err)
      }
    }
    else {
      if (lockserver()) LOGFAIL(errno)
      gotlock = 1;

      if (getpauseserver()) {
        printf("Already paused\n");
      }
      else {
        pauseserver();
        printf("Paused\n");
      }

      if (unlockserver()) LOGFAIL(errno)
      gotlock = 0;
      continue;
    }

    /* resume */
    if ((err = regexec(&resume_preg, buf, NMATCH, pmatch, 0))) {
      if (err != REG_NOMATCH) {
        LOGFAIL(EREG_OFFSET + err)
      }
    }
    else {
      if (lockserver()) LOGFAIL(errno)
      gotlock = 1;

      if (getpauseserver()) {
        resumeserver();
        printf("Resuming...\n");
      }
      else {
        printf("Not Paused\n");
      }

      if (unlockserver()) LOGFAIL(errno)
      gotlock = 0;
      continue;
    }

    /* allow join */
    if ((err = regexec(&allowjoin_preg, buf, NMATCH, pmatch, 0))) {
      if (err != REG_NOMATCH) {
        LOGFAIL(EREG_OFFSET + err)
      }
    }
    else {
      if (lockserver()) LOGFAIL(errno)
      gotlock = 1;

      if (getallowjoinserver()) {
        printf("Allow Join Already Enabled\n");
      }
      else {
        setallowjoinserver(1);
        printf("Allow Join Enabled\n");
      }

      if (unlockserver()) LOGFAIL(errno)
      gotlock = 0;
      continue;
    }

    /* disallow join */
    if ((err = regexec(&disallowjoin_preg, buf, NMATCH, pmatch, 0))) {
      if (err != REG_NOMATCH) {
        LOGFAIL(EREG_OFFSET + err)
      }
    }
    else {
      if (lockserver()) LOGFAIL(errno)
      gotlock = 1;

      if (getallowjoinserver()) {
        setallowjoinserver(0);
        printf("Allow Join Disabled\n");
      }
      else {
        printf("Allow Join Already Disabled\n");
      }

      if (unlockserver()) LOGFAIL(errno)
      gotlock = 0;
      continue;
    }

    /* kick player */
    if ((err = regexec(&kick_preg, buf, NMATCH, pmatch, 0))) {
      if (err != REG_NOMATCH) {
        LOGFAIL(EREG_OFFSET + err)
      }
    }
    else {
      int player;

      /* probably should do some error checking here */
      player = strtol(buf + pmatch[1].rm_so, NULL, 10);

      if (player < 0 || player >= MAXPLAYERS) {
        printf("Input Player ID in range of [0, 15]\n");
        continue;
      }

      if (kickplayer(player)) LOGFAIL(errno)
      continue;
    }

    /* ban player */
    if ((err = regexec(&ban_preg, buf, NMATCH, pmatch, 0))) {
      if (err != REG_NOMATCH) {
        LOGFAIL(EREG_OFFSET + err)
      }
    }
    else {
      int player;

      /* probably should do some error checking here */
      player = strtol(buf + pmatch[1].rm_so, NULL, 10);

      if (player < 0 || player >= MAXPLAYERS) {
        printf("Input Player ID in range of [0, 15]\n");
        continue;
      }

      if (banplayer(player)) LOGFAIL(errno)
      continue;
    }

    /* unban player */
    if ((err = regexec(&unban_preg, buf, NMATCH, pmatch, 0))) {
      if (err != REG_NOMATCH) {
        LOGFAIL(EREG_OFFSET + err)
      }
    }
    else {
      int player;

      /* probably should do some error checking here */
      player = strtol(buf + pmatch[1].rm_so, NULL, 10);

      if (player < 0) {
        printf("Input Ban ID in range of [0, +infinity)\n");
        continue;
      }

      if (unbanplayer(player)) LOGFAIL(errno)
      continue;
    }

    printf(
        "commands:\n"
        "\tquit\n"
        "\tstatus\n"
        "\tpause\n"
        "\tresume\n"
        "\tallowjoin\n"
        "\tdisallowjoin\n"
        "\tkick [id]\n"
        "\tban [id]\n"
        "\tunban [id]\n"
      );
  }

  regfree(&quit_preg);
  regfree(&status_preg);
  regfree(&pause_preg);
  regfree(&resume_preg);
  regfree(&allowjoin_preg);
  regfree(&disallowjoin_preg);
  regfree(&kick_preg);
  regfree(&ban_preg);
  regfree(&unban_preg);

CLEANUP
  switch (ERROR) {
  case 0:
    RETURN(0)

  default:
    if (gotlock) {
      unlockserver();
    }

    RETERR(-1)
  }
END
}
