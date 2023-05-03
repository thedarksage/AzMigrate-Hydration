//-----------------------------------------------------------------------
// <copyright file="IWritableStorage.cs" company="Microsoft">
//     Copyright (c) Microsoft Corporation. All rights reserved.
// </copyright>
//-----------------------------------------------------------------------
namespace Microsoft.Test.Storages
{
    using System.Threading;
    using System.Threading.Tasks;

    public interface IWritableStorage : IStorage
    {
        uint Write(byte[] buffer, uint numberOfBytesToWrite, ulong storageOffset);

        Task<uint> WriteAsync(byte[] buffer, uint numberOfBytesToWrite, ulong storageOffset);

        Task<uint> WriteAsync(byte[] buffer, uint numberOfBytesToWrite, ulong storageOffset, CancellationToken ct);
    }
}