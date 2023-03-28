using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using InMage.APIModel;

namespace InMage.APIHelpers
{
    public class ErrorDetails
    {
        public string ErrorCode { get; private set; }
        public string ErrorMessage { get; private set; }
        public string PossibleCauses { get; private set; }
        public string Recommendation { get; private set; }
        public Dictionary<string, string> Placeholders { get; private set; }

        public ErrorDetails(ParameterGroup parameterGroup)
        {
            ErrorCode = parameterGroup.GetParameterValue("ErrorCode");
            ErrorMessage = parameterGroup.GetParameterValue("ErrorMessage");
            PossibleCauses = parameterGroup.GetParameterValue("PossibleCauses");
            Recommendation = parameterGroup.GetParameterValue("Recommendation");
            ParameterGroup placeholdersParameterGroup = parameterGroup.GetParameterGroup("PlaceHolders");
            if (placeholdersParameterGroup != null && placeholdersParameterGroup.ChildList.Count > 0)
            {
                Placeholders = new Dictionary<string, string>();
                Parameter parameter;
                foreach(var item in placeholdersParameterGroup.ChildList)
                {
                    if(item is Parameter)
                    {
                        parameter = item as Parameter;
                        if (!String.IsNullOrEmpty(parameter.Name))
                        {
                            Placeholders[parameter.Name] = parameter.Value;
                        }
                    }
                }
            }
        }
    }
}
