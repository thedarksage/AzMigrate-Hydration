//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       :cdpv1fixdbimpl.cpp
//
// Description: 
//

#include "cdpv1fixdbimpl.h"
#include "cdpv1database.h"
#include "cdpplatform.h"
#include "cdpv1globals.h"
#include "cdputil.h"
#include "portable.h"
#include <sstream>
#include <fstream>
#include "svdparse.h"
#include "ace/ACE.h"

CDPV1fixdbImpl::CDPV1fixdbImpl(const std::string &dbname, const std::string &tempdir)
:db_name(dbname),
tempDir(tempdir)
{
}

bool CDPV1fixdbImpl::fixDBErrors(int flags)
{
	bool rv = true;

	if( flags & FIX_TIME )
	{
		DebugPrintf(SV_LOG_INFO, "\nFixing time issue.\n");
		if(redate(FIX_TIME))
		{
			DebugPrintf(SV_LOG_INFO, "\nSucessfully fixed time issue.\n\n");
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR, "\nFixing time issue failed.\n\n");
			rv = false;
		}
	}
	if( flags & FIX_STALEINODES )
	{
		DebugPrintf(SV_LOG_INFO, "\nDeleting stale inodes.\n");
		if(redate(FIX_STALEINODES))
		{
			DebugPrintf(SV_LOG_INFO, "\nSucessfully deleted stale inodes.\n\n");
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR, "\nDeleting stale inodes failed.\n\n");
			rv = false;
		}
	}
	if( flags & FIX_PHYSICALSIZE )
	{
		DebugPrintf(SV_LOG_INFO, "\nCorrecting physical size.\n");
		if(fixPhysicalSize())
		{
			DebugPrintf(SV_LOG_INFO, "\nSucessfully corrected physical size.\n");
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR, "\nCorrecting physical size failed.\n");
			rv = false;
		}
    }

    //print the summary message
    if(rv)
    {
        DebugPrintf(SV_LOG_INFO, "\nAll errors have been fixed successfully.\n");
    } else
    {
    	//Bug #7934
		if(!CDPUtil::QuitRequested())
		{
    	    DebugPrintf(SV_LOG_ERROR, "\nOperation failed. see above messages and log file for details.\n");
		}
	}

	return rv;
}

bool CDPV1fixdbImpl::fixPhysicalSize()
{
	bool rv = true;
	std::string dirName = dirname_r(db_name.c_str());
	std::string filePattern = "cdpv*.dat";
	SV_ULONGLONG physicalSize = 0;
    do
    {
        //Open directory
		// PR#10815: Long Path support
		ACE_DIR *dirp = sv_opendir(dirName.c_str());
        
        if (dirp == NULL)
        {
            DebugPrintf(SV_LOG_ERROR, "opendir failed, path = %s, errno = 0x%x\n", dirName.c_str(), ACE_OS::last_error()); 
            rv = false;
            break;
        }

        struct ACE_DIRENT *dp = NULL;

        do
        {
            ACE_OS::last_error(0);

            //Get directory entry
            if ((dp = ACE_OS::readdir(dirp)) != NULL)
            {
                //Find a matching entry
				if (RegExpMatch(filePattern.c_str(), ACE_TEXT_ALWAYS_CHAR(dp->d_name)))
                {
					std::string filePath;
                    
                    filePath = dirName + ACE_DIRECTORY_SEPARATOR_CHAR_A;
                    filePath += ACE_TEXT_ALWAYS_CHAR(dp->d_name);

                    //get file size
					ACE_stat db_stat ={0};
					// PR#10815: Long Path support
					if(sv_stat(getLongPathName(filePath.c_str()).c_str(),&db_stat) != 0)
					{
						DebugPrintf( SV_LOG_INFO, "Unable to find file: %s.\n", filePath.c_str() );
						rv = false;
						break;
					}
					physicalSize += db_stat.st_size;
                }
            }
        } while(dp);
		std::stringstream temp;
		temp << "The totalsize of dat files is " << physicalSize;
		DebugPrintf( SV_LOG_INFO, "%s\n", temp.str().c_str() );

        //Close directory
        ACE_OS::closedir(dirp);

		CDPV1Database db(db_name);
		if(!db.Exists())
		{
			DebugPrintf(SV_LOG_INFO, "%s does not exist check the path and try again.\n", db_name.c_str());
			rv = false;break;
		}

		if(!db.Connect())
		{
			DebugPrintf(SV_LOG_INFO, "Cannot connect to the retention database.\n");
			rv = false;break;
		}

		if(!db.BeginTransaction())
		{
			rv = false;
			break;
		}

		DebugPrintf(SV_LOG_INFO, "\nUpdating physicalsize in %s.\n", CDPV1SUPERBLOCKTBLNAME);
		CDPV1SuperBlock superblock;
		CDPV1Database::Reader superBlockRdr = db.ReadSuperBlock();
		if(superBlockRdr.read(superblock) != SVS_OK)
		{
			DebugPrintf(SV_LOG_INFO, "Unable to read from %s\n", CDPV1SUPERBLOCKTBLNAME);
			rv = false;
			break;
		}
		superBlockRdr.close();

		superblock.c_physicalspace = physicalSize;
		if(!db.Update(superblock))
		{
			DebugPrintf(SV_LOG_INFO, "Unable to update %s\n", CDPV1SUPERBLOCKTBLNAME);
			rv = false;
			break;
		}
		else
		{
			DebugPrintf(SV_LOG_INFO, "Completed updating %s.\n", CDPV1SUPERBLOCKTBLNAME);
		}

		if(db.CommitTransaction())
		{
			rv = false;
			break;
		}

    } while(false);
	//Bug #7934	
	if(CDPUtil::QuitRequested())
	{
		DebugPrintf(SV_LOG_ERROR, "\nOperation Aborted as Quit is Requested\n");
		rv = false;
	}
	return rv;
}

