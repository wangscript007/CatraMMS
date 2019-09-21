
#include <fstream>
#include "MMSStorage.h"
#include "catralibraries/FileIO.h"
#include "catralibraries/System.h"
#include "catralibraries/DateTime.h"

MMSStorage::MMSStorage(
        Json::Value configuration,
        shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade,
        shared_ptr<spdlog::logger> logger) 
{

    _logger             = logger;
    _mmsEngineDBFacade  = mmsEngineDBFacade;

    _hostName = System::getHostName();

    _storage = configuration["storage"].get("path", "XXX").asString();
    if (_storage.back() != '/')
        _storage.push_back('/');
    _freeSpaceToLeaveInEachPartitionInMB = configuration["storage"].get("freeSpaceToLeaveInEachPartitionInMB", 5).asInt();

    _ingestionRootRepository = _storage + "IngestionRepository/users/";
    _mmsRootRepository = _storage + "MMSRepository/";
    _downloadRootRepository = _storage + "DownloadRepository/";
    _streamingRootRepository = _storage + "StreamingRepository/";

    _stagingRootRepository = _storage + "MMSWorkingAreaRepository/Staging/";
    _transcoderStagingRootRepository = _storage + "MMSTranscoderWorkingAreaRepository/Staging/";

    string ffmpegArea = _storage + "MMSTranscoderWorkingAreaRepository/ffmpeg/";
    
    string nginxArea = _storage + "MMSWorkingAreaRepository/nginx/";

    _profilesRootRepository = _storage + "MMSRepository/EncodingProfiles/";

    bool noErrorIfExists = true;
    bool recursive = true;
    _logger->info(__FILEREF__ + "Creating directory (if needed)"
        + ", _ingestionRootRepository: " + _ingestionRootRepository
    );
    FileIO::createDirectory(_ingestionRootRepository,
            S_IRUSR | S_IWUSR | S_IXUSR |
            S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH, noErrorIfExists, recursive);

    _logger->info(__FILEREF__ + "Creating directory (if needed)"
        + ", _mmsRootRepository: " + _mmsRootRepository
    );
    FileIO::createDirectory(_mmsRootRepository,
            S_IRUSR | S_IWUSR | S_IXUSR |
            S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH, noErrorIfExists, recursive);

    // create MMS_0000 in case it does not exist (first running of MMS)
    {
        string MMS_0000Path = _mmsRootRepository + "MMS_0000";


        _logger->info(__FILEREF__ + "Creating directory (if needed)"
            + ", MMS_0000 Path: " + MMS_0000Path
        );
        FileIO::createDirectory(MMS_0000Path,
                S_IRUSR | S_IWUSR | S_IXUSR |
                S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH, noErrorIfExists, recursive);
    }

    _logger->info(__FILEREF__ + "Creating directory (if needed)"
        + ", _downloadRootRepository: " + _downloadRootRepository
    );
    FileIO::createDirectory(_downloadRootRepository,
            S_IRUSR | S_IWUSR | S_IXUSR |
            S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH, noErrorIfExists, recursive);

    _logger->info(__FILEREF__ + "Creating directory (if needed)"
        + ", _streamingRootRepository: " + _streamingRootRepository
    );
    FileIO::createDirectory(_streamingRootRepository,
            S_IRUSR | S_IWUSR | S_IXUSR |
            S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH, noErrorIfExists, recursive);

    _logger->info(__FILEREF__ + "Creating directory (if needed)"
        + ", _profilesRootRepository: " + _profilesRootRepository
    );
    FileIO::createDirectory(_profilesRootRepository,
            S_IRUSR | S_IWUSR | S_IXUSR |
            S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH, noErrorIfExists, recursive);

    _logger->info(__FILEREF__ + "Creating directory (if needed)"
        + ", _stagingRootRepository: " + _stagingRootRepository
    );
    FileIO::createDirectory(_stagingRootRepository,
            S_IRUSR | S_IWUSR | S_IXUSR |
            S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH, noErrorIfExists, recursive);

    _logger->info(__FILEREF__ + "Creating directory (if needed)"
        + ", _transcoderStagingRootRepository: " + _transcoderStagingRootRepository
    );
    FileIO::createDirectory(_transcoderStagingRootRepository,
            S_IRUSR | S_IWUSR | S_IXUSR |
            S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH, noErrorIfExists, recursive);

    _logger->info(__FILEREF__ + "Creating directory (if needed)"
        + ", ffmpegArea: " + ffmpegArea
    );
    FileIO::createDirectory(ffmpegArea,
            S_IRUSR | S_IWUSR | S_IXUSR |
            S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH, noErrorIfExists, recursive);

    _logger->info(__FILEREF__ + "Creating directory (if needed)"
        + ", nginxArea: " + nginxArea
    );
    FileIO::createDirectory(nginxArea,
            S_IRUSR | S_IWUSR | S_IXUSR 
            | S_IRGRP | S_IWGRP | S_IXGRP
            | S_IROTH | S_IWOTH | S_IXOTH, 
            noErrorIfExists, recursive);

    // Partitions staff
    {
        char pMMSPartitionName [64];
        unsigned long long ullUsedInKB;
        unsigned long long ullAvailableInKB;
        long lPercentUsed;


        lock_guard<recursive_mutex> locker(_mtMMSPartitions);

        unsigned long ulMMSPartitionsNumber = 0;
        bool mmsAvailablePartitions = true;

        _ulCurrentMMSPartitionIndex = 0;

        // inizializzare FreeSize
        while (mmsAvailablePartitions) 
        {
            string pathNameToGetFileSystemInfo(_mmsRootRepository);

            sprintf(pMMSPartitionName, "MMS_%04lu", ulMMSPartitionsNumber);

            pathNameToGetFileSystemInfo.append(pMMSPartitionName);

            try 
            {
                FileIO::getFileSystemInfo(pathNameToGetFileSystemInfo,
                        &ullUsedInKB, &ullAvailableInKB, &lPercentUsed);
            }
            catch (...) 
            {
                break;
            }

            _mmsPartitionsFreeSizeInMB.push_back(ullAvailableInKB / 1024);

            ulMMSPartitionsNumber++;
        }

        if (ulMMSPartitionsNumber == 0) {
            throw runtime_error("No MMS partition found");
        }

        refreshPartitionsFreeSizes(-1);
    }
}

MMSStorage::~MMSStorage(void) {
}

string MMSStorage::getMMSRootRepository(void) {
    return _mmsRootRepository;
}

string MMSStorage::getStreamingRootRepository(void) {
    return _streamingRootRepository;
}

string MMSStorage::getDownloadRootRepository(void) {
    return _downloadRootRepository;
}

string MMSStorage::getIngestionRootRepository(void) {
    return _ingestionRootRepository;
}

