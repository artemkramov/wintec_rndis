#include <string.h>       // strncpy
#include <sys/ioctl.h>    // SIOCGIFFLAGS
#include <errno.h>        // errno
#include <netinet/in.h>   // IPPROTO_IP
#include <net/if.h>       // IFF_*, ifreq
#include <stdio.h>        // printf
#include <stdlib.h>       // malloc
#include "core-libraries.h"

#define DEBUG 1 //Debug macro
#define ERROR(fmt, ...) do { printf(fmt, __VA_ARGS__); return -1; } while(0) //Macro for error handling
#define BUFFER_BASH_COMMAND_LENGTH 200 //Length for the system command
#define INTERNET_TICK 5 //Period for the check of Internet connection

/**
* Function for the Internet sharing for interface ifname
*/
void setInternetConnection(char *ifname);

/**
* Get interface flags for the further operations
*/
int getInterfaceState(char *ifname) {
    int state = -1;
    int socId = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (socId < 0) ERROR("Socket failed. Errno = %d\n", errno);

    struct ifreq if_req;
    (void) strncpy(if_req.ifr_name, ifname, sizeof(if_req.ifr_name));
    int rv = ioctl(socId, SIOCGIFFLAGS, &if_req);

    if (rv == -1) {
        close(socId);
		ERROR("Ioctl failed. Errno = %d\n", errno);
    }

    close(socId);

    return if_req.ifr_flags;
}

/**
* Check if the interface is in UP mode
* by the given interface flags
*/
int isInterfaceUp(int flags) {
    return (flags & IFF_UP) && (flags & IFF_RUNNING);
}

/**
* Bring up the registered interface
* by the given name
*/
int upInterface(char *ifname) {
    char *commandInterfaceUp = "/system/bin/ifconfig %s 192.168.48.1 netmask 255.255.255.0 up";
    char command[BUFFER_BASH_COMMAND_LENGTH];
    sprintf(command, commandInterfaceUp, ifname);
    return WEXITSTATUS(systemCall(command));
}

/**
* Share internet on interface from the given @shareIfname
*/
void shareInternetOnInterface(char *ifname, char *shareIfname) {
	char command[BUFFER_BASH_COMMAND_LENGTH];
	char *commandInterface = "ndc ipfwd enable%s";
	sprintf(command, commandInterface, "");
	systemCall(command);

	commandInterface = "iptables -A FORWARD --in-interface %s -j ACCEPT";
	sprintf(command, commandInterface, ifname);
	systemCall(command);
	
	commandInterface = "ndc tether dns set 8.8.4.4 8.8.8.8";
	sprintf(command, commandInterface, ifname);
	systemCall(command);
	
	commandInterface = "iptables -t nat -A POSTROUTING --out-interface %s -j MASQUERADE";
	sprintf(command, commandInterface, shareIfname);
	systemCall(command);
	
	commandInterface = "ndc nat enable %s %s 0";
	sprintf(command, commandInterface, ifname, shareIfname);
	systemCall(command);
}

/**
* Add unicast route to the given interface
*/
int addInterfaceUnicastRoute(char *ifname) {
    char *commandInterface = "/system/bin/ip route add table local unicast 192.168.48.0/24 via 192.168.48.1%s";
    char command[BUFFER_BASH_COMMAND_LENGTH];
    sprintf(command, commandInterface, "");
    return WEXITSTATUS(systemCall(command));
}

/**
* Listen interface due to changes on it
*/
void listenInterface(char *ifname) {
    int interfaceFlags = getInterfaceState(ifname);
    /*
	* Check if the interface is already registered
	* and if it is in DOWN mode
	*/
	if (interfaceFlags != -1 && !isInterfaceUp(interfaceFlags)) {
		   /*
		   * Up interface by the given name
		   */
		   int state = upInterface(ifname);
           /*
		   * Check if the interface was upped successfully
		   * then add the unicast route from Android to cash register
		   */
		   if (state != -1) {
              state = addInterfaceUnicastRoute(ifname);
			  setInternetConnection(ifname);
           }
        
    }
}

/**
* Check Internet connection on usb interface
* and up it if it's a necessary
*/
void checkInternetSharing(char *ifname, char *shareIfname)
{
    FILE *fpipe; 
	/*
	* Run the command to show the list of rules
	*/
	char *commandInterface = "/system/bin/ip rule list | grep %s | grep %s";
    char command[BUFFER_BASH_COMMAND_LENGTH];
    sprintf(command, commandInterface, ifname, shareIfname);
	char buf[256];
	if (!(fpipe = (FILE*)popenCall(command, "r"))) exit(1);
	/*
	* If the result is empty
	* Then the Internet isn't configured yet
	*/ 
	if (!fgets(buf, sizeof buf, fpipe)) {
		shareInternetOnInterface(ifname, shareIfname);
	}
	pclose(fpipe);
}

/**
* Share internet on interface from wlan0
*/
void setInternetConnection(char *ifname)
{
	checkInternetSharing(ifname, "wlan0");
	checkInternetSharing(ifname, "eth0");
}

/**
* Entry point of the program
*/
main()
{
	int tick = 0;
	
	/*
	* Interface name
	*/
	char *ifname = "usb0";
	
	/*
	* Call DNS server service for this interface 
	*/
	char *dnsServerCommand = "dnsmasq --dhcp-range=192.168.48.2,192.168.48.2,1h --pid-file --interface=%s";
	char *command = malloc(strlen(dnsServerCommand) + 2);
    sprintf(command, dnsServerCommand, ifname);	
    systemCall(command);
    
	/*
	* Listen interface every second
	*/
    while (1) {
	    tick++;
		listenInterface(ifname);
		if (tick == INTERNET_TICK) {
			tick = 0;
			setInternetConnection(ifname);
		}
        sleep(1);
    }
    return 1;
}