bool CDPV1fixdbImpl::redate(int flags)
{
    bool rv = true;
    std::string DBFile;
    std::string tempDBFile;
    bool fixtime,fixstaleinodes;
    SV_ULONGLONG deletedInodeCount = 0;
    fixtime = ( flags & FIX_TIME );
    fixstaleinodes = ( flags & FIX_STALEINODES );

    bool DbBackupCreated = false;

    do
    {
        //stat the DB file for getting its size.
        ACE_stat db_stat ={0};
		// PR#10815: Long Path support
		if(sv_stat(getLongPathName(db_name.c_str()).c_str(),&db_stat) != 0)
        {
            DebugPrintf( SV_LOG_INFO, "Unable to find the db file.\n" );
            rv = false;break;
        }

        //Make sure the temporary directory provided by the user exists.
        if (SVMakeSureDirectoryPathExists(tempDir.c_str()).failed())
        {
            DebugPrintf(SV_LOG_INFO, "Could not create the directory = %s\n", tempDir.c_str());
            rv = false;break;
        }

		// PR#10815: Long Path support
        std::vector<char> vdb_dir(SV_MAX_PATH, '\0');
        SV_ULONGLONG freespace;

        //Get the free disk space of the disk on which the temporary directory is.
        DirName(tempDir.c_str(), &vdb_dir[0], vdb_dir.size());
        if(!GetDiskFreeCapacity(std::string(vdb_dir.data()), freespace))
        {
            DebugPrintf( SV_LOG_INFO, "Unable to get free disk space in %s.\n", tempDir.c_str() );
            rv = false;break;
        }

        //check if the free space on the disk is sufficient for backing up the file before any modification.
        if( db_stat.st_size + 335544320 > freespace )
        {
            std::stringstream temp;
            temp << "Not enough free space is available on the disk.\nProvide a different <Temporary Folder>.\n";
            temp << "Atleast " << (db_stat.st_size + 335544320) << " bytes must be free on the disk.\n" ;
            DebugPrintf(SV_LOG_INFO, "%s", temp.str().c_str());
            rv = false;break;
        }

        CDPV1Database db(db_name);

        if(!db.Exists())
        {
            DebugPrintf(SV_LOG_INFO, "%s does not exist check the path and try again.\n", db_name.c_str());
            rv = false;break;
        }

        if(!db.Connect())
        {
            DebugPrintf(SV_LOG_INFO, "Cannot connect to the retention database.\n");
            rv = false;break;
        }

		if(!db.BeginTransaction())
		{
			rv = false;
			break;
		}

        DBFile = db_name;
        tempDBFile = tempDir + basename_r(db_name.c_str());

        //take the backup of DB file to the temp dir.
        DebugPrintf(SV_LOG_INFO, "\nTaking backup of retention database to %s.\n\n", tempDBFile.c_str());
        if(!copyFile(DBFile, tempDBFile))
        {
            DebugPrintf(SV_LOG_INFO, "Unable to copy file %s to %s.\n", DBFile.c_str(),tempDBFile.c_str());
            rv = false;
            break;
        }

        DbBackupCreated = true;



        CDPVersion dbver;
        if(!db.read(dbver))
        {
            rv = false;
            break;
        }

        if ( dbver.c_version != CDPVER )
        {
            std::stringstream l_stdfatal;
            l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
                << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
                << " Error: incorrect cdp version " << "\n"
                << " Database : " << db_name << "\n"
                << " Expected Version :" << CDPVER << "\n"
                << " Actual Version : " << dbver.c_version << "\n" ;				   

            DebugPrintf(SV_LOG_FATAL, "%s", l_stdfatal.str().c_str());
            rv = false;
            break;
        }

        if(fixtime)
        {
            std::vector<std::string> columns;

            DebugPrintf(SV_LOG_INFO, "updating table %s...\n", CDPV1SUPERBLOCKTBLNAME);
            columns.clear();
            columns.push_back("c_tsfc");
            columns.push_back("c_tslc");
            if(!db.redate(CDPV1SUPERBLOCKTBLNAME,columns))
            {
                DebugPrintf(SV_LOG_INFO, "Unable to update table: %s\n", CDPV1SUPERBLOCKTBLNAME);
                rv = false;break;
            }
            else
            {
                DebugPrintf(SV_LOG_INFO, "Completed updating %s.\n\n", CDPV1SUPERBLOCKTBLNAME);
            }

            DebugPrintf(SV_LOG_INFO, "updating table %s...\n", CDPV1DATADIRTBLNAME);
            columns.clear();
            columns.push_back("c_tsfc");
            columns.push_back("c_tslc");
            if(!db.redate(CDPV1DATADIRTBLNAME,columns))
            {
                DebugPrintf(SV_LOG_INFO, "Unable to update table: %s\n", CDPV1DATADIRTBLNAME);
                rv = false;break;
            }
            else
            {
                DebugPrintf(SV_LOG_INFO, "Completed updating %s.\n\n", CDPV1DATADIRTBLNAME);
            }

            DebugPrintf(SV_LOG_INFO, "updating table %s...\n", CDPV1EVENTTBLNAME);
            columns.clear();
            columns.push_back("c_eventtime");
            if(!db.redate(CDPV1EVENTTBLNAME,columns))
            {
                DebugPrintf(SV_LOG_INFO, "Unable to update table: %s\n", CDPV1EVENTTBLNAME);
                rv = false;break;
            }
            else
            {
                DebugPrintf(SV_LOG_INFO, "Completed updating %s.\n\n", CDPV1EVENTTBLNAME);
            }

            DebugPrintf(SV_LOG_INFO, "updating table %s...\n", CDPV1PENDINGEVENTTBLNAME);
            columns.clear();
            columns.push_back("c_eventtime");
            if(!db.redate(CDPV1PENDINGEVENTTBLNAME,columns))
            {
                DebugPrintf(SV_LOG_INFO, "Unable to update table: %s\n", CDPV1PENDINGEVENTTBLNAME);
                rv = false;break;
            }
            else
            {
                DebugPrintf(SV_LOG_INFO, "Completed updating %s.\n\n", CDPV1PENDINGEVENTTBLNAME);
            }

            DebugPrintf(SV_LOG_INFO, "updating table %s...\n", CDPV1TIMERANGETBLNAME);
            columns.clear();
            columns.push_back("c_starttime");
            columns.push_back("c_endtime");
            if(!db.redate(CDPV1TIMERANGETBLNAME,columns))
            {
                DebugPrintf(SV_LOG_INFO, "Unable to update table: %s\n", CDPV1TIMERANGETBLNAME);
                rv = false;break;
            }
            else
            {
                DebugPrintf(SV_LOG_INFO, "Completed updating %s.\n\n", CDPV1TIMERANGETBLNAME);
            }

            DebugPrintf(SV_LOG_INFO, "updating table %s...\n", CDPV1PENDINGTIMERANGETBLNAME);
            columns.clear();
            columns.push_back("c_starttime");
            columns.push_back("c_endtime");
            if(!db.redate(CDPV1PENDINGTIMERANGETBLNAME,columns))
            {
                DebugPrintf(SV_LOG_INFO, "Unable to update table: %s\n", CDPV1PENDINGTIMERANGETBLNAME);
                rv = false;break;
            }
            else
            {
                DebugPrintf(SV_LOG_INFO, "Completed updating %s.\n\n", CDPV1PENDINGTIMERANGETBLNAME);
            }
        }

        DebugPrintf(SV_LOG_INFO, "updating table %s\n", CDPV1INODETBLNAME);
        CDPV1Inode inode;

        //get the oldest inode number from t_inode table
        CDPV1Database::Reader oldestInodeReader = db.FetchOldestInode();
        if(oldestInodeReader.read(inode, false) != SVS_OK)
        {
            DebugPrintf(SV_LOG_ERROR, "Unable to read oldest inode from inode table.\n");
            rv = false;break;
        }
        oldestInodeReader.close();
        SV_LONGLONG oldestInodeNo = inode.c_inodeno;

        std::stringstream tempOut;
        tempOut << "Oldest inode no is: "<< oldestInodeNo << std::endl;
        DebugPrintf(SV_LOG_DEBUG, "%s", tempOut.str().c_str());

        //get the latest inode number from t_inode table
        CDPV1Database::Reader latestInodeReader = db.FetchLatestInode();
        if(latestInodeReader.read(inode,false) != SVS_OK)
        {
            DebugPrintf(SV_LOG_ERROR, "Unable to read latest inode from inode table.\n");
            rv = false;break;
        }
        latestInodeReader.close();
        SV_LONGLONG latestInodeNo = inode.c_inodeno;

        tempOut.str("");
        tempOut << "Latest inode no is: "<< latestInodeNo << std::endl;
        DebugPrintf(SV_LOG_DEBUG, "%s", tempOut.str().c_str());

        std::string input = "2100/12/31";
        SV_ULONGLONG futureTime = 0;
        CDPUtil::InputTimeToFileTime(input,futureTime);

        input = "2000/1/1";
        SV_ULONGLONG pastTime = 0;
        CDPUtil::InputTimeToFileTime(input,pastTime);

        SV_ULONGLONG epochdiff = (static_cast<SV_ULONGLONG>(116444736) * static_cast<SV_ULONGLONG>(1000000000));

        for(SV_LONGLONG currentInodeNo = oldestInodeNo; currentInodeNo <= latestInodeNo; currentInodeNo++)
        {
            CDPV1Database::Reader currentInodeReader = db.FetchInode(currentInodeNo);
            if(currentInodeReader.read(inode,true) != SVS_OK)
            {
                //if reading of any inode fails print a debug message and continue
                //because if some stale inodes are deleted, inode numbers may not be continuous
                tempOut.str("");
                tempOut << "ERROR: Failed to read inode " << currentInodeNo << " from inode table." << std::endl;
                DebugPrintf(SV_LOG_DEBUG, "%s", tempOut.str().c_str());
                //rv = false;break;
                DebugPrintf(SV_LOG_DEBUG, "Continueing with the next one...\n");
                continue;
            }
            currentInodeReader.close();

            if( fixstaleinodes && inode.c_logicalsize == 0)
            {
                //if fix staleindoes option is specified delete the inode whose logicalsize = 0
                tempOut.str("");
                tempOut << "Inode: " << currentInodeNo << " has c_logicalsize == 0. Deleting it." << std::endl;
                DebugPrintf(SV_LOG_DEBUG, "%s", tempOut.str().c_str());
                db.Delete(inode);
                deletedInodeCount++;
                continue;
            }

            //fix the time issue inode only if the fix time option is specified
            if(fixtime)
            {
                bool updateInode = false;
                if(inode.c_starttime > futureTime)
                {
                    tempOut.str("");
                    tempOut << "Updating c_starttime of inode: " << inode.c_inodeno << std::endl;
                    DebugPrintf(SV_LOG_DEBUG, "%s", tempOut.str().c_str());
                    inode.c_starttime -= epochdiff;
                    updateInode = true;
                }
                else if(inode.c_starttime < pastTime)
                {
                    tempOut.str("");
                    tempOut << "Updating c_starttime of inode: " << inode.c_inodeno << std::endl;
                    DebugPrintf(SV_LOG_DEBUG, "%s", tempOut.str().c_str());
                    inode.c_starttime += epochdiff;
                    updateInode = true;
                }

                if(inode.c_endtime > futureTime)
                {
                    tempOut.str("");
                    tempOut << "Updating c_endtime of inode: " << inode.c_inodeno << std::endl;
                    DebugPrintf(SV_LOG_DEBUG, "%s", tempOut.str().c_str());
                    inode.c_endtime -= epochdiff;
                    updateInode = true;
                }
                else if(inode.c_endtime < pastTime)
                {
                    tempOut.str("");
                    tempOut << "Updating c_endtime of inode: " << inode.c_inodeno << std::endl;
                    DebugPrintf(SV_LOG_DEBUG, "%s", tempOut.str().c_str());
                    inode.c_endtime += epochdiff;
                    updateInode = true;
                }

                CDPV1DiffInfosConstIterator iter = inode.c_diffinfos.begin();
                CDPV1DiffInfosConstIterator end = inode.c_diffinfos.end();

                tempOut.str("");
                tempOut << "Updating diffInfos of inode: " << inode.c_inodeno << std::endl;
                DebugPrintf(SV_LOG_DEBUG, "%s", tempOut.str().c_str());
                for ( ; iter != end; ++iter )
                {
                    CDPV1DiffInfoPtr diffInfoPtr = *iter;

                    if( diffInfoPtr->tsfc > futureTime)
                    {
                        diffInfoPtr->tsfc -= epochdiff;
                        updateInode = true;
                    }
                    else if( diffInfoPtr->tsfc < pastTime)
                    {
                        diffInfoPtr->tsfc += epochdiff;
                        updateInode = true;
                    }

                    if( diffInfoPtr->tslc > futureTime)
                    {
                        diffInfoPtr->tslc -= epochdiff;
                        updateInode = true;
                    }
                    else if( diffInfoPtr->tslc < pastTime)
                    {
                        diffInfoPtr->tslc += epochdiff;
                        updateInode = true;
                    }
                }

                //correct the .dat file only if the time issue is corrected in the inode 
                if(updateInode)
                {
                    std::string datFile = inode.c_datadir + inode.c_datafile;
                    std::string tempFile = tempDir + inode.c_datafile;
                    tempOut.str("");
                    tempOut << "correcting " << inode.c_datafile << "\n";
                    DebugPrintf(SV_LOG_DEBUG, "%s", tempOut.str().c_str());
                    DebugPrintf(SV_LOG_INFO, ".");

                    //create a backup of the .dat file to temporary directory before modification
                    DebugPrintf(SV_LOG_DEBUG, "creating a backup copy to %s\n", tempFile.c_str());
                    if(!copyFile(datFile, tempFile))
                    {
                        DebugPrintf(SV_LOG_ERROR, "Unable to copy file %s to %s.\n", datFile.c_str(),tempFile.c_str());
                        rv = false;break;
                    }

                    //redate the .dat file
                    if(redateDATFile(datFile.c_str(), inode.c_logicalsize))
                    {
                        //.dat file is sucessfully modified so remove the backup copy
                        DebugPrintf(SV_LOG_DEBUG, "removing backup copy %s\n", tempFile.c_str());
						// PR#10815: Long Path support
						if(ACE_OS::unlink(getLongPathName(tempFile.c_str()).c_str()))
                        {
                            DebugPrintf(SV_LOG_ERROR, "Cannot delete temporary data file %s.\n", tempFile.c_str());
                        }
                    }
                    else//.dat file modification failed. replace the original file and break
                    {
                        tempOut.str("");
                        tempOut << "Failed to update datafile "<< inode.c_datafile << ". Copying back from "
                            << tempFile << std::endl;
                        DebugPrintf(SV_LOG_INFO, "%s", tempOut.str().c_str());

                        //as update .dat failed, overwrite the original .dat file with the backup copy
                        if(copyFile(tempFile, datFile))
                        {
                            //if successfully copied the .dat file, delete the temporary copy
                            DebugPrintf(SV_LOG_DEBUG, "removing backup copy %s\n", tempFile.c_str());
							// PR#10815: Long Path support
							if(ACE_OS::unlink(getLongPathName(tempFile.c_str()).c_str()))
                            {
                                DebugPrintf(SV_LOG_ERROR, "Cannot delete temporary data file %s.\n", tempFile.c_str());
                            }

                            rv = false;
                            break;
                        }

                        DebugPrintf(SV_LOG_ERROR, "Unable to copy file %s back to %s.\n", tempFile.c_str(), datFile.c_str());
                        rv = false;
                        break;
                    }

                    //update the inode only if the .dat file is successfully corrected
                    tempOut.str("");
                    tempOut << "Trying to update inode: " << inode.c_inodeno << std::endl;
                    DebugPrintf(SV_LOG_DEBUG, "%s", tempOut.str().c_str());
                    if( !db.Update(inode) )
                    {
                        tempOut.str("");
                        tempOut << "Unable to update inode: " << inode.c_inodeno << std::endl;
                        DebugPrintf(SV_LOG_ERROR, "%s", tempOut.str().c_str());
                        rv = false;break;
                    }
                }
            }
        }

        //if the fix staleinodes is specified and really some staleinodes are deleted then update
        //the superblock table
        if( fixstaleinodes && (deletedInodeCount != 0) )
        {
            tempOut.str("");
            tempOut << deletedInodeCount << " staleinodes deleted sucessfully." << inode.c_inodeno << std::endl;
            DebugPrintf(SV_LOG_INFO, "%s", tempOut.str().c_str());
            DebugPrintf(SV_LOG_INFO, "\nUpdating inode count in %s.\n\n", CDPV1SUPERBLOCKTBLNAME);
            CDPV1SuperBlock superblock;
            CDPV1Database::Reader superBlockRdr = db.ReadSuperBlock();
            if(superBlockRdr.read(superblock) != SVS_OK)
            {
                DebugPrintf(SV_LOG_INFO, "Unable to read from %s\n", CDPV1SUPERBLOCKTBLNAME);
                rv = false;
                break;
            }
            superBlockRdr.close();

            superblock.c_numinodes -= deletedInodeCount;
            if(!db.Update(superblock))
            {
                DebugPrintf(SV_LOG_INFO, "Unable to update inode count in %s\n", CDPV1SUPERBLOCKTBLNAME);
                rv = false;
                break;
            }
            else
            {
                DebugPrintf(SV_LOG_INFO, "Completed updating inode count in %s.\n\n", CDPV1SUPERBLOCKTBLNAME);
            }

        }

        if(rv)
        {
            DebugPrintf(SV_LOG_INFO, "\nCompleted updating %s.\n\n", CDPV1INODETBLNAME);
        }
        else
        {
            DebugPrintf(SV_LOG_INFO, "\nUpdating %s failed.\n\n", CDPV1INODETBLNAME);
        }

		if(!db.CommitTransaction())
		{
			rv = false;
			break;
		}

    }while(false);


    //
    // if backup copy exits
    //   if database could not be updated, restore it using the backup copy
    // fi
    // delete the backup copy if database is in sanity condition
    //
    if(DbBackupCreated)
    {
        bool DeleteDBBackup = true;

        if(!rv)
        {
            DebugPrintf(SV_LOG_INFO, "\nrestoring %s using the backup copy.\n\n",
                DBFile.c_str());
            if(!copyFile(tempDBFile, DBFile))
            {
                //
                // keep the backup copy since the db is in bad state. the copy can be used to restore the db
                // using manual procedures
                //
                DeleteDBBackup = false;
                DebugPrintf(SV_LOG_INFO, "Unable to copy %s to %s.\n", tempDBFile.c_str(), DBFile.c_str());
                rv = false;
            }
        }

        if(DeleteDBBackup)
        {

            DebugPrintf(SV_LOG_INFO, "Deleting the temporary backup file %s.\n", tempDBFile.c_str());
			// PR#10815: Long Path support
			if(ACE_OS::unlink(getLongPathName(tempDBFile.c_str()).c_str()))
            {
                DebugPrintf(SV_LOG_INFO, "Cannot delete temporary DB file %s.\n", tempDBFile.c_str());
            }
        }
    }

	//Bug# 7934
	if(CDPUtil::QuitRequested())
	{
		DebugPrintf(SV_LOG_ERROR, "\nOperation Aborted as Quit is Requested\n");
		rv = false;
	}

    return rv;
}