tuple<int64_t, string, string, int64_t, string> MMSStorage::getPhysicalPath(
	int64_t mediaItemKey, int64_t encodingProfileKey)
{
    tuple<int64_t, int, shared_ptr<Workspace>, string, string, string, string, int64_t, bool> storageDetails =
        _mmsEngineDBFacade->getStorageDetails(mediaItemKey, encodingProfileKey);

    int64_t physicalPathKey;
    int mmsPartitionNumber;
    shared_ptr<Workspace> workspace;
    string relativePath;
    string fileName;
    int64_t sizeInBytes;
    string deliveryFileName;
    string title;
	bool externalReadOnlyStorage;
    tie(physicalPathKey, mmsPartitionNumber, workspace, relativePath, 
            fileName, deliveryFileName, title, sizeInBytes, externalReadOnlyStorage) = storageDetails;

	_logger->info(__FILEREF__ + "getMMSAssetPathName ..."
		+ ", mmsPartitionNumber: " + to_string(mmsPartitionNumber)
		+ ", workspaceDirectoryName: " + workspace->_directoryName
		+ ", relativePath: " + relativePath
		+ ", fileName: " + fileName
	);
    string physicalPath = getMMSAssetPathName(
		externalReadOnlyStorage,
        mmsPartitionNumber,
        workspace->_directoryName,
        relativePath,
        fileName);
    
    return make_tuple(physicalPathKey, physicalPath, fileName,
			sizeInBytes, deliveryFileName);
}

tuple<string, string, int64_t, string> MMSStorage::getPhysicalPath(
	int64_t physicalPathKey)
{    
    tuple<int64_t, int, shared_ptr<Workspace>, string, string, string, string, int64_t, bool> storageDetails =
        _mmsEngineDBFacade->getStorageDetails(physicalPathKey);

    int mmsPartitionNumber;
    shared_ptr<Workspace> workspace;
    string relativePath;
    string fileName;
    string deliveryFileName;
    string title;
    int64_t sizeInBytes;
	bool externalReadOnlyStorage;
    tie(ignore, mmsPartitionNumber, workspace, relativePath, fileName, 
            deliveryFileName, title, sizeInBytes, externalReadOnlyStorage) = storageDetails;

	_logger->info(__FILEREF__ + "getMMSAssetPathName ..."
		+ ", mmsPartitionNumber: " + to_string(mmsPartitionNumber)
		+ ", workspaceDirectoryName: " + workspace->_directoryName
		+ ", relativePath: " + relativePath
		+ ", fileName: " + fileName
	);
    string physicalPath = getMMSAssetPathName(
		externalReadOnlyStorage,
        mmsPartitionNumber,
        workspace->_directoryName,
        relativePath,
        fileName);

    return make_tuple(physicalPath, fileName, sizeInBytes, deliveryFileName);
}

pair<string, string> MMSStorage::getDeliveryURI(int64_t physicalPathKey, bool save,
		shared_ptr<Workspace> requestWorkspace)
{
    tuple<int64_t, int, shared_ptr<Workspace>, string, string, string, string, int64_t, bool> storageDetails =
        _mmsEngineDBFacade->getStorageDetails(physicalPathKey);

    int mmsPartitionNumber;
    shared_ptr<Workspace> contentWorkspace;
    string relativePath;
    string fileName;
    string deliveryFileName;
    string title;
	bool externalReadOnlyStorage;
    tie(ignore, mmsPartitionNumber, contentWorkspace, relativePath, fileName, 
            deliveryFileName, title, ignore, externalReadOnlyStorage) = storageDetails;

	if (save)
	{
		if (deliveryFileName == "")
			deliveryFileName = title;

		if (deliveryFileName != "")
		{
			// use the extension of fileName
			size_t extensionIndex = fileName.find_last_of(".");
			if (extensionIndex != string::npos)
				deliveryFileName.append(fileName.substr(extensionIndex));
		}
	}

	if (contentWorkspace->_workspaceKey != requestWorkspace->_workspaceKey)
	{
		string errorMessage =
			string ("Workspace of the content and Workspace of the requester is different")
			+ ", contentWorkspace->_workspaceKey: " + to_string(contentWorkspace->_workspaceKey)
			+ ", requestWorkspace->_workspaceKey: " + to_string(requestWorkspace->_workspaceKey)
		;
		_logger->error(__FILEREF__ + errorMessage);

		throw runtime_error(errorMessage);
	}

	string deliveryURI;
	if (externalReadOnlyStorage)
	{
		deliveryURI =
			string("/ExternalStorage_")
			+ contentWorkspace->_directoryName
			+ relativePath
			+ fileName;
	}
	else
	{
		char pMMSPartitionName [64];


		sprintf(pMMSPartitionName, "/MMS_%04d/", mmsPartitionNumber);

		deliveryURI =
			pMMSPartitionName
			+ contentWorkspace->_directoryName
			+ relativePath
			+ fileName;
	}

	return make_pair(deliveryFileName, deliveryURI);
}

tuple<int64_t, string, string> MMSStorage::getDeliveryURI(
		int64_t mediaItemKey, int64_t encodingProfileKey, bool save,
		shared_ptr<Workspace> requestWorkspace)
{
    tuple<int64_t,int,shared_ptr<Workspace>,string,string,string,string,int64_t, bool> storageDetails =
        _mmsEngineDBFacade->getStorageDetails(mediaItemKey, encodingProfileKey);

	int64_t physicalPathKey;
    int mmsPartitionNumber;
    shared_ptr<Workspace> contentWorkspace;
    string relativePath;
    string fileName;
    string deliveryFileName;
    string title;
	bool externalReadOnlyStorage;
	tie(physicalPathKey, mmsPartitionNumber, contentWorkspace, relativePath, fileName,
                    deliveryFileName, title, ignore, externalReadOnlyStorage) = storageDetails;

	if (save)
	{
		if (deliveryFileName == "")
			deliveryFileName = title;

		if (deliveryFileName != "")
		{
			// use the extension of fileName
			size_t extensionIndex = fileName.find_last_of(".");
			if (extensionIndex != string::npos)
				deliveryFileName.append(fileName.substr(extensionIndex));
		}
	}

	if (contentWorkspace->_workspaceKey != requestWorkspace->_workspaceKey)
	{
		string errorMessage =
			string ("Workspace of the content and Workspace of the requester is different")
			+ ", contentWorkspace->_workspaceKey: " + to_string(contentWorkspace->_workspaceKey)
			+ ", requestWorkspace->_workspaceKey: " + to_string(requestWorkspace->_workspaceKey)
		;
		_logger->error(__FILEREF__ + errorMessage);

		throw runtime_error(errorMessage);
	}

	string deliveryURI;
	{
		char pMMSPartitionName [64];


		sprintf(pMMSPartitionName, "/MMS_%04d/", mmsPartitionNumber);

		deliveryURI =
			pMMSPartitionName
			+ contentWorkspace->_directoryName
			+ relativePath
			+ fileName;
	}

	return make_tuple(physicalPathKey, deliveryFileName, deliveryURI);
}

