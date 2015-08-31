#include <stdio.h>
#include <string.h>
#include <string>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <array>
#include "ocstack.h"
#include "logger.h"

typedef struct LIGHTRESOURCE {
    OCResourceHandle 	handle;
    bool 				state;
    int 				power;
} LightResource;

char *gResourceUri				= (char *)"/a/light";
const char *contentType 		= "myContentType";
const char *dateOfManufacture 	= "myDateOfManufacture";
const char *deviceName 			= "myDeviceName";
const char *deviceUUID 			= "myDeviceUUID";
const char *firmwareVersion 	= "myFirmwareVersion";
const char *hostName 			= "myHostName";
const char *manufacturerName 	= "myManufacturerNa";
const char *manufacturerUrl 	= "myManufacturerUrl";
const char *modelNumber 		= "myModelNumber";
const char *platformVersion 	= "myPlatformVersion";
const char *supportUrl 			= "mySupportUrl";
const char *version 			= "myVersion";

static uint16_t OC_WELL_KNOWN_PORT = 5683;
int gQuitFlag = 0;

OCDeviceInfo deviceInfo;
static LightResource Light;

OCEntityHandlerResult OCDeviceEntityHandlerCb (OCEntityHandlerFlag flag, OCEntityHandlerRequest *entityHandlerRequest, char* uri);
void DeleteDeviceInfo();
bool DuplicateString(char** targetString, const char* sourceString);
OCStackResult SetDeviceInfo(const char *contentType, const char *dateOfManufacture,
        const char *deviceName, const char *deviceUUID, const char *firmwareVersion,
        const char *hostName, const char *manufacturerName, const char *manufacturerUrl,
        const char *modelNumber, const char *platformVersion, const char *supportUrl,
        const char *version);

OCEntityHandlerResult
OCDeviceEntityHandlerCb (OCEntityHandlerFlag flag,
        OCEntityHandlerRequest *entityHandlerRequest, char* uri) {
	OCEntityHandlerResult ehResult = OC_EH_OK;
    printf ("Inside device default entity handler - flags: 0x%x, uri: %s\n", flag, uri);

    return ehResult;
}

OCEntityHandlerResult
OCEntityHandlerCb (OCEntityHandlerFlag flag,
        OCEntityHandlerRequest *entityHandlerRequest) {
    printf ("Inside entity handler - flags: 0x%x\n", flag);

	OCEntityHandlerResult ehResult = OC_EH_OK;
    OCEntityHandlerResponse response;
    // char payload[MAX_RESPONSE_LENGTH] = {0};
    std::string payload;

    // Validate pointer
    if (!entityHandlerRequest) {
        printf ("Invalid request pointer\n");
        return OC_EH_ERROR;
    }

    // Initialize certain response fields
    response.numSendVendorSpecificHeaderOptions = 0;
    memset(response.sendVendorSpecificHeaderOptions, 0, sizeof response.sendVendorSpecificHeaderOptions);
    memset(response.resourceUri, 0, sizeof response.resourceUri);

    if (flag & OC_INIT_FLAG) {
        printf ("Flag includes OC_INIT_FLAG\n");
    }

    if (flag & OC_REQUEST_FLAG) {
        printf ("Flag includes OC_REQUEST_FLAG\n");
        /*
         * GET REQUEST HANDLER
         */
        if (OC_REST_GET == entityHandlerRequest->method) {
            payload = "{\"href\":\"/a/light\",\"rep\":{\"state\":\"off\",\"power\":5}}";
            printf ("Received OC_REST_GET from client (%s)\n", payload.c_str());
        }
        /*
         * PUT REQUEST HANDLER
         */
        if (OC_REST_PUT == entityHandlerRequest->method) {
            printf ("Received OC_REST_PUT from client (%s)\n", payload.c_str());
        }
        /*
         * GENRATE RESPONE MESSAGE TO THE CLIENT
         */
        if (!((ehResult == OC_EH_ERROR) || (ehResult == OC_EH_FORBIDDEN))) {
            printf ("Sendig RESPONSE\n");
            // Format the response.  Note this requires some info about the request
            response.requestHandle 			= entityHandlerRequest->requestHandle;
            response.resourceHandle 		= entityHandlerRequest->resource;
            response.ehResult 				= ehResult;
            response.payload 				= (unsigned char *)payload.c_str();
            response.payloadSize 			= strlen(payload.c_str());
            // Indicate that response is NOT in a persistent buffer
            response.persistentBufferFlag 	= 0;

            // Send the response
            if (OCDoResponse(&response) != OC_STACK_OK) {
                printf ("Error sending response\n");
                ehResult = OC_EH_ERROR;
            }
        }
    }

	return ehResult;
}

int 
createLightResource (char *uri, LightResource *lightResource) {
    if (!uri) {
        printf ("Resource URI cannot be NULL\n");
        return -1;
    }

    lightResource->state 	= false;
    lightResource->power	= 0;
    OCStackResult res = OCCreateResource(&(lightResource->handle),
            "core.light",
            "oc.mi.def",
            uri,
            OCEntityHandlerCb,
            OC_DISCOVERABLE|OC_OBSERVABLE);

    return 0;
}

