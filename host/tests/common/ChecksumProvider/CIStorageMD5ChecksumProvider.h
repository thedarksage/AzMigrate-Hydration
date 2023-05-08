///
///  \file  CIStorageMD5ChecksumProvider.h
///
///  \brief contains CIStorageMD5ChecksumProvider class
///

#ifndef CISTORAGEMD5CHECKSUMPROVIDER_H
#define CISTORAGEMD5CHECKSUMPROVIDER_H

#include <string>
#include <sstream>
#include <fstream>

#include "boost/shared_ptr.hpp"
#include "boost/shared_array.hpp"

#include "IChecksumProvider.h"
#include "IStorage.h"

#include "inm_md5.h"

/// \brief CIStorageMD5ChecksumProvider Class
class CIStorageMD5ChecksumProvider : public IChecksumProvider
{
public:
	/// \brief Pointer type
	typedef boost::shared_ptr<CIStorageMD5ChecksumProvider> Ptr; ///< Pointer type

	/// \brief Constructor
	CIStorageMD5ChecksumProvider(IStoragePtr pIStorage) ///< storage
		: m_pIStorage(pIStorage)
	{
	}

	/// \brief Returns checksum for given offset and length
	/// 
	/// throws std::exception
	ChecksumHashPtr GetChecksum(const unsigned long long &offset, ///< offset
		const unsigned &length                                    ///< length
		)
	{
		// Allocate buffer to read
		const unsigned CHUNK_SIZE = 1024 * 1024; //1MB
		boost::shared_array<unsigned char> buf(new unsigned char[CHUNK_SIZE]);

		// Allocate hash
		ChecksumHashPtr pHash(new ChecksumHash(DIGEST_LEN));

		// seek to offset
		if (!m_pIStorage->SeekFile(BEGIN, offset))
			throw ERROR_EXCEPTION << "Failed to seek to offset " << offset << " for storage ID: " << m_pIStorage->GetDeviceId();

		// Initialize md5
		INM_MD5_CTX ctx;
		INM_MD5Init(&ctx);
		
		// Read and update checksum
		unsigned bytesRemaining = length;
		unsigned bytesToRead;
		unsigned long bytesRead;
		do {
			bytesRead = 0;
			bytesToRead = (bytesRemaining > CHUNK_SIZE) ? CHUNK_SIZE : bytesRemaining;
			if (m_pIStorage->Read(buf.get(), bytesToRead, bytesRead))
				INM_MD5Update(&ctx, buf.get(), bytesRead);
			else
				throw ERROR_EXCEPTION << "Failed to read from IStorage object with ID " << m_pIStorage->GetDeviceId() << ", at offset " << offset << ", length " << length;

			bytesRemaining -= bytesRead;

		} while (bytesRemaining);
		
		// Finalize checksum
		INM_MD5Final(&((*pHash)[0]), &ctx);

		return pHash;
	}

	/// \brief Places complete checksums, in units of chunkSize, into file
	///
	/// throws std::exception
	void GetChecksumsInFile(const unsigned &chunkSize, ///< chunk size
		const std::string &file,                       ///< file into which checksums are placed
		const bool &formatHashAsText,                  ///< boolean requesting hash to be formatted as text, separated by newline for each chunk
		const bool &separateHashesByNewline            ///< Applies only when formatHashAsText is true. Writes checksums in text form with each block's checksum separated by a newline
		)
	{
		// Get checksums from offset 0 to end
		GetChecksumsInFile(0, m_pIStorage->GetDeviceSize(), chunkSize, file, formatHashAsText, separateHashesByNewline);
	}

	/// \brief Places checksums, in units of chunkSize, for given offset and length into a file
	///
	/// throws std::exception
	void GetChecksumsInFile(const unsigned long long &offset, ///< offset
		const unsigned long long &length,                     ///< length
		const unsigned &chunkSize,                            ///< chunk size
		const std::string &file,                              ///< file into which checksums are placed
		const bool &formatHashAsText,                         ///< boolean requesting hash to be formatted as text, separated by newline for each chunk
		const bool &separateHashesByNewline                   ///< Applies only when formatHashAsText is true. Writes checksums in text form with each block's checksum separated by a newline
		)
	{
		std::ofstream f;
		std::ios_base::openmode mode = std::ios::trunc;
		WriteHashFunction_t w = separateHashesByNewline ? &CIStorageMD5ChecksumProvider::WriteHashInTextFormWithNewlineSep : &CIStorageMD5ChecksumProvider::WriteHashInTextFormWithoutNewlineSep;
		if (!formatHashAsText) {
			mode |= std::ios::binary;
			w = &CIStorageMD5ChecksumProvider::WriteHash;
		}

		// Open checksums file
		f.open(file.c_str(), std::ios::trunc | std::ios::binary);
		if (!f)
			throw ERROR_EXCEPTION << "Failed to open checksums file " << file;

		// Keep writing checksums
		unsigned long long checksumOffset = offset;
		unsigned long long bytesRemaining = length;
		unsigned checksumBytes;
		do {
			checksumBytes = (unsigned)((bytesRemaining > chunkSize) ? chunkSize : bytesRemaining);
			ChecksumHashPtr pHash = GetChecksum(checksumOffset, checksumBytes);
			(this->*w)(pHash, f);
			if (!f)
				throw ERROR_EXCEPTION << "Failed to write checksums to file " << file << ", for offset " << checksumOffset << ", length " << checksumBytes;
			bytesRemaining -= checksumBytes;
			checksumOffset += checksumBytes;
		} while (bytesRemaining);
	}

	/// \brief Write hash function pointer
	typedef void (CIStorageMD5ChecksumProvider::*WriteHashFunction_t)(ChecksumHashPtr pHash, ///< hash
		std::ofstream &f                                                                     ///< file to write
		);

	/// \brief Writes checksum in text form into the file. Puts a newline after the checksum.
	///
	/// throws std::exception
	void WriteHashInTextFormWithNewlineSep(ChecksumHashPtr pHash, ///< hash
		std::ofstream &f                                          ///< file to write
		)
	{
		const unsigned char *puC = &((*pHash)[0]);
		f << GetPrintableChecksum(puC) << '\n';
	}

	/// \brief Writes checksum in text form into the file.
	///
	/// throws std::exception
	void WriteHashInTextFormWithoutNewlineSep(ChecksumHashPtr pHash, ///< hash
		std::ofstream &f                                             ///< file to write
		)
	{
		const unsigned char *puC = &((*pHash)[0]);
		f << GetPrintableChecksum(puC);
	}

	/// \brief Writes checksum
	///
	/// throws std::exception
	void WriteHash(ChecksumHashPtr pHash, ///< hash
		std::ofstream &f                  ///< file to write
		)
	{
		const unsigned char *puC = &((*pHash)[0]);
		const char *pC = reinterpret_cast<const char *>(puC);
		f.write(pC, pHash->size());
	}

	/// \brief returns printable checksum from checksum bytes
	std::string GetPrintableChecksum(const unsigned char *hash) ///< hash
	{
		stringstream ss;
		for (int i = 0; i < INM_MD5TEXTSIGLEN / 2; i++)
			ss << std::setfill('0') << std::setw(2) << std::hex << std::uppercase << (unsigned int)(hash[i]);
		return ss.str();
	}

private:
	IStoragePtr m_pIStorage; ///< storage
};

#endif /* CISTORAGEMD5CHECKSUMPROVIDER_H */