void MMSStorage::removePhysicalPath(int64_t physicalPathKey)
{    
    
    try
    {
        _logger->info(__FILEREF__ + "getStorageDetailsByPhysicalPathKey ..."
            + ", physicalPathKey: " + to_string(physicalPathKey)
        );
        
        tuple<int64_t,int,shared_ptr<Workspace>,string,string,string,string,int64_t, bool> storageDetails =
            _mmsEngineDBFacade->getStorageDetails(physicalPathKey);

        int mmsPartitionNumber;
        shared_ptr<Workspace> workspace;
        string relativePath;
        string fileName;
        string deliveryFileName;
        string title;
        int64_t sizeInBytes;
		bool externalReadOnlyStorage;
        tie(ignore, mmsPartitionNumber, workspace, relativePath, fileName, 
                deliveryFileName, title, sizeInBytes, externalReadOnlyStorage) = storageDetails;

        _logger->info(__FILEREF__ + "getMMSAssetPathName ..."
            + ", mmsPartitionNumber: " + to_string(mmsPartitionNumber)
            + ", workspaceDirectoryName: " + workspace->_directoryName
            + ", relativePath: " + relativePath
            + ", fileName: " + fileName
        );
        string mmsAssetPathName = getMMSAssetPathName(
			externalReadOnlyStorage,
            mmsPartitionNumber,
            workspace->_directoryName,
            relativePath,
            fileName);
        
        _logger->info(__FILEREF__ + "removePhysicalPathKey ..."
            + ", physicalPathKey: " + to_string(physicalPathKey)
        );

        _mmsEngineDBFacade->removePhysicalPath(physicalPathKey);
        
        {
            FileIO::DirectoryEntryType_t detSourceFileType;

            detSourceFileType = FileIO::getDirectoryEntryType(mmsAssetPathName);

            if (detSourceFileType == FileIO::TOOLS_FILEIO_DIRECTORY) 
            {
                _logger->info(__FILEREF__ + "Remove directory"
                    + ", mmsAssetPathName: " + mmsAssetPathName
                );
                bool removeRecursively = true;
                FileIO::removeDirectory(mmsAssetPathName, removeRecursively);
            } 
            else if (detSourceFileType == FileIO::TOOLS_FILEIO_REGULARFILE) 
            {
                _logger->info(__FILEREF__ + "Remove file"
                    + ", mmsAssetPathName: " + mmsAssetPathName
                );
                FileIO::remove(mmsAssetPathName);
            } 
            else 
            {
                string errorMessage = string("Unexpected directory entry")
                        + ", detSourceFileType: " + to_string(detSourceFileType);
                
                _logger->error(__FILEREF__ + errorMessage);
                
                throw runtime_error(errorMessage);
            }
        }
    }
    catch(runtime_error e)
    {
        string errorMessage = string("removePhysicalPath failed")
            + ", physicalPathKey: " + to_string(physicalPathKey)
        ;
        
        _logger->info(__FILEREF__ + errorMessage);
        
        throw runtime_error(errorMessage);
    }
    catch(exception e)
    {
        string errorMessage = string("removePhysicalPath failed")
            + ", physicalPathKey: " + to_string(physicalPathKey)
        ;
        
        _logger->info(__FILEREF__ + errorMessage);
        
        throw runtime_error(errorMessage);
    }    
}

void MMSStorage::removeMediaItem(int64_t mediaItemKey)
{
    try
    {
        _logger->info(__FILEREF__ + "getAllStorageDetails ..."
            + ", mediaItemKey: " + to_string(mediaItemKey)
        );
        
        vector<tuple<int, string, string, string, bool>> allStorageDetails;
        _mmsEngineDBFacade->getAllStorageDetails(mediaItemKey, allStorageDetails);

        for (tuple<int, string, string, string, bool>& storageDetails: allStorageDetails)
        {
            int mmsPartitionNumber;
            string workspaceDirectoryName;
            string relativePath;
            string fileName;
			bool externalReadOnlyStorage;
            tie(mmsPartitionNumber, workspaceDirectoryName, relativePath,
					fileName, externalReadOnlyStorage) = storageDetails;

			if (!externalReadOnlyStorage)
            {
				_logger->info(__FILEREF__ + "getMMSAssetPathName ..."
					+ ", externalReadOnlyStorage: " + to_string(externalReadOnlyStorage)
					+ ", mmsPartitionNumber: " + to_string(mmsPartitionNumber)
					+ ", workspaceDirectoryName: " + workspaceDirectoryName
					+ ", relativePath: " + relativePath
					+ ", fileName: " + fileName
				);
				string mmsAssetPathName = getMMSAssetPathName(
					externalReadOnlyStorage,
					mmsPartitionNumber,
					workspaceDirectoryName,
					relativePath,
					fileName);

                FileIO::DirectoryEntryType_t detSourceFileType;
				bool fileExist = true;

				try
				{
					detSourceFileType = FileIO::getDirectoryEntryType(mmsAssetPathName);
				}
				catch(FileNotExisting fne)
				{
					string errorMessage = string("file/directory not present")
						+ ", mediaItemKey: " + to_string(mediaItemKey)
						+ ", mmsAssetPathName: " + mmsAssetPathName
					;
       
					_logger->warn(__FILEREF__ + errorMessage);

					fileExist = false;
				}
				catch(runtime_error e)
				{
					throw e;
				}
				catch(exception e)
				{
					throw e;
				}

				if (fileExist)
				{
					if (detSourceFileType == FileIO::TOOLS_FILEIO_DIRECTORY) 
					{
						try
						{
							_logger->info(__FILEREF__ + "Remove directory"
								+ ", mmsAssetPathName: " + mmsAssetPathName
							);
							bool removeRecursively = true;
							FileIO::removeDirectory(mmsAssetPathName, removeRecursively);
						}
						catch(DirectoryNotExisting dne)
						{
							string errorMessage = string("directory not present")
								+ ", mediaItemKey: " + to_string(mediaItemKey)
								+ ", mmsAssetPathName: " + mmsAssetPathName
							;
        
							_logger->warn(__FILEREF__ + errorMessage);
						}
						catch(runtime_error e)
						{
							throw e;
						}
						catch(exception e)
						{
							throw e;
						}
					} 
					else if (detSourceFileType == FileIO::TOOLS_FILEIO_REGULARFILE) 
					{
						try
						{
							_logger->info(__FILEREF__ + "Remove file"
								+ ", mmsAssetPathName: " + mmsAssetPathName
							);
							FileIO::remove(mmsAssetPathName);
						}
						catch(FileNotExisting fne)
						{
							string errorMessage = string("file not present")
								+ ", mediaItemKey: " + to_string(mediaItemKey)
								+ ", mmsAssetPathName: " + mmsAssetPathName
							;
        
							_logger->warn(__FILEREF__ + errorMessage);
						}
						catch(runtime_error e)
						{
							throw e;
							}
						catch(exception e)
						{
							throw e;
						}
					} 
					else 
					{
						string errorMessage = string("Unexpected directory entry")
                            + ", detSourceFileType: " + to_string(detSourceFileType);

						_logger->error(__FILEREF__ + errorMessage);

						throw runtime_error(errorMessage);
					}
				}
            }
        }

        _logger->info(__FILEREF__ + "removeMediaItem ..."
            + ", mediaItemKey: " + to_string(mediaItemKey)
        );
        _mmsEngineDBFacade->removeMediaItem(mediaItemKey);
    }
    catch(runtime_error e)
    {
        string errorMessage = string("removeMediaItem failed")
            + ", mediaItemKey: " + to_string(mediaItemKey)
            + ", exception: " + e.what()
        ;
        
        _logger->info(__FILEREF__ + errorMessage);

        throw runtime_error(errorMessage);
    }
    catch(exception e)
    {
        string errorMessage = string("removeMediaItem failed")
            + ", mediaItemKey: " + to_string(mediaItemKey)
        ;

        _logger->info(__FILEREF__ + errorMessage);
        
        throw runtime_error(errorMessage);
    }
}

