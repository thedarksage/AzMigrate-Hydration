#include <iostream>
#include <string>
#include <sstream>
#include <ace/OS.h>
#include <ace/Init_ACE.h>
#include "volumescopier.h"
#include "cdpvolumeutil.h"
#include "volumegroupsettings.h"
#include "volumeinfocollector.h"
#include "volumereporter.h"
#include "volumereporterimp.h"
#include "logger.h"
#include "portablehelpers.h"
#include "simplevolumeapplier.h"
#include "simplevolumescopierapp.h"
#include "localconfigurator.h"
#include "readinfo.h"
#include "configurator2.h"
#include "inmsafecapis.h"

Configurator* TheConfigurator = 0; // singleton

bool DoPreCopyOperations(int argc, char **argv, LocalConfigurator &localConfigurator, const std::string &filesystem,
                         cdp_volume_util &u, cdp_volume_util::ClusterBitmapCustomizationInfos_t &cbcustinfos,
						 bool &profile, bool &pipeline, bool &comparethencopy, bool &unbufferedtargetio, unsigned int &niobufs);

bool DoPostCopyOperations(const std::string &sourcename, const std::string &targetname, const VolumeReporter::VolumeReport_t &vr);

int main(int argc, char **argv)
{
    init_inm_safe_c_apis();
    const unsigned int RDBUFSIZ = 1024*1024;
    const unsigned int COPIERMONITORINTERVAL = 5;
    const unsigned char APPLYCHANGEPERCENTFORUPDATE = 1;

    ACE::init();

    LocalConfigurator localConfigurator;

    cdp_volume_util u;
    VolumesCopier::VolumeInstantiator_t vi = std::bind1st(std::mem_fun(&cdp_volume_util::CreateVolume), &u);
    VolumesCopier::VolumeDestroyer_t vd = std::bind1st(std::mem_fun(&cdp_volume_util::DestroyVolume), &u);
    VolumesCopier::VolumeApplierInstantiator_t vai = std::bind1st(std::mem_fun(&cdp_volume_util::CreateSimpleVolumeApplier), &u);
    VolumesCopier::VolumeApplierDestroyer_t vad = std::bind1st(std::mem_fun(&cdp_volume_util::DestroySimpleVolumeApplier), &u);

    SetLogFileName("testvolumescopier.log");
     
    if (argc < 3)
    {
        std::cout << "Usage: " << argv[0] << " source target [-n <number of io buffers>] [-p] [-l] [-c] [-u] [exclusion files/directories]\n";
		std::cout << "-p: profile\n";
		std::cout << "-l: pipeline read and write\n";
        std::cout << "-c: compare then copy\n";
        std::cout << "-u: unbuffered io on target\n";
		std::cout << "-n: next numerical string specifies the number of io buffers (applicable only if pipeline is enabled)\n";
        return -1;
    }

    VolumeDynamicInfos_t volumeDynamicInfos;
    VolumeInfoCollector volumeCollector;
    VolumeSummaries_t VolumeSummaries;
    volumeCollector.GetVolumeInfos(VolumeSummaries, volumeDynamicInfos, false);

    std::string sourcename = argv[1];
    std::string targetname = argv[2];
    VolumeReporter *pvr = new VolumeReporterImp();
    VolumeReporter::VolumeReport_t vr;
    vr.m_ReportLevel = VolumeReporter::ReportVolumeLevel;
    pvr->GetVolumeReport(sourcename, VolumeSummaries, vr);
    pvr->PrintVolumeReport(vr);
    delete pvr;

    cdp_volume_util::ClusterBitmapCustomizationInfos_t cbcustinfos;
	bool profile = false;
    bool pipeline = false;
    bool comparethencopy = false;
    bool unbufferedtargetio = false;
	unsigned int niobufs = 2;
    if (!DoPreCopyOperations(argc, argv, localConfigurator, vr.m_FileSystem, u, cbcustinfos, profile, pipeline, comparethencopy, unbufferedtargetio, niobufs))
    {
        std::cerr << "Precopy operations not done\n";
        return -1;
    }

    SimpleVolumesCopierApp svca;
    VolumesCopier::ActionOnBytesCovered_t startaction = std::bind1st(std::mem_fun(&SimpleVolumesCopierApp::ActionOnNoBytesCovered), &svca);
    VolumesCopier::ActionOnBytesCovered_t regularaction = std::bind1st(std::mem_fun(&SimpleVolumesCopierApp::ActionOnBytesCovered), &svca);
    VolumesCopier::ActionOnBytesCovered_t completedaction = std::bind1st(std::mem_fun(&SimpleVolumesCopierApp::ActionOnAllBytesCovered), &svca);
    QuitFunction_t qf = std::bind1st(std::mem_fun(&SimpleVolumesCopierApp::ShouldQuit), &svca);
    ReadInfo::ReadRetryInfo_t rri(false, 0, 0);
    VolumesCopier::CopyInfo ci(sourcename, targetname, vi, vd,
                               vr.m_FileSystem, vr.m_Size, vr.m_StartOffset,
                               comparethencopy, std::string(), pipeline, niobufs, RDBUFSIZ,
							   localConfigurator.getVxAlignmentSize(),
							   E_SET_NOFS_CAPABILITIES_IFNOSUPPORT,
                               startaction, completedaction, regularaction, COPIERMONITORINTERVAL, 
                               vai, vad, qf, rri, unbufferedtargetio, APPLYCHANGEPERCENTFORUPDATE, &cbcustinfos,
							   localConfigurator.getLengthForFileSystemClustersQuery(), localConfigurator.getInstallPath(), profile);

    VolumesCopier vc(ci);
    if (!vc.Init())
    {
        std::cerr << "Failed to initialize volumes copier with error message " << vc.GetErrorMessage() << '\n';
        return -1;
    }

    if (!vc.Copy())
    {
        std::cerr << "Failed to copy with error message: " << vc.GetErrorMessage() << '\n';
        return -1;
    }

	if (profile)
	{
		std::cout << "Time for reading source: " << vc.GetTimeForSourceRead().sec() << " seconds" << '\n';
		std::cout << "Time for reading target: " << vc.GetTimeForTargetRead().sec() << " seconds" << '\n';
		std::cout << "Time for applying: " << vc.GetTimeForApply().sec() << " seconds" << '\n';
		std::cout << "Wait time by writer stage for buffer: " << vc.GetWriterWaitTimeForBuffer().sec() << " seconds" << '\n';
		std::cout << "Time for checksums compute and compare: " << vc.GetTimeForChecksumsComputeAndCompare().sec() << " seconds" << '\n';
	}

    if (vr.m_Size == svca.GetBytesCovered())
    {
        if (!DoPostCopyOperations(sourcename, targetname, vr))
        {
            std::cerr << "Failed to do post copy operations\n";
            return -1;
        }
    }

    return 0;
}


