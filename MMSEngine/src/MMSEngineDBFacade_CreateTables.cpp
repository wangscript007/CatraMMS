
#include <fstream>
#include <sstream>
#include "catralibraries/FileIO.h"
#include "MMSEngineDBFacade.h"


void MMSEngineDBFacade::createTablesIfNeeded()
{
    shared_ptr<MySQLConnection> conn = nullptr;

    string      lastSQLCommand;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());

        try
        {
            // This table has to be present before the connection pool is created
            // otherwise the connection pool fails.
            // It has to be created before running the executable
            lastSQLCommand = 
                "create table if not exists MMS_TestConnection ("
                    "testConnectionKey          BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                    "constraint MMS_TestConnection_PK PRIMARY KEY (testConnectionKey))"
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);    
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }    

        try
        {
            lastSQLCommand = 
                "create table if not exists MMS_Lock ("
					"type						VARCHAR (128) NOT NULL,"
					"start						DATETIME(3) NOT NULL,"
					"end						DATETIME(3) NULL,"
					"active						TINYINT(1) NOT NULL,"
					"lastUpdate					DATETIME NOT NULL,"
					"lastDurationInMilliSecs	INT NOT NULL,"
					"owner						VARCHAR (128) NULL,"
                    "maxDurationInMinutes		INT NOT NULL,"
					"data						VARCHAR (128) NULL,"
					"constraint MMS_Lock PRIMARY KEY (type))"
					"ENGINE=InnoDB";
			statement->execute(lastSQLCommand);
		}
		catch(sql::SQLException se)
		{
			if (isRealDBError(se.what()))
			{
				_logger->error(__FILEREF__ + "SQL exception"
					+ ", lastSQLCommand: " + lastSQLCommand
					+ ", se.what(): " + se.what()
				);

				throw se;
			}
		}

        try
        {
			LockType lockType = LockType::Ingestion;
			int maxDurationInMinutes = 5;
			{
				lastSQLCommand = 
					"select count(*) from MMS_Lock where type = ?";
				shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
				int queryParameterIndex = 1;
				preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(lockType));
				shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
				if (resultSet->next())
				{
					if (resultSet->getInt64(1) == 0)
					{
						lastSQLCommand = 
							"insert into MMS_Lock (type, start, end, active, lastUpdate, lastDurationInMilliSecs, owner, maxDurationInMinutes, data) "
							"values (?, NOW(), NOW(), 0, NOW(), 0, NULL, ?, NULL)";

						shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
						int queryParameterIndex = 1;
						preparedStatement->setString(queryParameterIndex++, toString(lockType));
						preparedStatement->setInt(queryParameterIndex++, maxDurationInMinutes);

						preparedStatement->executeUpdate();
					}
				}
            }
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }    
        
        try
        {
            // maxEncodingPriority (0: low, 1: default, 2: high)
            // workspaceType: (0: Live Sessions only, 1: Ingestion + Delivery, 2: Encoding Only)
            // encodingPeriod: 0: Daily, 1: Weekly, 2: Monthly

            lastSQLCommand = 
                "create table if not exists MMS_Workspace ("
                    "workspaceKey					BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                    "creationDate                   TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
                    "name                           VARCHAR (64) NOT NULL,"
                    "directoryName                  VARCHAR (64) NOT NULL,"
                    "workspaceType                  TINYINT NOT NULL,"
                    "deliveryURL                    VARCHAR (256) NULL,"
                    "isEnabled                      TINYINT (1) NOT NULL,"
                    "maxEncodingPriority            VARCHAR (32) NOT NULL,"
                    "encodingPeriod                 VARCHAR (64) NOT NULL,"
                    "maxIngestionsNumber            INT NOT NULL,"
                    "maxStorageInMB                 INT UNSIGNED NOT NULL,"
                    "languageCode                   VARCHAR (16) NOT NULL,"
                    "constraint MMS_Workspace_PK PRIMARY KEY (workspaceKey),"
                    "UNIQUE (name))"
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);    
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }    

        try
        {
            lastSQLCommand = 
                "create unique index MMS_Workspace_idx on MMS_Workspace (directoryName)";
            statement->execute(lastSQLCommand);    
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }    

        try
        {
            lastSQLCommand = 
                "create table if not exists MMS_User ("
                    "userKey				BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                    "loginType				VARCHAR (128) NOT NULL,"
                    "name					VARCHAR (128) NULL,"
                    "eMailAddress			VARCHAR (128) NULL,"
                    "password				VARCHAR (128) NOT NULL,"
                    "country				VARCHAR (64) NULL,"
                    "creationDate			TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
                    "expirationDate			DATETIME NOT NULL,"
                    "lastSuccessfulLogin	DATETIME NULL,"
                    "constraint MMS_User_PK PRIMARY KEY (userKey), "
                    "UNIQUE KEY loginTypeAndEmailAddress (loginType, eMailAddress))"
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }

        try
        {
            lastSQLCommand = 
                "create table if not exists MMS_APIKey ("
                    "apiKey             VARCHAR (128) NOT NULL,"
                    "userKey            BIGINT UNSIGNED NOT NULL,"
                    "workspaceKey       BIGINT UNSIGNED NOT NULL,"
                    "isOwner            TINYINT (1) NOT NULL,"
                    // same in MMS_ConfirmationCode
                    "flags              SET('ADMIN', 'INGEST_WORKFLOW', 'CREATE_PROFILES', 'DELIVERY_AUTHORIZATION', 'SHARE_WORKSPACE', 'EDIT_MEDIA', 'EDIT_CONFIGURATION', 'KILL_ENCODING') NOT NULL,"
                    "creationDate		TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
                    "expirationDate		DATETIME NOT NULL,"
                    "constraint MMS_APIKey_PK PRIMARY KEY (apiKey), "
                    "constraint MMS_APIKey_FK foreign key (userKey) "
                        "references MMS_User (userKey) on delete cascade, "
                    "constraint MMS_APIKey_FK2 foreign key (workspaceKey) "
                        "references MMS_Workspace (workspaceKey) on delete cascade) "
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }

        try
        {
            lastSQLCommand = 
                "create unique index MMS_APIKey_idx on MMS_APIKey (userKey, workspaceKey)";
            statement->execute(lastSQLCommand);    
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }    

        try
        {
            lastSQLCommand = 
                "create table if not exists MMS_ConfirmationCode ("
                    "userKey                        BIGINT UNSIGNED NOT NULL,"
                    // same in MMS_APIKey
                    "flags              SET('ADMIN', 'INGEST_WORKFLOW', 'CREATE_PROFILES', 'DELIVERY_AUTHORIZATION', 'SHARE_WORKSPACE', 'EDIT_MEDIA', 'EDIT_CONFIGURATION', 'KILL_ENCODING') NOT NULL,"
                    "workspaceKey                   BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                    "isSharedWorkspace              TINYINT (1) NOT NULL,"
                    "creationDate                   TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
                    "confirmationCode               VARCHAR (64) NOT NULL,"
                    "constraint MMS_ConfirmationCode_PK PRIMARY KEY (userKey, workspaceKey),"
                    "constraint MMS_ConfirmationCode_FK foreign key (userKey) "
                        "references MMS_User (userKey) on delete cascade, "
                    "constraint MMS_ConfirmationCode_FK2 foreign key (workspaceKey) "
                        "references MMS_Workspace (workspaceKey) on delete cascade, "
                    "UNIQUE (confirmationCode))"
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);    
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }

        /*
        try
        {
            // The territories are present only if the Workspace is a 'Content Provider'.
            // In this case we could have two scenarios:
            // - workspace not having territories (we will have just one row in this table with Name set as 'default')
            // - workspace having at least one territory (we will as much rows in this table according the number of territories)
            lastSQLCommand = 
                "create table if not exists MMS_Territory ("
                    "territoryKey  				BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                    "workspaceKey  				BIGINT UNSIGNED NOT NULL,"
                    "name					VARCHAR (64) NOT NULL,"
                    "currency					VARCHAR (16) DEFAULT NULL,"
                    "constraint MMS_Territory_PK PRIMARY KEY (territoryKey),"
                    "constraint MMS_Territory_FK foreign key (workspaceKey) "
                        "references MMS_Workspace (workspaceKey) on delete cascade, "
                    "UNIQUE (workspaceKey, name))"
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }
        */    

        try
        {
            // create table MMS_WorkspaceMoreInfo. This table was created to move the fields
            //		that are updated during the ingestion from MMS_Workspace.
            //		That will avoid to put a lock in the MMS_Workspace during the update
            //		since the MMS_Workspace is a wide used table
            lastSQLCommand = 
                "create table if not exists MMS_WorkspaceMoreInfo ("
                    "workspaceKey  			BIGINT UNSIGNED NOT NULL,"
                    "currentDirLevel1			INT NOT NULL,"
                    "currentDirLevel2			INT NOT NULL,"
                    "currentDirLevel3			INT NOT NULL,"
                    "startDateTime			DATETIME NOT NULL,"
                    "endDateTime			DATETIME NOT NULL,"
                    "currentIngestionsNumber	INT NOT NULL,"
                    "constraint MMS_WorkspaceMoreInfo_PK PRIMARY KEY (workspaceKey), "
                    "constraint MMS_WorkspaceMoreInfo_FK foreign key (workspaceKey) "
                        "references MMS_Workspace (workspaceKey) on delete cascade) "
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }    

        try
        {
            lastSQLCommand = 
                "create table if not exists MMS_ContentProvider ("
                    "contentProviderKey                     BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                    "workspaceKey                            BIGINT UNSIGNED NOT NULL,"
                    "name					VARCHAR (64) NOT NULL,"
                    "constraint MMS_ContentProvider_PK PRIMARY KEY (contentProviderKey), "
                    "constraint MMS_ContentProvider_FK foreign key (workspaceKey) "
                        "references MMS_Workspace (workspaceKey) on delete cascade, "
                    "UNIQUE (workspaceKey, name))" 
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }    

        try
        {
            // Technology.
            //      0: Images (Download),
            //      1: 3GPP (Streaming+Download),
            //      2: MPEG2-TS (IPhone Streaming),
            //      3: WEBM (VP8 and Vorbis)
            //      4: WindowsMedia,
            //      5: Adobe
            // workspaceKey NULL means predefined encoding profile
            lastSQLCommand = 
                "create table if not exists MMS_EncodingProfile ("
                    "encodingProfileKey  		BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                    "workspaceKey  			BIGINT UNSIGNED NULL,"
                    "label				VARCHAR (64) NOT NULL,"
                    "contentType			VARCHAR (32) NOT NULL,"
                    "technology         		TINYINT NOT NULL,"
                    "jsonProfile    			VARCHAR (512) NOT NULL,"
                    "constraint MMS_EncodingProfile_PK PRIMARY KEY (encodingProfileKey), "
                    "constraint MMS_EncodingProfile_FK foreign key (workspaceKey) "
                        "references MMS_Workspace (workspaceKey) on delete cascade, "
                    "UNIQUE (workspaceKey, contentType, label)) "
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }    

        try
        {
            string predefinedProfilesDirectoryPath[3] = {
                _predefinedVideoProfilesDirectoryPath,
                _predefinedAudioProfilesDirectoryPath,
                _predefinedImageProfilesDirectoryPath
            };
            string videoSuffix("video");
            string audioSuffix("audio");
            string imageSuffix("image");
            
            for (string predefinedProfileDirectoryPath: predefinedProfilesDirectoryPath)
            {
                MMSEngineDBFacade::ContentType contentType;
                MMSEngineDBFacade::EncodingTechnology encodingTechnology;
                
                if (predefinedProfileDirectoryPath.size() >= videoSuffix.size() 
                        && 0 == predefinedProfileDirectoryPath.compare(predefinedProfileDirectoryPath.size()-videoSuffix.size(), 
                            videoSuffix.size(), videoSuffix))
                {
                    contentType = MMSEngineDBFacade::ContentType::Video;
                    encodingTechnology = MMSEngineDBFacade::EncodingTechnology::MP4;
                }
                else if (predefinedProfileDirectoryPath.size() >= audioSuffix.size() 
                        && 0 == predefinedProfileDirectoryPath.compare(predefinedProfileDirectoryPath.size()-audioSuffix.size(), 
                            audioSuffix.size(), audioSuffix))
                {
                    contentType = MMSEngineDBFacade::ContentType::Audio;
                    encodingTechnology = MMSEngineDBFacade::EncodingTechnology::MP4;
                }
                else if (predefinedProfileDirectoryPath.size() >= imageSuffix.size() 
                        && 0 == predefinedProfileDirectoryPath.compare(predefinedProfileDirectoryPath.size()-imageSuffix.size(), 
                            imageSuffix.size(), imageSuffix))
                {
                    contentType = MMSEngineDBFacade::ContentType::Image;
                    encodingTechnology = MMSEngineDBFacade::EncodingTechnology::Image;
                }
                else
                {
                    string errorMessage = __FILEREF__ + "Wrong predefinedProfileDirectoryPath"
                           + ", predefinedProfileDirectoryPath: " + predefinedProfileDirectoryPath
                    ;
                    _logger->error(errorMessage);

                    continue;
                }

                FileIO::DirectoryEntryType_t detDirectoryEntryType;
                shared_ptr<FileIO::Directory> directory = FileIO::openDirectory (predefinedProfileDirectoryPath + "/");

                bool scanDirectoryFinished = false;
                while (!scanDirectoryFinished)
                {
                    string directoryEntry;
                    try
                    {
                        string directoryEntry = FileIO::readDirectory (directory,
                            &detDirectoryEntryType);

                        if (detDirectoryEntryType != FileIO::TOOLS_FILEIO_REGULARFILE)
                            continue;

                        size_t extensionIndex = directoryEntry.find_last_of(".");
                        if (extensionIndex == string::npos
                                || directoryEntry.substr(extensionIndex) != ".json")
                        {
                            string errorMessage = __FILEREF__ + "Wrong filename (encoding profile) extention"
                                   + ", directoryEntry: " + directoryEntry
                            ;
                            _logger->error(errorMessage);

                            continue;
                        }

                        string jsonProfile;
                        {        
                            ifstream profileFile(predefinedProfileDirectoryPath + "/" + directoryEntry);
                            stringstream buffer;
                            buffer << profileFile.rdbuf();

                            jsonProfile = buffer.str();

                            _logger->info(__FILEREF__ + "Reading profile"
                                + ", profile pathname: " + (predefinedProfileDirectoryPath + "/" + directoryEntry)
                                + ", profile: " + jsonProfile
                            );                            
                        }

                        string label = directoryEntry.substr(0, extensionIndex);
                        {                               
                            lastSQLCommand = 
                                "select encodingProfileKey from MMS_EncodingProfile where workspaceKey is null and contentType = ? and label = ?";
                            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                            int queryParameterIndex = 1;
                            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(contentType));
                            preparedStatement->setString(queryParameterIndex++, label);
                            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
                            if (resultSet->next())
                            {
                                int64_t encodingProfileKey     = resultSet->getInt64("encodingProfileKey");

                                lastSQLCommand = 
                                    "update MMS_EncodingProfile set technology = ?, jsonProfile = ? where encodingProfileKey = ?";

                                shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                                int queryParameterIndex = 1;
                                preparedStatement->setInt(queryParameterIndex++, static_cast<int>(encodingTechnology));
                                preparedStatement->setString(queryParameterIndex++, jsonProfile);
                                preparedStatement->setInt64(queryParameterIndex++, encodingProfileKey);

                                preparedStatement->executeUpdate();
                            }
                            else
                            {
                                lastSQLCommand = 
                                    "insert into MMS_EncodingProfile ("
                                    "encodingProfileKey, workspaceKey, label, contentType, technology, jsonProfile) values ("
                                    "NULL, NULL, ?, ?, ?, ?)";

                                shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                                int queryParameterIndex = 1;
                                    preparedStatement->setString(queryParameterIndex++, label);
                                preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(contentType));
                                preparedStatement->setInt(queryParameterIndex++, static_cast<int>(encodingTechnology));
                                preparedStatement->setString(queryParameterIndex++, jsonProfile);

                                preparedStatement->executeUpdate();

                                // encodingProfileKey = getLastInsertId(conn);
                            }
                        }
                    }
                    catch(DirectoryListFinished e)
                    {
                        scanDirectoryFinished = true;
                    }
                    catch(runtime_error e)
                    {
                        string errorMessage = __FILEREF__ + "listing directory failed"
                               + ", e.what(): " + e.what()
                        ;
                        _logger->error(errorMessage);

                        throw e;
                    }
                    catch(exception e)
                    {
                        string errorMessage = __FILEREF__ + "listing directory failed"
                               + ", e.what(): " + e.what()
                        ;
                        _logger->error(errorMessage);

                        throw e;
                    }
                }

                FileIO::closeDirectory (directory);
            }
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }    
        
        try
        {
            // workspaceKey and name
            //      both NULL: global/system EncodingProfiles for the ContentType
            //      only name NULL: Workspace default EncodingProfiles for the ContentType
            //      both different by NULL: named Workspace EncodingProfiles for the ContentType
            lastSQLCommand = 
                "create table if not exists MMS_EncodingProfilesSet ("
                    "encodingProfilesSetKey  	BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                    "workspaceKey  				BIGINT UNSIGNED NOT NULL,"
                    "contentType			VARCHAR (32) NOT NULL,"
                    "label					VARCHAR (64) NOT NULL,"
                    "constraint MMS_EncodingProfilesSet_PK PRIMARY KEY (encodingProfilesSetKey)," 
                    "constraint MMS_EncodingProfilesSet_FK foreign key (workspaceKey) "
                        "references MMS_Workspace (workspaceKey) on delete cascade, "
                    "UNIQUE (workspaceKey, contentType, label)) "
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }    

        try
        {
            // EncodingProfiles associated to each family (EncodingProfilesSet)
            lastSQLCommand = 
                "create table if not exists MMS_EncodingProfilesSetMapping ("
                    "encodingProfilesSetKey  	BIGINT UNSIGNED NOT NULL,"
                    "encodingProfileKey			BIGINT UNSIGNED NOT NULL,"
                    "constraint MMS_EncodingProfilesSetMapping_PK PRIMARY KEY (encodingProfilesSetKey, encodingProfileKey), "
                    "constraint MMS_EncodingProfilesSetMapping_FK1 foreign key (encodingProfilesSetKey) "
                        "references MMS_EncodingProfilesSet (encodingProfilesSetKey) on delete cascade, "
                    "constraint MMS_EncodingProfilesSetMapping_FK2 foreign key (encodingProfileKey) "
                        "references MMS_EncodingProfile (encodingProfileKey) on delete cascade) "
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }
        
        try
        {
            lastSQLCommand = 
                "create table if not exists MMS_IngestionRoot ("
                    "ingestionRootKey           BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                    "workspaceKey               BIGINT UNSIGNED NOT NULL,"
                    "type                       VARCHAR (64) NOT NULL,"
                    "label                      VARCHAR (256) NULL,"
                    "metaDataContent			MEDIUMTEXT CHARACTER SET utf8 COLLATE utf8_bin NOT NULL,"
                    "ingestionDate              DATETIME NOT NULL,"
                    "lastUpdate                 TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
                    "status           			VARCHAR (64) NOT NULL,"
                    "constraint MMS_IngestionRoot_PK PRIMARY KEY (ingestionRootKey), "
                    "constraint MMS_IngestionRoot_FK foreign key (workspaceKey) "
                        "references MMS_Workspace (workspaceKey) on delete cascade) "	   	        				
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }

        try
        {
            lastSQLCommand = 
                "create index MMS_IngestionRoot_idx on MMS_IngestionRoot (workspaceKey, label)";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }    

        try
        {
            lastSQLCommand = 
                "create index MMS_IngestionRoot_idx2 on MMS_IngestionRoot (workspaceKey, ingestionDate)";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }    

        try
        {
            lastSQLCommand = 
                "create table if not exists MMS_IngestionJob ("
                    "ingestionJobKey  			BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                    "ingestionRootKey           BIGINT UNSIGNED NOT NULL,"
                    "label                      VARCHAR (256) NULL,"
                    "metaDataContent            TEXT CHARACTER SET utf8 COLLATE utf8_bin NOT NULL,"
                    "ingestionType              VARCHAR (64) NOT NULL,"
                    "startProcessing            DATETIME NULL,"
                    "endProcessing              DATETIME NULL,"
                    "downloadingProgress        DECIMAL(4,1) NULL,"
                    "uploadingProgress          DECIMAL(4,1) NULL,"
                    "sourceBinaryTransferred    INT NOT NULL,"
                    "processorMMS               VARCHAR (128) NULL,"
                    "status           			VARCHAR (64) NOT NULL,"
                    "errorMessage               VARCHAR (1024) NULL,"
                    "constraint MMS_IngestionJob_PK PRIMARY KEY (ingestionJobKey), "
                    "constraint MMS_IngestionJob_FK foreign key (ingestionRootKey) "
                        "references MMS_IngestionRoot (ingestionRootKey) on delete cascade) "	   	        				
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }

        try
        {
            lastSQLCommand = 
                "create index MMS_IngestionJob_idx on MMS_IngestionJob (processorMMS, ingestionType, status)";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }    

        try
        {
            lastSQLCommand = 
                "create table if not exists MMS_IngestionJobOutput ("
                    "ingestionJobKey			BIGINT UNSIGNED NOT NULL,"
                    "mediaItemKey			BIGINT UNSIGNED NOT NULL,"
                    "physicalPathKey  			BIGINT UNSIGNED NOT NULL,"
                    "UNIQUE (ingestionJobKey, mediaItemKey, physicalPathKey)) "
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }

        try
        {
            lastSQLCommand = 
                "create table if not exists MMS_IngestionJobDependency ("
                    "ingestionJobDependencyKey  	BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                    "ingestionJobKey  			BIGINT UNSIGNED NOT NULL,"
                    "dependOnSuccess                    TINYINT (1) NOT NULL,"
                    "dependOnIngestionJobKey            BIGINT UNSIGNED NULL,"
                    "orderNumber                        INT UNSIGNED NOT NULL,"
                    "constraint MMS_IngestionJob_PK PRIMARY KEY (ingestionJobDependencyKey), "
                    "constraint MMS_IngestionJobDependency_FK foreign key (ingestionJobKey) "
                        "references MMS_IngestionJob (ingestionJobKey) on delete cascade, "	   	        				
                    "constraint MMS_IngestionJobDependency_FK2 foreign key (dependOnIngestionJobKey) "
                        "references MMS_IngestionJob (ingestionJobKey) on delete cascade) "	   	        				
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }

        try
        {
            lastSQLCommand = 
                "create index MMS_IngestionJobDependency_idx on MMS_IngestionJobDependency (ingestionJobKey)";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }    

        try
        {
            bool jsonTypeSupported = isJsonTypeSupported(statement);
            
            string userDataDefinition;
            if (jsonTypeSupported)
                userDataDefinition = "JSON";
            else
                userDataDefinition = "VARCHAR (512) CHARACTER SET utf8 COLLATE utf8_bin NULL";
                
            
            // workspaceKey is the owner of the content
            // IngestedRelativePath MUST start always with '/' and ends always with '/'
            // IngestedFileName and IngestedRelativePath refer the ingested content independently
            //		if it is encoded or uncompressed
            // if EncodingProfilesSet is NULL, it means the ingested content is already encoded
            // The ContentProviderKey is the entity owner of the content. For example H3G is our workspace and EMI is the ContentProvider.
            lastSQLCommand = 
                "create table if not exists MMS_MediaItem ("
                    "mediaItemKey           BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                    "workspaceKey           BIGINT UNSIGNED NOT NULL,"
                    "contentProviderKey     BIGINT UNSIGNED NOT NULL,"
                    "title                  VARCHAR (256) CHARACTER SET utf8 COLLATE utf8_bin NOT NULL,"
                    "ingester               VARCHAR (128) NULL,"
                    "userData               " + userDataDefinition + ","
                    "deliveryFileName       VARCHAR (128) NULL,"
                    "ingestionJobKey        BIGINT UNSIGNED NOT NULL,"
                    "ingestionDate          DATETIME NOT NULL,"
                    "contentType            VARCHAR (32) NOT NULL,"
                    "startPublishing        DATETIME NOT NULL,"
                    "endPublishing          DATETIME NOT NULL,"
                    "retentionInMinutes     BIGINT UNSIGNED NOT NULL,"
                    "processorMMSForRetention	VARCHAR (128) NULL,"
                    "constraint MMS_MediaItem_PK PRIMARY KEY (mediaItemKey), "
                    "constraint MMS_MediaItem_FK foreign key (workspaceKey) "
                        "references MMS_Workspace (workspaceKey) on delete cascade, "
                    "constraint MMS_MediaItem_FK2 foreign key (contentProviderKey) "
                        "references MMS_ContentProvider (contentProviderKey)) "
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }

        try
        {
            lastSQLCommand = 
                "create index MMS_MediaItem_idx2 on MMS_MediaItem (contentType, ingestionDate)";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }

        try
        {
            lastSQLCommand = 
                "create index MMS_MediaItem_idx3 on MMS_MediaItem (contentType, startPublishing, endPublishing)";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }

        try
        {
            lastSQLCommand = 
                "create index MMS_MediaItem_idx4 on MMS_MediaItem (contentType, title)";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }

        try
        {
            lastSQLCommand = 
                "create table if not exists MMS_Tag ("
                    "tagKey			BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                    "mediaItemKey		BIGINT UNSIGNED NOT NULL,"
                    "name			VARCHAR (256) NOT NULL,"
                    "constraint MMS_Tag_PK PRIMARY KEY (tagKey), "
                    "constraint MMS_Tag_FK foreign key (mediaItemKey) "
                        "references MMS_MediaItem (mediaItemKey) on delete cascade, "
                    "UNIQUE (mediaItemKey, name)) "
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }

        try
        {
            // we cannot have two equal UniqueNames within the same workspace
            // we can have two equal UniqueNames on two different workspaces
            lastSQLCommand = 
                "create table if not exists MMS_ExternalUniqueName ("
                    "workspaceKey			BIGINT UNSIGNED NOT NULL,"
                    "uniqueName      		VARCHAR (128) CHARACTER SET utf8 COLLATE utf8_bin NOT NULL,"
                    "mediaItemKey  			BIGINT UNSIGNED NOT NULL,"
                    "constraint MMS_ExternalUniqueName_PK PRIMARY KEY (workspaceKey, uniqueName), "
                    "constraint MMS_ExternalUniqueName_FK foreign key (workspaceKey) "
                        "references MMS_Workspace (workspaceKey) on delete cascade, "
                    "constraint MMS_ExternalUniqueName_FK2 foreign key (mediaItemKey) "
                        "references MMS_MediaItem (mediaItemKey)) "
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }

        try
        {
            lastSQLCommand = 
                "create index MMS_ExternalUniqueName_idx on MMS_ExternalUniqueName (workspaceKey, mediaItemKey)";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }
        
        try
        {
            // DRM. 0: NO DRM, 1: YES DRM
            // EncodedFileName and EncodedRelativePath are NULL only if the content is un-compressed.
            //  EncodedRelativePath MUST start always with '/' and ends always with '/'
            // EncodingProfileKey will be NULL only in case of
            //      - an un-compressed video or audio
            //      - an Application
            // MMSPartitionNumber. -1: live partition, >= 0: partition for any other content
            // IsAlias (0: false): it is used for a PhysicalPath that is an alias and
            //  it really refers another existing PhysicalPath. It was introduced to manage the XLE live profile
            //  supporting really multi profiles: rtsp, hls, adobe. So for every different profiles, we will
            //  create just an alias
            lastSQLCommand = 
                "create table if not exists MMS_PhysicalPath ("
                    "physicalPathKey  			BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                    "mediaItemKey			BIGINT UNSIGNED NOT NULL,"
                    "drm	             		TINYINT NOT NULL,"
                    "fileName				VARCHAR (128) CHARACTER SET utf8 COLLATE utf8_bin NOT NULL,"
                    "relativePath			VARCHAR (256) NOT NULL,"
                    "partitionNumber			INT NULL,"
                    "sizeInBytes			BIGINT UNSIGNED NOT NULL,"
                    "encodingProfileKey			BIGINT UNSIGNED NULL,"
                    "isAlias				INT NOT NULL DEFAULT 0,"
                    "creationDate			TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
                    "constraint MMS_PhysicalPath_PK PRIMARY KEY (physicalPathKey), "
                    "constraint MMS_PhysicalPath_FK foreign key (mediaItemKey) "
                        "references MMS_MediaItem (mediaItemKey) on delete cascade, "
                    "constraint MMS_PhysicalPath_FK2 foreign key (encodingProfileKey) "
                        "references MMS_EncodingProfile (encodingProfileKey), "
                    "UNIQUE (mediaItemKey, relativePath, fileName, isAlias), "
                    "UNIQUE (mediaItemKey, encodingProfileKey)) "	// it is not possible to have the same content using the same encoding profile key
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }

        try
        {
            // that index is important for the periodical select done by 'checkPublishing'
            lastSQLCommand = 
                "create index MMS_PhysicalPath_idx2 on MMS_PhysicalPath (mediaItemKey, physicalPathKey, encodingProfileKey, partitionNumber)";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }

        try
        {
            // that index is important for the periodical select done by 'checkPublishing'
            lastSQLCommand = 
                "create index MMS_PhysicalPath_idx3 on MMS_PhysicalPath (relativePath, fileName)";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }

        try
        {
            lastSQLCommand = 
                "create table if not exists MMS_VideoItemProfile ("
                    "physicalPathKey			BIGINT UNSIGNED NOT NULL,"
                    "durationInMilliSeconds		BIGINT NULL,"
                    "bitRate            		INT NULL,"
                    "width              		INT NULL,"
                    "height             		INT NULL,"
                    "avgFrameRate			VARCHAR (64) NULL,"
                    "videoCodecName			VARCHAR (64) NULL,"
                    "videoBitRate             		INT NULL,"
                    "videoProfile			VARCHAR (128) NULL,"
                    "audioCodecName			VARCHAR (64) NULL,"
                    "audioBitRate             		INT NULL,"
                    "audioSampleRate             	INT NULL,"
                    "audioChannels             		INT NULL,"
                    "constraint MMS_VideoItemProfile_PK PRIMARY KEY (physicalPathKey), "
                    "constraint MMS_VideoItemProfile_FK foreign key (physicalPathKey) "
                        "references MMS_PhysicalPath (physicalPathKey) on delete cascade) "
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }
        
        try
        {
            lastSQLCommand = 
                "create table if not exists MMS_AudioItemProfile ("
                    "physicalPathKey			BIGINT UNSIGNED NOT NULL,"
                    "durationInMilliSeconds		BIGINT NULL,"
                    "codecName          		VARCHAR (64) NULL,"
                    "bitRate             		INT NULL,"
                    "sampleRate                  	INT NULL,"
                    "channels             		INT NULL,"
                    "constraint MMS_AudioItemProfile_PK PRIMARY KEY (physicalPathKey), "
                    "constraint MMS_AudioItemProfile_FK foreign key (physicalPathKey) "
                        "references MMS_PhysicalPath (physicalPathKey) on delete cascade) "
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }
        
        try
        {
            lastSQLCommand = 
                "create table if not exists MMS_ImageItemProfile ("
                    "physicalPathKey			BIGINT UNSIGNED NOT NULL,"
                    "width				INT NOT NULL,"
                    "height				INT NOT NULL,"
                    "format                       	VARCHAR (64) NULL,"
                    "quality				INT NOT NULL,"
                    "constraint MMS_ImageItemProfile_PK PRIMARY KEY (physicalPathKey), "
                    "constraint MMS_ImageItemProfile_FK foreign key (physicalPathKey) "
                        "references MMS_PhysicalPath (physicalPathKey) on delete cascade) "
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }
        
        try
        {
            lastSQLCommand = 
                "create table if not exists MMS_Conf_YouTube ("
                    "confKey                    BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                    "workspaceKey               BIGINT UNSIGNED NOT NULL,"
                    "label                      VARCHAR (128) NOT NULL,"
                    "refreshToken               VARCHAR (128) NOT NULL,"
                    "constraint MMS_Conf_YouTube_PK PRIMARY KEY (confKey), "
                    "constraint MMS_Conf_YouTube_FK foreign key (workspaceKey) "
                        "references MMS_Workspace (workspaceKey) on delete cascade, "
                    "UNIQUE (workspaceKey, label)) "
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }
        
        try
        {
            lastSQLCommand = 
                "create table if not exists MMS_Conf_Facebook ("
                    "confKey                    BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                    "workspaceKey               BIGINT UNSIGNED NOT NULL,"
                    "label                      VARCHAR (128) NOT NULL,"
                    "pageToken                  VARCHAR (256) NOT NULL,"
                    "constraint MMS_Conf_Facebook_PK PRIMARY KEY (confKey), "
                    "constraint MMS_Conf_Facebook_FK foreign key (workspaceKey) "
                        "references MMS_Workspace (workspaceKey) on delete cascade, "
                    "UNIQUE (workspaceKey, label)) "
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }
        
        try
        {
            lastSQLCommand = 
                "create table if not exists MMS_Conf_LiveURL ("
                    "confKey                    BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                    "workspaceKey               BIGINT UNSIGNED NOT NULL,"
                    "label                      VARCHAR (128) NOT NULL,"
                    "liveURL					VARCHAR (128) NOT NULL,"
                    "constraint MMS_Conf_LiveURL_PK PRIMARY KEY (confKey), "
                    "constraint MMS_Conf_LiveURL_FK foreign key (workspaceKey) "
                        "references MMS_Workspace (workspaceKey) on delete cascade, "
                    "UNIQUE (workspaceKey, label)) "
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }
        
        try
        {
            lastSQLCommand = 
                "create table if not exists MMS_Conf_FTP ("
                    "confKey                    BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                    "workspaceKey               BIGINT UNSIGNED NOT NULL,"
                    "label                      VARCHAR (128) NOT NULL,"
                    "server						VARCHAR (64) NOT NULL,"
                    "port						INT UNSIGNED NOT NULL,"
                    "userName					VARCHAR (64) NOT NULL,"
                    "password					VARCHAR (64) NOT NULL,"
                    "remoteDirectory			VARCHAR (256) NOT NULL,"
                    "constraint MMS_Conf_FTP_PK PRIMARY KEY (confKey), "
                    "constraint MMS_Conf_FTP_FK foreign key (workspaceKey) "
                        "references MMS_Workspace (workspaceKey) on delete cascade, "
                    "UNIQUE (workspaceKey, label)) "
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }
        
        try
        {
            lastSQLCommand = 
                "create table if not exists MMS_Conf_EMail ("
                    "confKey                    BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                    "workspaceKey               BIGINT UNSIGNED NOT NULL,"
                    "label                      VARCHAR (128) NOT NULL,"
                    "addresses					VARCHAR (1024) NOT NULL,"
                    "subject					VARCHAR (128) NOT NULL,"
                    "message					VARCHAR (1024) NOT NULL,"
                    "constraint MMS_Conf_EMail_PK PRIMARY KEY (confKey), "
                    "constraint MMS_Conf_EMail_FK foreign key (workspaceKey) "
                        "references MMS_Workspace (workspaceKey) on delete cascade, "
                    "UNIQUE (workspaceKey, label)) "
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }
        
        /*
        try
        {
            // Reservecredit is not NULL only in case of PayPerEvent or Bundle. In these cases, it will be 0 or 1.
            lastSQLCommand = 
                "create table if not exists MMS_DefaultTerritoryInfo ("
                    "defaultTerritoryInfoKey  			BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                    "mediaItemKey				BIGINT UNSIGNED NOT NULL,"
                    "territoryKey				BIGINT UNSIGNED NOT NULL,"
                    "startPublishing				DATETIME NOT NULL,"
                    "endPublishing				DATETIME NOT NULL,"
                    "constraint MMS_DefaultTerritoryInfo_PK PRIMARY KEY (defaultTerritoryInfoKey), "
                    "constraint MMS_DefaultTerritoryInfo_FK foreign key (mediaItemKey) "
                        "references MMS_MediaItem (mediaItemKey) on delete cascade, "
                    "constraint MMS_DefaultTerritoryInfo_FK2 foreign key (territoryKey) "
                        "references MMS_Territory (territoryKey) on delete cascade, "
                    "UNIQUE (mediaItemKey, territoryKey)) "
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }
        */
        
        /*
        try
        {
            // PublishingStatus. 0: not published, 1: published
            // In this table it is considered the publishing 'per content'.
            // In MMS_Publishing, a content is considered published if all his profiles are published.
            lastSQLCommand = 
                "create table if not exists MMS_Publishing ("
                    "publishingKey                  BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                    "mediaItemKey                   BIGINT UNSIGNED NOT NULL,"
                    "territoryKey                   BIGINT UNSIGNED NOT NULL,"
                    "startPublishing                DATETIME NOT NULL,"
                    "endPublishing                  DATETIME NOT NULL,"
                    "publishingStatus               TINYINT (1) NOT NULL,"
                    "processorMMS                   VARCHAR (128) NULL,"
                    "constraint MMS_Publishing_PK PRIMARY KEY (publishingKey), "
                    "constraint MMS_Publishing_FK foreign key (mediaItemKey) "
                        "references MMS_MediaItem (mediaItemKey) on delete cascade, "
                    "constraint MMS_Publishing_FK2 foreign key (territoryKey) "
                        "references MMS_Territory (territoryKey) on delete cascade, "
                    "UNIQUE (mediaItemKey, territoryKey)) "
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }
        
        try
        {
            // PublishingStatus. 0: not published, 1: published
            // In this table it is considered the publishing 'per content'.
            // In MMS_Publishing, a content is considered published if all his profiles are published.
            lastSQLCommand = 
                "create index MMS_Publishing_idx2 on MMS_Publishing (mediaItemKey, startPublishing, endPublishing, publishingStatus)";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }

        try
        {
            // PublishingStatus. 0: not published, 1: published
            // In this table it is considered the publishing 'per content'.
            // In MMS_Publishing, a content is considered published if all his profiles are published.
            lastSQLCommand = 
                "create index MMS_Publishing_idx3 on MMS_Publishing (publishingStatus, startPublishing, endPublishing)";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }

        try
        {
            // PublishingStatus. 0: not published, 1: published
            // In this table it is considered the publishing 'per content'.
            // In MMS_Publishing, a content is considered published if all his profiles are published.
            lastSQLCommand = 
                "create index MMS_Publishing_idx4 on MMS_Publishing (publishingStatus, endPublishing, startPublishing)";            
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }
        */

        try
        {
            // The MMS_EncodingJob table include all the contents that have to be encoded
            //  RelativePath: it is the relative path of the original uncompressed file name
            //  PhysicalPathKey: it is the physical path key of the original uncompressed file name
            //  The ContentType was added just to avoid a big join to retrieve this info
            //  ProcessorMMS is the MMSEngine processing the encoding
            //  Status.
            //      0: TOBEPROCESSED
            //      1: PROCESSING
            //      2: SUCCESS (PROCESSED)
            //      3: FAILED
            //  EncodingPriority:
            //      0: low
            //      1: default
            //      2: high
            lastSQLCommand = 
                "create table if not exists MMS_EncodingJob ("
                    "encodingJobKey  			BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                    "ingestionJobKey			BIGINT UNSIGNED NOT NULL,"
                    "type                       VARCHAR (64) NOT NULL,"
                    "parameters                 LONGTEXT NOT NULL,"
                    "encodingPriority			TINYINT NOT NULL,"
                    "encodingJobStart			TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
                    "encodingJobEnd             DATETIME NULL,"
                    "encodingProgress           INT NULL,"
                    "status           			VARCHAR (64) NOT NULL,"
                    "processorMMS               VARCHAR (128) NULL,"
                    "transcoder					VARCHAR (128) NULL,"
                    "failuresNumber           	INT NOT NULL,"
                    "constraint MMS_EncodingJob_PK PRIMARY KEY (encodingJobKey), "
                    "constraint MMS_EncodingJob_FK foreign key (ingestionJobKey) "
                        "references MMS_IngestionJob (ingestionJobKey) on delete cascade) "
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }

        try
        {
            // that index is important because it will be used by the query looking every 15 seconds if there are
            // contents to be encoded
            lastSQLCommand = 
                "create index MMS_EncodingJob_idx2 on MMS_EncodingJob (status, processorMMS, failuresNumber, encodingJobStart)";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }
    
        try
        {
            // create table MMS_RequestsAuthorization
            // MediaItemKey or ExternalKey cannot be both null
            // DeliveryMethod:
            //    0: download
            //    1: 3gpp streaming
            //    2: RTMP Flash Streaming
            //    3: WindowsMedia Streaming
            // SwitchingType: 0: None, 1: FCS, 2: FTS
            // NetworkCoverage. 0: 2G, 1: 2.5G, 2: 3G
            // IngestedPathName: [<live prefix>]/<customer name>/<territory name>/<relative path>/<content name>
            // ToBeContinued. 0 or 1
            // ForceHTTPRedirection. 0: HTML page, 1: HTTP Redirection
            // TimeToLive is measured in seconds
            lastSQLCommand = 
                "create table if not exists MMS_DeliveryAuthorization ("
                    "deliveryAuthorizationKey	BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                    "userKey    				BIGINT UNSIGNED NOT NULL,"
                    "clientIPAddress			VARCHAR (16) NULL,"
                    "physicalPathKey			BIGINT UNSIGNED NOT NULL,"
                    "deliveryURI    			VARCHAR (1024) NOT NULL,"
                    "ttlInSeconds               INT NOT NULL,"
                    "currentRetriesNumber       INT NOT NULL,"
                    "maxRetries                 INT NOT NULL,"
                    "authorizationTimestamp		TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
                    "constraint MMS_DeliveryAuthorization_PK PRIMARY KEY (deliveryAuthorizationKey), "
                    "constraint MMS_DeliveryAuthorization_FK foreign key (userKey) "
                        "references MMS_User (userKey) on delete cascade, "
                    "constraint MMS_DeliveryAuthorization_FK2 foreign key (physicalPathKey) "
                        "references MMS_PhysicalPath (physicalPathKey) on delete cascade) "
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }
        
    /*
    # create table MMS_HTTPSessions
    # One session is per userKey and UserAgent
    create table if not exists MMS_HTTPSessions (
            HTTPSessionKey				BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            userKey					BIGINT UNSIGNED NOT NULL,
            UserAgent					VARCHAR (512) NOT NULL,
            CreationDate				TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            ExpirationDate				DATETIME NOT NULL,
            constraint MMS_HTTPSessions_PK PRIMARY KEY (HTTPSessionKey), 
            constraint MMS_HTTPSessions_FK foreign key (userKey) 
                    references MMS_User (userKey) on delete cascade) 
            ENGINE=InnoDB;
    create unique index MMS_HTTPSessions_idx on MMS_HTTPSessions (userKey, UserAgent);

    # create table MMS_ReportsConfiguration
    # Type. 0: Billing Statistics, 1: Content Access, 2: Active Users,
    #		3: Streaming Statistics, 4: Usage (how to call the one in XHP today?)
    # Period. 0: Hourly, 1: Daily, 2: Weekly, 3: Monthly, 4: Yearly
    # Format. 0: CSV, 1: HTML
    # EmailAddresses. List of email addresses separated by ‘;’
    create table if not exists MMS_ReportsConfiguration (
            ReportConfigurationKey		BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            customerKey             	BIGINT UNSIGNED NOT NULL,
            Type						INT NOT NULL,
            Period						INT NOT NULL,
            TimeOfDay					INT NOT NULL,
            Format						INT NOT NULL,
            EmailAddresses				VARCHAR (1024) NULL,
            constraint MMS_ReportsConfiguration_PK PRIMARY KEY (ReportConfigurationKey), 
            constraint MMS_ReportsConfiguration_FK foreign key (customerKey) 
                    references MMS_Workspaces (customerKey) on delete cascade, 
            UNIQUE (customerKey, Type, Period)) 
            ENGINE=InnoDB;

    # create table MMS_ReportURLCategory
    create table if not exists MMS_ReportURLCategory (
            ReportURLCategoryKey		BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            name             			VARCHAR (128) NOT NULL,
            URLsPattern				VARCHAR (512) NOT NULL,
            ReportConfigurationKey		BIGINT UNSIGNED NOT NULL,
            constraint MMS_ReportURLCategory_PK PRIMARY KEY (ReportURLCategoryKey), 
            constraint MMS_ReportURLCategory_FK foreign key (ReportConfigurationKey) 
                    references MMS_ReportsConfiguration (ReportConfigurationKey) on delete cascade) 
            ENGINE=InnoDB;

    # create table MMS_CustomersSharable
    create table if not exists MMS_CustomersSharable (
            customerKeyOwner			BIGINT UNSIGNED NOT NULL,
            CustomerKeySharable		BIGINT UNSIGNED NOT NULL,
            constraint MMS_CustomersSharable_PK PRIMARY KEY (CustomerKeyOwner, CustomerKeySharable), 
            constraint MMS_CustomersSharable_FK1 foreign key (CustomerKeyOwner) 
                    references MMS_Customers (CustomerKey) on delete cascade, 
            constraint MMS_CustomersSharable_FK2 foreign key (CustomerKeySharable) 
                    references MMS_Customers (CustomerKey) on delete cascade)
            ENGINE=InnoDB;

    # create table Handsets
    # It represent a family of handsets
    # Description is something like: +H.264, +enh-aac-plus, 
    # FamilyType: 0: Delivery, 1: Music/Presentation (used by MMS Application Images)
    # SupportedDelivery: see above the definition for iSupportedDelivery_*
    create table if not exists MMS_HandsetsFamilies (
            HandsetFamilyKey  			BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            Description				VARCHAR (128) NOT NULL,
            FamilyType					INT NOT NULL,
            SupportedDelivery			INT NOT NULL DEFAULT 3,
            SmallSingleBannerProfileKey	BIGINT UNSIGNED NULL,
            MediumSingleBannerProfileKey	BIGINT UNSIGNED NULL,
            SmallIconProfileKey		BIGINT UNSIGNED NULL,
            MediumIconProfileKey		BIGINT UNSIGNED NULL,
            LargeIconProfileKey		BIGINT UNSIGNED NULL,
            constraint MMS_HandsetsFamilies_PK PRIMARY KEY (HandsetFamilyKey)) 
            ENGINE=InnoDB;

    # create table Handsets
    # The Model format is: <brand>_<Model>. i.e.: Nokia_N95
    # HTTPRedirectionForRTSP. 0 if supported, 0 if not supported
    # DRMMethod. 0: no DRM, 1: oma1forwardlock, 2: cfm, 3: cfm+
    # If HandsetFamilyKey is NULL, it means the handset is not connected to his family
    # SupportedNetworkCoverage. NULL: no specified, 0: 2G, 1: 2.5G, 2: 3G
    create table if not exists MMS_Handsets (
            HandsetKey  				BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            HandsetFamilyKey			BIGINT UNSIGNED NULL,
            MusicHandsetFamilyKey		BIGINT UNSIGNED NULL,
            Brand						VARCHAR (32) NOT NULL,
            Model						VARCHAR (32) NOT NULL,
            Alias						VARCHAR (32) NULL,
            OperativeSystem			VARCHAR (32) NOT NULL,
            HTTPRedirectionForRTSP		TINYINT (1) NOT NULL,
            DRMMethod					TINYINT NOT NULL,
            ScreenWidth				INT NOT NULL,
            ScreenHeight				INT NOT NULL,
            ScreenDensity				INT NULL,
            SupportedNetworkCoverage	INT NULL,
            CreationDate				TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            constraint MMS_Handsets_PK PRIMARY KEY (HandsetKey), 
            constraint MMS_Handsets_FK foreign key (HandsetFamilyKey) 
                    references MMS_HandsetsFamilies (HandsetFamilyKey)  on delete cascade, 
            constraint MMS_Handsets_FK2 foreign key (MusicHandsetFamilyKey) 
                    references MMS_HandsetsFamilies (HandsetFamilyKey)  on delete cascade, 
            UNIQUE (Brand, Model)) 
            ENGINE=InnoDB;

    # create table UserAgents
    # The Model format is: <brand>_<Model>. i.e.: Nokia_N95
    create table if not exists MMS_UserAgents (
            UserAgentKey  				BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            HandsetKey					BIGINT UNSIGNED NOT NULL,
            UserAgent					VARCHAR (512) NOT NULL,
            CreationDate				TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            constraint MMS_UserAgents_PK PRIMARY KEY (UserAgentKey), 
            constraint MMS_UserAgents_FK foreign key (HandsetKey) 
                    references MMS_Handsets (HandsetKey) on delete cascade, 
            UNIQUE (UserAgent)) 
            ENGINE=InnoDB;

    # create table HandsetsProfilesMapping
    # This table perform a mapping between (HandsetKey, NetworkCoverage) with EncodingProfileKey and a specified Priority
    # NetworkCoverage: 0: 2G, 1: 2.5G, 2: 3G.
    # If CustomerKey is NULL, it means the mapping is the default mapping
    # Priority: 1 (the best), 2, runtime_error e
    create table if not exists MMS_HandsetsProfilesMapping (
            HandsetProfileMappingKey	BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            CustomerKey  				BIGINT UNSIGNED NULL,
            ContentType                TINYINT NOT NULL,
            HandsetFamilyKey			BIGINT UNSIGNED NOT NULL,
            NetworkCoverage			TINYINT NOT NULL,
            EncodingProfileKey			BIGINT UNSIGNED NOT NULL,
            Priority					INT NOT NULL,
            constraint MMS_HandsetsProfilesMapping_PK primary key (HandsetProfileMappingKey), 
            constraint MMS_HandsetsProfilesMapping_FK foreign key (CustomerKey) 
                    references MMS_Customers (CustomerKey) on delete cascade, 
            constraint MMS_HandsetsProfilesMapping_FK2 foreign key (HandsetFamilyKey) 
                    references MMS_HandsetsFamilies (HandsetFamilyKey) on delete cascade, 
            constraint MMS_HandsetsProfilesMapping_FK3 foreign key (EncodingProfileKey) 
                    references MMS_EncodingProfiles (encodingProfileKey), 
            UNIQUE (CustomerKey, ContentType, HandsetFamilyKey, NetworkCoverage, EncodingProfileKey, Priority)) 
            ENGINE=InnoDB;


    # create table MMS_GenresTranslation
    create table if not exists MMS_GenresTranslation (
            TranslationKey				BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            GenreKey 	 				BIGINT UNSIGNED NOT NULL,
            Field						VARCHAR (64) NOT NULL,
            languageCode				VARCHAR (16) NOT NULL,
            Translation				TEXT CHARACTER SET utf8 COLLATE utf8_bin NOT NULL,
            constraint MMS_GenresTranslation_PK PRIMARY KEY (TranslationKey), 
            constraint MMS_GenresTranslation_FK foreign key (GenreKey) 
                    references MMS_Genres (GenreKey) on delete cascade) 
            ENGINE=InnoDB;
    create unique index MMS_GenresTranslation_idx on MMS_GenresTranslation (GenreKey, Field, languageCode);


    # create table MMS_MediaItemsTranslation
    create table if not exists MMS_MediaItemsTranslation (
            TranslationKey				BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            MediaItemKey 	 			BIGINT UNSIGNED NOT NULL,
            Field						VARCHAR (64) NOT NULL,
            languageCode				VARCHAR (16) NOT NULL,
            Translation				TEXT CHARACTER SET utf8 COLLATE utf8_bin NOT NULL,
            constraint MMS_MediaItemsTranslation_PK PRIMARY KEY (TranslationKey), 
            constraint MMS_MediaItemsTranslation_FK foreign key (MediaItemKey) 
                    references MMS_MediaItems (MediaItemKey) on delete cascade) 
            ENGINE=InnoDB;
    create unique index MMS_MediaItemsTranslation_idx on MMS_MediaItemsTranslation (MediaItemKey, Field, languageCode);

    # create table MMS_GenresMediaItemsMapping
    create table if not exists MMS_GenresMediaItemsMapping (
            GenresMediaItemsMappingKey  	BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            GenreKey						BIGINT UNSIGNED NOT NULL,
            MediaItemKey					BIGINT UNSIGNED NOT NULL,
            constraint MMS_GenresMediaItemsMapping_PK PRIMARY KEY (GenresMediaItemsMappingKey), 
            constraint MMS_GenresMediaItemsMapping_FK foreign key (GenreKey) 
                    references MMS_Genres (GenreKey) on delete cascade, 
            constraint MMS_GenresMediaItemsMapping_FK2 foreign key (MediaItemKey) 
                    references MMS_MediaItems (MediaItemKey) on delete cascade, 
            UNIQUE (GenreKey, MediaItemKey))
            ENGINE=InnoDB;

    # create table MMS_MediaItemsCustomerMapping
    # customerType could be 0 (Owner of the content) or 1 (User of the shared content)
    # MMS_MediaItemsCustomerMapping table will contain one row for the Customer Ownerof the content and one row for each shared content
    create table if not exists MMS_MediaItemsCustomerMapping (
            MediaItemKey				BIGINT UNSIGNED NOT NULL,
            CustomerKey				BIGINT UNSIGNED NOT NULL,
            customerType				TINYINT NOT NULL,
            constraint MMS_MediaItemsCustomerMapping_PK PRIMARY KEY (MediaItemKey, CustomerKey), 
            constraint MMS_MediaItemsCustomerMapping_FK1 foreign key (MediaItemKey) 
                    references MMS_MediaItems (MediaItemKey) on delete cascade, 
            constraint MMS_MediaItemsCustomerMapping_FK2 foreign key (CustomerKey) 
                    references MMS_Customers (CustomerKey) on delete cascade)
            ENGINE=InnoDB;

    # create table MediaItemsRemoved
    create table if not exists MMS_MediaItemsRemoved (
            MediaItemRemovedKey  		BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            DeletionDate				TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            MediaItemKey  				BIGINT UNSIGNED NOT NULL,
            CustomerKey				BIGINT UNSIGNED NOT NULL,
            ContentProviderKey			BIGINT UNSIGNED NOT NULL,
            DisplayName				VARCHAR (256) CHARACTER SET utf8 COLLATE utf8_bin NOT NULL,
            Ingester					VARCHAR (128) NULL,
            Description				TEXT CHARACTER SET utf8 COLLATE utf8_bin NULL,
            Country					VARCHAR (32) NULL,
            IngestionDate				TIMESTAMP,
            ContentType                TINYINT NOT NULL,
            LogicalType				VARCHAR (32) NULL,
            IngestedFileName			VARCHAR (128) CHARACTER SET utf8 COLLATE utf8_bin NULL,
            IngestedRelativePath		VARCHAR (256) NULL, 
            constraint MMS_MediaItemsRemoved_PK PRIMARY KEY (MediaItemRemovedKey)) 
            ENGINE=InnoDB;

    # create table MMS_CrossReferences
    # This table will be used to set cross references between MidiaItems
    # Type could be:
    #	0: <not specified>
    #	1: IsScreenshotOfVideo
    #	2: IsImageOfAlbum
    create table if not exists MMS_CrossReferences (
            SourceMediaItemKey		BIGINT UNSIGNED NOT NULL,
            Type					TINYINT NOT NULL,
            TargetMediaItemKey		BIGINT UNSIGNED NOT NULL,
            constraint MMS_CrossReferences_PK PRIMARY KEY (SourceMediaItemKey, TargetMediaItemKey), 
            constraint MMS_CrossReferences_FK1 foreign key (SourceMediaItemKey) 
                    references MMS_MediaItems (MediaItemKey) on delete cascade, 
            constraint MMS_CrossReferences_FK2 foreign key (TargetMediaItemKey) 
                    references MMS_MediaItems (MediaItemKey) on delete cascade) 
            ENGINE=InnoDB;
    create index MMS_CrossReferences_idx on MMS_CrossReferences (SourceMediaItemKey, TargetMediaItemKey);

    # create table MMS_3SWESubscriptions
    # This table will be used to set cross references between MidiaItems
    create table if not exists MMS_3SWESubscriptions (
            ThreeSWESubscriptionKey  	BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            name						VARCHAR (64) NOT NULL,
            constraint MMS_3SWESubscriptions_PK PRIMARY KEY (ThreeSWESubscriptionKey), 
            UNIQUE (name)) 
            ENGINE=InnoDB;

    # create table MMS_3SWESubscriptionsMapping
    # This table will be used to specified the contents to be added in an HTML presentation
    create table if not exists MMS_3SWESubscriptionsMapping (
            MediaItemKey 	 			BIGINT UNSIGNED NOT NULL,
            ThreeSWESubscriptionKey	BIGINT UNSIGNED NOT NULL,
            constraint MMS_3SWESubscriptionsMapping_PK PRIMARY KEY (MediaItemKey, ThreeSWESubscriptionKey), 
            constraint MMS_3SWESubscriptionsMapping_FK1 foreign key (MediaItemKey) 
                    references MMS_MediaItems (MediaItemKey) on delete cascade, 
            constraint MMS_3SWESubscriptionsMapping_FK2 foreign key (ThreeSWESubscriptionKey) 
                    references MMS_3SWESubscriptions (ThreeSWESubscriptionKey) on delete cascade) 
            ENGINE=InnoDB;
    create index MMS_3SWESubscriptionsMapping_idx on MMS_3SWESubscriptionsMapping (MediaItemKey, ThreeSWESubscriptionKey);

    # create table MMS_3SWEMoreChargingInfo
    # This table include the information included into the Billing definition and
    # missing in ChargingInfo table
    create table if not exists MMS_3SWEMoreChargingInfo (
            ChargingKey				BIGINT UNSIGNED NOT NULL,
            AssetType					VARCHAR (16) NOT NULL,
            AmountTax					INT NOT NULL,
            PartnerID					VARCHAR (32) NOT NULL,
            Category					VARCHAR (32) NOT NULL,
            RetailAmount				INT NOT NULL,
            RetailAmountTax			INT NOT NULL,
            RetailAmountWithSub		INT NOT NULL,
            RetailAmountTaxWithSub		INT NOT NULL,
            constraint MMS_3SWEMoreChargingInfo_PK PRIMARY KEY (ChargingKey), 
            constraint MMS_3SWEMoreChargingInfo_FK foreign key (ChargingKey) 
                    references ChargingInfo (ChargingKey) on delete cascade) 
            ENGINE=InnoDB;

    # create table MMS_Advertisements
    # territoryKey: if NULL the ads is valid for any territory
    # Type:
    #		0: pre-roll
    #		1: post-roll
    create table if not exists MMS_Advertisements (
            AdvertisementKey			BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            CustomerKey				BIGINT UNSIGNED NOT NULL,
            territoryKey				BIGINT UNSIGNED NULL,
            name						VARCHAR (32) NOT NULL,
            ContentType				TINYINT NOT NULL,
            isEnabled	                TINYINT (1) NOT NULL,
            Type						TINYINT NOT NULL,
            ValidityStart				TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
            ValidityEnd				TIMESTAMP NOT NULL,
            constraint MMS_Advertisements_PK PRIMARY KEY (AdvertisementKey), 
            constraint MMS_Advertisements_FK foreign key (CustomerKey) 
                    references MMS_Customers (CustomerKey) on delete cascade, 
            constraint MMS_Advertisements_FK2 foreign key (territoryKey) 
                    references MMS_Territory (territoryKey) on delete cascade) 
            ENGINE=InnoDB;
    create unique index MMS_Advertisements_idx on MMS_Advertisements (CustomerKey, territoryKey, name);

    # create table MMS_AdvertisementAdvertisings
    create table if not exists MMS_Advertisement_Ads (
            AdvertisementKey			BIGINT UNSIGNED NOT NULL,
            MediaItemKey				BIGINT UNSIGNED NOT NULL,
            constraint MMS_Advertisement_Ads_PK PRIMARY KEY (AdvertisementKey, MediaItemKey), 
            constraint MMS_Advertisement_Ads_FK foreign key (AdvertisementKey) 
                    references MMS_Advertisements (AdvertisementKey) on delete cascade, 
            constraint MMS_Advertisement_Ads_FK2 foreign key (MediaItemKey) 
                    references MMS_MediaItems (MediaItemKey) on delete cascade) 
            ENGINE=InnoDB;

    # create table MMS_AdvertisementContents
    create table if not exists MMS_Advertisement_Contents (
            AdvertisementKey			BIGINT UNSIGNED NOT NULL,
            MediaItemKey				BIGINT UNSIGNED NOT NULL,
            constraint MMS_Advertisement_Contents_PK PRIMARY KEY (AdvertisementKey, MediaItemKey), 
            constraint MMS_Advertisement_Contents_FK foreign key (AdvertisementKey) 
                    references MMS_Advertisements (AdvertisementKey) on delete cascade, 
            constraint MMS_Advertisement_Contents_FK2 foreign key (MediaItemKey) 
                    references MMS_MediaItems (MediaItemKey) on delete cascade) 
            ENGINE=InnoDB;



    # create table MMS_RequestsStatistics
    # Status:
    #	- 0: Received
    #	- 1: Failed (final status)
    #	- 2: redirected (final status)
    create table if not exists MMS_RequestsStatistics (
            RequestStatisticKey		BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            Status           			TINYINT (2) NOT NULL,
            ErrorMessage				VARCHAR (512) NULL,
            RedirectionURL				VARCHAR (256) NULL,
            RequestAuthorizationKey	BIGINT UNSIGNED NULL,
            CustomerKey				BIGINT UNSIGNED NULL,
            UserAgent					VARCHAR (512) NULL,
            HandsetKey					BIGINT UNSIGNED NULL,
            PartyID					VARCHAR (64) NULL,
            MSISDN						VARCHAR (32) NULL,
            PhysicalPathKey			BIGINT UNSIGNED NULL,
            AuthorizationKey			BIGINT UNSIGNED NULL,
            RequestReceivedTimestamp	TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            ElapsedSecondsToManageTheRequest	INT NULL,
            constraint MMS_RequestsStatistics_PK PRIMARY KEY (RequestStatisticKey)) 
            ENGINE=InnoDB;
    create index MMS_RequestsStatistics_idx2 on MMS_RequestsStatistics (AuthorizationKey);



    # create table MMS_MediaItemsPublishing
    # In this table it is considered the publishing 'per content'.
    # In MMS_MediaItemsPublishing, a content is considered published if all his profiles are published.
    create table if not exists MMS_MediaItemsPublishing (
            MediaItemPublishingKey		BIGINT UNSIGNED NOT NULL AUTO_INCREMENT UNIQUE,
            MediaItemKey				BIGINT UNSIGNED NOT NULL,
            territoryKey				BIGINT UNSIGNED NOT NULL,
            constraint MMS_MediaItemsPublishing_PK PRIMARY KEY (territoryKey, MediaItemKey), 
            constraint MMS_MediaItemsPublishing_FK1 foreign key (MediaItemKey) 
                    references MMS_MediaItems (MediaItemKey) on delete cascade, 
            constraint MMS_MediaItemsPublishing_FK2 foreign key (territoryKey) 
                    references MMS_Territory (territoryKey) on delete cascade)
            ENGINE=InnoDB;

    // Done by a Zoli music SQL script:
    //ALTER TABLE MMS_MediaItemsPublishing 
    //	ADD COLUMN AccessCount INT NOT NULL DEFAULT 0,
    //	ADD COLUMN Popularity DECIMAL(12, 2) NOT NULL DEFAULT 0,
    //	ADD COLUMN LastAccess DATETIME NOT NULL DEFAULT 0;
    //ALTER TABLE MMS_MediaItemsPublishing 
    //	ADD KEY idx_AccessCount (territoryKey, AccessCount),
    //	ADD KEY idx_Popularity (territoryKey, Popularity),
    //	ADD KEY idx_LastAccess (territoryKey, LastAccess);


    # create table MediaItemsBillingInfo
    # Reservecredit is not NULL only in case of PayPerEvent or Bundle. In these cases, it will be 0 or 1.
    create table if not exists MMS_MediaItemsBillingInfo (
            MediaItemsBillingInfoKey  	BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            PhysicalPathKey			BIGINT UNSIGNED NOT NULL,
            DeliveryMethod				TINYINT NOT NULL,
            territoryKey				BIGINT UNSIGNED NOT NULL,
            ChargingKey1				BIGINT UNSIGNED NOT NULL,
            ChargingKey2				BIGINT UNSIGNED NOT NULL,
            ReserveCredit				TINYINT (1) NULL,
            ExternalBillingName		VARCHAR (64) NULL,
            MaxRetries					INT NOT NULL,
            TTLInSeconds				INT NOT NULL,
            constraint MMS_MediaItemsBillingInfo_PK PRIMARY KEY (MediaItemsBillingInfoKey), 
            constraint MMS_MediaItemsBillingInfo_FK foreign key (PhysicalPathKey) 
                    references MMS_PhysicalPath (physicalPathKey) on delete cascade, 
            constraint MMS_MediaItemsBillingInfo_FK2 foreign key (territoryKey) 
                    references MMS_Territory (territoryKey) on delete cascade, 
            constraint MMS_MediaItemsBillingInfo_FK3 foreign key (ChargingKey1) 
                    references ChargingInfo (ChargingKey), 
            constraint MMS_MediaItemsBillingInfo_FK4 foreign key (ChargingKey2) 
                    references ChargingInfo (ChargingKey), 
            UNIQUE (PhysicalPathKey, DeliveryMethod, territoryKey)) 
            ENGINE=InnoDB;


    # create table MMS_AllowedDeliveryMethods
    create table if not exists MMS_AllowedDeliveryMethods (
            ContentType				TINYINT NOT NULL,
            DeliveryMethod				TINYINT NOT NULL,
            constraint MMS_AllowedDeliveryMethods_PK PRIMARY KEY (ContentType, DeliveryMethod)) 
            ENGINE=InnoDB;


    # create table MMS_Artists
    create table if not exists MMS_Artists (
            ArtistKey  				BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            name						VARCHAR (255) CHARACTER SET utf8 COLLATE utf8_bin NOT NULL,
            Country					VARCHAR (128) NULL,
            HomePageURL				VARCHAR (256) NULL,
            constraint MMS_Artists_PK PRIMARY KEY (ArtistKey), 
            UNIQUE (name)) 
            ENGINE=InnoDB;


    # create table MMS_ArtistsTranslation
    create table if not exists MMS_ArtistsTranslation (
            TranslationKey				BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            ArtistKey 	 				BIGINT UNSIGNED NOT NULL,
            Field						VARCHAR (64) NOT NULL,
            languageCode				VARCHAR (16) NOT NULL,
            Translation				TEXT CHARACTER SET utf8 COLLATE utf8_bin NOT NULL,
            constraint MMS_ArtistsTranslation_PK PRIMARY KEY (TranslationKey), 
            constraint MMS_ArtistsTranslation_FK foreign key (ArtistKey) 
                    references MMS_Artists (ArtistKey) on delete cascade) 
            ENGINE=InnoDB;
    create unique index MMS_ArtistsTranslation_idx on MMS_ArtistsTranslation (ArtistKey, Field, languageCode);

    # create table MMS_CustomerCatalogMoreInfo
    # GlobalHomePage: 0 or 1 (it specifies if his home page has to be the global one or his private home page)
    # IsPublic: 0 or 1
    create table if not exists MMS_CustomerCatalogMoreInfo (
            CustomerKey				BIGINT UNSIGNED NOT NULL,
            GlobalHomePage				INT NOT NULL,
            IsPublic					INT NOT NULL,
            CatalogImageMediaItemKey	BIGINT UNSIGNED NULL,
            HeaderIsEnabled_0					INT UNSIGNED NOT NULL,
            HeaderPresentationFunction_0		INT UNSIGNED NULL,
            HeaderFunctionParameters_0			VARCHAR (64) NULL,
            HeaderImageMediaItemKey_0			BIGINT UNSIGNED NULL,
            HeaderIsEnabled_1					INT UNSIGNED NOT NULL,
            HeaderPresentationFunction_1		INT UNSIGNED NULL,
            HeaderFunctionParameters_1			VARCHAR (64) NULL,
            HeaderImageMediaItemKey_1			BIGINT UNSIGNED NULL,
            HeaderIsEnabled_2					INT UNSIGNED NOT NULL,
            HeaderPresentationFunction_2		INT UNSIGNED NULL,
            HeaderFunctionParameters_2			VARCHAR (64) NULL,
            HeaderImageMediaItemKey_2			BIGINT UNSIGNED NULL,
            HeaderIsEnabled_3					INT UNSIGNED NOT NULL,
            HeaderPresentationFunction_3		INT UNSIGNED NULL,
            HeaderFunctionParameters_3			VARCHAR (64) NULL,
            HeaderImageMediaItemKey_3			BIGINT UNSIGNED NULL,
            HeaderIsEnabled_4					INT UNSIGNED NOT NULL,
            HeaderPresentationFunction_4		INT UNSIGNED NULL,
            HeaderFunctionParameters_4			VARCHAR (64) NULL,
            HeaderImageMediaItemKey_4			BIGINT UNSIGNED NULL,
            constraint MMS_CustomerCatalogMoreInfo_PK PRIMARY KEY (CustomerKey), 
            constraint MMS_CustomerCatalogMoreInfo_FK foreign key (CustomerKey) 
                    references MMS_Customers (CustomerKey) on delete cascade, 
            constraint MMS_CustomerCatalogMoreInfo_FK2 foreign key (CatalogImageMediaItemKey) 
                    references MMS_MediaItems (MediaItemKey) on delete cascade) 
            ENGINE=InnoDB;

    # create table MMS_PresentationWorkspaces
    # name: if NULL, it is the Production Workspace
    create table if not exists MMS_PresentationWorkspaces (
            PresentationWorkspaceKey	BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            CustomerKey				BIGINT UNSIGNED NOT NULL,
            name						VARCHAR (128) NULL,
            constraint MMS_PresentationWorkspaces_PK PRIMARY KEY (PresentationWorkspaceKey), 
            constraint MMS_PresentationWorkspaces_FK foreign key (CustomerKey) 
                    references MMS_Customers (CustomerKey) on delete cascade) 
            ENGINE=InnoDB;
    create unique index MMS_PresentationWorkspaces_idx on MMS_PresentationWorkspaces (CustomerKey, name);

    # create table MMS_PresentationItems
    # PresentationItemType: see PresentationItemType in MMSClient.h
    # NodeType:
    #	0: internal (no root type)
    #	1: MainRoot
    #	2: Root_1
    #	3: Root_2
    #	4: Root_3
    #	5: Root_4
    create table if not exists MMS_PresentationItems (
            PresentationItemKey		BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            PresentationWorkspaceKey	BIGINT UNSIGNED NOT NULL,
            ParentPresentationItemKey	BIGINT UNSIGNED NULL,
            NodeType					INT UNSIGNED NOT NULL,
            MediaItemKey				BIGINT UNSIGNED NULL,
            PresentationItemType		INT UNSIGNED NOT NULL,
            ImageMediaItemKey			BIGINT UNSIGNED NULL,
            Title						VARCHAR (128) NULL,
            SubTitle					VARCHAR (256) NULL,
            Parameter					VARCHAR (256) NULL,
            PositionIndex				INT UNSIGNED NOT NULL,
            ToolbarIsEnabled_1					INT UNSIGNED NOT NULL,
            ToolbarPresentationFunction_1		INT UNSIGNED NULL,
            ToolbarFunctionParameters_1			VARCHAR (64) NULL,
            ToolbarImageMediaItemKey_1			BIGINT UNSIGNED NULL,
            ToolbarIsEnabled_2					INT UNSIGNED NOT NULL,
            ToolbarPresentationFunction_2		INT UNSIGNED NULL,
            ToolbarFunctionParameters_2			VARCHAR (64) NULL,
            ToolbarImageMediaItemKey_2			BIGINT UNSIGNED NULL,
            ToolbarIsEnabled_3					INT UNSIGNED NOT NULL,
            ToolbarPresentationFunction_3		INT UNSIGNED NULL,
            ToolbarFunctionParameters_3			VARCHAR (64) NULL,
            ToolbarImageMediaItemKey_3			BIGINT UNSIGNED NULL,
            ToolbarIsEnabled_4					INT UNSIGNED NOT NULL,
            ToolbarPresentationFunction_4		INT UNSIGNED NULL,
            ToolbarFunctionParameters_4			VARCHAR (64) NULL,
            ToolbarImageMediaItemKey_4			BIGINT UNSIGNED NULL,
            ToolbarIsEnabled_5					INT UNSIGNED NOT NULL,
            ToolbarPresentationFunction_5		INT UNSIGNED NULL,
            ToolbarFunctionParameters_5			VARCHAR (64) NULL,
            ToolbarImageMediaItemKey_5			BIGINT UNSIGNED NULL,
            constraint MMS_PresentationItems_PK PRIMARY KEY (PresentationItemKey), 
            constraint MMS_PresentationItems_FK foreign key (PresentationWorkspaceKey) 
                    references MMS_PresentationWorkspaces (PresentationWorkspaceKey) on delete cascade, 
            constraint MMS_PresentationItems_FK2 foreign key (MediaItemKey) 
                    references MMS_MediaItems (MediaItemKey) on delete cascade, 
            constraint MMS_PresentationItems_FK3 foreign key (ImageMediaItemKey) 
                    references MMS_MediaItems (MediaItemKey) on delete cascade) 
            ENGINE=InnoDB;
    create unique index MMS_PresentationItems_idx on MMS_PresentationItems (ParentPresentationItemKey, PositionIndex);

    # create table MMS_Albums
    # AlbumKey: it is the PlaylistMediaItemKey
    create table if not exists MMS_Albums (
            AlbumKey  					BIGINT UNSIGNED NOT NULL,
            Version					VARCHAR (128) CHARACTER SET utf8 COLLATE utf8_bin NULL,
            UPC						VARCHAR (32) NOT NULL,
            Type						VARCHAR (32) NULL,
            ReleaseDate				DATETIME NULL,
            Title						VARCHAR (256) CHARACTER SET utf8 COLLATE utf8_bin NOT NULL,
            LabelName					VARCHAR (128) CHARACTER SET utf8 COLLATE utf8_bin NULL,
            Supplier					VARCHAR (64) NULL,
            SubSupplier				VARCHAR (64) NULL,
            PCopyright					VARCHAR (256) CHARACTER SET utf8 COLLATE utf8_bin NULL,
            Copyright					VARCHAR (256) CHARACTER SET utf8 COLLATE utf8_bin NULL,
            Explicit					VARCHAR (16) NULL,
            constraint MMS_Albums_PK PRIMARY KEY (AlbumKey), 
            constraint MMS_Albums_FK foreign key (AlbumKey) 
                    references MMS_MediaItems (MediaItemKey) on delete cascade) 
            ENGINE=InnoDB;
    create unique index MMS_Albums_idx2 on MMS_Albums (Supplier, UPC);

    # create table MMS_ArtistsMediaItemsMapping
    # Role:
    #	- 'NOROLE' if not present in the XML
    #	- 'MAINARTIST' if compareToIgnoreCase(main artist)
    #	- 'FEATURINGARTIST' if compareToIgnoreCase(featuring artist)
    #	- any other string in upper case without any space
    create table if not exists MMS_ArtistsMediaItemsMapping (
            ArtistsMediaItemsMappingKey  	BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            ArtistKey						BIGINT UNSIGNED NOT NULL,
            MediaItemKey					BIGINT UNSIGNED NOT NULL,
            Role							VARCHAR (128) NOT NULL,
            constraint MMS_ArtistsMediaItemsMapping_PK PRIMARY KEY (ArtistsMediaItemsMappingKey), 
            constraint MMS_ArtistsMediaItemsMapping_FK foreign key (ArtistKey) 
                    references MMS_Artists (ArtistKey) on delete cascade, 
            constraint MMS_ArtistsMediaItemsMapping_FK2 foreign key (MediaItemKey) 
                    references MMS_MediaItems (MediaItemKey) on delete cascade, 
            UNIQUE (ArtistKey, MediaItemKey, Role))
            ENGINE=InnoDB;
    create index MMS_ArtistsMediaItemsMapping_idx on MMS_ArtistsMediaItemsMapping (ArtistKey, Role, MediaItemKey);
    create index MMS_ArtistsMediaItemsMapping_idx2 on MMS_ArtistsMediaItemsMapping (MediaItemKey, Role);

    # create table MMS_ISRCMapping
    # SourceISRC: VideoItem or Ringtone -----> TargetISRC: AudioItem (Track)
    create table if not exists MMS_ISRCMapping (
            SourceISRC					VARCHAR (32) NOT NULL,
            TargetISRC					VARCHAR (32) NOT NULL,
            constraint MMS_ISRCMapping_PK PRIMARY KEY (SourceISRC, TargetISRC)) 
            ENGINE=InnoDB;
    create unique index MMS_ISRCMapping_idx on MMS_ISRCMapping (TargetISRC, SourceISRC);


    # create table MMS_SubTitlesTranslation
    create table if not exists MMS_SubTitlesTranslation (
            TranslationKey				BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            MediaItemKey 	 			BIGINT UNSIGNED NOT NULL,
            Field						VARCHAR (64) NOT NULL,
            languageCode				VARCHAR (16) NOT NULL,
            Translation				MEDIUMTEXT CHARACTER SET utf8 COLLATE utf8_bin NOT NULL,
            constraint MMS_SubTitlesTranslation_PK PRIMARY KEY (TranslationKey), 
            constraint MMS_SubTitlesTranslation_FK foreign key (MediaItemKey) 
                    references MMS_MediaItems (MediaItemKey) on delete cascade) 
            ENGINE=InnoDB;
    create unique index MMS_SubTitlesTranslation_idx on MMS_SubTitlesTranslation (MediaItemKey, Field, languageCode);


    # create table ApplicationItems
    create table if not exists MMS_ApplicationItems (
            MediaItemKey				BIGINT UNSIGNED NOT NULL,
            ReleaseDate				DATETIME NULL,
            constraint MMS_ApplicationItems_PK PRIMARY KEY (MediaItemKey), 
            constraint MMS_ApplicationItems_FK foreign key (MediaItemKey) 
                    references MMS_MediaItems (MediaItemKey) on delete cascade) 
            ENGINE=InnoDB;

    # create table MMS_ApplicationHandsetsMapping
    create table if not exists MMS_ApplicationHandsetsMapping (
            PhysicalPathKey			BIGINT UNSIGNED NOT NULL,
            HandsetKey					BIGINT UNSIGNED NOT NULL,
            JadFile					TEXT NULL,
            constraint MMS_ApplicationHandsetsMapping_PK PRIMARY KEY (PhysicalPathKey, HandsetKey), 
            constraint MMS_ApplicationHandsetsMapping_FK foreign key (PhysicalPathKey) 
                    references MMS_PhysicalPath (physicalPathKey) on delete cascade, 
            constraint MMS_ApplicationHandsetsMapping_FK2 foreign key (HandsetKey) 
                    references MMS_Handsets (HandsetKey)) 
            ENGINE=InnoDB;
    create index MMS_ApplicationHandsetsMapping_idx on MMS_ApplicationHandsetsMapping (PhysicalPathKey, HandsetKey);

    # create table PlaylistItems
    # ClipType. 0 (iContentType_Video): video, 1 (iContentType_Audio): audio
    # ScheduledStartTime: used only in case of Linear Playlist (playlist of clips plus the scheduled_start_time field)
    create table if not exists MMS_PlaylistItems (
            MediaItemKey				BIGINT UNSIGNED NOT NULL,
            ClipType					INT NOT NULL,
            ScheduledStartTime			DATETIME NULL,
            constraint MMS_PlaylistItems_PK PRIMARY KEY (MediaItemKey), 
            constraint MMS_PlaylistItems_FK foreign key (MediaItemKey) 
                    references MMS_MediaItems (MediaItemKey) on delete cascade) 
            ENGINE=InnoDB;

    # create table MMS_PlaylistClips
    # if ClipMediaItemKey is null it means the playlist item is a live and the LiveChannelURL will be initialized
    # LiveType. NULL: ClipMediaItemKey is initialized, 0: live from XAC/XLE, 1: live from the SDP file
    # ClipDuration: duration of the clip in milliseconds (NULL in case of live)
    # Seekable: 0 or 1 (NULL in case of live)
    create table if not exists MMS_PlaylistClips (
            PlaylistClipKey  			BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            PlaylistMediaItemKey		BIGINT UNSIGNED NOT NULL,
            ClipMediaItemKey			BIGINT UNSIGNED NULL,
            ClipSequence           	INT NOT NULL,
            ClipDuration           	BIGINT NULL,
            Seekable           		TINYINT NULL,
            WaitingProfileSince		DATETIME NULL,
            constraint MMS_PlaylistClips_PK PRIMARY KEY (PlaylistClipKey), 
            constraint MMS_PlaylistClips_FK foreign key (PlaylistMediaItemKey) 
                    references MMS_MediaItems (MediaItemKey) on delete cascade, 
            constraint MMS_PlaylistClips_FK2 foreign key (ClipMediaItemKey) 
                    references MMS_MediaItems (MediaItemKey)) 
            ENGINE=InnoDB;
    create unique index MMS_PlaylistClips_idx2 on MMS_PlaylistClips (PlaylistMediaItemKey, ClipSequence);

    # create table LiveItems
    # FeedType. 0: video, 1: audio
    create table if not exists MMS_LiveItems (
            MediaItemKey				BIGINT UNSIGNED NOT NULL,
            ReleaseDate				DATETIME NULL,
            FeedType					INT NOT NULL,
            constraint MMS_LiveItems_PK PRIMARY KEY (MediaItemKey), 
            constraint MMS_LiveItems_FK foreign key (MediaItemKey) 
                    references MMS_MediaItems (MediaItemKey) on delete cascade) 
            ENGINE=InnoDB;


    # create table MimeTypes
    # HandsetBrandPattern, HandsetModelPattern and HandsetOperativeSystem must be all different from null or all equal to null
    #		we cannot have i.e. HandsetBrandPattern == null and HandsetModelPattern != null
    create table if not exists MMS_MimeTypes (
            MimeTypeKey  				BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            MimeType					VARCHAR (64) NOT NULL,
            ContentType                TINYINT NOT NULL,
            HandsetBrandPattern		VARCHAR (32) NULL,
            HandsetModelPattern		VARCHAR (32) NULL,
            HandsetOperativeSystem		VARCHAR (32) NULL,
            EncodingProfileNamePattern	VARCHAR (64) NOT NULL,
            Description             	VARCHAR (64) NULL,
            constraint MMS_MimeTypes_PK PRIMARY KEY (MimeTypeKey)) 
            ENGINE=InnoDB;
         */

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", se.what(): " + se.what()
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }
    }    
}

bool MMSEngineDBFacade::isRealDBError(string exceptionMessage)
{        
    transform(
            exceptionMessage.begin(), 
            exceptionMessage.end(), 
            exceptionMessage.begin(), 
            [](unsigned char c){return tolower(c); } );

    if (exceptionMessage.find("already exists") == string::npos &&            // error (table) on mysql
            exceptionMessage.find("duplicate key name") == string::npos    // error (index) on mysql
            )
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool MMSEngineDBFacade::isJsonTypeSupported(shared_ptr<sql::Statement> statement)
{        
    bool jsonTypeSupported = true;

    try
    {
        string lastSQLCommand = 
            "create table if not exists MMS_JsonTest ("
                "userData               JSON) "
                "ENGINE=InnoDB";
        statement->execute(lastSQLCommand);
        
        lastSQLCommand = 
            "drop table MMS_JsonTest";
        statement->execute(lastSQLCommand);
    }
    catch(sql::SQLException se)
    {
        jsonTypeSupported = false;
    }
    
    return jsonTypeSupported;
}