string MMSStorage::getWorkspaceIngestionRepository(shared_ptr<Workspace> workspace)
{
    string workspaceIngestionDirectory = getIngestionRootRepository();
    workspaceIngestionDirectory.append(workspace->_directoryName);
    
    if (!FileIO::directoryExisting(workspaceIngestionDirectory)) 
    {
        _logger->info(__FILEREF__ + "Create directory"
            + ", workspaceIngestionDirectory: " + workspaceIngestionDirectory
        );

        bool noErrorIfExists = true;
        bool recursive = true;
        FileIO::createDirectory(workspaceIngestionDirectory,
                S_IRUSR | S_IWUSR | S_IXUSR |
                S_IRGRP | S_IXGRP |
                S_IROTH | S_IXOTH, noErrorIfExists, recursive);
    }

    return workspaceIngestionDirectory;
}

void MMSStorage::deleteWorkspace(
		shared_ptr<Workspace> workspace)
{
	{
		string workspaceIngestionDirectory = getIngestionRootRepository();
		workspaceIngestionDirectory.append(workspace->_directoryName);

        if (FileIO::directoryExisting(workspaceIngestionDirectory))
        {
			_logger->info(__FILEREF__ + "Remove directory"
				+ ", workspaceIngestionDirectory: " + workspaceIngestionDirectory
			);
			bool removeRecursively = true;
			FileIO::removeDirectory(workspaceIngestionDirectory, removeRecursively);
        }
	}

	{
		char pMMSPartitionName [64];


		lock_guard<recursive_mutex> locker(_mtMMSPartitions);

		for (unsigned long ulMMSPartitionIndex = 0;
            ulMMSPartitionIndex < _mmsPartitionsFreeSizeInMB.size();
            ulMMSPartitionIndex++) 
		{
			string mmsPartitionRepository(_mmsRootRepository);

			sprintf(pMMSPartitionName, "MMS_%04lu", ulMMSPartitionIndex);

			mmsPartitionRepository.append(pMMSPartitionName);

			if (FileIO::directoryExisting(mmsPartitionRepository))
			{
				_logger->info(__FILEREF__ + "Remove directory"
					+ ", mmsPartitionRepository: " + mmsPartitionRepository
				);
				bool removeRecursively = true;
				FileIO::removeDirectory(mmsPartitionRepository, removeRecursively);
			}
		}
	}
}

string MMSStorage::getStagingRootRepository(void) {
    return _stagingRootRepository;
}

void MMSStorage::moveContentInRepository(
        string filePathName,
        RepositoryType rtRepositoryType,
        string workspaceDirectoryName,
        bool addDateTimeToFileName)
{

    contentInRepository(
        1,
        filePathName,
        rtRepositoryType,
        workspaceDirectoryName,
        addDateTimeToFileName);
}

void MMSStorage::copyFileInRepository(
        string filePathName,
        RepositoryType rtRepositoryType,
        string workspaceDirectoryName,
        bool addDateTimeToFileName)
{

    contentInRepository(
        0,
        filePathName,
        rtRepositoryType,
        workspaceDirectoryName,
        addDateTimeToFileName);
}

string MMSStorage::getRepository(RepositoryType rtRepositoryType) 
{

    switch (rtRepositoryType) 
    {
        case RepositoryType::MMSREP_REPOSITORYTYPE_MMSCUSTOMER:
        {
            return _mmsRootRepository;
        }
        case RepositoryType::MMSREP_REPOSITORYTYPE_DOWNLOAD:
        {
            return _downloadRootRepository;
        }
        case RepositoryType::MMSREP_REPOSITORYTYPE_STREAMING:
        {
            return _streamingRootRepository;
        }
        case RepositoryType::MMSREP_REPOSITORYTYPE_STAGING:
        {
            return _stagingRootRepository;
        }
        case RepositoryType::MMSREP_REPOSITORYTYPE_INGESTION:
        {
            return _ingestionRootRepository;
        }
        default:
        {
            throw runtime_error(string("Wrong argument")
                    + ", rtRepositoryType: " + to_string(static_cast<int>(rtRepositoryType))
                    );
        }
    }
}

void MMSStorage::contentInRepository(
        unsigned long ulIsCopyOrMove,
        string contentPathName,
        RepositoryType rtRepositoryType,
        string workspaceDirectoryName,
        bool addDateTimeToFileName)
{

    tm tmDateTime;
    unsigned long ulMilliSecs;
    FileIO::DirectoryEntryType_t detSourceFileType;


    // pDestRepository includes the '/' at the end
    string metaDataFileInDestRepository(getRepository(rtRepositoryType));
    metaDataFileInDestRepository
        .append(workspaceDirectoryName)
        .append("/");

    DateTime::get_tm_LocalTime(&tmDateTime, &ulMilliSecs);

    if (rtRepositoryType == RepositoryType::MMSREP_REPOSITORYTYPE_STAGING) 
    {
        char pDateTime [64];
        bool directoryExisting;


        sprintf(pDateTime,
                "%04lu_%02lu_%02lu",
                (unsigned long) (tmDateTime. tm_year + 1900),
                (unsigned long) (tmDateTime. tm_mon + 1),
                (unsigned long) (tmDateTime. tm_mday));

        metaDataFileInDestRepository.append(pDateTime);

        if (!FileIO::directoryExisting(metaDataFileInDestRepository)) 
        {
            _logger->info(__FILEREF__ + "Create directory"
                + ", metaDataFileInDestRepository: " + metaDataFileInDestRepository
            );

            bool noErrorIfExists = true;
            bool recursive = true;
            FileIO::createDirectory(metaDataFileInDestRepository,
                    S_IRUSR | S_IWUSR | S_IXUSR |
                    S_IRGRP | S_IXGRP |
                    S_IROTH | S_IXOTH, noErrorIfExists, recursive);
        }

        metaDataFileInDestRepository.append("/");
    }

    if (addDateTimeToFileName) 
    {
        char pDateTime [64];


        sprintf(pDateTime,
                "%04lu_%02lu_%02lu_%02lu_%02lu_%02lu_%04lu_",
                (unsigned long) (tmDateTime. tm_year + 1900),
                (unsigned long) (tmDateTime. tm_mon + 1),
                (unsigned long) (tmDateTime. tm_mday),
                (unsigned long) (tmDateTime. tm_hour),
                (unsigned long) (tmDateTime. tm_min),
                (unsigned long) (tmDateTime. tm_sec),
                ulMilliSecs);

        metaDataFileInDestRepository.append(pDateTime);
    }

    size_t fileNameStart;
    string fileName;
    if ((fileNameStart = contentPathName.find_last_of('/')) == string::npos)
        fileName = contentPathName;
    else
        fileName = contentPathName.substr(fileNameStart + 1);

    metaDataFileInDestRepository.append(fileName);

    // file in case of .3gp content OR
    // directory in case of IPhone content
    detSourceFileType = FileIO::getDirectoryEntryType(contentPathName);

    if (ulIsCopyOrMove == 1) 
    {
        if (detSourceFileType == FileIO::TOOLS_FILEIO_DIRECTORY) 
        {
            _logger->info(__FILEREF__ + "Move directory"
                + ", from: " + contentPathName
                + ", to: " + metaDataFileInDestRepository
            );

            FileIO::moveDirectory(contentPathName,
                    metaDataFileInDestRepository,
                    S_IRUSR | S_IWUSR | S_IXUSR |
                    S_IRGRP | S_IXGRP |
                    S_IROTH | S_IXOTH);
        } 
        else // if (detSourceFileType == FileIO:: TOOLS_FILEIO_REGULARFILE
        {
            _logger->info(__FILEREF__ + "Move file"
                + ", from: " + contentPathName
                + ", to: " + metaDataFileInDestRepository
            );

            FileIO::moveFile(contentPathName, metaDataFileInDestRepository);
        }
    } 
    else 
    {
        if (detSourceFileType == FileIO::TOOLS_FILEIO_DIRECTORY) 
        {
            _logger->info(__FILEREF__ + "Copy directory"
                + ", from: " + contentPathName
                + ", to: " + metaDataFileInDestRepository
            );

            FileIO::copyDirectory(contentPathName,
                    metaDataFileInDestRepository,
                    S_IRUSR | S_IWUSR | S_IXUSR |
                    S_IRGRP | S_IXGRP |
                    S_IROTH | S_IXOTH);
        } 
        else 
        {
            _logger->info(__FILEREF__ + "Copy file"
                + ", from: " + contentPathName
                + ", to: " + metaDataFileInDestRepository
            );

            FileIO::copyFile(contentPathName,
                    metaDataFileInDestRepository);
        }
    }
}