void 
handleSigInt(int signum) {
    if (signum == SIGINT) {
        gQuitFlag = 1;
        printf ("OCServer is closing...\n");
    }
}

int 
main(int argc, char** argv) {
    uint8_t 	addr[20] = {0};
    uint8_t* 	paddr = NULL;
    uint16_t 	port = OC_WELL_KNOWN_PORT;
    uint8_t 	ifname[] = "wlan0";
    pthread_t 	threadId;
    pthread_t 	threadId_presence;
    int 		opt;

    printf ("OCServer is starting...\n");
    if (OCGetInterfaceAddress(ifname, sizeof(ifname), AF_INET, addr,
                sizeof(addr)) == ERR_SUCCESS) {
        printf ("Starting ocserver on address %s:%d\n",addr,port);
        paddr = addr;
    }
	/*
     * Init IoTivity as a SERVER.
     */
    if (OCInit((char *) paddr, port, OC_SERVER) != OC_STACK_OK) {
        printf ("OCStack init error\n");
        return 0;
    }

    OCSetDefaultDeviceEntityHandler(OCDeviceEntityHandlerCb);
    OCStackResult deviceResult = SetDeviceInfo(contentType, dateOfManufacture, deviceName,
            deviceUUID, firmwareVersion, hostName, manufacturerName,
            manufacturerUrl, modelNumber, platformVersion, supportUrl, version);
    deviceResult = OCSetDeviceInfo(deviceInfo);
    if (deviceResult != OC_STACK_OK) {
        printf ("Device Registration failed!\n");
        exit (EXIT_FAILURE);
    }
	DeleteDeviceInfo();

	/*
     * Create ONE light resource.
     */
	createLightResource(gResourceUri, &Light);

	printf ("Entering ocserver main loop...\n");
    signal(SIGINT, handleSigInt);
    while (!gQuitFlag) {
		/*
		 * Main IoTivity function for processing.
		 */
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

void 
DeleteDeviceInfo()
{
    free(deviceInfo.contentType);
    free(deviceInfo.dateOfManufacture);
    free(deviceInfo.deviceName);
    free(deviceInfo.deviceUUID);
    free(deviceInfo.firmwareVersion);
    free(deviceInfo.hostName);
    free(deviceInfo.manufacturerName);
    free(deviceInfo.manufacturerUrl);
    free(deviceInfo.modelNumber);
    free(deviceInfo.platformVersion);
    free(deviceInfo.supportUrl);
    free(deviceInfo.version);
}

bool 
DuplicateString(char** targetString, const char* sourceString)
{
    if(!sourceString)
    {
        return false;
    }
    else
    {
        *targetString = (char *) malloc(strlen(sourceString) + 1);

        if(targetString)
        {
            strncpy(*targetString, sourceString, (strlen(sourceString) + 1));
            return true;
        }
    }
    return false;
}

OCStackResult 
SetDeviceInfo(const char *contentType, const char *dateOfManufacture,
        const char *deviceName, const char *deviceUUID, const char *firmwareVersion,
        const char *hostName, const char *manufacturerName, const char *manufacturerUrl,
        const char *modelNumber, const char *platformVersion, const char *supportUrl,
        const char *version)
{

    bool success = true;

    if(manufacturerName != NULL && (strlen(manufacturerName) > MAX_MANUFACTURER_NAME_LENGTH))
    {
        return OC_STACK_INVALID_PARAM;
    }

    if(manufacturerUrl != NULL && (strlen(manufacturerUrl) > MAX_MANUFACTURER_URL_LENGTH))
    {
        return OC_STACK_INVALID_PARAM;
    }

    if(!DuplicateString(&deviceInfo.contentType, contentType))
    {
        success = false;
    }

    if(!DuplicateString(&deviceInfo.dateOfManufacture, dateOfManufacture))
    {
        success = false;
    }

    if(!DuplicateString(&deviceInfo.deviceName, deviceName))
    {
        success = false;
    }

    if(!DuplicateString(&deviceInfo.deviceUUID, deviceUUID))
    {
        success = false;
    }

    if(!DuplicateString(&deviceInfo.firmwareVersion, firmwareVersion))
    {
        success = false;
    }

    if(!DuplicateString(&deviceInfo.hostName, hostName))
    {
        success = false;
    }

    if(!DuplicateString(&deviceInfo.manufacturerName, manufacturerName))
    {
        success = false;
    }

    if(!DuplicateString(&deviceInfo.manufacturerUrl, manufacturerUrl))
    {
        success = false;
    }

    if(!DuplicateString(&deviceInfo.modelNumber, modelNumber))
    {
        success = false;
    }

    if(!DuplicateString(&deviceInfo.platformVersion, platformVersion))
    {
        success = false;
    }

    if(!DuplicateString(&deviceInfo.supportUrl, supportUrl))
    {
        success = false;
    }

    if(!DuplicateString(&deviceInfo.version, version))
    {
        success = false;
    }

    if(success)
    {
        return OC_STACK_OK;
    }

    DeleteDeviceInfo();
    return OC_STACK_ERROR;
}
