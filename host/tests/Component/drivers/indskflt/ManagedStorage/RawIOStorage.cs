//-----------------------------------------------------------------------
// <copyright file="RawIOStorage.cs" company="Microsoft">
//     Copyright (c) Microsoft Corporation. All rights reserved.
// </copyright>
//-----------------------------------------------------------------------
namespace Microsoft.Test.Storages
{
    using Microsoft.Test.Utils.NativeMacros;

    public abstract class RawIOStorage : Win32ApiFile
    {
        protected string SpecialFileName { get; private set; }

        protected new static readonly ShareModes DEFAULT_SHARE_MODES = ShareModes.Read | ShareModes.Write;
        protected new static readonly AccessMasks DEFAULT_ACCESS_MASKS = AccessMasks.Read | AccessMasks.Write;
        protected new static readonly CreationDisposition DEFAULT_CREATE_DISPOSITION = CreationDisposition.OpenExisting;
        protected new static readonly FileFlagsAndAttributes DEFAULT_FILE_FLAGS_AND_ATTRIBS = FileFlagsAndAttributes.Normal;

        protected RawIOStorage(string specialFileName)
            : this(specialFileName, DEFAULT_FILE_FLAGS_AND_ATTRIBS)
        {
        }

        protected RawIOStorage(string specialFileName, FileFlagsAndAttributes flagsAndAttributes)
            : this(specialFileName, DEFAULT_SHARE_MODES, DEFAULT_ACCESS_MASKS, DEFAULT_CREATE_DISPOSITION, flagsAndAttributes)
        {
        }

        protected RawIOStorage(string specialFileName, ShareModes shareModes, AccessMasks accessMasks, CreationDisposition creationDisposition, FileFlagsAndAttributes flagsAndAttributes)
            : base(specialFileName, shareModes, accessMasks, creationDisposition, flagsAndAttributes, null)
        {
            this.SpecialFileName = specialFileName;
        }
    }
}