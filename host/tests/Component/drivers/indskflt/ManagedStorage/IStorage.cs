//-----------------------------------------------------------------------
// <copyright file="IStorage.cs" company="Microsoft">
//     Copyright (c) Microsoft Corporation. All rights reserved.
// </copyright>
//-----------------------------------------------------------------------
namespace Microsoft.Test.Storages
{
    using System;

    public interface IStorage : IDisposable
    {
        bool IsInAsyncMode { get; }


        void Close();
    }
}