//
//  redateDATFile (const char *fileName, const SV_ULONGLONG offset)
//  Scans an SV data file to ensure proper formatting
//  Parameter:
//      char *fileName  Path and name of teh file to be verified
//  Return Value:
//      1               On succesful completion
//      0               On detecting a problem with the file
//
int CDPV1fixdbImpl::redateDATFile (const char *fileName, const SV_ULONGLONG offset)
{
    assert (NULL != fileName);

    bool        eof = false;
    int         readin = 0;
    int         chunkerr = 1;
    int         retVal = 0;
    char        *tagname = NULL;
    FILE        *in = NULL;
    SVD_PREFIX  prefix = {0};

    // Open and verify that the existance of the requested file
    in = fopen(fileName, "rb+");

    if (NULL == in)
    {
        DebugPrintf(SV_LOG_DEBUG, "No such file %s, dwErr = %d\n", fileName , errno);
        return (retVal);
    }

    // Scan through the file one chunk at a time
    while (!eof)
    {
        // Read the chunk prefix
        /* NOTE: The record size and record number fields have intentionally    *
        *   been swapped.  This way, we can distinguish between a full prefix   *
        *   being read (result = sizeof (prefix)), an end-of-file marker in the *
        *   middle of the prefix (0 < result < sizeof (prefix), the only error  *
        *   condition), and a proper end-of-file at the end of the last chunk   *
        *   (result = 0)                                                        */
        readin = fread(&prefix, 1, sizeof (prefix), in);

        if ( (0 == readin) || (ftell(in) >= offset) )
        {
            retVal = 1;
            eof = true;
        }
        else if (sizeof (prefix) != readin)
        {
            DebugPrintf(SV_LOG_DEBUG, "Couldn't read prefix from file %s\n", fileName);
            retVal = 0;
            eof = true;
        }
        else
        {
            // Now that we have the prefix, determine the tag and branch accordingly
            switch (prefix.tag)
            {
                // File header
            case SVD_TAG_HEADER1:
                chunkerr = OnHeader (in, prefix.count, prefix.Flags);
                tagname = SVD_TAG_HEADER1_NAME;
                break;

                // File header v2
            case SVD_TAG_HEADER_V2:
                chunkerr = OnHeaderV2 (in, prefix.count, prefix.Flags);
                tagname = SVD_TAG_HEADER2_NAME;
                break;

                // Dirty block with data
            case SVD_TAG_DIRTY_BLOCK_DATA:
                chunkerr = OnDirtyBlocksData (in, prefix.count, prefix.Flags);
                tagname = SVD_TAG_DIRTY_BLOCK_DATA_NAME;
                break;

                // Dirty block with data V2
            case SVD_TAG_DIRTY_BLOCK_DATA_V2:
                chunkerr = OnDirtyBlocksDataV2 (in, prefix.count, prefix.Flags);
                tagname = SVD_TAG_DIRTY_BLOCK_DATA_V2_NAME;
                break;

                // Dirty block
            case SVD_TAG_DIRTY_BLOCKS:
                chunkerr = OnDirtyBlocks (in, prefix.count, prefix.Flags);
                tagname = "DIRT";
                break;

                //Time Stamp First Change
            case SVD_TAG_TIME_STAMP_OF_FIRST_CHANGE:
                chunkerr = OnTimeStampInfo (in, prefix.count, prefix.tag);
                tagname = SVD_TAG_TSFC_NAME;
                break;

                //Time Stamp First Change V2
            case SVD_TAG_TIME_STAMP_OF_FIRST_CHANGE_V2:
                chunkerr = OnTimeStampInfoV2 (in, prefix.count, prefix.tag);
                tagname = SVD_TAG_TSFC_V2_NAME;
                break;

                //Time Stamp Last Change
            case SVD_TAG_TIME_STAMP_OF_LAST_CHANGE:
                chunkerr = OnTimeStampInfo (in, prefix.count, prefix.tag);
                tagname = SVD_TAG_TSLC_NAME;
                break;

                //Time Stamp Last Change V2
            case SVD_TAG_TIME_STAMP_OF_LAST_CHANGE_V2:
                chunkerr = OnTimeStampInfoV2 (in, prefix.count, prefix.tag);
                tagname = SVD_TAG_TSLC_V2_NAME;
                break;

                //DRTD Changes
            case SVD_TAG_LENGTH_OF_DRTD_CHANGES:
                chunkerr = OnDRTDChanges (in, prefix.count, prefix.Flags);
                tagname = SVD_TAG_LODC_NAME;
                break;

                //User Defined Tag
            case SVD_TAG_USER:
                chunkerr = OnUDT(in, prefix.count, prefix.Flags);
                tagname = SVD_TAG_USER_NAME;
                break;

                // HCDs (hash compare data for fast sync)
                //case SVD_TAG_SYNC_HASH_COMPARE_DATA:
                //    chunkerr = OnHashCompareData (in, prefix.count, prefix.Flags);
                //    tagname = "HCDS";
                //    break;

                // Unkown chunk.  Error condition
            default:
                DebugPrintf(SV_LOG_DEBUG, "FAILED: Encountered an unknown tag 0x%lX at offset %ld\n", prefix.tag, ftell(in));
                retVal = 0;
                eof = true;
            }

            // If the individual chunk reader failed, so should we
            if (chunkerr == 0)
            {
                DebugPrintf(SV_LOG_DEBUG, "FAILED: Encountered error in chunk \"%s\" at offset %ld\n", tagname, ftell(in));
                retVal = 0;
                eof = true;
            }
        }
    }

    // Close the file
    fclose(in);

    // Exit
    return (retVal);
}

