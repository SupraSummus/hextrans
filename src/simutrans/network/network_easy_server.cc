/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 *
 * Easy-server helpers: external-IP discovery + UPnP port forwarding,
 * for hosting games behind routers with dynamic IPs.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "network.h"
#include "network_file_transfer.h"
#include "../dataobj/environment.h"
#include "../simdebug.h"
#include "../simversion.h"
#include "../utils/cbuffer.h"


bool get_external_IP( cbuffer_t &myIPaddr, cbuffer_t &altIPaddr )
{
	myIPaddr.clear();
	altIPaddr.clear();
	// query for IP (faster than asking router using uPnP and we can get IP6 too)
	const char *err = network_http_get( QUERY_ADDR_IP, QUERY_ADDR_URL, altIPaddr );
	// if we have a dual stack system, IP6 should be preferred, i.e. we have now the IP6
	if(  err==NULL  &&  strstr(altIPaddr,":")  ) {
		// try to get and IPv4 address too
		if(  !network_http_get( QUERY_ADDR_IPv4_ONLY, QUERY_ADDR_URL, myIPaddr )  ) {
			if(  strcmp( myIPaddr, altIPaddr ) == 0   ) {
				// same, no alternative address
				altIPaddr.clear();
			}
		}
	}
	else {
		myIPaddr = altIPaddr;
		altIPaddr.clear();
	}

#ifdef LOOKUP_OWN_IP_NAME
	// enable to try to get a symbolic name for IPv4
	if(  !err  ) {
		struct sockaddr_in sin;
		memset(&sin, 0, sizeof(sin));
		sin.sin_family      = AF_INET;
		sin.sin_addr.s_addr = inet_addr(myIPaddr);
		sin.sin_port        = 0; // If 0, port is chosen by system
		char hostname[1024];
		hostname[0] = 0;

		int failed = getnameinfo((const sockaddr *)&sin, sizeof(sin), hostname, lengthof(hostname), NULL, 0, 0);
		if(  !failed  &&  *hostname  ) {
			myIPaddr.clear();
			myIPaddr.append( hostname );
		}
	}
#endif

	return err==NULL;
}

#ifdef USE_UPNP
/*
 **** The following functions are used to open ports in UPnP router and will query the IP address ****
 **** So it will become much easier to set up network games at home.
 */

extern "C" {
#define MINIUPNPC_DECLSPEC_H_INCLUDED
#define MINIUPNP_LIBSPEC extern

//#define MINIUPNP_STATICLIB
#include <miniupnpc/miniupnpc.h>
#include <miniupnpc/upnpcommands.h>
}

#if MINIUPNPC_API_VERSION < 14
#define upnpDiscover(a,b,c,d,e,f,g) upnpDiscover(a,b,c,d,e,g)
#define UPNP_LOCAL_PORT_ANY 0
#endif


