/*
 * netsniff-ng - the packet sniffing beast
 * Copyright 2013 Daniel Borkmann.
 * Subject to the GPL, version 2.
 */

#ifndef GEOIP_H
#define GEOIP_H

#include <netinet/in.h>

extern void init_geoip(void);
extern const char *geoip4_city_name(struct sockaddr_in sa);
extern const char *geoip6_city_name(struct sockaddr_in6 sa);
extern const char *geoip4_country_name(struct sockaddr_in sa);
extern const char *geoip6_country_name(struct sockaddr_in6 sa);
extern float geoip4_longitude(struct sockaddr_in sa);
extern float geoip4_latitude(struct sockaddr_in sa);
extern float geoip6_longitude(struct sockaddr_in6 sa);
extern float geoip6_latitude(struct sockaddr_in6 sa);
extern void destroy_geoip(void);

#endif /* GEOIP_H */