//
// OnHeader (FILE *in, unsigned int size)
//  Scans a block with a SVD_HEADER1 tag
//  Parameters:
//      FILE *in            Handle for the file to be read from
//      unsigned long size  count value from the block header
//      unsigned long flags flags value from the block header
//  Return Value:
//      1               On succesful completion
//      0               On detecting an error within the header
//
int CDPV1fixdbImpl::OnHeader (FILE *in, const unsigned long size, const unsigned long flags)
{
    assert (in);
    assert (size > 0);

    return fseek(in, sizeof (SVD_HEADER1), SEEK_CUR) == 0?1:0;
}

//
// OnHeaderV2 (FILE *in, unsigned int size)
//  Scans a block with a SVD_HEADER_V2 tag
//  Parameters:
//      FILE *in            Handle for the file to be read from
//      unsigned long size  count value from the block header
//      unsigned long flags flags value from the block header
//  Return Value:
//      1               On succesful completion
//      0               On detecting an error within the header
//
int CDPV1fixdbImpl::OnHeaderV2 (FILE *in, const unsigned long size, const unsigned long flags)
{
    assert (in);
    assert (size > 0);

    return fseek(in, sizeof (SVD_HEADER_V2), SEEK_CUR) == 0?1:0;
}

//
// OnDirtyBlocksData (FILE *in, unsigned int size)
//  Scans a block with a SVD_HEADER1 tag
//  Parameters:
//      FILE *in            Handle for the file to be read from
//      unsigned long size  count value from the block header
//      unsigned long flags flags value from the block header
//  Return Value:
//      1               On succesful completion
//      0               On detecting an error within the header
//
int CDPV1fixdbImpl::OnDirtyBlocksData (FILE *in, const unsigned long size, const unsigned long flags)
{
    assert (in);
    assert (size > 0);

    unsigned long   i = 0;
    unsigned long   offset = 0;
    int             retVal = 1;
    SVD_DIRTY_BLOCK header = {0};
    unsigned long   eof_offset = 0;

    //Store the current offset
    offset = ftell(in);

    //Compute the file length
    (void)fseek(in, 0, SEEK_END);
    eof_offset = ftell(in);

    //Restore to the original offset
    (void)fseek(in, offset, SEEK_SET);


    // Cycle through all records, reading each one
    for (i = 0; i < size; i++)
    {

        // Read the record header
        if (fread(&header, sizeof (SVD_DIRTY_BLOCK), 1, in) == 0)
        {
            DebugPrintf(SV_LOG_DEBUG, "FAILED: Record #%lu of %lu does not exist\n", i + 1, size);
            retVal = 0;
            break;
        }

        offset = ftell(in);

        // If, for some reason we're biting off more than we can chew we stop
        if (header.Length > ULONG_MAX)
        {
            DebugPrintf(SV_LOG_DEBUG, "FAILED: Block header too long (%lu)\n", (unsigned long)header.Length);
            retVal = 0;
            break;
        }

        //Check if we are crossing an end-of-file mark.
        if ( (offset + (unsigned long) header.Length) > eof_offset) {
            DebugPrintf(SV_LOG_DEBUG, "FAILED: Encountered an end-of-file at offset %lu, While trying to seek %lu bytes from offset %lu \n", eof_offset, (unsigned long)header.Length, offset);
            retVal = 0;
            break;

        }

        // Try to skip past the actual data.
        if (fseek(in, (long)header.Length, SEEK_CUR) != 0)
        {
            DebugPrintf(SV_LOG_DEBUG, "FAILED: Could not seek to end of chunk (%lu bytes from offset %lu)\n", (unsigned long)header.Length, offset);
            retVal = 0;
            break;
        }
    }
    return retVal;
}