bool prepare_for_server( char *externalIPAddress, char *externalAltIPAddress, int port )
{
	char lanaddr[64] = "unset"; /* my ip address on the LAN */
	int error = 0;
	const char *multicastif = 0;
	const char *minissdpdpath = 0;
	int localport = UPNP_LOCAL_PORT_ANY;
	int ipv6 = 0; // probably not needed for IPv6 ever ...
	unsigned char ttl = 2; (void)ttl; /* defaulting to 2 */
	struct UPNPDev *devlist = 0;
	bool has_IP = false;

	if(  (devlist = upnpDiscover( 2000, multicastif, minissdpdpath, localport, ipv6, ttl, &error ))  ) {
		struct UPNPUrls urls;
		struct IGDdatas data;

#if MINIUPNPC_API_VERSION <= 17
		UPNP_GetValidIGD( devlist, &urls, &data, lanaddr, sizeof(lanaddr) );
#else
		char wanaddr[64] = "uset";
		UPNP_GetValidIGD(devlist, &urls, &data, lanaddr, sizeof(lanaddr), wanaddr, sizeof(lanaddr));
#endif
		// we must know our IP address first
		if(  UPNP_GetExternalIPAddress(urls.controlURL, data.first.servicetype, externalIPAddress) ==  UPNPCOMMAND_SUCCESS  ) {
			// this is our ID (at least the routes tells us this)
			char eport[19];
			char *iport = eport;
			sprintf( eport, "%d", port );
			// remove anz forwarding
			UPNP_DeletePortMapping(urls.controlURL, data.first.servicetype, eport, "TCP", NULL);
			// setting up tcp redirect forever (last parameter "0")
			if(  UPNP_AddPortMapping(urls.controlURL, data.first.servicetype, eport, iport, lanaddr, "simutrans", "TCP", 0, "0")  ==  UPNPCOMMAND_SUCCESS  ) {
				// ok, we have our ID and redirected a port to us
				has_IP = true;
			}
			else {
				dbg->warning( "prepare_for_server()", "Could not redirect port (but may be still ok" );
				has_IP = true;
			}
		}
		FreeUPNPUrls(&urls);
	}
	freeUPNPDevlist(devlist);

	externalAltIPAddress[0] = 0;
	// use the same routine as later the announce routine, otherwise update with dynamic IP fails
	cbuffer_t myIPaddr, altIPaddr;
	if(  get_external_IP( myIPaddr, altIPaddr )  ) {
		has_IP = true;
		strcpy( externalIPAddress, myIPaddr );
		if(  altIPaddr.len()  ) {
			strcpy( externalAltIPAddress, altIPaddr );
		}
	}

	return has_IP;
}


// removes the redirect (or do nothing)
void remove_port_forwarding( int port )
{
	if(  port <= 0  ||  env_t::easy_server != 1  ) {
		return;
	}

	char lanaddr[64] = "unset"; /* my ip address on the LAN */
	char externalIPAddress[64];
	int error = 0;
	const char *multicastif = 0;
	const char *minissdpdpath = 0;
	int localport = UPNP_LOCAL_PORT_ANY;
	int ipv6 = 0; // probably not needed for IPv6 ever ...
	unsigned char ttl = 2; (void)ttl; /* defaulting to 2 */
	struct UPNPDev *devlist = 0;

	if(  (devlist = upnpDiscover( 2000, multicastif, minissdpdpath, localport, ipv6, ttl, &error ))  ) {
		struct UPNPUrls urls;
		struct IGDdatas data;

#if MINIUPNPC_API_VERSION <= 17
		UPNP_GetValidIGD(devlist, &urls, &data, lanaddr, sizeof(lanaddr));
#else
		char wanaddr[64] = "uset";
		UPNP_GetValidIGD(devlist, &urls, &data, lanaddr, sizeof(lanaddr), wanaddr, sizeof(lanaddr));
#endif

		// we must know our IP address first
		if(  UPNP_GetExternalIPAddress(urls.controlURL, data.first.servicetype, externalIPAddress) ==  UPNPCOMMAND_SUCCESS  ) {
			// this is our ID (at least the routes tells us this)
			char eport[19];
			sprintf( eport, "%d", port );
			UPNP_DeletePortMapping(urls.controlURL, data.first.servicetype, eport, "TCP", NULL);
		}
		FreeUPNPUrls(&urls);
	}
	freeUPNPDevlist(devlist);
}
#else
// or we just get only our IP and hope we are not behind a router ...

bool prepare_for_server(char* externalIPAddress, char* externalAltIPAddress, int /*port*/)
{
	externalAltIPAddress[0] = 0;
	// use the same routine as later the announce routine, otherwise update with dynamic IP fails
	cbuffer_t myIPaddr, altIPaddr;
	if (get_external_IP(myIPaddr, altIPaddr)) {
		strcpy(externalIPAddress, myIPaddr);
		if (altIPaddr.len()) {
			strcpy(externalAltIPAddress, altIPaddr);
		}
		return true;
	}

	return false;
}

void remove_port_forwarding( int )
{
}
#endif