string MMSStorage::moveAssetInMMSRepository(
        string sourceAssetPathName,
        string workspaceDirectoryName,
        string destinationAssetFileName,
        string relativePath,

        bool partitionIndexToBeCalculated,
        unsigned long *pulMMSPartitionIndexUsed, // OUT if bIsPartitionIndexToBeCalculated is true, IN is bIsPartitionIndexToBeCalculated is false

        bool deliveryRepositoriesToo,
        Workspace::TerritoriesHashMap& phmTerritories
        )
{
    FileIO::DirectoryEntryType_t detSourceFileType;

    if (relativePath.front() != '/' || pulMMSPartitionIndexUsed == (unsigned long *) NULL) 
    {
            throw runtime_error(string("Wrong argument")
                    + ", relativePath: " + relativePath
                    );
    }

    lock_guard<recursive_mutex> locker(_mtMMSPartitions);

    // file in case of .3gp content OR
    // directory in case of IPhone content
    detSourceFileType = FileIO::getDirectoryEntryType(sourceAssetPathName);

    if (detSourceFileType != FileIO::TOOLS_FILEIO_DIRECTORY &&
            detSourceFileType != FileIO::TOOLS_FILEIO_REGULARFILE) 
    {
        throw runtime_error("Wrong directory entry type");
    }

    if (partitionIndexToBeCalculated) 
    {
        unsigned long long ullFSEntrySizeInBytes;


        if (detSourceFileType == FileIO::TOOLS_FILEIO_DIRECTORY) 
        {
            ullFSEntrySizeInBytes = FileIO::getDirectorySizeInBytes(sourceAssetPathName);
        } 
        else // if (detSourceFileType == FileIO:: TOOLS_FILEIO_REGULARFILE)
        {
            unsigned long ulFileSizeInBytes;
            bool inCaseOfLinkHasItToBeRead = false;


            ulFileSizeInBytes = FileIO::getFileSizeInBytes(sourceAssetPathName, inCaseOfLinkHasItToBeRead);

            ullFSEntrySizeInBytes = ulFileSizeInBytes;
        }

        // find the MMS partition index
        unsigned long ulMMSPartitionIndex;
        for (ulMMSPartitionIndex = 0;
                ulMMSPartitionIndex < _mmsPartitionsFreeSizeInMB.size();
                ulMMSPartitionIndex++) 
        {
            unsigned long long mmsPartitionsFreeSizeInKB = (unsigned long long)
                ((_mmsPartitionsFreeSizeInMB [_ulCurrentMMSPartitionIndex]) * 1024);

            if (mmsPartitionsFreeSizeInKB <=
                    (_freeSpaceToLeaveInEachPartitionInMB * 1024)) 
            {
                _logger->info(__FILEREF__ + "Partition space too low"
                    + ", _ulCurrentMMSPartitionIndex: " + to_string(_ulCurrentMMSPartitionIndex)
                    + ", mmsPartitionsFreeSizeInKB: " + to_string(mmsPartitionsFreeSizeInKB)
                    + ", _freeSpaceToLeaveInEachPartitionInMB * 1024: " + to_string(_freeSpaceToLeaveInEachPartitionInMB * 1024)
                );

                if (_ulCurrentMMSPartitionIndex + 1 >= _mmsPartitionsFreeSizeInMB.size())
                    _ulCurrentMMSPartitionIndex = 0;
                else
                    _ulCurrentMMSPartitionIndex++;

                continue;
            }

            if ((unsigned long long) (mmsPartitionsFreeSizeInKB -
                    (_freeSpaceToLeaveInEachPartitionInMB * 1024)) >
                    (ullFSEntrySizeInBytes / 1024)) 
            {
                break;
            }

            if (_ulCurrentMMSPartitionIndex + 1 >= _mmsPartitionsFreeSizeInMB.size())
                _ulCurrentMMSPartitionIndex = 0;
            else
                _ulCurrentMMSPartitionIndex++;
        }

        if (ulMMSPartitionIndex == _mmsPartitionsFreeSizeInMB.size()) 
        {
            string errorMessage = string("No more space in MMS Partitions")
                    + ", ullFSEntrySizeInBytes: " + to_string(ullFSEntrySizeInBytes)
                    ;
            for (ulMMSPartitionIndex = 0;
                ulMMSPartitionIndex < _mmsPartitionsFreeSizeInMB.size();
                ulMMSPartitionIndex++) 
            {
                errorMessage +=
                    (", _mmsPartitionsFreeSizeInMB [" + to_string(ulMMSPartitionIndex) + "]: " + to_string(_mmsPartitionsFreeSizeInMB [ulMMSPartitionIndex]))
                    ;
            }

            _logger->error(__FILEREF__ + errorMessage);
            
            throw runtime_error(errorMessage);
        }

        *pulMMSPartitionIndexUsed = _ulCurrentMMSPartitionIndex;
    }

    // creating directories and build the bMMSAssetPathName
    string mmsAssetPathName;
    {
        // to create the content provider directory and the
        // territories directories (if not already existing)
        mmsAssetPathName = creatingDirsUsingTerritories(*pulMMSPartitionIndexUsed,
            relativePath, workspaceDirectoryName, deliveryRepositoriesToo,
            phmTerritories);

        mmsAssetPathName.append(destinationAssetFileName);
    }

    _logger->info(__FILEREF__ + "Selected MMS Partition for the content"
        + ", workspaceDirectoryName: " + workspaceDirectoryName
        + ", *pulMMSPartitionIndexUsed: " + to_string(*pulMMSPartitionIndexUsed)
        + ", mmsAssetPathName: " + mmsAssetPathName
        + ", _mmsPartitionsFreeSizeInMB [_ulCurrentMMSPartitionIndex]: " + to_string(_mmsPartitionsFreeSizeInMB [_ulCurrentMMSPartitionIndex])
    );

    // move the file in case of .3gp content OR
    // move the directory in case of IPhone content
    {
        if (detSourceFileType == FileIO::TOOLS_FILEIO_DIRECTORY) 
        {
            _logger->info(__FILEREF__ + "Move directory"
                + ", from: " + sourceAssetPathName
                + ", to: " + mmsAssetPathName
            );

            FileIO::moveDirectory(sourceAssetPathName,
                    mmsAssetPathName,
                    S_IRUSR | S_IWUSR | S_IXUSR |
                    S_IRGRP | S_IXGRP |
                    S_IROTH | S_IXOTH);
        } 
        else // if (detDirectoryEntryType == FileIO:: TOOLS_FILEIO_REGULARFILE)
        {
            _logger->info(__FILEREF__ + "Move file"
                + ", from: " + sourceAssetPathName
                + ", to: " + mmsAssetPathName
            );

            FileIO::moveFile(sourceAssetPathName,
                    mmsAssetPathName);
        }
    }

    // update _pullMMSPartitionsFreeSizeInMB ONLY if bIsPartitionIndexToBeCalculated
    if (partitionIndexToBeCalculated) 
    {
		refreshPartitionsFreeSizes(_ulCurrentMMSPartitionIndex); 
		/*
        unsigned long long ullUsedInKB;
        unsigned long long ullAvailableInKB;
        long lPercentUsed;


        FileIO::getFileSystemInfo(mmsAssetPathName,
                &ullUsedInKB, &ullAvailableInKB, &lPercentUsed);

        _mmsPartitionsFreeSizeInMB [_ulCurrentMMSPartitionIndex] =
            ullAvailableInKB / 1024;

        _logger->info(__FILEREF__ + "Available space"
            + ", mmsAssetPathName: " + mmsAssetPathName
            + ", _mmsPartitionsFreeSizeInMB[" + to_string(_ulCurrentMMSPartitionIndex) + "]: " + to_string(_mmsPartitionsFreeSizeInMB[_ulCurrentMMSPartitionIndex])
        );
		*/
    }


    return mmsAssetPathName;
}