//
// OnDirtyBlocksDataV2 (FILE *in, unsigned int size)
//  Scans a block with a SVD_HEADER1 tag
//  Parameters:
//      FILE *in            Handle for the file to be read from
//      unsigned long size  count value from the block header
//      unsigned long flags flags value from the block header
//  Return Value:
//      1               On succesful completion
//      0               On detecting an error within the header
//
int CDPV1fixdbImpl::OnDirtyBlocksDataV2 (FILE *in, const unsigned long size, const unsigned long flags)
{
    assert (in);
    assert (size > 0);

    unsigned long   i = 0;
    unsigned long   offset = 0;
    int             retVal = 1;
    SVD_DIRTY_BLOCK_V2 headerV2 = {0};
    unsigned long   eof_offset = 0;

    //Store the current offset
    offset = ftell(in);

    //Compute the file length
    (void)fseek(in, 0, SEEK_END);
    eof_offset = ftell(in);

    //Restore to the original offset
    (void)fseek(in, offset, SEEK_SET);


    // Cycle through all records, reading each one
    for (i = 0; i < size; i++)
    {

        // Read the record header
        if (fread(&headerV2, sizeof (SVD_DIRTY_BLOCK_V2), 1, in) == 0)
        {
            DebugPrintf(SV_LOG_DEBUG, "FAILED: Record #%lu of %lu does not exist\n", i + 1, size);
            retVal = 0;
            break;
        }

        offset = ftell(in);

        //Check if we are crossing an end-of-file mark.
        if ( (offset + (unsigned long) headerV2.Length) > eof_offset) {
            DebugPrintf(SV_LOG_DEBUG, "FAILED: Encountered an end-of-file at offset %lu, While trying to seek %lu bytes from offset %lu \n", eof_offset, (unsigned long)headerV2.Length, offset);
            retVal = 0;
            break;

        }

        // Try to skip past the actual data.
        if (fseek(in, (long)headerV2.Length, SEEK_CUR) != 0)
        {
            DebugPrintf(SV_LOG_DEBUG, "FAILED: Could not seek to end of chunk (%lu bytes from offset %lu)\n", (unsigned long)headerV2.Length, offset);
            retVal = 0;
            break;
        }
    }
    return retVal;
}

