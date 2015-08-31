#include <stdio.h>
#include <string.h>
#include <string>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <array>
#include <sstream>
#include "ocstack.h"
#include "logger.h"

#define DEFAULT_CONTEXT_VALUE 0x99

static int 			UNICAST_DISCOVERY 							= 0;
static int 			TEST_CASE 									= 0;
static const char * TEST_APP_UNICAST_DISCOVERY_QUERY 			= "coap://0.0.0.0:5683/oc/core";
static const char * TEST_APP_UNICAST_DEVICE_DISCOVERY_QUERY 	= "coap://0.0.0.0:5683/oc/core/d";
static const char * TEST_APP_MULTICAST_DEVICE_DISCOVERY_QUERY 	= "coap://224.0.1.187:5683/oc/core/d";
static std::string 	putPayload 									= "{\"state\":\"on\",\"power\":5}";
static std::string 	coapServerIP 								= "255.255.255.255";
static std::string 	coapServerPort 								= "5683";
static std::string 	coapServerResource 							= "/a/light";
static uint16_t 	OC_WELL_KNOWN_PORT 							= 5683;

int gQuitFlag = 0;
OCDoHandle gObserveDoHandle;

OCStackResult InvokeOCDoResource(std::ostringstream &query,
                                 OCMethod method, OCQualityOfService qos,
                                 OCClientResponseHandler cb, OCHeaderOption * options, uint8_t numOptions);
const char *getResult(OCStackResult result);
std::string getIPAddrTBServer(OCClientResponse * clientResponse);
std::string getPortTBServer(OCClientResponse * clientResponse);
void parseClientResponse(OCClientResponse * clientResponse);

void
handleSigInt(int signum) {
    if (signum == SIGINT) {
        gQuitFlag = 1;
        printf ("OCClient is closing...\n");
    }
}

/*
 * GET Callback
 */
OCStackApplicationResult
getReqCB (void* ctx, OCDoHandle handle, OCClientResponse * clientResponse) {
    if(clientResponse == NULL) {
        printf ("The clientResponse is NULL\n");
        return OC_STACK_DELETE_TRANSACTION;
    }

    if(ctx == (void*)DEFAULT_CONTEXT_VALUE) {
        printf ("Callback Context for GET query recvd successfully\n");
    }

    printf ("StackResult: %s\n",  	getResult(clientResponse->result));
    printf ("Sequence #: %d\n", 	clientResponse->sequenceNumber);
    printf ("JSON Payload: %s\n", 	clientResponse->resJSONPayload);

    if(clientResponse->rcvdVendorSpecificHeaderOptions &&
            clientResponse->numRcvdVendorSpecificHeaderOptions) {
        printf ("Received vendor specific options\n"); 
    }

    return OC_STACK_DELETE_TRANSACTION;
}

/*
 * PUT Callback
 */
OCStackApplicationResult
putReqCB (void* ctx, OCDoHandle handle, OCClientResponse * clientResponse) {
    if(ctx == (void*)DEFAULT_CONTEXT_VALUE){
        printf ("Callback Context for PUT recvd successfully\n");
    }

    if(clientResponse) {
        printf ("StackResult: %s\n",  getResult(clientResponse->result));
        printf ("JSON Payload: %s\n", clientResponse->resJSONPayload);
    }

    return OC_STACK_DELETE_TRANSACTION;
}

/*
 * Descover new devices Callback
 */
OCStackApplicationResult
discoveryReqCB(void* ctx, OCDoHandle handle,
        OCClientResponse * clientResponse) {
    uint8_t remoteIpAddr[4];
    uint16_t remotePortNu;

    if (ctx == (void*) DEFAULT_CONTEXT_VALUE) {
        printf ("Callback Context for DISCOVER query recvd successfully\n");
    }

    if (clientResponse) {
        printf ("StackResult: %s\n", getResult(clientResponse->result));

        OCDevAddrToIPv4Addr((OCDevAddr *) clientResponse->addr, remoteIpAddr,
                remoteIpAddr + 1, remoteIpAddr + 2, remoteIpAddr + 3);
        OCDevAddrToPort((OCDevAddr *) clientResponse->addr, &remotePortNu);

        printf ("Discovered device: %s @ %d.%d.%d.%d:%d\n", clientResponse->resJSONPayload, 
				remoteIpAddr[0], remoteIpAddr[1],
                remoteIpAddr[2], remoteIpAddr[3], 
				remotePortNu);

        parseClientResponse(clientResponse);

        // Init GET request to the server.
        std::ostringstream query;
        query << "coap://" << coapServerIP << ":" << coapServerPort << coapServerResource;
        InvokeOCDoResource(query, OC_REST_GET, OC_LOW_QOS, getReqCB, NULL, 0);
        // InvokeOCDoResource(query, OC_REST_PUT, OC_LOW_QOS, putReqCB, NULL, 0);
    }

    return (UNICAST_DISCOVERY) ? OC_STACK_DELETE_TRANSACTION : OC_STACK_KEEP_TRANSACTION;
}

int
InitDiscovery () {
    OCStackResult ret;
    OCCallbackData cbData;
    OCDoHandle handle;
    char szQueryUri[64] = { 0 };

    cbData.cb 		= discoveryReqCB;
    cbData.context 	= (void*)DEFAULT_CONTEXT_VALUE;
	cbData.cd 		= NULL;

    //strcpy(szQueryUri, OC_WELL_KNOWN_QUERY);
    strncpy(szQueryUri, TEST_APP_MULTICAST_DEVICE_DISCOVERY_QUERY,
                (strlen(TEST_APP_MULTICAST_DEVICE_DISCOVERY_QUERY) + 1));
	/* 
	 * Search for resources.
	 */
    ret = OCDoResource(&handle, OC_REST_GET, szQueryUri, 0, 0, OC_LOW_QOS, &cbData, NULL, 0);
    if (ret != OC_STACK_OK) {
        printf("OCStack device error\n");
    }

    return ret;
}

