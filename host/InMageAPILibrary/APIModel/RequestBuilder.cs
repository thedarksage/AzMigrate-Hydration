using System;
using System.Collections.Generic;
using System.Security.Cryptography;
using System.Collections;
using System.IO;
using System.Text;
using System.Xml;
using HttpCommunication;

namespace InMage.APIModel
{

    /*
     * Class: RequestBuild - A Helper class builds API Request Object
     */
    public class RequestBuilder
    {
        private CXSignature signature;

        /*
         *  RequestBuilder
         */
        public RequestBuilder() { }

        public RequestBuilder(String accesskeyid, String passphrase, String version)
        {
            signature = new CXSignature(accesskeyid, passphrase, version);            
        }

        public RequestBuilder(String accesskeyid, String passphrase, String version, String fingerPrint)
        {
            signature = new CXSignature(accesskeyid, passphrase, version, fingerPrint);            
        }

        /*
         *  Create Request Object for multiple function calls 
         */
        public Request createRequestObject(String requestId, ArrayList functionRequestObjects)
        {
            Authentication authentication = new Authentication();
            authentication.AccessKeyID = signature.AccessKeyId;
            authentication.AuthMethod = AuthMethod.MessageAuth;
            string[] functionNames = new string[functionRequestObjects.Count];
            int index = 0;
            foreach (FunctionRequest functionRequest in functionRequestObjects)
            {
                functionNames[index] = functionRequest.Name;
                ++index;
            }
            authentication.AccessSignature = signature.ComputeSignatureMessageAuth(requestId, functionNames);
            return createRequestObject(requestId, functionRequestObjects, authentication);
        }

        /*
         *  Create Request Object for multiple function calls
         */
        public Request createRequestObject(String requestId, AuthMethod authMethod, ArrayList functionRequestObjects, String userName, String pwd)
        {
            Authentication authentication = new Authentication();
            authentication.AccessKeyID = signature.AccessKeyId;
            authentication.AuthMethod = authMethod;
            authentication.UserName = userName;
            authentication.Password = CXSignature.CreateSHA256(pwd);
            return createRequestObject(requestId, functionRequestObjects, authentication);
        }

        /*
         *  Create Request Object for multiple Function Calls 
         */
        public Request createRequestObject(String requestId, AuthMethod authMethod, ArrayList functionRequestObjects, Uri uri, String requestMethod, String authMethodVersion)
        {
            Authentication authentication = new Authentication();
            authentication.AccessKeyID = signature.AccessKeyId;
            authentication.AuthMethod = authMethod;
            authentication.AuthMethodVersion = authMethodVersion;
            string[] functionNames = new string[functionRequestObjects.Count];
            int index = 0;
            foreach(FunctionRequest functionRequest in functionRequestObjects)
            {
                functionNames[index] = functionRequest.Name;
                ++index;
            }
            authentication.AccessSignature = signature.ComputeSignatureComponentAuth(requestId, functionNames, uri, requestMethod);
            return createRequestObject(requestId, functionRequestObjects, authentication);
        }

        private Request createRequestObject(String requestId, ArrayList functionRequestObjects, Authentication authentication)
        {
            // Construct the Request
            Request request = new Request();
            request.Id = requestId;
            request.Version = signature.Version;

            // Construct the Header and add it to Request Object
            Header header = new Header();
            header.Authentication = authentication;
            request.Header = header;

            // Construct the Body and add it to body object
            Body body = new Body();
            foreach (FunctionRequest funcRequests in functionRequestObjects)
            {
                body.addChildObj(funcRequests);
            }
            request.Body = body;
            return request;
        }

        /*
         *  Create RequestXML
         */
        public String createRequestXML(Request req)
        {
            return req.ToXML();
        }


        /*
        * Builds Request Objects from Request XML document 
        */
        public Request build(String requesteXML)
        {
            Request request = null;
            using (TextReader tr = new StringReader(requesteXML))
            {
                using (XmlReader reader = XmlReader.Create(tr))
                {
                    // Build Child Objects
                    while (reader.Read())
                    {
                        switch (reader.NodeType)
                        {
                            case XmlNodeType.Element:
                                if (String.Compare(reader.Name, "Request", true) == 0)
                                {
                                    request = buildRequest(reader);
                                }
                                break;
                            case XmlNodeType.Text:
                                break;
                            case XmlNodeType.EndElement:
                                break;
                            case XmlNodeType.Attribute:
                                break;
                        }
                    }
                }
            }
            return request;
        }

