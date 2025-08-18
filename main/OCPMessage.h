#ifndef OCP_MESSAGE_H
#define OCP_MESSAGE_H

#include <stdio.h>

// message key version
#define OCPMESSAGE_KEY_VERSION						"version"
// message key msgType
#define OCPMESSAGE_KEY_MSGTYPE						"msgType"
// message key funcType
#define OCPMESSAGE_KEY_FUNCTYPE						"funcType"
// message key sId
#define OCPMESSAGE_KEY_SID							"sId"
// message key tpId
#define OCPMESSAGE_KEY_TPID							"tpId"
// message key tId
#define OCPMESSAGE_KEY_TID							"tId"
// message key msgCode
#define OCPMESSAGE_KEY_MSGCODE						"msgCode"
// message key msgId
#define OCPMESSAGE_KEY_MSGID						"msgId"
// message key msgDate
#define OCPMESSAGE_KEY_MSGDATE						"msgDate"		//size_t
// message key resCode
#define OCPMESSAGE_KEY_RESCODE						"resCode"
// message key resMsg
#define OCPMESSAGE_KEY_RESMSG						"resMsg"
// message key dataFormat
#define OCPMESSAGE_KEY_DATAFORMAT					"dataFormat"
// message key severity
#define OCPMESSAGE_KEY_SEVERITY						"severity"
// message key encType
#define OCPMESSAGE_KEY_ENCTYPE						"encType"
// message key authToken
#define OCPMESSAGE_KEY_AUTHTOKEN					"authToken"
// message key data
#define OCPMESSAGE_KEY_DATA							"data"			//char*
// message len version 
#define OCPMESSAGE_LEN_VERSION 						16
// message len msgType 
#define OCPMESSAGE_LEN_MSGTYPE						8
// message len funcType 
#define OCPMESSAGE_LEN_FUNCTYPE						8
// message len sId 
#define OCPMESSAGE_LEN_SID							64
// message len tpId 
#define OCPMESSAGE_LEN_TPID							256
// message len tId 
#define OCPMESSAGE_LEN_TID							256
// message len msgCode 
#define OCPMESSAGE_LEN_MSGCODE						24
// message len msgId 
#define OCPMESSAGE_LEN_MSGID						64
// message len resCode 
#define OCPMESSAGE_LEN_RESCODE						8
// message len resMsg 
#define OCPMESSAGE_LEN_RESMSG						1024
// message len dataFormat 
#define OCPMESSAGE_LEN_DATAFORMAT					64
// message len severity 
#define OCPMESSAGE_LEN_SEVERITY						4
// message len encType 
#define OCPMESSAGE_LEN_ENCTYPE						8
// message len authToken 
#define OCPMESSAGE_LEN_AUTHTOKEN					128			//64
// message 의 resCode 가 OK 인 값
#define OCPMESSAGE_RES_OK							"200"
// msgType Q
#define OCPMESSAGE_MSGTYPE_REQUEST 					"Q"
// msgType A
#define OCPMESSAGE_MSGTYPE_ANSWER 					"A"
// msgType N
#define OCPMESSAGE_MSGTYPE_NOTIFY 					"N"
// Auth 메시지의 prefix
#define OCPMESSAGE_MSGCODE_AUTH 							"MSGAUTH"
// msgCode preauthorize Q
#define OCPMESSAGE_MSGCODE_PRESAUTHPROCESS_REQ 				"MSGAUTH00000"
// msgCode preauthorize A
#define OCPMESSAGE_MSGCODE_PRESAUTHPROCESS_RES 				"MSGAUTH00001"
// msgCode authorize Q
#define OCPMESSAGE_MSGCODE_AUTHPROCESS_REQ 					"MSGAUTH00002"
// msgCode authorize A
#define OCPMESSAGE_MSGCODE_AUTHPROCESS_RES 					"MSGAUTH00003"
// msgCode heartbeat Q
#define OCPMESSAGE_MSGCODE_SEND_HEARTBEAT_REQ 				"MSGBA0030001"
// msgCode heartbeat A
#define OCPMESSAGE_MSGCODE_SEND_HEARTBEAT_RES 				"MSGBA0030002"
// msgCode activate rootthing Q
#define OCPMESSAGE_MSGCODE_ACTIVATE_ROOTTHING_REQ 			"MSGBA0110001"
// msgCode activate rootthing A
#define OCPMESSAGE_MSGCODE_ACTIVATE_ROOTTHING_RES 			"MSGBA0110002"
// msgCode register edgething Q
#define OCPMESSAGE_MSGCODE_REGISTER_EDGETHING_REQ 			"MSGBA0110003"
// msgCode register edgething A
#define OCPMESSAGE_MSGCODE_REGISTER_EDGETHING_RES 			"MSGBA0110004"
// msgCode inactive thing Q
#define OCPMESSAGE_MSGCODE_INACTIVATE_THING_REQ 			"MSGBA0120001"
// msgCode inactive thing A
#define OCPMESSAGE_MSGCODE_INACTIVATE_THING_RES 			"MSGBA0120002"
// msgCode bulk insert Q
#define OCPMESSAGE_MSGCODE_BULK_INSERT_REQ 					"MSGBA0210011"
// msgCode bulk insert A
#define OCPMESSAGE_MSGCODE_BULK_INSERT_RES 					"MSGBA0210012"
// msgCode request active firmware Q
#define OCPMESSAGE_MSGCODE_REQUEST_ACTIVEFIRMWARE_REQ 		"MSGBA0420001" 
// msgCode request active firmware A
#define OCPMESSAGE_MSGCODE_REQUEST_ACTIVEFIRMWARE_RES 		"MSGBA0420002"
// msgCode noti passive firmware Q
#define OCPMESSAGE_MSGCODE_NOTI_PASSIVEFIRMWARE_REQ 		"MSGBA0420003" 
// msgCode noti passive firmware A
#define OCPMESSAGE_MSGCODE_NOTI_PASSIVEFIRMWARE_RES 		"MSGBA0420004"
// msgCode firmware result Q
#define OCPMESSAGE_MSGCODE_SEND_FIRMWARERESULT_REQ 			"MSGBA0420005"
// msgCode firmware result A
#define OCPMESSAGE_MSGCODE_SEND_FIRMWARERESULT_RES 			"MSGBA0420006"
// msgCode signed firmware Q
#define OCPMESSAGE_MSGCODE_GET_SIGNEDFIRMWARE_REQ 			"MSGBA0420007"
// msgCode signed firmware A
#define OCPMESSAGE_MSGCODE_GET_SIGNEDFIRMWARE_RES 			"MSGBA0420008"
// msgCode noti request Q
#define OCPMESSAGE_MSGCODE_NOTIREQUEST_REQ 					"MSGBA0300001"
// msgCode noti request A
#define OCPMESSAGE_MSGCODE_NOTIREQUEST_RES 					"MSGBA0300002"
// request wbc mc value
#define OCPMESSAGE_MSGCODE_WBC_MC_REQ						"MSGBA0110017"
// response to get wbc mc value
#define OCPMESSAGE_MSGCODE_WBC_MC_RES						"MSGBA0110018"
// 사물의 속성정보 전송 메시지
#define OCPMESSAGE_MSGCODE_BASIC_ATTR_GROUP					"Basic-AttrGroup"
// 사물의 basic provisioning 메시지
#define OCPMESSAGE_MSGCODE_BASIC_PROVISIONING				"Basic-Provisioning"
// Basic-AttrGroup 메시지의 response
#define OCPMESSAGE_MSGCODE_REGISTER_THING_ATTRIBUTE_RES		"MSGBA0210002"
// 파일 upload uri 요청 request
#define OCPMESSAGE_MSGCODE_REQUEST_UPLOADURI_REQ			"MSGBA0410001"
// 파일 upload uri 요청 request 에 대한 응답
#define OCPMESSAGE_MSGCODE_REQUEST_UPLOADURI_RES			"MSGBA0410002"
// 파일업로드 결과 request
#define OCPMESSAGE_MSGCODE_SEND_UPLOADRESULT_REQ			"MSGBA0410017"
// 파일업로드 결과 request 에 대한 응답
#define OCPMESSAGE_MSGCODE_SEND_UPLOADRESULT_RES			"MSGBA0410018"

