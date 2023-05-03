using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using InMage.APIModel;

namespace InMage.APIHelpers
{
    public interface ICXRequest
    {
        FunctionRequest Request { get; set; }
    }
}
