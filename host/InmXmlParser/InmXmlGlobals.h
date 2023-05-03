#ifndef INMXML__GLOBALS__H_
#define INMXML__GLOBALS__H_

#include <string>
#include <map>



//==========================================================
//     FUNCTION REQUEST XML FORMAT
//==========================================================
//<Request Id="Intel" Version="1.0">
//    <Header>
//		<Authentication>
//			<AccessKeyID>Intel</AccessKeyID>
//			<AccessSignature></AccessSignature>
//			<Parameter Name="AAA" Value="100"/>        
//		</Authentication>
//		<ParameterGroup Id="PG001">
//			<Parameter Name="BBB" Value="200"/>        
//		</ParameterGroup>
//    </Header>
//    <Body>
//		<FunctionRequest Name="ListVolumes" Id="XXXX" include="yes">
//			<ParameterGroup Id="PG002">
//				<Parameter Name="CCC" Value="300"/>        
//			</ParameterGroup>
//		</FunctionRequest>
//    </Body> 
//</Request>
//
//===========================================================
//	FUNCTION RESPONSE XML FORMAT
//===========================================================
//
//<Response Id="XXXXX" Version="1.0" Returncode=0 Message=”” >
//	<Body>  
//		<Function Name="ListVolumes" Id="X1" ReturnCode="0" Message="">
//			<FunctionRequest Name="ListVolumes">
//				<ParameterGroup Id="PG002">
//					<Parameter Name="CCC" Value="300"/>        
//					<Parameter Name="CCC" Value="300"/>        
//					<Parameter Name="CCC" Value="300"/>        
//				</ParameterGroup>
//			</FunctionRequest> 
//			<FunctionResponse> 
//				<ParameterGroup Name="" Id="" >
//					<Parameter Name="" Value="" />
//					</ParameterGroup>
//			</FunctionResponse> 		 
//		</Function>
//	</Body>
//</Response>
//
//==========================================================


#define XMLATTR		"xmlattr"
#define PARTERNAME  "Intel"
#define REQUESTTAG  "Request"
#define	HEADERTAG   "Header"
#define AUTHENTICATETAG "Authentication"
#define ACCESSKEYID "AccessKeyID"
#define ACCESSSIGNATURE "AccessSignature"

#define PARAMETERTAG	"Parameter"
#define PARAMETER_GROUPTAG "ParameterGroup"
#define FUNCTION_REQUEST_TAG "FunctionRequest"


#define NAME		"Name"
#define TYPE		"Type"
#define VALUE		"Value"
#define ID			"Id"
#define VERSION     "Version"
#define INCLUDE		"Include"
#define RETURNCODE	"ReturnCode"
#define MESSAGE		"Message"

#define HEADER_PATH "Request.Header"
#define AUTHENTICATE_PATH					"Request.Header.Authentication"
#define	AUTHENTICATE_ACCESSKEYID_PATH		"Request.Header.Authentication.AccessKeyID"
#define	AUTHENTICATE_ACCESSSIGNATURE_PATH	"Request.Header.Authentication.AccessSignature"
#define	AUTHENTICATE_PARAMETER_PATH			"Request.Header.Authentication.Parameter"

#define HEADER_PARAMETERGROUP_PATH			"Request.Header.ParameterGroup"
#define HEADER_PARAMETERGROUP_PARAMS_PATH	"Request.Header.ParameterGroup.Parameter"
#define HEADER_PARAMETER_PATH				"Request.Header.Parameter"

#define BODY "Body"
#define BODYPATH "Request.Body"
#define RESPONSE_BODY_PATH "Response.Body"



#define FUNCTION_REQUEST_PATH				   "Request.Body.FunctionRequest"
#define FUN_REQUEST_PAMETERGROUP_PATH          "Request.Body.FunctionRequest.ParameterGroup"
#define FUN_REQUEST_PAMETERGROUP_PARAMS_PATH   "Request.Body.FunctionRequest.ParameterGroup.Parameter"
#define FUN_REQUEST_PARAMETER_PATH             "Request.Body.FunctionRequest.Parameter"


//for function response

#define RESPONSE_TAG "Response"
#define FUNCTIONTAG "Function"
#define FUNCTION_PATH "Response.Body.Functon"
#define FUNCTION_RESPONSE_TAG "FunctionResponse"

#define MAIN_FUNCTION_IN_RESPONSE_PATH					"Response.Body.Function"
#define FUNREQUEST_IN_RESPONSE_PATH						"Response.Body.Function.FunctionRequest"
#define RESPONSE_FUNREQUEST_PARAMETERGROUP_PATH			"Response.Body.Function.FunctionRequest.ParameterGroup"
#define RESPONSE_FUNREQUEST_PARAMETERGROUP_PARAMS_PATH	"Response.Body.Function.FunctionRequest.ParameterGroup.Parameter"
#define RESPONSE_FUNREQUEST_PARAMETER_PATH              "Response.Body.Function.FunctionRequest.Parameter"

#define FUNCTION_RESPONSE_PATH							"Response.Body.Function.FunctionResponse"
#define FUN_RESPONSE_PARAMETERGROUP_PATH				"Response.Body.Function.FunctionResponse.ParameterGroup"
#define FUN_RESPONSE_PARAMETERGROUP_PARAMS_PATH			"Response.Body.Function.FunctionResponse.ParameterGroup.Parameter"
#define FUN_RESPONSE_PARAMETER_PATH						"Response.Body.Function.FunctionResponse.Parameter"




#endif /* INMXML__GLOBALS__H_ */