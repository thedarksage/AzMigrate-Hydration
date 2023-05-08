using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Xml;
using System.IO;

namespace InMage.APIModel
{

    /*
     *  Class: ResonseBuilder - constructs Response Objec hierarchy based on the responseXML
     */
   public class ResponseBuilder
    {
        /*
         * Create ResponseXML
         */
        public String createResponseXML(Response response)
        {
            return response.ToXML();
        }

        /*
         * Builds Response Objects from Response XML document 
         */
        public Response build(String responseXML)
        {
            Response response = null;
            using (TextReader tr = new StringReader(responseXML))
            {
                using (XmlReader reader = XmlReader.Create(tr))
                {
                    // Build Child Objects
                    while (reader.Read())
                    {
                        switch (reader.NodeType)
                        {
                            case XmlNodeType.Element:
                                if (String.Compare(reader.Name, "Response", true) == 0)
                                {
                                    response = buildResponse(reader);
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
            return response;
        }

        /*
         * Builds Response Object
         */
        private Response buildResponse(XmlReader reader)
        {
            Response response = new Response();
            //Assign Attributes
            while (reader.MoveToNextAttribute())
            {
                if (String.Compare(reader.Name, "Id", true) == 0)
                {
                    response.Id = reader.Value;
                }
                else if (String.Compare(reader.Name, "version", true) == 0)
                {
                    response.Version = reader.Value;
                }
                else if (String.Compare(reader.Name, "ReturnCode", true) == 0)
                {
                    response.Returncode = reader.Value;
                }
                else if (String.Compare(reader.Name, "Message", true) == 0)
                {
                    response.Message = reader.Value;
                }
            }

            // Build Child Objects
            while (reader.Read())
            {
                switch (reader.NodeType)
                {
                    case XmlNodeType.Element:
                        if (String.Compare(reader.Name, "Body", true) == 0)
                        {
                            Body body = buildBody(reader);
                            response.Body = body;
                        }

                        break;
                    default:
                        break;
                }
            }

            return response;
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
                        if (String.Compare(reader.Name, "Function", true) == 0)
                        {
                            Function function = buildFunction(reader);
                            body.addChildObj(function);
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
         * Build Function Object 
         */
        private Function buildFunction(XmlReader reader)
        {
            Function function = new Function();

            //Assign Attributes
            while (reader.MoveToNextAttribute())
            {
                if (String.Compare(reader.Name, "Name", true) == 0)
                {
                    function.Name = reader.Value;
                }
                else if (String.Compare(reader.Name, "Id", true) == 0)
                {
                    function.Id = reader.Value;
                }
                else if (String.Compare(reader.Name, "ReturnCode", true) == 0)
                {
                    function.Returncode = reader.Value;
                }
                else if (String.Compare(reader.Name, "Message", true) == 0)
                {
                    function.Message = reader.Value;
                }
            }

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
                            function.FunctionRequest = functionRequest;

                        }
                        else if (String.Compare(reader.Name, "FunctionResponse", true) == 0)
                        {
                            FunctionResponse functionResponse = buildFunctionResponse(reader);
                            function.FunctionResponse = functionResponse;
                        }                        
                        else if (String.Compare(reader.Name, "ParameterGroup", true) == 0)
                        {
                            ParameterGroup parameterGroup = buildParameterGroup(reader);
                            function.ErrorDetails = parameterGroup;
                        }
                        break;

                    case XmlNodeType.EndElement:
                        if (String.Compare(reader.Name, "Function", true) == 0)
                        {
                            cond = false;
                        }
                        break;

                    default:
                        break;
                }
                if (!cond) break;
            }

            return function;
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
         * Build Function Response Object 
         */
        private FunctionResponse buildFunctionResponse(XmlReader reader)
        {
            FunctionResponse functionResponse = new FunctionResponse();
            if (reader.IsEmptyElement == false)
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
                                functionResponse.addChildObj(parameterGroupChild);

                            }
                            else if (String.Compare(reader.Name, "Parameter", true) == 0)
                            {
                                Parameter parameterChild = buildParameter(reader);
                                functionResponse.addChildObj(parameterChild);
                            }
                            break;

                        case XmlNodeType.EndElement:
                            if (String.Compare(reader.Name, "FunctionResponse", true) == 0)
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
            return functionResponse;
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