        /*
         * Builds Request Object
         */
        private Request buildRequest(XmlReader reader)
        {
            Request request = new Request();
            //Assign Attributes
            while (reader.MoveToNextAttribute())
            {
                if (String.Compare(reader.Name, "Id", true) == 0)
                {
                    request.Id = reader.Value;
                }
                else if (String.Compare(reader.Name, "version", true) == 0)
                {
                    request.Version = reader.Value;
                }
            }

            // Build Child Objects
            while (reader.Read())
            {
                switch (reader.NodeType)
                {
                    case XmlNodeType.Element:
                        if (String.Compare(reader.Name, "Header", true) == 0)
                        {
                            Header header = buildHeader(reader);
                            request.Header = header;
                        }
                        else if (String.Compare(reader.Name, "Body", true) == 0)
                        {
                            Body body = buildBody(reader);
                            request.Body = body;
                        }
                        break;
                    default:
                        break;
                }
            }

            return request;
        }

        /*
         * Builds Header Obect 
         */
        private Header buildHeader(XmlReader reader)
        {
            Header header = new Header();

            // Build Child Elements
            bool cond = true;
            while (reader.Read())
            {
                switch (reader.NodeType)
                {
                    case XmlNodeType.Element:
                        if (String.Compare(reader.Name, "Authentication", true) == 0)
                        {
                            Authentication authentication = buildAuthentication(reader);
                            header.Authentication = authentication;
                        }
                        break;
                    case XmlNodeType.EndElement:
                        if (String.Compare(reader.Name, "Header", true) == 0)
                        {
                            cond = false;
                        }
                        break;
                    default:
                        break;
                }
                if (!cond) break;
            }
            return header;
        }

        /*
         * Builds Authentication Obect 
         */
        private Authentication buildAuthentication(XmlReader reader)
        {
            Authentication authentication = new Authentication();

            // Build Child Elements
            bool cond = true;
            while (reader.Read())
            {
                switch (reader.NodeType)
                {
                    case XmlNodeType.Element:
                        if (String.Compare(reader.Name, "AccessKeyID", true) == 0)
                        {
                            authentication.AccessKeyID = getAccessKeyID(reader);
                        }
                        else if (String.Compare(reader.Name, "AccessSignature", true) == 0)
                        {
                            authentication.AccessSignature = getAccessSignature(reader);
                        }
                        break;
                    case XmlNodeType.EndElement:
                        if (String.Compare(reader.Name, "Authentication", true) == 0)
                        {
                            cond = false;
                        }
                        break;
                    default:
                        break;
                }
                if (!cond) break;
            }
            return authentication;
        }

        /*
         * Returns AccessKeyID 
         */
        private string getAccessKeyID(XmlReader reader)
        {
            string accessKeyID = "";

            // Build Child Elements
            bool cond = true;
            while (reader.Read())
            {
                switch (reader.NodeType)
                {
                    case XmlNodeType.Text:
                        accessKeyID = reader.Value;
                        break;
                    case XmlNodeType.EndElement:
                        if (String.Compare(reader.Name, "AccessKeyID", true) == 0)
                        {
                            cond = false;
                        }
                        break;
                    default:
                        break;
                }
                if (!cond) break;
            }
            return accessKeyID;
        }

        /*
         * Returns AccessSignature 
         */
        private string getAccessSignature(XmlReader reader)
        {
            string accessSignature = "";

            // Build Child Elements
            bool cond = true;
            while (reader.Read())
            {
                switch (reader.NodeType)
                {
                    case XmlNodeType.Text:
                        accessSignature = reader.Value;
                        break;
                    case XmlNodeType.EndElement:
                        if (String.Compare(reader.Name, "AccessSignature", true) == 0)
                        {
                            cond = false;
                        }
                        break;
                    default:
                        break;
                }
                if (!cond) break;
            }
            return accessSignature;
        }