//
// OnDirtyBlocks (FILE *in, unsigned int size)
//  Scans a block with a SVD_HEADER1 tag
//  Parameters:
//      FILE *in            Handle for the file to be read from
//      unsigned long size  count value from the block header
//      unsigned long flags flags value from the block header
//  Return Value:
//      1               On succesful completion
//      0               On detecting an error within the header
//
int CDPV1fixdbImpl::OnDirtyBlocks (FILE *in, unsigned long size, unsigned long flags)
{
    assert (in);
    assert (size > 0);

    return fseek(in, sizeof (SVD_DIRTY_BLOCK), SEEK_CUR) == 0?1:0;
}

// OnTimeStmapInfo (FILE *in, unsigned int size)
//  Scans a block with a SVD_TIME_STAMP tag
//  Parameters:
//      FILE *in            Handle for the file to be read from
//      unsigned long size  count value from the block header
//      unsigned long flags Tag that tells whether it is a TOFC or TOLC
//  Return Value:
//      1               On succesful completion
//      0               On detecting an error within the header
//
int CDPV1fixdbImpl::OnTimeStampInfo (FILE *in, unsigned long size, unsigned long tag)
{
    assert (in);
    assert (size > 0);
    unsigned long int i;
    int retVal = 1;
    SVD_TIME_STAMP timestamp;

    std::string input = "2100/12/31";
    SV_ULONGLONG futureTime = 0;
    CDPUtil::InputTimeToFileTime(input,futureTime);

    input = "2000/1/1";
    SV_ULONGLONG pastTime = 0;
    CDPUtil::InputTimeToFileTime(input,pastTime);
    SV_ULONGLONG epochdiff = (static_cast<SV_ULONGLONG>(116444736) * static_cast<SV_ULONGLONG>(1000000000));

    for(i = 0 ; i < size ; i++ )
    {
        long offset = ftell(in);
        DebugPrintf(SV_LOG_DEBUG, "Offset of this TS is %ld\n", offset);
        if (fread(&timestamp, sizeof (SVD_TIME_STAMP), 1, in) == 0)
        {
            DebugPrintf(SV_LOG_DEBUG, "FAILED: Record to read record %lu while reading timstamp Info\n", i+1);
            retVal = 0;
            break;
        }

        if( timestamp.TimeInHundNanoSecondsFromJan1601 > futureTime)
        {
            DebugPrintf(SV_LOG_DEBUG, "Time Stamp is in future. Setting it Properly.\n");
            timestamp.TimeInHundNanoSecondsFromJan1601 = timestamp.TimeInHundNanoSecondsFromJan1601 - epochdiff;
            seek_and_fwrite(&timestamp,  sizeof (SVD_TIME_STAMP), 1, in, offset);
        }
        else if( timestamp.TimeInHundNanoSecondsFromJan1601 < pastTime)
        {
            DebugPrintf(SV_LOG_DEBUG, "Time Stamp is in past. Setting it Properly.\n");
            timestamp.TimeInHundNanoSecondsFromJan1601 = timestamp.TimeInHundNanoSecondsFromJan1601 + epochdiff;
            seek_and_fwrite(&timestamp,  sizeof (SVD_TIME_STAMP), 1, in, offset);
        }
        else
        {
            DebugPrintf(SV_LOG_DEBUG, "Time Stamp is in Proper.\n");
        }
    }

    return retVal;
}

