// ftp_server.cpp — Local FTP Server for SD Card PDF Report Access Implementation
#include "ftp_server.h"
#include "config.h"
#include <SimpleFTPServer.h>
#include <SD.h>

static FtpServer ftp;

void ftpServerInit() {
  ftp.begin(FTP_USER, FTP_PASS);
  Serial.printf("[FTP] FTP Server started on port %d (User: %s)\n", FTP_PORT, FTP_USER);
}

void ftpServerProcess() {
  ftp.handleFTP();
}
