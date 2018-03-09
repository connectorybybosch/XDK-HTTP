// Copyright © Robert Bosch México - Daniel Reyes Sánchez
// HttpExample XDK Project

#include "BSP_BoardType.h"
#include "BCDS_BSP_LED.h"
#include "BCDS_BSP_Button.h"


#include <stdio.h>
#include "FreeRTOS.h"
#include "timers.h"
#include "HttpExample.h"
#include "BCDS_CmdProcessor.h"
#include "BCDS_Assert.h"
// For WLAN
#include "BCDS_WlanConnect.h"
#include "BCDS_NetworkConfig.h"
#include "PAL_initialize_ih.h"
#include "PAL_socketMonitor_ih.h"
#include "PIp.h"
// For HTTP Requests
#include <Serval_HttpClient.h>
#include <Serval_Network.h>
// For HTTP Requests via REST
#include <Serval_RestClient.h>







static void onRESTRequestSent(Msg_T *msg_ptr, retcode_t status){
    (void)msg_ptr;
    if (status != RC_OK){
        printf("Failed to send REST request! %i\r\n", status);
    }
}


static retcode_t onRESTResponseReceived(RestSession_T *restSession,
                                        Msg_T *msg_ptr, retcode_t status){
    (void)restSession;
    if (status == RC_OK && msg_ptr != NULL){
        Http_StatusCode_T statusCode = HttpMsg_getStatusCode(msg_ptr);
        char const *contentType = HttpMsg_getContentType(msg_ptr);
        char const *content_ptr;
        unsigned int contentLength = 0;
        HttpMsg_getContent(msg_ptr, &content_ptr, &contentLength);
        char content[contentLength + 1];
        strncpy(content, content_ptr, contentLength);
        content[contentLength] = 0;
        printf("HTTP RESPONSE: %d [%s]\r\n", statusCode, contentType);
        printf("%s\r\n", content);
    }else{
        printf("Failed to receive REST response!\r\n");
    }
    return (RC_OK);
}

retcode_t serializeMyHeaders(OutMsgSerializationHandover_T *handover){

    handover->len = 0;
    static const char *header1 = "Content-Type: application/json\r\n";
    static const char *header2 = "Content-Length: 18\r\n";
    TcpMsg_copyStaticContent(handover, header1, strlen(header1));
    handover->position = 1;
    TcpMsg_copyStaticContent(handover, header2, strlen(header2));
    handover->position = 2;
    return RC_OK;
}

static void performPostRequest(){

	Ip_Address_T destAddr;
	Msg_T *msg_ptr;
	RestClient_initialize();
	HttpMsg_serializeCustomHeaders(msg_ptr, serializeMyHeaders);
	Ip_convertOctetsToAddr(172, 17, 30, 154, &destAddr); //172.17.30.64
	Ip_Port_T port = Ip_convertIntToPort(DEST_SERVER_PORT);
	Rest_ContentFormat_T acceptedFormats[1] = {REST_CONTENT_FMT_JSON};
	RestClient_ReqInfo_T postRequest;
	postRequest.method = REST_POST;
	postRequest.uriPath_ptr = DEST_POST_PATH;
	postRequest.uriQuery_ptr = "";
	postRequest.acceptBuffer_ptr = acceptedFormats;
	postRequest.numAccept = 1;
	postRequest.payloadLen = 16; //size of the payload -> ONE BYTE PER CHARACTER
  postRequest.payload_ptr = (uint8_t*) "{\"status\": true}"; // 16 characters
	postRequest.contentFormat = REST_CONTENT_FMT_JSON;
	postRequest.rangeLength = 0;
	postRequest.host = NULL;
	RestClient_initReqMsg(&destAddr, port, &msg_ptr, REST_CLIENT_PROTOCOL_HTTP);
	RestClient_fillReqMsg(msg_ptr, &postRequest);
	RestClient_request(msg_ptr, &onRESTRequestSent, &onRESTResponseReceived);
}







// WLAN
static retcode_t WlanConnect(void){



    NetworkConfig_IpSettings_T myIpSettings;
    char ipAddress[PAL_IP_ADDRESS_SIZE] = { 0 };
    Ip_Address_T* IpaddressHex = Ip_getMyIpAddr();
    WlanConnect_SSID_T connectSSID;
    WlanConnect_PassPhrase_T connectPassPhrase;
    Retcode_T ReturnValue = (Retcode_T) RETCODE_FAILURE;
    int32_t Result = INT32_C(-1);

    if (RETCODE_OK != WlanConnect_Init()){
        return (RC_PLATFORM_ERROR);
    }

    printf("Connecting to WiFi network: %s  \n\r", WLAN_CONNECT_WPA_SSID);
    connectSSID = (WlanConnect_SSID_T) WLAN_CONNECT_WPA_SSID;
    connectPassPhrase = (WlanConnect_PassPhrase_T) WLAN_CONNECT_WPA_PASS;
    ReturnValue = NetworkConfig_SetIpDhcp(NULL);
    if (RETCODE_OK != ReturnValue){
        printf("Error in setting IP to DHCP  \n\r");
        return (RC_PLATFORM_ERROR);
    }
    if (RETCODE_OK == WlanConnect_WPA(connectSSID, connectPassPhrase, NULL)){
        ReturnValue = NetworkConfig_GetIpSettings(&myIpSettings);
        if (RETCODE_OK == ReturnValue){
            *IpaddressHex = Basics_htonl(myIpSettings.ipV4);
            Result = Ip_convertAddrToString(IpaddressHex, ipAddress);
            if (Result < 0){
                printf("Couldn't convert the IP address to string format  \n\r");
                return (RC_PLATFORM_ERROR);
            }
            printf("Connected to WPA network successfully  \n\r");
            printf(" Ip address of the device %s  \n\r", ipAddress);
            return (RC_OK);
        }else{
            printf("Error in getting IP settings  \n\r");
            return (RC_PLATFORM_ERROR);
        }
    }else{
        return (RC_PLATFORM_ERROR);
    }

}





static void setupWlan(void) {
	retcode_t rc = RC_OK;
	rc = WlanConnect();
	if (RC_OK != rc){ printf("Network init/connection failed %i  \n\r", rc); return; }
	rc = PAL_initialize();
	if (RC_OK != rc){ printf("PAL and network initialize %i \r\n", rc); return; }
	PAL_socketMonitorInit();
	rc = HttpClient_initialize();
	if (rc != RC_OK){ printf("Failed to initialize http client \r\n "); return; }
}







// * - Main Function
void appInitSystem(void * CmdProcessorHandle, uint32_t param2)
{
    if (CmdProcessorHandle == NULL)
    {
        printf("Command processor handle is null \n\r");
        assert(false);
    }
    BCDS_UNUSED(param2);


    setupWlan();
    performPostRequest();
}
