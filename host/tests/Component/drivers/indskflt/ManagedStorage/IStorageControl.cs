//-----------------------------------------------------------------------
// <copyright file="IStorageControl.cs" company="Microsoft">
//     Copyright (c) Microsoft Corporation. All rights reserved.
// </copyright>
//-----------------------------------------------------------------------
using Microsoft.Test.Utils;
namespace Microsoft.Test.Storages
{
    public interface IStorageControl
    {
        void Online(bool readOnly);

        void Offline(bool readOnly);

        bool SetFilePointer(long ullOffset);

        NativeStructures.GET_DISK_ATTRIBUTES GetDiskAttributes();
    }
}
