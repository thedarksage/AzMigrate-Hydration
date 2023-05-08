//-----------------------------------------------------------------------
// <copyright file="IReadableStorage.cs" company="Microsoft">
//     Copyright (c) Microsoft Corporation. All rights reserved.
// </copyright>
//-----------------------------------------------------------------------
namespace Microsoft.Test.Storages
{
    using System.Threading;
    using System.Threading.Tasks;

    public interface IReadableStorage : IStorage
    {
        uint Read(byte[] buffer, uint numberOfBytesToRead, ulong storageOffset);
        Task<uint> ReadAsync(byte[] buffer, uint numberOfBytesToRead, ulong storageOffset);
        Task<uint> ReadAsync(byte[] buffer, uint numberOfBytesToRead, ulong storageOffset, CancellationToken ct);
    }
}