int
main(int argc, char** argv) {
    uint8_t 	addr[20] 	= {0};
    uint8_t* 	paddr 		= NULL;
    uint16_t    port 		= USE_RANDOM_PORT;
    uint8_t 	ifname[] 	= "wlan0";
    int 		opt;

    printf ("OCClient is starting...\n");
    if (OCGetInterfaceAddress(ifname, sizeof(ifname), AF_INET, addr,
                sizeof(addr)) == ERR_SUCCESS) {
        printf ("Starting occlient on address %s\n",addr);
        paddr = addr;
    }

    if (OCInit((char *) paddr, port, OC_CLIENT) != OC_STACK_OK) {
        printf ("OCStack init error\n");
        return 0;
    }

    InitDiscovery ();

    signal(SIGINT, handleSigInt);
    while (!gQuitFlag) {
        if (OCProcess() != OC_STACK_OK) {
            printf ("OCStack process error\n");
            return 0;
        }

        sleep(2);
    }

    if (OCStop() != OC_STACK_OK) {
        printf ("OCStack process error\n");
    }

    return 0;
}

OCStackResult
InvokeOCDoResource(std::ostringstream &query,
                                 OCMethod method, OCQualityOfService qos,
                                 OCClientResponseHandler cb, OCHeaderOption * options, uint8_t numOptions) {
    OCStackResult ret;
    OCCallbackData cbData;
    OCDoHandle handle;

    cbData.cb 		= cb;
    cbData.context 	= (void*)DEFAULT_CONTEXT_VALUE;
    cbData.cd 		= NULL;

    ret = OCDoResource(&handle, method, query.str().c_str(), 0,
                       (method == OC_REST_PUT) ? putPayload.c_str() : NULL,
                       qos, &cbData, options, numOptions);

    if (ret != OC_STACK_OK) {
        printf ("OCDoResource returns error %d with method %d\n", ret, method);
    } else if (method == OC_REST_OBSERVE || method == OC_REST_OBSERVE_ALL) {
        gObserveDoHandle = handle;
    }

    return ret;
}

const char *
getResult(OCStackResult result) {
    switch (result) {
    case OC_STACK_OK:
        return "OC_STACK_OK";
    case OC_STACK_RESOURCE_CREATED:
        return "OC_STACK_RESOURCE_CREATED";
    case OC_STACK_RESOURCE_DELETED:
        return "OC_STACK_RESOURCE_DELETED";
    case OC_STACK_INVALID_URI:
        return "OC_STACK_INVALID_URI";
    case OC_STACK_INVALID_QUERY:
        return "OC_STACK_INVALID_QUERY";
    case OC_STACK_INVALID_IP:
        return "OC_STACK_INVALID_IP";
    case OC_STACK_INVALID_PORT:
        return "OC_STACK_INVALID_PORT";
    case OC_STACK_INVALID_CALLBACK:
        return "OC_STACK_INVALID_CALLBACK";
    case OC_STACK_INVALID_METHOD:
        return "OC_STACK_INVALID_METHOD";
    case OC_STACK_NO_MEMORY:
        return "OC_STACK_NO_MEMORY";
    case OC_STACK_COMM_ERROR:
        return "OC_STACK_COMM_ERROR";
    case OC_STACK_INVALID_PARAM:
        return "OC_STACK_INVALID_PARAM";
    case OC_STACK_NOTIMPL:
        return "OC_STACK_NOTIMPL";
    case OC_STACK_NO_RESOURCE:
        return "OC_STACK_NO_RESOURCE";
    case OC_STACK_RESOURCE_ERROR:
        return "OC_STACK_RESOURCE_ERROR";
    case OC_STACK_SLOW_RESOURCE:
        return "OC_STACK_SLOW_RESOURCE";
    case OC_STACK_NO_OBSERVERS:
        return "OC_STACK_NO_OBSERVERS";
    #ifdef WITH_PRESENCE
    case OC_STACK_VIRTUAL_DO_NOT_HANDLE:
        return "OC_STACK_VIRTUAL_DO_NOT_HANDLE";
    case OC_STACK_PRESENCE_STOPPED:
        return "OC_STACK_PRESENCE_STOPPED";
    #endif
    case OC_STACK_ERROR:
        return "OC_STACK_ERROR";
    default:
        return "UNKNOWN";
    }
}

std::string
getIPAddrTBServer(OCClientResponse * clientResponse) {
    if(!clientResponse) return "";
    if(!clientResponse->addr) return "";
    uint8_t a, b, c, d = 0;
    if(0 != OCDevAddrToIPv4Addr(clientResponse->addr, &a, &b, &c, &d) ) return "";

    char ipaddr[16] = {'\0'};
    snprintf(ipaddr,  sizeof(ipaddr), "%d.%d.%d.%d", a,b,c,d); // ostringstream not working correctly here, hence snprintf
    return std::string (ipaddr);
}

std::string
getPortTBServer(OCClientResponse * clientResponse) {
    if(!clientResponse) return "";
    if(!clientResponse->addr) return "";
    uint16_t p = 0;
    if(0 != OCDevAddrToPort(clientResponse->addr, &p) ) return "";
    std::ostringstream ss;
    ss << p;
    return ss.str();
}

void
parseClientResponse(OCClientResponse * clientResponse) {
    coapServerIP = getIPAddrTBServer(clientResponse);
    coapServerPort = getPortTBServer(clientResponse);
    coapServerResource = "/a/light";
}
