using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace InMage.APIHelpers
{
    public class ResponseStatus
    {
        public int ReturnCode { get; set; }
        public string Message { get; set; }
        public ErrorDetails ErrorDetails { get; set; }

        public ResponseStatus()
        {
            this.ReturnCode = Constants.UndefinedReturnCode;
        }

        public ResponseStatus(int returnCode, string message)
        {
            this.ReturnCode = returnCode;
            this.Message = message;
        }
    }
}
