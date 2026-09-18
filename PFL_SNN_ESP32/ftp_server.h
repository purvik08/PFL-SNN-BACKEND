// ftp_server.h — Local FTP Server for SD Card PDF Report Access
#pragma once
#include <Arduino.h>

void ftpServerInit();
void ftpServerProcess(); // Non-blocking service loop