// OnTimeStmapInfoV2 (FILE *in, unsigned int size)
//  Scans a block with a SVD_TIME_STAMP_V2 tag
//  Parameters:
//      FILE *in            Handle for the file to be read from
//      unsigned long size  count value from the block header
//      unsigned long flags Tag that tells whether it is a TOFC or TOLC
//  Return Value:
//      1               On succesful completion
//      0               On detecting an error within the header
//
int CDPV1fixdbImpl::OnTimeStampInfoV2 (FILE *in, unsigned long size, unsigned long tag)
{
    assert (in);
    assert (size > 0);
    unsigned long int i;
    int retVal = 1;
    SVD_TIME_STAMP_V2 timestampV2;

    std::string input = "2100/12/31";
    SV_ULONGLONG futureTime = 0;
    CDPUtil::InputTimeToFileTime(input,futureTime);

    input = "2000/1/1";
    SV_ULONGLONG pastTime = 0;
    CDPUtil::InputTimeToFileTime(input,pastTime);
    SV_ULONGLONG epochdiff = (static_cast<SV_ULONGLONG>(116444736) * static_cast<SV_ULONGLONG>(1000000000));

    for(i = 0 ; i < size ; i++ )
    {
        long offset = ftell(in);
        DebugPrintf(SV_LOG_DEBUG, "Offset of this TS is %ld\n", offset);
        if (fread(&timestampV2, sizeof (SVD_TIME_STAMP_V2), 1, in) == 0)
        {
            DebugPrintf(SV_LOG_DEBUG, "FAILED: Record to read record %lu while reading timstamp Info\n", i+1);
            retVal = 0;
            break;
        }

        if( timestampV2.TimeInHundNanoSecondsFromJan1601 > futureTime)
        {
            DebugPrintf(SV_LOG_DEBUG, "Time Stamp V2 is in future. Setting it Properly.\n");
            timestampV2.TimeInHundNanoSecondsFromJan1601 = timestampV2.TimeInHundNanoSecondsFromJan1601 - epochdiff;
            seek_and_fwrite(&timestampV2,  sizeof (SVD_TIME_STAMP_V2), 1, in, offset);
        }
        else if( timestampV2.TimeInHundNanoSecondsFromJan1601 < pastTime)
        {
            DebugPrintf(SV_LOG_DEBUG, "Time Stamp V2 is in past. Setting it Properly.\n");
            timestampV2.TimeInHundNanoSecondsFromJan1601 = timestampV2.TimeInHundNanoSecondsFromJan1601 + epochdiff;
            seek_and_fwrite(&timestampV2,  sizeof (SVD_TIME_STAMP_V2), 1, in, offset);
        }
        else
        {
            DebugPrintf(SV_LOG_DEBUG, "Time Stamp V2 is Proper.\n");
        }
    }

    return retVal;
}

