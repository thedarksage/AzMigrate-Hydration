//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       :cdpv1database.cpp
//
// Description: 
//


#include "cdpglobals.h"
#include "cdputil.h"
#include "cdpv1validateimpl.h"
#include "cdpv1database.h"
#include "cdpv1datafilereader.h"

#include <ace/OS_NS_sys_stat.h>

#include <sstream>
#include <iostream>
#include <iomanip>
using namespace std;

CDPV1ValidateImpl::CDPV1ValidateImpl(const std::string &dbname, bool verbose)
:m_dbname(dbname), m_verbose(verbose)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

bool CDPV1ValidateImpl::IsDataFile(const std::string & fname)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = true;

    do
    {

        ACE_stat statbuf = {0};
		// PR#10815: Long Path support
		if (0 != sv_stat(getLongPathName(fname.c_str()).c_str(), &statbuf))
        {
            cerr << "Error: Missing data file:" << fname << "\n\n";
            rv = false;
            break;
        }

        if ( (statbuf.st_mode & S_IFREG)!= S_IFREG )
        {
            cerr << "Error: " << fname << " is not a regular file\n\n";
            rv = false;
            break;
        }

    } while (FALSE);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}

bool CDPV1ValidateImpl::validate()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool rv = true;
    bool bOk = true;

    do
    {
        /*
        1. check it is version 1 database
        2. get exclusive access
        3. check each inode, verify it exists, parse the file and compare with inode
        4. check the superblock

        */

        CDPV1Database db(m_dbname);
        if(!db.Exists())
        {
            stringstream l_stdfatal;
            l_stdfatal << "Error: Database " << m_dbname 
                << " does not represent a valid CDP database" << "\n";
            DebugPrintf(SV_LOG_FATAL, "%s", l_stdfatal.str().c_str());
            rv = false;
            break;
        }

        if(!db.Connect())
        {
            rv = false;
            break;
        }

		//if(!db.BeginTransaction())
		//{
		//	rv = false;
		//	break;
		//}

        CDPVersion dbver;
        if(!db.read(dbver))
        {
            rv = false;
            break;
        }

        if ( dbver.c_version != CDPVER )
        {
            stringstream l_stdfatal;
            l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
                << " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
                << " Error: incorrect cdp version " << "\n"
                << " Database : " << m_dbname << "\n"
                << " Expected Version :" << CDPVER << "\n"
                << " Actual Version : " << dbver.c_version << "\n" ;				   

            DebugPrintf(SV_LOG_FATAL, "%s", l_stdfatal.str().c_str());
            rv = false;
            break;
        }

        CDPV1Database::Reader reader = db.FetchInodesinOldestFirstOrder();
        SVERROR hr_read;
        CDPV1Inode inode;

        while ( (hr_read = reader.read(inode,true)) == SVS_OK )
        {
			//Bug# 7934
			if(CDPUtil::QuitRequested())
			{
				rv = false;
				bOk = true;
				break;
			}

            string fname = inode.c_datadir;
            fname += ACE_DIRECTORY_SEPARATOR_CHAR_A;
            fname += inode.c_datafile;

            cout << "-----------------------------\n" ;
            cout << "verifying data file :" <<  fname << "\n";
            if(!IsDataFile(fname))
            {
                bOk = false;
                continue;
            }

            CDPDataFileReader::Ptr filereader;
			CDPV1DataFileReader * reader = new(std::nothrow) CDPV1DataFileReader(fname);
			if(!reader)
			{
				stringstream l_stdfatal;
				l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
					<< " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
					<< " Error Message: " << Error::Msg() << "\n";

				DebugPrintf(SV_LOG_FATAL, "%s", l_stdfatal.str().c_str());
				bOk = false;
				continue;
			}

			filereader.reset(reader);
			filereader ->Init();

			
            SV_ULONGLONG logicalsize = filereader -> LogicalSize();
            bool uncommitedtrans = false;
            if(inode.c_logicalsize > logicalsize)
            {
                cerr << "End of file mismatch\n";

                cerr << "Actual End of File: " << logicalsize << " bytes\n"
                    << "Expected :" << inode.c_logicalsize << " bytes\n";

                bOk = false;
                continue;
            }
            else if(inode.c_logicalsize < logicalsize)
            {
                cout << "Actual End of File: " << logicalsize << " bytes\n"
                    << "Allocated Size" << inode.c_logicalsize << " bytes\n";
                uncommitedtrans  = true;
                filereader -> SetEndOffset((SV_OFFSET_TYPE)inode.c_logicalsize);
            }
            else
            {
                cout << "End of File:" << logicalsize << " bytes\n";
            }
			
            SV_ULONGLONG phsyicalsize = filereader -> PhysicalSize();
			if( (inode.c_logicalsize == 0) && (phsyicalsize == 0) )
			{
				DebugPrintf(SV_LOG_DEBUG, 
					"Ignoring the data file %s, as physical_size == logical_size == 0\n",
					fname.c_str());
				continue;
			}

            if(inode.c_physicalsize != phsyicalsize)
            {
                cerr << "Warning: Size on Disk mismatch";
                //if(uncommitedtrans)
                //{
                //	cerr << "due to uncommited transaction.\n"
                //	     << "Ignore this mismatch, it will be rolled back on next invocation"
                //		 << " of the writer\n";
                //}
                //else
                //{
                cerr << "\n";
                //bOk = false;
                //}

                cerr << "Actual Size on Disk: " << phsyicalsize << " bytes\n"
                    << "Expected :" << inode.c_physicalsize << " bytes\n";
            }
            else
            {
                cout << "Size on Disk:" << phsyicalsize << " bytes\n";
            }


            if(!filereader -> Parse())
            {
                bOk = false;
                continue;
            }	

            if(inode.c_starttime != filereader -> StartTime())
            {
                bOk = false;
                cerr << "Start Time does not match\n";
                string actual, expected;
                if(!CDPUtil::ToDisplayTimeOnConsole(filereader ->StartTime(), actual))
                {
                    continue;
                }

                if(!CDPUtil::ToDisplayTimeOnConsole(inode.c_starttime, expected))
                {
                    continue;
                }

                cerr << "Start Timein data file: " << actual << "\n"
                    << "Expected Start Time :" << expected << "\n";

                continue;	
            }
            else
            {
                string actual;
                if(!CDPUtil::ToDisplayTimeOnConsole(filereader ->StartTime(), actual))
                {
                    bOk = false;
                    continue;
                }
                cout << "Start Time:" << actual << "\n";
            }

            if(inode.c_endtime != filereader -> EndTime())
            {
                bOk = false;
                cerr << "End Time does not match\n";
                string actual, expected;
                if(!CDPUtil::ToDisplayTimeOnConsole(filereader ->EndTime(), actual))
                {
                    continue;
                }

                if(!CDPUtil::ToDisplayTimeOnConsole(inode.c_endtime, expected))
                {
                    continue;
                }

                cerr << "End Timein data file: " << actual << "\n"
                    << "Expected End Time :" << expected << "\n";

                continue;	
            }
            else
            {
                string actual;
                if(!CDPUtil::ToDisplayTimeOnConsole(filereader ->EndTime(), actual))
                {
                    bOk = false;
                    continue;
                }
                cout << "End Time:" << actual << "\n";
            }

            if(inode.c_startseq != filereader -> StartSeq())
            {
                bOk = false;
                cerr << "Start Sequence does not match\n";
                cerr << "Start Sequence in data file: " << filereader -> StartSeq() << "\n"
                    << "Expected Start Sequence :" << inode.c_startseq << "\n";

                continue;	
            }
            else
            {
                cout << "Start Sequence:" << inode.c_startseq << "\n";
            }

            if(inode.c_endseq != filereader -> EndSeq())
            {
                bOk = false;
                cerr << "End Sequence does not match\n";
                cerr << "End Sequence in data file: " << filereader -> EndSeq() << "\n"
                    << "Expected End Sequence :" << inode.c_endseq << "\n";

                continue;	
            }
            else
            {
                cout << "End Sequence:" << inode.c_endseq << "\n";
            }
            SV_UINT  numevents = filereader ->NumEvents();
            if(inode.c_numevents != numevents)		
            {
                cerr << "Number of Consistency Points Mismatch\n"
                    << " In data file: " << numevents<< "\n"
                    << "Expected :" << inode.c_numevents << "\n";

                bOk = false;
                continue;
            }
            else
            {
                cout << "Number of Consistency Points:"  << numevents << "\n";
            }

            SV_UINT numdiffs = filereader -> NumDiffs();
            if(inode.c_numdiffs != numdiffs)		
            {
                cerr << "Num Differentials Mismatch\n"
                    << "Num Differentials in data file: " << numdiffs<< "\n"
                    << "Expected Num Differentials :" << inode.c_numdiffs << "\n";

                bOk = false;
                continue;
            }
            else
            {
                cout << "Num Differentials :"  << numdiffs << "\n";
            }

            DiffIterator diff_iter = filereader ->DiffsBegin();
            DiffIterator diff_end  = filereader ->DiffsEnd();
            CDPV1DiffInfosIterator diffinfo_iter = inode.c_diffinfos.begin();
            CDPV1DiffInfosIterator diffinfo_end = inode.c_diffinfos.end();

            SV_UINT diffnum = 0;
            for ( ; ((diff_iter != diff_end) && (diffinfo_iter != diffinfo_end)); ++diff_iter, ++diffinfo_iter )
            {
                DiffPtr diffptr = *diff_iter;
                CDPV1DiffInfoPtr diffinfoptr = *diffinfo_iter;

                cout << "Validating Differential " << ++diffnum << "\n";

                if(diffinfoptr ->tsfc != diffptr -> starttime())
                {
                    bOk = false;
                    cerr << "Start Time does not match\n";
                    string actual, expected;
                    if(!CDPUtil::ToDisplayTimeOnConsole(diffptr -> starttime(), actual))
                    {
                        break;
                    }

                    if(!CDPUtil::ToDisplayTimeOnConsole(diffinfoptr ->tsfc, expected))
                    {
                        break;
                    }

                    cerr << "Actual Start Time: " << actual << "\n"
                        << "Expected Start Time :" << expected << "\n";
                    break;	
                }
                else
                {
                    string actual;
                    if(!CDPUtil::ToDisplayTimeOnConsole(diffptr ->starttime(), actual))
                    {
                        bOk = false;
                        break;
                    }
                    cout << "Start Time:" << actual << "\n";
                }

                if(diffinfoptr ->tslc != diffptr -> endtime())
                {
                    bOk = false;
                    cerr << "End Time does not match\n";
                    string actual, expected;
                    if(!CDPUtil::ToDisplayTimeOnConsole(diffptr -> endtime(), actual))
                    {
                        break;
                    }

                    if(!CDPUtil::ToDisplayTimeOnConsole(diffinfoptr ->tslc, expected))
                    {
                        break;
                    }

                    cerr << "Actual Start Time: " << actual << "\n"
                        << "Expected Start Time :" << expected << "\n";
                    break;	
                }
                else
                {
                    string actual;
                    if(!CDPUtil::ToDisplayTimeOnConsole(diffptr -> endtime(), actual))
                    {
                        bOk = false;
                        break;
                    }
                    cout << "End Time:" << actual << "\n";
                }

                if(inode.c_version >= CDPV1_INODE_PERIOVERSION)
                {
                    if(diffinfoptr ->tsfcseq != diffptr -> StartTimeSequenceNumber())
                    {
                        bOk = false;
                        cerr << "Start Sequence does not match\n";
                        cerr << "Actual Start Sequence: " << diffptr -> StartTimeSequenceNumber() << "\n"
                            << "Expected Start Sequence :" << diffinfoptr ->tsfcseq << "\n";
                        break;	
                    }
                    else
                    {
                        cout << "Start Sequence:" << diffinfoptr ->tsfcseq << "\n";
                    }

                    if(diffinfoptr ->tslcseq != diffptr -> EndTimeSequenceNumber())
                    {
                        bOk = false;
                        cerr << "End Sequence does not match\n";
                        cerr << "Actual End Sequence: " << diffptr -> EndTimeSequenceNumber() << "\n"
                            << "Expected End Sequence :" << diffinfoptr ->tslcseq << "\n";
                        break;	
                    }
                    else
                    {
                        cout << "End Sequence:" << diffinfoptr ->tslcseq << "\n";
                    }
                }

                SV_ULONGLONG expectedsize = (diffinfoptr -> end) - (diffinfoptr ->start);
                if(expectedsize != diffptr -> size())
                {
                    bOk = false;
                    cerr << "Size of differential does not match\n";

                    cerr << "Actual Size: " <<  diffptr -> size() << " bytes\n"
                        << "Expected: " << expectedsize << " bytes\n";
                    break;	
                }
                else
                {
                    cout << "Size:" <<  diffptr -> size() << " bytes\n";
                }

                if(diffinfoptr ->numchanges != diffptr -> NumDrtds())
                {
                    bOk = false;
                    cerr << "Number of changes does not match\n";

                    cerr << "Actual Changes: " <<  diffptr -> NumDrtds() << "\n"
                        << "Expected: " << diffinfoptr ->numchanges << "\n";
                    break;	
                }
                else
                {
                    cout << "Number of Changes:" <<  diffptr -> NumDrtds() << "\n";
                }

                cdp_drtdv2s_iter_t drtds_iter = diffptr -> DrtdsBegin();
                cdp_drtdv2s_iter_t drtds_end = diffptr -> DrtdsEnd();
                cdp_drtdv2s_iter_t change_iter = diffinfoptr -> changes.begin();
                cdp_drtdv2s_iter_t change_end = diffinfoptr -> changes.end();


                SV_ULONG changenum =0;
                for ( ; ((drtds_iter != drtds_end) && (change_iter != change_end)); ++drtds_iter, ++change_iter )
                {
                    cdp_drtdv2_t drtd = *drtds_iter;
                    cdp_drtdv2_t change = *change_iter;

                    cout << "Validating Change Record " << ++changenum << "\n";
					if(drtd.get_volume_offset() != change.get_volume_offset())
                    {
                        bOk = false;
                        cerr << "Change's Volume Offset Mismatch\n";

						cerr << "Actual: " << drtd.get_volume_offset() << "\n"
							<< "Expected: " << change.get_volume_offset() << "\n";
                        break;	
                    }
                    else
                    {
						cout << "Volume Offset:" <<  drtd.get_volume_offset() << "\n";
                    }

					if(drtd.get_file_offset() != change.get_file_offset())
                    {
                        bOk = false;
                        cerr << "Change's File Offset Mismatch\n";

						cerr << "Actual: " << drtd.get_file_offset() << "\n"
							<< "Expected: " << change.get_file_offset() << "\n";
                        break;	
                    }
                    else
                    {
						cout << "File Offset:" <<  drtd.get_file_offset() << "\n";
                    }

					if(drtd.get_length() != change.get_length())
                    {
                        bOk = false;
                        cerr << "Change's Length Mismatch\n";

						cerr << "Actual: " << drtd.get_length() << "\n"
							<< "Expected: " << change.get_length() << "\n";
                        break;	
                    }
                    else
                    {
						cout << "Size:" <<  drtd.get_length() << "\n";
                    }

					if(drtd.get_seqdelta() != change.get_seqdelta())
                    {
                        bOk = false;
                        cerr << "Change's SequenceDelta Mismatch\n";

						cerr << "Actual: " << drtd.get_seqdelta() << "\n"
							<< "Expected: " << change.get_seqdelta() << "\n";
                        break;	
                    }
                    else
                    {
						cout << "SequenceDelta:" <<  drtd.get_seqdelta() << "\n";
                    }

					if(drtd.get_timedelta() != change.get_timedelta())
                    {
                        bOk = false;
                        cerr << "Change's TimeDelta Mismatch\n";

						cerr << "Actual: " << drtd.get_timedelta() << "\n"
							<< "Expected: " << change.get_timedelta() << "\n";
                        break;	
                    }
                    else
                    {
						cout << "TimeDelta:" <<  drtd.get_timedelta() << "\n";
                    }

                } // end of drtds iteration

                if(drtds_iter != drtds_end)
                {
                    break;
                }

                UserTagsIterator events_iter = diffptr -> UserTagsBegin();
                UserTagsIterator events_end = diffptr -> UserTagsEnd();

                if(events_iter != events_end)
                {
                    cout << "Consistency Events:\n";
                    cout << "----------------------------------------------------------------\n";
                    cout << setw(8)  << setiosflags( ios::left ) << "No."
                        << setw(13) << setiosflags( ios::left ) << "Application"
                        << setiosflags( ios::left ) << "Event" << endl;
                    cout << "----------------------------------------------------------------\n";
                }

                SV_UINT eventnum = 0;
                for ( ; events_iter != events_end; ++events_iter)
                {
                    SvMarkerPtr eventptr = *events_iter;

                    cout << setw(8)  << setiosflags( ios::left ) << ++eventnum
                        << setw(13) << setiosflags( ios::left ) << eventptr ->AppName()
                        << setiosflags( ios::left ) << eventptr -> Tag().get()
                        << "\n";
                } // end of event iteration

            } // end of diffs iteration
        } // while loop validate inode

        if(hr_read.failed())
        {
            rv=false;
            bOk=false;
        }
        if(!bOk)
        {
            cout << "\n\nErrors found during validation."
                << "Please check above output for details.\n";
        }
        else
        {
        	//Bug# 7934
			if(CDPUtil::QuitRequested())
			{
				cout << "\n\nValidation stopped as Quit Requested.\n";
			}
			else
			{
            cout << "\n\nValidation Completed Successfully.\n";
        }
        }
    } while (FALSE);

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return rv;
}
