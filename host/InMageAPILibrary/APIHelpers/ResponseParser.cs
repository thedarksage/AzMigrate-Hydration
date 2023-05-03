using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using InMage.APIModel;
using System.Collections;

namespace InMage.APIHelpers
{
    public class ResponseParser
    {
        public static string GetRequestIdFromResponse(Response response, string requestFunction)
        {
            FunctionResponse functionResponse = GetFunctionResponse(response, requestFunction);
            if (functionResponse != null)
            {
                return functionResponse.GetParameterValue(Constants.KeyRequestId);
            }

            return null;
        }

        public static Function GetFunction(Response response, string functionName)
        {
            if (response != null)
            {
                foreach (var item in response.Body.ChildList)
                {
                    if (item is Function)
                    {
                        Function function = item as Function;
                        if (function.Name == functionName)
                        {
                            return function;
                        }
                    }
                }
            }

            return null;
        }

        public static FunctionResponse GetFunctionResponse(Response response, string functionName)
        {
            if (response != null)
            {
                foreach (var item in response.Body.ChildList)
                {
                    if (item is Function)
                    {
                        Function function = item as Function;
                        if (function.Name == functionName)
                        {
                            return function.FunctionResponse;
                        }
                    }
                }
            }

            return null;
        }

        public static Parameter GetParameter(ArrayList arrayList, string paramKey)
        {
            if (arrayList != null)
            {
                foreach (var param in arrayList)
                {
                    if (param is Parameter && (param as Parameter).Name == paramKey)
                    {
                        return param as Parameter;
                    }
                }
            }

            return null;
        }

        public static ParameterGroup GetParameterGroup(ArrayList arrayList, string paramGroupId)
        {
            if (arrayList != null)
            {
                foreach (var param in arrayList)
                {
                    if (param is ParameterGroup && (param as ParameterGroup).Id == paramGroupId)
                    {
                        return param as ParameterGroup;
                    }
                }
            }

            return null;
        }

        public static bool ValidateResponse(Response response, string requestFunction, ResponseStatus responseStatus)
        {
            int returnCode;
            if (response != null)
            {
                if (response.Returncode == Constants.SuccessReturnCode)
                {
                    Function function = ResponseParser.GetFunction(response, requestFunction);
                    if (function != null)
                    {
                        if (function.Name == requestFunction)
                        {
                            if (int.TryParse(function.Returncode, out returnCode) && responseStatus != null)
                            {
                                responseStatus.ReturnCode = returnCode;
                                responseStatus.Message = function.Message;
                                if (function.ErrorDetails != null)
                                {
                                    responseStatus.ErrorDetails = new ErrorDetails(function.ErrorDetails);
                                }
                            }
                            if (function.Returncode == Constants.SuccessReturnCode)
                            {
                                return true;
                            }                            
                        }
                    }
                }
                else
                {
                    if (int.TryParse(response.Returncode, out returnCode) && responseStatus != null)
                    {
                        responseStatus.ReturnCode = returnCode;
                        responseStatus.Message = response.Message;
                    }
                }
            }

            return false;
        }
    }
}
