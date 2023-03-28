	<<archive>>
			|
			|
			|-----> hcaparchive
			|
			|
			|-----> someotherarchive

	<<archive>>
			+ archive file
			+ archive directory
			+ delete file
			+ delete directory
			+ specifiy metadata

	<<archiveObj>>
			|----->hcaphttpArchiveObject
			|
			|----->hcapnfsArchiveObject
			|
			|----->hcapcifsArchiveObject
			|
			|------>someotherhttpArcvhiveObject
			|
			|------>someothercifsArchiveObject
			|
			|------>someothernfsArchiveObject
			
		<<archiveObj>>
			+ prepare DestPath
			+ archive file
			+ archive dirk
			+ delete file
			+ delete dir
			+ specify metadata
			
	<<archiveProcess>
			|
			|------>httphcapArcProcess
			|
			|------>cifshcaparcProcess
			|
			|------>somotherhttpProcess
			
			
	<<archiveProcess>>
			+ do Process

	<<transport>>
			|------->ftp
			|
			|------> http
			|
			|------> nfs
			|
			|------> cifs


	<<transport>>
			+ copyfile
			+ delete file
			+ move file
			+ file exists
			+ file size
			+ directory exists
			+ directory size
			+ listDir
			
			
			
			
			
			
			
			
			
			
			




























Comments :
1. ConfigValueObject 
		Should have all get/set methods
		this is read-only and singleton
			provide singleton interface
2. File Lister
	Should report errors while calling next method in case of any failure
	A thread can do traversal and fill the objects in Queue to improve performance
	getNext would just get the objects from the Queue.
	should operate in following modes
	===========
		1. takes a queue and fills it up..
		2. no queue
		3. fills in-built queue
		
 3. seperate init function to handle errors/exceptions for all classes
 4. archive process no file lister
 5. Quit requested function to handle exits
 
 
 
 