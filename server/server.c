#include "gbnpacket.c"
#include <arpa/inet.h>
#include <errno.h>
#include <memory.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define ECHOMAX 255

void DieWithError(char *errorMessage);
void CatchAlarm(int ignored);

int main(int argc, char *argv[]) {
  char buffer[8193];
  buffer[8192] = '\0';
  int sock;
  struct sockaddr_in gbnServAddr;
  struct sockaddr_in gbnClntAddr;
  unsigned int cliAddrLen;
  unsigned short gbnServPort;
  int recvMsgSize;
  int packet_rcvd = -1;
  struct sigaction myAction;
  double lossRate;

  bzero(buffer, 8192);
  if (argc < 3 || argc > 4) {
    fprintf(stderr, "Usage:  %s <UDP SERVER PORT> <CHUNK SIZE> [<LOSS RATE>]\n",
            argv[0]);
    exit(1);
  }

  gbnServPort = atoi(argv[1]);
  int chunkSize = atoi(argv[2]);
  if (argc == 4)
    lossRate = atof(argv[3]);
  else
    lossRate = 0.0;

  srand48(123456789);
  if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
    DieWithError("socket() failed");

  memset(&gbnServAddr, 0, sizeof(gbnServAddr));
  gbnServAddr.sin_family = AF_INET;
  gbnServAddr.sin_addr.s_addr = htonl(INADDR_ANY);
  gbnServAddr.sin_port = htons(gbnServPort);

  if (bind(sock, (struct sockaddr *)&gbnServAddr, sizeof(gbnServAddr)) < 0)
    DieWithError("bind() failed");
  myAction.sa_handler = CatchAlarm;
  if (sigfillset(&myAction.sa_mask) < 0)
    DieWithError("sigfillset failed");
  myAction.sa_flags = 0;
  if (sigaction(SIGALRM, &myAction, 0) < 0)
    DieWithError("sigaction failed for SIGALRM");
  for (;;) {
    cliAddrLen = sizeof(gbnClntAddr);
    struct gbnpacket currPacket;
    memset(&currPacket, 0, sizeof(currPacket));
    recvMsgSize = recvfrom(sock, &currPacket, sizeof(currPacket), 0,
                           (struct sockaddr *)&gbnClntAddr, &cliAddrLen);
    currPacket.type = ntohl(currPacket.type);
    currPacket.length = ntohl(currPacket.length);
    currPacket.seq_no = ntohl(currPacket.seq_no);
    if (currPacket.type == 4) {
      printf("%s\n", buffer);
      struct gbnpacket ackmsg;
      ackmsg.type = htonl(8);
      ackmsg.seq_no = htonl(0);
      ackmsg.length = htonl(0);
      if (sendto(sock, &ackmsg, sizeof(ackmsg), 0,
                 (struct sockaddr *)&gbnClntAddr,
                 cliAddrLen) != sizeof(ackmsg)) {
        DieWithError("Error sending tear-down ack");
      }
      alarm(7);
      while (1) {
        while ((recvfrom(sock, &currPacket, sizeof(int) * 3 + chunkSize, 0,
                         (struct sockaddr *)&gbnClntAddr, &cliAddrLen)) < 0) {
          if (errno == EINTR) {
            exit(0);
          } else
            ;
        }
        if (ntohl(currPacket.type) == 4) {
          ackmsg.type = htonl(8);
          ackmsg.seq_no = htonl(0);
          ackmsg.length = htonl(0);
          if (sendto(sock, &ackmsg, sizeof(ackmsg), 0,
                     (struct sockaddr *)&gbnClntAddr,
                     cliAddrLen) != sizeof(ackmsg)) {
            DieWithError("Error sending tear-down ack");
          }
        }
      }
      DieWithError("recvfrom() failed");
    } else {
      if (lossRate > drand48())
        continue;
      printf("---- RECEIVE PACKET %d length %d\n", currPacket.seq_no,
             currPacket.length);
      if (currPacket.seq_no == packet_rcvd + 1) {
        packet_rcvd++;
        int buff_offset = chunkSize * currPacket.seq_no;
        memcpy(&buffer[buff_offset], currPacket.data, currPacket.length);
      }
      printf("---- SEND ACK %d\n", packet_rcvd);
      struct gbnpacket currAck;
      currAck.type = htonl(2);
      currAck.seq_no = htonl(packet_rcvd);
      currAck.length = htonl(0);
      if (sendto(sock, &currAck, sizeof(currAck), 0,
                 (struct sockaddr *)&gbnClntAddr,
                 cliAddrLen) != sizeof(currAck))
        DieWithError("sendto() sent a different number of bytes than expected");
    }
  }
}

void DieWithError(char *errorMessage) {
  perror(errorMessage);
  exit(1);
}

void CatchAlarm(int ignored) { exit(0); }