bool DoPreCopyOperations(int argc, char **argv, LocalConfigurator &localConfigurator, const std::string &filesystem, 
                         cdp_volume_util &u, cdp_volume_util::ClusterBitmapCustomizationInfos_t &cbcustinfos,
						 bool &profile, bool &pipeline, bool &comparethencopy, bool &unbufferedtargetio, unsigned int &niobufs)
{
    bool bdone = true;

    std::string sourcename = *++argv;
	--argc;
    std::string targetname = *++argv;
	--argc;

	bool exit;
	int c;
	while ((--argc > 0) && ((*++argv)[0] == '-'))
	{
		while (c = *++argv[0])
		{
			exit = false;
			switch (c)
			{
				case 'p':
					profile = true;
					break;
                case 'l':
                    pipeline = true;
                    break;
                case 'c':
                    comparethencopy = true;
                    break;
                case 'u':
                    unbufferedtargetio = true;
                    break;
				case 'n':
					{
                        std::stringstream ssn(*++argv);
						--argc;
						ssn >> niobufs;
						exit = true;
					}
                    break;
				default:
					std::cerr << "illegal option \'" << c << "\' provided; Ignoring this\n";
					break;
			}
			if (exit)
			{
				break;
			}
		}
	}

	svector_t exclusions;
	for (int i = 0; i < argc; i++)
	{
		exclusions.push_back(argv[i]);
	}

	std::cout << "source: " << sourcename 
		<< ", target: " << targetname
		<< ", profile: " << STRBOOL(profile) 
		<< ", pipeline: " << STRBOOL(pipeline)
		<< ", number of io buffers: " << niobufs << '\n';
	std::cout << "exclusion list:\n";
	for (constsvectoriter_t it = exclusions.begin(); it != exclusions.end(); it++)
	{
		std::cout << *it << '\n';
	}

    if (!exclusions.empty())
    {
        bool recurse = true;
        bool exclude = true;
        u.FormClusterBitmapCustomizationInfos(exclusions, filesystem, recurse, exclude, cbcustinfos);
    }

    if (!File::IsExisting(sourcename))
    {
        bdone = false;
        std::cerr << "source " << sourcename << " does not exist\n";
    }

    if (bdone)
    {
        std::string dev_name(targetname);
        std::string sparsefile;
        bool isnewvirtualvolume = false;
        if(!localConfigurator.IsVolpackDriverAvailable() && (IsVolPackDevice(dev_name.c_str(),sparsefile,isnewvirtualvolume)))
        {
            dev_name = sparsefile;
        }

        if(!isnewvirtualvolume)
        {
            if(!File::IsExisting(targetname))
            {
                std::cerr << "target " << targetname << " does not exist\n";
                bdone = false;
            }
        }

        if (bdone)
        {
            cdp_volume_t tgtvol(dev_name, isnewvirtualvolume);
            VOLUME_STATE VolumeState    = GetVolumeState(targetname.c_str());
            if( VolumeState != VOLUME_HIDDEN )
            {
                if(!tgtvol.Hide() || !tgtvol.Good())
                {
                    std::stringstream msg;
                    msg << "failed to hide target volume :" 
                        << tgtvol.GetName()<< " error: " << tgtvol.LastError() << '\n';
                    std::cerr << msg.str();
                    bdone = false;
                }
            }
        }
    }

    return bdone;
}