string MMSStorage::getMMSAssetPathName(
		bool externalReadOnlyStorage,
        unsigned long ulPartitionNumber,
        string workspaceDirectoryName,
        string relativePath, // using '/'
        string fileName)
{
	string assetPathName;

	if (externalReadOnlyStorage)
	{
		assetPathName = getMMSRootRepository() + "ExternalStorage_" + workspaceDirectoryName
			+ relativePath + fileName;
	}
	else
	{
		char pMMSPartitionName [64];

		sprintf(pMMSPartitionName, "MMS_%04lu/", ulPartitionNumber);

		assetPathName = _mmsRootRepository;
		assetPathName
			.append(pMMSPartitionName)
			.append(workspaceDirectoryName)
			.append(relativePath)
			.append(fileName);
	}

    return assetPathName;
}

string MMSStorage::getDownloadLinkPathName(
        unsigned long ulPartitionNumber,
        string workspaceDirectoryName,
        string territoryName,
        string relativePath,
        string fileName,
        bool downloadRepositoryToo)
{

    char pMMSPartitionName [64];
    string linkPathName;

    if (downloadRepositoryToo) 
    {
        sprintf(pMMSPartitionName, "MMS_%04lu/", ulPartitionNumber);

        linkPathName = _downloadRootRepository;
        linkPathName
            .append(pMMSPartitionName)
            .append(workspaceDirectoryName)
            .append("/")
            .append(territoryName)
            .append(relativePath)
            .append(fileName);
    } 
    else
    {
        sprintf(pMMSPartitionName, "/MMS_%04lu/", ulPartitionNumber);

        linkPathName = pMMSPartitionName;
        linkPathName
            .append(workspaceDirectoryName)
            .append("/")
            .append(territoryName)
            .append(relativePath)
            .append(fileName);
    }


    return linkPathName;
}

string MMSStorage::getStreamingLinkPathName(
        unsigned long ulPartitionNumber, // IN
        string workspaceDirectoryName, // IN
        string territoryName, // IN
        string relativePath, // IN
        string fileName) // IN
{
    char pMMSPartitionName [64];
    string linkPathName;


    sprintf(pMMSPartitionName, "MMS_%04lu/", ulPartitionNumber);

    linkPathName = _streamingRootRepository;
    linkPathName
        .append(pMMSPartitionName)
        .append(workspaceDirectoryName)
        .append("/")
        .append(territoryName)
        .append(relativePath)
        .append(fileName);


    return linkPathName;
}