        /*
         * Builds Body Obect 
         */
        private Body buildBody(XmlReader reader)
        {
            Body body = new Body();

            // Build Child Elements
            bool cond = true;
            while (reader.Read())
            {
                switch (reader.NodeType)
                {
                    case XmlNodeType.Element:
                        if (String.Compare(reader.Name, "FunctionRequest", true) == 0)
                        {
                            FunctionRequest functionRequest = buildFunctionRequest(reader);
                            body.addChildObj(functionRequest);
                        }
                        break;
                    case XmlNodeType.EndElement:
                        if (String.Compare(reader.Name, "Body", true) == 0)
                        {
                            cond = false;
                        }
                        break;

                    default:
                        break;
                }
                if (!cond) break;
            }
            return body;
        }

        /*
        * Build Function Request Object 
        */
        private FunctionRequest buildFunctionRequest(XmlReader reader)
        {
            FunctionRequest functionRequest = new FunctionRequest();

            // Parse Attribute
            while (reader.MoveToNextAttribute())
            {
                if (String.Compare(reader.Name, "Name", true) == 0)
                {
                    functionRequest.Name = reader.Value;
                }
                else if (String.Compare(reader.Name, "Id", true) == 0)
                {
                    functionRequest.Id = reader.Value;
                }
                else if (String.Compare(reader.Name, "Include", true) == 0)
                {
                    if (String.Compare(reader.Name, "Y", true) == 0 || String.Compare(reader.Name, "Yes", true) == 0 || String.Compare(reader.Name, "true", true) == 0)
                    {
                        functionRequest.Include = true;
                    }
                }
            }

            // Build Child Elements
            bool cond = true;
            while (reader.Read())
            {
                switch (reader.NodeType)
                {
                    case XmlNodeType.Element:
                        if (String.Compare(reader.Name, "ParameterGroup", true) == 0)
                        {
                            ParameterGroup parameterGroupChild = buildParameterGroup(reader);
                            functionRequest.addChildObj(parameterGroupChild);
                        }
                        else if (String.Compare(reader.Name, "Parameter", true) == 0)
                        {
                            Parameter parameterChild = buildParameter(reader);
                            functionRequest.addChildObj(parameterChild);
                        }
                        break;
                    case XmlNodeType.EndElement:
                        if (String.Compare(reader.Name, "FunctionRequest", true) == 0)
                        {
                            cond = false;
                        }
                        break;

                    default:
                        break;
                }

                if (!cond) break;
            }
            return functionRequest;
        }

        /*
         *  Builds ParameterGroup Object
         */
        private ParameterGroup buildParameterGroup(XmlReader reader)
        {
            ParameterGroup parameterGroup = new ParameterGroup();
            bool isEmptyElement = reader.IsEmptyElement;

            // Assign Attributes
            while (reader.MoveToNextAttribute())
            {
                if (String.Compare(reader.Name, "Id", true) == 0)
                {
                    parameterGroup.Id = reader.Value;
                }
            }
            if (!isEmptyElement)
            {
                // Build Child Elements
                bool cond = true;
                while (reader.Read())
                {
                    switch (reader.NodeType)
                    {
                        case XmlNodeType.Element:

                            if (String.Compare(reader.Name, "ParameterGroup", true) == 0)
                            {
                                ParameterGroup parameterGroupChild = buildParameterGroup(reader);
                                parameterGroup.addChildObj(parameterGroupChild);

                            }
                            else if (String.Compare(reader.Name, "Parameter", true) == 0)
                            {
                                Parameter parameterChild = buildParameter(reader);
                                parameterGroup.addChildObj(parameterChild);
                            }
                            break;

                        case XmlNodeType.EndElement:
                            if (String.Compare(reader.Name, "ParameterGroup", true) == 0)
                            {
                                cond = false;
                            }
                            break;

                        default:
                            break;
                    }
                    if (!cond) break;
                }
            }
            return parameterGroup;
        }

        /*
         * Builds Parameter Object
         */
        private Parameter buildParameter(XmlReader reader)
        {
            Parameter parameter = new Parameter();

            // Assign Attributes
            while (reader.MoveToNextAttribute())
            {
                if (String.Compare(reader.Name, "Name", true) == 0)
                {
                    parameter.Name = reader.Value;
                }
                else if (String.Compare(reader.Name, "Value", true) == 0)
                {
                    parameter.Value = reader.Value;
                }
            }

            return parameter;
        }


    }
}