//ocpmessage struct
typedef struct
{
	char version[OCPMESSAGE_LEN_VERSION];
	char msgType[OCPMESSAGE_LEN_MSGTYPE];
	char funcType[OCPMESSAGE_LEN_FUNCTYPE];
	char siteId[OCPMESSAGE_LEN_SID];
	char thingName[OCPMESSAGE_LEN_TPID];
	char tId[OCPMESSAGE_LEN_TID];
	char msgCode[OCPMESSAGE_LEN_MSGCODE];
	char msgId[OCPMESSAGE_LEN_MSGID];
	unsigned long long msgDate;
	char resCode[OCPMESSAGE_LEN_RESCODE];
	char resMsg[OCPMESSAGE_LEN_RESMSG];
	char dataFormat[OCPMESSAGE_LEN_DATAFORMAT];
	char severity[OCPMESSAGE_LEN_SEVERITY];
	char encType[OCPMESSAGE_LEN_ENCTYPE];
	char authToken[OCPMESSAGE_LEN_AUTHTOKEN];
	unsigned long long datalen;
	void* data;
}OCPMessage;
//ocpmessage initiailizer
#define OCPMESSAGE_INITIALIZER { {0,}, {0,}, {0,}, {0,}, {0,}, {0,}, {0,}, {0,}, 0 , {0,}, {0,}, {0,}, {0,}, {0,}, {0,}, 0, NULL}

#endif