string MMSStorage::getStagingAssetPathName(
		// neededForTranscoder=true uses a faster file system i.e. for recording
		bool neededForTranscoder,

        string workspaceDirectoryName,
        
        // it is a prefix of the directory name because I saw two different threads got the same dir name,
        // even if the directory name generated here contains the datetime including millisecs. 
        // Same dir name created a problem when the directory was removed by one thread because 
        // it was still used by the other thread
        string directoryNamePrefix,
        string relativePath,
        string fileName,                // may be empty ("")
        long long llMediaItemKey,       // used only if fileName is ""
        long long llPhysicalPathKey,    // used only if fileName is ""
        bool removeLinuxPathIfExist)
{
    char pUniqueFileName [256];
    string localFileName;
    tm tmDateTime;
    unsigned long ulMilliSecs;
    char pDateTime [64];
    string assetPathName;


    DateTime::get_tm_LocalTime(&tmDateTime, &ulMilliSecs);

    if (fileName == "") 
    {
        sprintf(pUniqueFileName,
                "%04lu_%02lu_%02lu_%02lu_%02lu_%02lu_%04lu_%lld_%lld_%s",
                (unsigned long) (tmDateTime. tm_year + 1900),
                (unsigned long) (tmDateTime. tm_mon + 1),
                (unsigned long) (tmDateTime. tm_mday),
                (unsigned long) (tmDateTime. tm_hour),
                (unsigned long) (tmDateTime. tm_min),
                (unsigned long) (tmDateTime. tm_sec),
                ulMilliSecs,
                llMediaItemKey,
                llPhysicalPathKey,
                _hostName.c_str());

        localFileName = pUniqueFileName;
    } 
    else 
    {
        localFileName = fileName;
    }

    sprintf(pDateTime,
            "%04lu_%02lu_%02lu_%02lu_%02lu_%02lu_%04lu",
            (unsigned long) (tmDateTime. tm_year + 1900),
            (unsigned long) (tmDateTime. tm_mon + 1),
            (unsigned long) (tmDateTime. tm_mday),
            (unsigned long) (tmDateTime. tm_hour),
            (unsigned long) (tmDateTime. tm_min),
            (unsigned long) (tmDateTime. tm_sec),
            ulMilliSecs
            );

    // create the 'date' directory in staging if not exist
    {
		if (neededForTranscoder)
			assetPathName = _transcoderStagingRootRepository;
		else
			assetPathName = _stagingRootRepository;
        assetPathName
            .append(workspaceDirectoryName)
            .append("_")    // .append("/")
            .append(directoryNamePrefix)
            .append("_")
            .append(pDateTime)
            .append(relativePath);

        if (!FileIO::directoryExisting(assetPathName)) 
        {
            _logger->info(__FILEREF__ + "Create directory"
                + ", assetPathName: " + assetPathName
            );

            bool noErrorIfExists = true;
            bool recursive = true;
            FileIO::createDirectory(
                    assetPathName,
                    S_IRUSR | S_IWUSR | S_IXUSR |
                    S_IRGRP | S_IXGRP |
                    S_IROTH | S_IXOTH, noErrorIfExists, recursive);
        }
    }

    {
        assetPathName.append(localFileName);

        if (removeLinuxPathIfExist) 
        {
            FileIO::DirectoryEntryType_t detSourceFileType;

            try 
            {
                detSourceFileType = FileIO::getDirectoryEntryType(assetPathName);

                if (detSourceFileType == FileIO::TOOLS_FILEIO_DIRECTORY) 
                {
                    _logger->info(__FILEREF__ + "Remove directory"
                        + ", assetPathName: " + assetPathName
                    );
                    bool removeRecursively = true;
                    FileIO::removeDirectory(assetPathName, removeRecursively);
                } 
                else if (detSourceFileType == FileIO::TOOLS_FILEIO_REGULARFILE) 
                {
                    _logger->info(__FILEREF__ + "Remove file"
                        + ", assetPathName: " + assetPathName
                    );
                    FileIO::remove(assetPathName);
                } 
                else 
                {
                    throw runtime_error(string("Unexpected file in staging")
                            + ", assetPathName: " + assetPathName
                            );
                }
            }
            catch (...) 
            {
                //				 * the entry does not exist
                //				 *
                //				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                //					(const char *) errFileIO, __FILE__, __LINE__);
                //
                //				Error err = ToolsErrors (__FILE__, __LINE__,
                //					TOOLS_FILEIO_GETDIRECTORYENTRYTYPE_FAILED,
                //					1, (const char *) bContentPathName);
                //				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
                //					(const char *) err, __FILE__, __LINE__);
                //
                //				return err;
            }
        }
    }


    return assetPathName;
}

string MMSStorage::getEncodingProfilePathName(
        long long llEncodingProfileKey,
        string profileFileNameExtension)
{
    string encodingProfilePathName(_profilesRootRepository);

    encodingProfilePathName
        .append(to_string(llEncodingProfileKey))
        .append(profileFileNameExtension);

    return encodingProfilePathName;
}

string MMSStorage::getFFMPEGEncodingProfilePathName(
        MMSEngineDBFacade::ContentType contentType,
        long long llEncodingProfileKey)
{

    if (contentType != MMSEngineDBFacade::ContentType::Video && 
            contentType != MMSEngineDBFacade::ContentType::Audio &&
            contentType != MMSEngineDBFacade::ContentType::Image)
    {
        throw runtime_error(string("Wrong argument")
                + ", contentType: " + to_string(static_cast<int>(contentType))
                );
    }

    string encodingProfilePathName(_profilesRootRepository);

    encodingProfilePathName
        .append(to_string(llEncodingProfileKey));

    if (contentType == MMSEngineDBFacade::ContentType::Video)
    {
        encodingProfilePathName.append(".vep");
    } 
    else if (contentType == MMSEngineDBFacade::ContentType::Audio)
    {
        encodingProfilePathName.append(".aep");
    } 
    else if (contentType == MMSEngineDBFacade::ContentType::Image)
    {
        encodingProfilePathName.append(".iep");
    }


    return encodingProfilePathName;
}

unsigned long MMSStorage::getWorkspaceStorageUsage(
        string workspaceDirectoryName)
{

    unsigned long ulStorageUsageInMB;

    unsigned long ulMMSPartitionIndex;
    unsigned long long ullDirectoryUsageInBytes;
    unsigned long long ullWorkspaceStorageUsageInBytes;


    lock_guard<recursive_mutex> locker(_mtMMSPartitions);

    ullWorkspaceStorageUsageInBytes = 0;

    for (ulMMSPartitionIndex = 0;
            ulMMSPartitionIndex < _mmsPartitionsFreeSizeInMB.size();
            ulMMSPartitionIndex++) 
    {
        string contentProviderPathName = getMMSAssetPathName(
				false,	// externalReadOnlyStorage
                ulMMSPartitionIndex, workspaceDirectoryName,
                string(""), string(""));

        try 
        {
            ullDirectoryUsageInBytes = FileIO::getDirectoryUsage(contentProviderPathName);
        } 
        catch (DirectoryNotExisting d) 
        {
            continue;
        } 
        catch (...) 
        {
            throw runtime_error(string("FileIO:: getDirectoryUsage failed")
                    + ", contentProviderPathName: " + contentProviderPathName
                    );
        }

        ullWorkspaceStorageUsageInBytes += ullDirectoryUsageInBytes;
    }


    ulStorageUsageInMB = (unsigned long)
            (ullWorkspaceStorageUsageInBytes / (1024 * 1024));

    return ulStorageUsageInMB;
}

