#ifndef _GPS_H_
#define _GPS_H_

extern char   gps_time[];
extern char   num_sats;

void gps_start_it();
void gps_parse(char* line);
void gps_read();

#endif