// OnDRTDChanges (FILE *in, unsigned int size)
//  Scans a block with a SVD_TAG_LENGTH_OF_DRTD_CHANGES tag
//  Parameters:
//      FILE *in            Handle for the file to be read from
//      unsigned long size  count value from the block header
//      unsigned long flags flags value from the block header
//  Return Value:
//      1               On succesful completion
//      0               On detecting an error within the header
//
int CDPV1fixdbImpl::OnDRTDChanges (FILE *in, unsigned long size, unsigned long flags)
{
    assert (in);
    assert (size > 0);
    unsigned long int i;

    for(i = 0 ; i < size ; i++ )
    {
        if(fseek(in, sizeof (SV_ULONGLONG), SEEK_CUR) != 0 )
            return  0 ;
    }
    return 1;
}

//  OnUDT
//  Scans a block with a SVD_TAG_USER tag
//  Parameters:
//      FILE *in            Handle for the file to be read from
//      unsigned long size  count value from the block header
//      unsigned long flags flags value from the block header
//        bool                verbose flag
//  Return Value:
//      1               On succesful completion
//      0               On detecting an error within the header
//
int CDPV1fixdbImpl::OnUDT (FILE *in, unsigned long size, unsigned long flags)
{
    return  fseek(in, flags, SEEK_CUR) == 0? 1: 0;
}

size_t CDPV1fixdbImpl::seek_and_fwrite(const void *buf, size_t size, size_t nitems, FILE *fpcontext, long where)
{
    size_t retval = 1;
    if(fpcontext == NULL) return 0;

    if (fseek(fpcontext, where, SEEK_SET) != 0)
    {
        DebugPrintf(SV_LOG_DEBUG, "Seek failed in seek_and_fwrite - Error %d\n", errno);
        return 0;
    }

    where = ftell(fpcontext);

    if((retval = fwrite (buf, size, nitems, fpcontext)) != nitems)
    {
        DebugPrintf(SV_LOG_DEBUG, "seek_and_fwrite failed to write - Error %d\n", errno);
    }
    where = ftell(fpcontext);
    fflush(fpcontext);
    return retval;
}

bool CDPV1fixdbImpl::copyFile(const std::string &fromFile, const std::string &toFile)
{
    bool rv = true;
    do
    {
        std::ifstream inFile(fromFile.c_str(), std::ios::binary);
        if(!inFile)
        {
            DebugPrintf(SV_LOG_ERROR, "Cannot open file %s for reading.\n", fromFile.c_str());
            rv = false;
            break;
        }

        std::ofstream outFile(toFile.c_str(), std::ios::binary);
        if(!outFile)
        {
            DebugPrintf(SV_LOG_ERROR, "Cannot open file %s for writing.\n", toFile.c_str());
            rv = false;
            break;
        }

        outFile << inFile.rdbuf();

        inFile.close();
        outFile.close();
    }while(false);

    return rv;
}