void MMSStorage::refreshPartitionsFreeSizes(long partitionIndexToBeRefreshed) 
{
    char pMMSPartitionName [64];
    unsigned long long ullUsedInKB;
    unsigned long long ullAvailableInKB;
    long lPercentUsed;


    lock_guard<recursive_mutex> locker(_mtMMSPartitions);

	if (partitionIndexToBeRefreshed == -1)
	{
		for (unsigned long ulMMSPartitionIndex = 0;
            ulMMSPartitionIndex < _mmsPartitionsFreeSizeInMB.size();
            ulMMSPartitionIndex++) 
		{
			string pathNameToGetFileSystemInfo(_mmsRootRepository);

			sprintf(pMMSPartitionName, "MMS_%04lu", ulMMSPartitionIndex);

			pathNameToGetFileSystemInfo.append(pMMSPartitionName);

			FileIO::getFileSystemInfo(pathNameToGetFileSystemInfo,
                &ullUsedInKB, &ullAvailableInKB, &lPercentUsed);

			{
				string partitionInfoPathName = pathNameToGetFileSystemInfo;

				partitionInfoPathName.append("/partitionInfo.json");
				_logger->info(__FILEREF__ + "Looking for the Partition info file"
					+ ", partitionInfoPathName: " + partitionInfoPathName
				);
				if (FileIO::fileExisting(partitionInfoPathName))
				{
					Json::Value partitionInfoJson;

					try
					{
						ifstream partitionInfoFile(partitionInfoPathName.c_str(), std::ifstream::binary);
						partitionInfoFile >> partitionInfoJson;

						int64_t maxStorageUsageInKB       = partitionInfoJson.get("maxStorageUsageInKB", 5).asInt64();
						_logger->info(__FILEREF__ + "Partition info"
							+ ", maxStorageUsageInKB: " + to_string(maxStorageUsageInKB)
							+ ", ullUsedInKB: " + to_string(ullUsedInKB)
						);

						if (maxStorageUsageInKB != -1)
						{
							if (maxStorageUsageInKB > ullUsedInKB)
								ullAvailableInKB = maxStorageUsageInKB - ullUsedInKB;
							else
								ullAvailableInKB = 0;
						}
					}
					catch(...)
					{
						_logger->error(__FILEREF__ + "wrong json partition info format"
							+ ", partitionInfoPathName: " + partitionInfoPathName
						);
					}
				}
			}

			_mmsPartitionsFreeSizeInMB[ulMMSPartitionIndex] =
                ullAvailableInKB / 1024;

			_logger->info(__FILEREF__ + "Available space"
				+ ", pathNameToGetFileSystemInfo: " + pathNameToGetFileSystemInfo
				+ ", _mmsPartitionsFreeSizeInMB[" + to_string(ulMMSPartitionIndex) + "]: "
					+ to_string(_mmsPartitionsFreeSizeInMB[ulMMSPartitionIndex])
			);
		}
	}
	else
	{
		if (partitionIndexToBeRefreshed < _mmsPartitionsFreeSizeInMB.size())
		{
			string pathNameToGetFileSystemInfo(_mmsRootRepository);

			sprintf(pMMSPartitionName, "MMS_%04ld", partitionIndexToBeRefreshed);

			pathNameToGetFileSystemInfo.append(pMMSPartitionName);

			FileIO::getFileSystemInfo(pathNameToGetFileSystemInfo,
                &ullUsedInKB, &ullAvailableInKB, &lPercentUsed);

			{
				string partitionInfoPathName = pathNameToGetFileSystemInfo;

				partitionInfoPathName.append("/partitionInfo.json");
				_logger->info(__FILEREF__ + "Looking for the Partition info file"
					+ ", partitionInfoPathName: " + partitionInfoPathName
				);
				if (FileIO::fileExisting(partitionInfoPathName))
				{
					Json::Value partitionInfoJson;

					try
					{
						ifstream partitionInfoFile(partitionInfoPathName.c_str(), std::ifstream::binary);
						partitionInfoFile >> partitionInfoJson;

						int64_t maxStorageUsageInKB       = partitionInfoJson.get("maxStorageUsageInKB", 5).asInt64();
						_logger->info(__FILEREF__ + "Partition info"
							+ ", maxStorageUsageInKB: " + to_string(maxStorageUsageInKB)
							+ ", ullUsedInKB: " + to_string(ullUsedInKB)
						);

						if (maxStorageUsageInKB != -1)
						{
							if (maxStorageUsageInKB > ullUsedInKB)
								ullAvailableInKB = maxStorageUsageInKB - ullUsedInKB;
							else
								ullAvailableInKB = 0;
						}
					}
					catch(...)
					{
						_logger->error(__FILEREF__ + "wrong json partition info format"
							+ ", partitionInfoPathName: " + partitionInfoPathName
						);
					}
				}
			}

			_mmsPartitionsFreeSizeInMB[partitionIndexToBeRefreshed] =
                ullAvailableInKB / 1024;

			_logger->info(__FILEREF__ + "Available space"
				+ ", pathNameToGetFileSystemInfo: " + pathNameToGetFileSystemInfo
				+ ", _mmsPartitionsFreeSizeInMB[" + to_string(partitionIndexToBeRefreshed) + "]: "
					+ to_string(_mmsPartitionsFreeSizeInMB[partitionIndexToBeRefreshed])
			);
		}
		else
		{
			_logger->info(__FILEREF__ + "Wrong input"
				+ ", partitionIndexToBeRefreshed: " + to_string(partitionIndexToBeRefreshed)
				+ ", _mmsPartitionsFreeSizeInMB.size(): " + to_string(_mmsPartitionsFreeSizeInMB.size())
			);
		}
	}
}

string MMSStorage::creatingDirsUsingTerritories(
        unsigned long ulCurrentMMSPartitionIndex,
        string relativePath,
        string workspaceDirectoryName,
        bool deliveryRepositoriesToo,
        Workspace::TerritoriesHashMap& phmTerritories)
{

    char pMMSPartitionName [64];


    sprintf(pMMSPartitionName, "MMS_%04lu/", ulCurrentMMSPartitionIndex);

    string mmsAssetPathName(_mmsRootRepository);
    mmsAssetPathName
        .append(pMMSPartitionName)
        .append(workspaceDirectoryName)
        .append(relativePath);

    if (!FileIO::directoryExisting(mmsAssetPathName)) 
    {
        _logger->info(__FILEREF__ + "Create directory"
            + ", mmsAssetPathName: " + mmsAssetPathName
        );

        bool noErrorIfExists = true;
        bool recursive = true;
        FileIO::createDirectory(mmsAssetPathName,
                S_IRUSR | S_IWUSR | S_IXUSR |
                S_IRGRP | S_IXGRP |
                S_IROTH | S_IXOTH, noErrorIfExists, recursive);
    }

    if (mmsAssetPathName.back() != '/')
        mmsAssetPathName.append("/");

    if (deliveryRepositoriesToo) 
    {
        Workspace::TerritoriesHashMap::iterator it;


        for (it = phmTerritories.begin(); it != phmTerritories.end(); ++it) 
        {
            string territoryName = it->second;

            string downloadAssetPathName(_downloadRootRepository);
            downloadAssetPathName
                .append(pMMSPartitionName)
                .append(workspaceDirectoryName)
                .append("/")
                .append(territoryName)
                .append(relativePath);

            string streamingAssetPathName(_streamingRootRepository);
            streamingAssetPathName
                .append(pMMSPartitionName)
                .append(workspaceDirectoryName)
                .append("/")
                .append(territoryName)
                .append(relativePath);

            if (!FileIO::directoryExisting(downloadAssetPathName)) 
            {
                _logger->info(__FILEREF__ + "Create directory"
                    + ", downloadAssetPathName: " + downloadAssetPathName
                );
                
                bool noErrorIfExists = true;
                bool recursive = true;
                FileIO::createDirectory(downloadAssetPathName,
                        S_IRUSR | S_IWUSR | S_IXUSR |
                        S_IRGRP | S_IXGRP |
                        S_IROTH | S_IXOTH, noErrorIfExists, recursive);
            }

            if (!FileIO::directoryExisting(streamingAssetPathName)) 
            {
                _logger->info(__FILEREF__ + "Create directory"
                    + ", streamingAssetPathName: " + streamingAssetPathName
                );

                bool noErrorIfExists = true;
                bool recursive = true;
                FileIO::createDirectory(streamingAssetPathName,
                        S_IRUSR | S_IWUSR | S_IXUSR |
                        S_IRGRP | S_IXGRP |
                        S_IROTH | S_IXOTH, noErrorIfExists, recursive);
            }
        }
    }


    return mmsAssetPathName;
}

