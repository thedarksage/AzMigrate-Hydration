//---------------------------------------------------------------
//  <copyright file="IOperation.cs" company="Microsoft">
//      Copyright (c) Microsoft Corporation. All rights reserved.
//  </copyright>
//
//  <summary>
//  Interface for a operation to be performed using VMware Vim SDK.
//  </summary>
//
//  History:     05-Nov-2017   rovemula     Created
//----------------------------------------------------------------

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace VMwareClient
{
    interface IOperation
    {
        /// <summary>
        /// Execute the operation.
        /// </summary>
        void Execute();
    }
}
