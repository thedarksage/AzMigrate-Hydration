using System;
using System.IO;
using System.Text;

namespace Microsoft.Azure.SiteRecovery.ProcessServer.Core.Utils
{
    /// <summary>
    /// Utilities for IO related operations
    /// </summary>
    internal static class IOUtils
    {
    }

    /// <summary>
    /// Stream writer that supports formatting
    /// </summary>
    internal class FormattingStreamWriter : StreamWriter
    {
        /// <summary>
        /// Initializes a new instance of the System.IO.StreamWriter
        /// class for the specified stream by using the specified
        /// encoding and buffer size, and optionally leaves
        /// the stream open.
        /// </summary>
        /// <param name="stream">The stream to write to.</param>
        /// <param name="formatProvider">Formatting provider.
        /// An System.IFormatProvider object for a specific culture,
        /// If null, the formatting of the current culture is used.
        /// </param>
        /// <param name="encoding">The character encoding to use.</param>
        /// <param name="bufferSize">The buffer size, in bytes.</param>
        /// <param name="leaveOpen">
        /// true to leave the stream open after the System.IO.StreamWriter
        /// object is disposed; otherwise, false.
        /// </param>
        /// <exception cref="ArgumentNullException">
        /// <paramref name="stream"/> or <paramref name="encoding"/> is null.
        /// </exception>
        /// <exception cref="ArgumentOutOfRangeException">
        /// <paramref name="bufferSize"/> is negative.
        /// </exception>
        /// <exception cref="ArgumentException">
        /// <paramref name="stream"/> is not writable.
        /// </exception>
        public FormattingStreamWriter(Stream stream, IFormatProvider formatProvider, Encoding encoding, int bufferSize, bool leaveOpen)
            : base(stream, encoding, bufferSize, leaveOpen)
        {
            FormatProvider = formatProvider;
        }
        
        /// <summary>
        /// Gets an object that controls formatting.
        /// An System.IFormatProvider object for a specific culture,
        /// or the formatting of the current culture if no other
        /// culture is specified.
        /// </summary>
        public override IFormatProvider FormatProvider { get; }
    }
}
