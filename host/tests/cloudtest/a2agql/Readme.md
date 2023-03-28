Following steps are performed to integrate DI validation with existing A2A GQLs.

1. Upload DI tool to replicating source A2A VM.
2. Run DI tool when we want to perform DI validation.
3. Upload the DI tool results to a storage container.
4. Download the DI tool results to Cloudtest VM.
5. Fetch bookmark for data disk vhd from Prot2 for specific marker id created by DI tool. Create a temp disk out of bookmark and fetch read SAS. Call DIService to calculate checksum for temp disk and match it with checksum calculated by DI tool.


DI tool currently offlines the data disk before creating marker, this ensures there are no pending writes before checksum calculation. 