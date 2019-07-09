#include "gbnpacket.c"
#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define TIMEOUT_SECS 3
#define MAXTRIES 10

int tries = 0;
int base = 0;
int windowSize = 0;
int sendflag = 1;

void DieWithError(char *errorMessage);
void CatchAlarm(int ignored);
int max(int a, int b);
int min(int a, int b);

int main(int argc, char *argv[]) {
  int sock;
  struct sockaddr_in gbnServAddr;
  struct sockaddr_in fromAddr;
  unsigned short gbnServPort;
  unsigned int fromSize;
  struct sigaction myAction;
  char *servIP;
  int respLen;
  int packet_received = -1;
  int packet_sent = -1;

  FILE *fp;
  long lSize;
  char *buffer;

  fp = fopen("input.txt", "rb");
  if (!fp)
    perror("input.txt"), exit(1);

  fseek(fp, 0L, SEEK_END);
  lSize = ftell(fp);
  rewind(fp);

  buffer = calloc(1, lSize + 1);
  if (!buffer)
    fclose(fp), fputs("memory alloc fails", stderr), exit(1);

  if (1 != fread(buffer, lSize, 1, fp))
    fclose(fp), free(buffer), fputs("entire read fails", stderr), exit(1);

  int n;
  n = strlen(buffer) - 1;
  const int datasize = n;
  int chunkSize;
  int nPackets = 0;

  if (argc != 5) {
    fprintf(
        stderr,
        "Usage: %s <Server IP> <Server Port No> <Chunk size> <Window Size>\n",
        argv[0]);
    exit(1);
  }

  servIP = argv[1];
  chunkSize = atoi(argv[3]);
  gbnServPort = atoi(argv[2]);
  windowSize = atoi(argv[4]);
  if (chunkSize >= 512) {
    fprintf(stderr, "chunk size must be less than 512\n");
    exit(1);
  }

  nPackets = datasize / chunkSize;
  if (datasize % chunkSize)
    nPackets++;
  if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
    DieWithError("socket() failed");
  printf("created socket");

  myAction.sa_handler = CatchAlarm;
  if (sigfillset(&myAction.sa_mask) < 0)
    DieWithError("sigfillset() failed");
  myAction.sa_flags = 0;

  if (sigaction(SIGALRM, &myAction, 0) < 0)
    DieWithError("sigaction() failed for SIGALRM");

  memset(&gbnServAddr, 0, sizeof(gbnServAddr));
  gbnServAddr.sin_family = AF_INET;
  gbnServAddr.sin_addr.s_addr = inet_addr(servIP);
  gbnServAddr.sin_port = htons(gbnServPort);

  while ((packet_received < nPackets - 1) && (tries < MAXTRIES)) {

    if (sendflag > 0) {
      sendflag = 0;
      int ctr;
      for (ctr = 0; ctr < windowSize; ctr++) {
        packet_sent = min(max(base + ctr, packet_sent), nPackets - 1);
        struct gbnpacket currpacket;
        if ((base + ctr) < nPackets) {
          memset(&currpacket, 0, sizeof(currpacket));
          printf("sending packet %d packet_sent %d packet_received %d\n",
                 base + ctr, packet_sent, packet_received);

          currpacket.type = htonl(1);
          currpacket.seq_no = htonl(base + ctr);
          int currlength;
          if ((datasize - ((base + ctr) * chunkSize)) >= chunkSize)
            currlength = chunkSize;
          else
            currlength = datasize % chunkSize;
          currpacket.length = htonl(currlength);
          memcpy(currpacket.data, buffer + ((base + ctr) * chunkSize),
                 currlength);
          if (sendto(sock, &currpacket, (sizeof(int) * 3) + currlength, 0,
                     (struct sockaddr *)&gbnServAddr,
                     sizeof(gbnServAddr)) != ((sizeof(int) * 3) + currlength))
            DieWithError(
                "sendto() sent a different number of bytes than expected");
        }
      }
    }

    fromSize = sizeof(fromAddr);
    alarm(TIMEOUT_SECS);
    struct gbnpacket currAck;
    while ((respLen = (recvfrom(sock, &currAck, sizeof(int) * 3, 0,
                                (struct sockaddr *)&fromAddr, &fromSize))) < 0)
      if (errno == EINTR) {
        if (tries < MAXTRIES) {
          printf("timed out, %d more tries...\n", MAXTRIES - tries);
          break;
        } else
          DieWithError("No Response");
      } else
        DieWithError("recvfrom() failed");

    if (respLen) {
      int acktype = ntohl(currAck.type);
      int ackno = ntohl(currAck.seq_no);
      if (ackno > packet_received && acktype == 2) {
        printf("received ack\n");
        packet_received++;
        base = packet_received;
        if (packet_received == packet_sent) {
          alarm(0);
          tries = 0;
          sendflag = 1;
        } else {
          tries = 0;
          sendflag = 0;
          alarm(TIMEOUT_SECS);
        }
      }
    }
  }
  int ctr;
  for (ctr = 0; ctr < 10; ctr++) {
    struct gbnpacket teardown;
    teardown.type = htonl(4);
    teardown.seq_no = htonl(0);
    teardown.length = htonl(0);
    sendto(sock, &teardown, (sizeof(int) * 3), 0,
           (struct sockaddr *)&gbnServAddr, sizeof(gbnServAddr));
  }
  close(sock);
  fclose(fp);
  free(buffer);
  exit(0);
}

void CatchAlarm(int ignored) {
  tries += 1;
  sendflag = 1;
}

void DieWithError(char *errorMessage) {
  perror(errorMessage);
  exit(1);
}

int max(int a, int b) {
  if (b > a)
    return b;
  return a;
}

int min(int a, int b) {
  if (b > a)
    return a;
  return b;
}
