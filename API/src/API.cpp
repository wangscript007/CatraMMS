/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   API.cpp
 * Author: giuliano
 * 
 * Created on February 18, 2018, 1:27 AM
 */

#include <fstream>
#include <sstream>
#include <curlpp/cURLpp.hpp>
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/Exception.hpp>
#include <curlpp/Infos.hpp>
#include "API.h"

int main(int argc, char** argv) 
{

    const char* configurationPathName = getenv("MMS_CONFIGPATHNAME");
    if (configurationPathName == nullptr)
    {
        cerr << "MMS API: the MMS_CONFIGPATHNAME environment variable is not defined" << endl;
        
        return 1;
    }
    
    Json::Value configuration = APICommon::loadConfigurationFile(configurationPathName);
    
    string logPathName =  configuration["log"].get("pathName", "XXX").asString();
    bool stdout =  configuration["log"].get("stdout", "XXX").asBool();
    
    std::vector<spdlog::sink_ptr> sinks;
    auto dailySink = make_shared<spdlog::sinks::daily_file_sink_mt> (logPathName.c_str(), 11, 20);
    sinks.push_back(dailySink);
    if (stdout)
    {
        auto stdoutSink = spdlog::sinks::stdout_sink_mt::instance();
        sinks.push_back(stdoutSink);
    }
    auto logger = std::make_shared<spdlog::logger>("API", begin(sinks), end(sinks));
    // shared_ptr<spdlog::logger> logger = spdlog::stdout_logger_mt("API");
    // shared_ptr<spdlog::logger> logger = spdlog::daily_logger_mt("API", logPathName.c_str(), 11, 20);
    
    // trigger flush if the log severity is error or higher
    logger->flush_on(spdlog::level::trace);
    
    string logLevel =  configuration["log"].get("level", "XXX").asString();
    logger->info(__FILEREF__ + "Configuration item"
        + ", log->level: " + logLevel
    );
    if (logLevel == "debug")
        spdlog::set_level(spdlog::level::debug); // trace, debug, info, warn, err, critical, off
    else if (logLevel == "info")
        spdlog::set_level(spdlog::level::info); // trace, debug, info, warn, err, critical, off
    else if (logLevel == "err")
        spdlog::set_level(spdlog::level::err); // trace, debug, info, warn, err, critical, off
    string pattern =  configuration["log"].get("pattern", "XXX").asString();
    logger->info(__FILEREF__ + "Configuration item"
        + ", log->pattern: " + pattern
    );
    spdlog::set_pattern(pattern);

    // globally register the loggers so so the can be accessed using spdlog::get(logger_name)
    // spdlog::register_logger(logger);

    /*
    // the log is written in the apache error log (stderr)
    _logger = spdlog::stderr_logger_mt("API");

    // make sure only responses are written to the standard output
    spdlog::set_level(spdlog::level::trace);
    
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] [tid %t] %v");
    
    // globally register the loggers so so the can be accessed using spdlog::get(logger_name)
    // spdlog::register_logger(logger);
     */

    logger->info(__FILEREF__ + "Creating MMSEngineDBFacade"
            );
    shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade = make_shared<MMSEngineDBFacade>(
            configuration, logger);

    logger->info(__FILEREF__ + "Creating MMSStorage"
            );
    shared_ptr<MMSStorage> mmsStorage = make_shared<MMSStorage>(
            configuration, logger);

    FCGX_Init();

    int threadsNumber = configuration["api"].get("threadsNumber", 1).asInt();
    logger->info(__FILEREF__ + "Configuration item"
        + ", api->threadsNumber: " + to_string(threadsNumber)
    );

    mutex fcgiAcceptMutex;
    API::FileUploadProgressData fileUploadProgressData;
    
    vector<shared_ptr<API>> apis;
    vector<thread> apiThreads;
    
    for (int threadIndex = 0; threadIndex < threadsNumber; threadIndex++)
    {
        shared_ptr<API> api = make_shared<API>(configuration, 
                mmsEngineDBFacade,
                mmsStorage,
                &fcgiAcceptMutex,
                &fileUploadProgressData,
                logger
            );

        apis.push_back(api);
        apiThreads.push_back(thread(&API::operator(), api));
    }

    // shutdown should be managed in some way:
    // - mod_fcgid send just one shutdown, so only one thread will go down
    // - mod_fastcgi ???
    if (threadsNumber > 0)
    {
        thread fileUploadProgressThread(&API::fileUploadProgressCheck, apis[0]);
        
        apiThreads[0].join();
        
        apis[0]->stopUploadFileProgressThread();
    }

    logger->info(__FILEREF__ + "API shutdown");

    return 0;
}

API::API(Json::Value configuration, 
            shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade,
            shared_ptr<MMSStorage> mmsStorage,
            mutex* fcgiAcceptMutex,
            FileUploadProgressData* fileUploadProgressData,
            shared_ptr<spdlog::logger> logger)
    :APICommon(configuration, 
            mmsEngineDBFacade,
            mmsStorage,
            fcgiAcceptMutex,
            logger) 
{
    string encodingPriority =  _configuration["api"].get("encodingPriorityCustomerDefaultValue", "XXX").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", api->encodingPriorityCustomerDefaultValue: " + encodingPriority
    );
    if (encodingPriority == "low")
        _encodingPriorityCustomerDefaultValue = MMSEngineDBFacade::EncodingPriority::Low;
    else
        _encodingPriorityCustomerDefaultValue = MMSEngineDBFacade::EncodingPriority::Low;

    string encodingPeriod =  _configuration["api"].get("encodingPeriodCustomerDefaultValue", "XXX").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", api->encodingPeriodCustomerDefaultValue: " + encodingPeriod
    );
    if (encodingPeriod == "daily")
        _encodingPeriodCustomerDefaultValue = MMSEngineDBFacade::EncodingPeriod::Daily;
    else
        _encodingPeriodCustomerDefaultValue = MMSEngineDBFacade::EncodingPeriod::Daily;

    _maxIngestionsNumberCustomerDefaultValue = _configuration["api"].get("maxIngestionsNumberCustomerDefaultValue", "XXX").asInt();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", api->maxIngestionsNumberCustomerDefaultValue: " + to_string(_maxIngestionsNumberCustomerDefaultValue)
    );
    _maxStorageInGBCustomerDefaultValue = _configuration["api"].get("maxStorageInGBCustomerDefaultValue", "XXX").asInt();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", api->maxStorageInGBCustomerDefaultValue: " + to_string(_maxStorageInGBCustomerDefaultValue)
    );

    Json::Value api = _configuration["api"];
    _binaryBufferLength             = api["binary"].get("binaryBufferLength", "XXX").asInt();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", api->binary->binaryBufferLength: " + to_string(_binaryBufferLength)
    );
    _progressUpdatePeriodInSeconds  = api["binary"].get("progressUpdatePeriodInSeconds", "XXX").asInt();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", api->binary->progressUpdatePeriodInSeconds: " + to_string(_progressUpdatePeriodInSeconds)
    );
    _webServerPort  = api["binary"].get("webServerPort", "XXX").asInt();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", api->binary->webServerPort: " + to_string(_webServerPort)
    );
    _maxProgressCallFailures  = api["binary"].get("maxProgressCallFailures", "XXX").asInt();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", api->binary->maxProgressCallFailures: " + to_string(_maxProgressCallFailures)
    );
    _progressURI  = api["binary"].get("progressURI", "XXX").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", api->binary->progressURI: " + _progressURI
    );
    

    _fileUploadProgressData     = fileUploadProgressData;
    _fileUploadProgressThreadShutdown       = false;
}

API::~API() {
}

void API::stopUploadFileProgressThread()
{
    _fileUploadProgressThreadShutdown       = true;
    
    this_thread::sleep_for(chrono::seconds(_progressUpdatePeriodInSeconds));
}

void API::getBinaryAndResponse(
        string requestURI,
        string requestMethod,
        string xCatraMMSResumeHeader,
        unordered_map<string, string> queryParameters,
        tuple<shared_ptr<Customer>,bool,bool>& customerAndFlags,
        unsigned long contentLength
)
{
    _logger->error(__FILEREF__ + "API application is able to manage ONLY NON-Binary requests");
    
    string errorMessage = string("Internal server error");
    _logger->error(__FILEREF__ + errorMessage);

    sendError(500, errorMessage);

    throw runtime_error(errorMessage);
}

void API::manageRequestAndResponse(
        FCGX_Request& request,
        string requestURI,
        string requestMethod,
        unordered_map<string, string> queryParameters,
        tuple<shared_ptr<Customer>,bool,bool>& customerAndFlags,
        unsigned long contentLength,
        string requestBody,
        string xCatraMMSResumeHeader,
        unordered_map<string, string>& requestDetails
)
{
    
    _logger->error(__FILEREF__ + "Received manageRequestAndResponse"
        + ", requestURI: " + requestURI
        + ", requestMethod: " + requestMethod
        + ", contentLength: " + to_string(contentLength)
        + ", xCatraMMSResumeHeader: " + xCatraMMSResumeHeader
    );

    auto methodIt = queryParameters.find("method");
    if (methodIt == queryParameters.end())
    {
        string errorMessage = string("The 'method' parameter is not found");
        _logger->error(__FILEREF__ + errorMessage);

        sendError(request, 400, errorMessage);

        throw runtime_error(errorMessage);
    }
    string method = methodIt->second;

    if (method == "authorization")
    {
        // since we are here, for sure user is authorized
        
        // retrieve the ingestionJobKey from the original URL to monitor the progress file upload
        auto originalURIIt = requestDetails.find("HTTP_X_ORIGINAL_URI");
        if (originalURIIt != requestDetails.end())
        {
            // originalURI is like /catramms/binary/16
            int ingestionJobKeyIndex = originalURIIt->second.find_last_of("/");
            
            if (ingestionJobKeyIndex != string::npos)
            {
                try
                {
                    int64_t ingestionJobKey = stol(originalURIIt->second.substr(ingestionJobKeyIndex + 1));
                    double                      _lastPercentageUpdated = 0;
                    int callFailures = 0;
                    
                    lock_guard<mutex> locker(_fileUploadProgressData->_mutex);
                    
                    _fileUploadProgressData->_filesUploadProgressToBeMonitored.push_back(make_tuple(ingestionJobKey, _lastPercentageUpdated, callFailures));
                    _logger->info(__FILEREF__ + "Added upload file progress to be monitored"
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                    );
                }
                catch (exception e)
                {
                    _logger->error(__FILEREF__ + "Ingestion job key not found"
                        + ", originalURIIt->second: " + originalURIIt->second
                    );
                }
            }
        }        
        
        string responseBody;
        sendSuccess(request, 200, responseBody);
    }
    else if (method == "registerCustomer")
    {
        bool isAdminAPI = get<1>(customerAndFlags);
        if (!isAdminAPI)
        {
            string errorMessage = string("APIKey flags does not have the ADMIN permission"
                    ", isAdminAPI: " + to_string(isAdminAPI)
                    );
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 403, errorMessage);

            throw runtime_error(errorMessage);
        }
        
        registerCustomer(request, requestBody);
    }
    else if (method == "confirmCustomer")
    {
        bool isAdminAPI = get<1>(customerAndFlags);
        if (!isAdminAPI)
        {
            string errorMessage = string("APIKey flags does not have the ADMIN permission"
                    ", isAdminAPI: " + to_string(isAdminAPI)
                    );
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 403, errorMessage);

            throw runtime_error(errorMessage);
        }
        
        confirmCustomer(request, queryParameters);
    }
    else if (method == "createAPIKey")
    {
        bool isAdminAPI = get<1>(customerAndFlags);
        if (!isAdminAPI)
        {
            string errorMessage = string("APIKey flags does not have the ADMIN permission"
                    ", isAdminAPI: " + to_string(isAdminAPI)
                    );
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 403, errorMessage);

            throw runtime_error(errorMessage);
        }
        
        createAPIKey(request, queryParameters);
    }
    else if (method == "ingestContent")
    {
        bool isUserAPI = get<2>(customerAndFlags);
        if (!isUserAPI)
        {
            string errorMessage = string("APIKey flags does not have the USER permission"
                    ", isUserAPI: " + to_string(isUserAPI)
                    );
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 403, errorMessage);

            throw runtime_error(errorMessage);
        }
        
        ingestContent(request, get<0>(customerAndFlags), queryParameters, requestBody);
    }
    else if (method == "uploadBinary")
    {
        bool isUserAPI = get<2>(customerAndFlags);
        if (!isUserAPI)
        {
            string errorMessage = string("APIKey flags does not have the USER permission"
                    ", isUserAPI: " + to_string(isUserAPI)
                    );
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 403, errorMessage);

            throw runtime_error(errorMessage);
        }
                
        uploadBinary(request, requestMethod, xCatraMMSResumeHeader,
            queryParameters, customerAndFlags, contentLength);
    }
    else
    {
        string errorMessage = string("No API is matched")
            + ", requestURI: " +requestURI
            + ", requestMethod: " +requestMethod;
        _logger->error(__FILEREF__ + errorMessage);

        sendError(request, 400, errorMessage);

        throw runtime_error(errorMessage);
    }
}

void API::fileUploadProgressCheck()
{

    while (!_fileUploadProgressThreadShutdown)
    {
        this_thread::sleep_for(chrono::seconds(_progressUpdatePeriodInSeconds));
        
        lock_guard<mutex> locker(_fileUploadProgressData->_mutex);

        for (auto itr = _fileUploadProgressData->_filesUploadProgressToBeMonitored.begin(); 
                itr != _fileUploadProgressData->_filesUploadProgressToBeMonitored.end(); )
        {
            int64_t ingestionJobKey;
            double lastPercentageUpdated;
            int callFailures;

            tie(ingestionJobKey, lastPercentageUpdated, callFailures) = *itr;
            
            if (callFailures >= _maxProgressCallFailures)
            {
                _logger->error(__FILEREF__ + "fileUploadProgressCheck: remove entry because of too many call failures"
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                    + ", callFailures: " + to_string(callFailures)
                    + ", _maxProgressCallFailures: " + to_string(_maxProgressCallFailures)
                );
                itr = _fileUploadProgressData->_filesUploadProgressToBeMonitored.erase(itr);	// returns iterator to the next element
                
                continue;
            }
            
            try 
            {
                string progressURL = string("http://localhost:") + to_string(_webServerPort) + _progressURI;
                string progressIdHeader = string("X-Progress-ID: ") + to_string(ingestionJobKey);

                _logger->info(__FILEREF__ + "Call for upload progress"
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                    + ", callFailures: " + to_string(callFailures)
                    + ", progressURL: " + progressURL
                    + ", progressIdHeader: " + progressIdHeader
                );

                curlpp::Cleanup cleaner;
                curlpp::Easy request;
                ostringstream response;

                list<string> header;
                header.push_back(progressIdHeader);

                // Setting the URL to retrive.
                request.setOpt(new curlpp::options::Url(progressURL));
                request.setOpt(new curlpp::options::HttpHeader(header));
                request.setOpt(new curlpp::options::WriteStream(&response));
                request.perform();

                _logger->info(__FILEREF__ + "Call for upload progress response"
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                    + ", callFailures: " + to_string(callFailures)
                    + ", response.str(): " + response.str()
                );

                try
                {
                    Json::Value uploadProgressResponse;

                    Json::CharReaderBuilder builder;
                    Json::CharReader* reader = builder.newCharReader();
                    string errors;

                    int lastChar = response.str().back();
                    _logger->info(__FILEREF__ + "Lastchar"
                            + ", last: " + to_string(lastChar)
                    );
                    lastChar = response.str().back();
                    _logger->info(__FILEREF__ + "Lastchar"
                            + ", last: " + to_string(lastChar)
                    );
                    
                    bool parsingSuccessful = reader->parse(response.str().c_str(),
                            response.str().c_str() + response.str().size(), 
                            &uploadProgressResponse, &errors);
                    delete reader;

                    if (!parsingSuccessful)
                    {
                        string errorMessage = __FILEREF__ + "failed to parse the response body"
                                + ", errors: " + errors
                                + ", response.str(): " + response.str()
                                ;
                        _logger->error(errorMessage);

                        throw runtime_error(errorMessage);
                    }

                    string state = uploadProgressResponse.get("state", "XXX").asString();
                    int64_t received = uploadProgressResponse.get("received", "XXX").asInt64();
                    int64_t size = uploadProgressResponse.get("size", "XXX").asInt64();

                    if (state != "uploading")
                    {
                        string errorMessage = string("file upload progress. State is wrong")
                            + ", state: " + state
                            + ", ingestionJobKey: " + to_string(ingestionJobKey)
                            + ", callFailures: " + to_string(callFailures)
                            + ", progressURL: " + progressURL
                            + ", progressIdHeader: " + progressIdHeader
                        ;
                        _logger->error(__FILEREF__ + errorMessage);

                        throw runtime_error(errorMessage);
                    }
                        
                    double progress = ((double) received / (double) size) * 100;
                    // int uploadingPercentage = floorf(progress * 100) / 100;
                    // this is to have one decimal in the percentage
                    double uploadingPercentage = ((double) ((int) (progress * 10))) / 10;

                    _logger->info(__FILEREF__ + "Upload still running"
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", progress: " + to_string(progress)
                        + ", uploadingPercentage: " + to_string(uploadingPercentage)
                        + ", lastPercentageUpdated: " + to_string(lastPercentageUpdated)
                        + ", received: " + to_string(received)
                        + ", size: " + to_string(size)
                    );

                    if (lastPercentageUpdated != uploadingPercentage)
                    {
                        _logger->info(__FILEREF__ + "Update IngestionJob"
                            + ", ingestionJobKey: " + to_string(ingestionJobKey)
                            + ", uploadingPercentage: " + to_string(uploadingPercentage)
                        );                            
                        _mmsEngineDBFacade->updateIngestionJobSourceUploadingInProgress (
                            ingestionJobKey, uploadingPercentage);

                        lastPercentageUpdated = uploadingPercentage;
                    }
                }
                catch(...)
                {
                    string errorMessage = string("response Body json is not well format")
                            + ", response.str(): " + response.str()
                            ;
                    _logger->error(__FILEREF__ + errorMessage);

                    throw runtime_error(errorMessage);
                }
            }
            catch (curlpp::LogicError & e) 
            {
                _logger->error(__FILEREF__ + "Call for upload progress failed (LogicError)"
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                    + ", callFailures: " + to_string(callFailures)
                    + ", exception: " + e.what()
                );

                get<2>(*itr) = ++callFailures;
            }
            catch (curlpp::RuntimeError & e) 
            {
                _logger->error(__FILEREF__ + "Call for upload progress failed (RuntimeError)"
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                    + ", callFailures: " + to_string(callFailures)
                    + ", exception: " + e.what()
                );

                get<2>(*itr) = ++callFailures;
            }
            catch (runtime_error e)
            {
                _logger->error(__FILEREF__ + "Call for upload progress failed (runtime_error)"
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                    + ", callFailures: " + to_string(callFailures)
                    + ", exception: " + e.what()
                );

                get<2>(*itr) = ++callFailures;
            }
            catch (exception e)
            {
                _logger->error(__FILEREF__ + "Call for upload progress failed (exception)"
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                    + ", callFailures: " + to_string(callFailures)
                    + ", exception: " + e.what()
                );

                get<2>(*itr) = ++callFailures;
            }

            itr++;
        }
    }
}

void API::registerCustomer(
        FCGX_Request& request,
        string requestBody)
{
    string api = "registerCustomer";

    _logger->info(__FILEREF__ + "Received " + api
        + ", requestBody: " + requestBody
    );

    try
    {
        string name;
        string email;
        string password;
        MMSEngineDBFacade::EncodingPriority encodingPriority;
        MMSEngineDBFacade::EncodingPeriod encodingPeriod;
        int maxIngestionsNumber;
        int maxStorageInGB;

        Json::Value metadataRoot;
        try
        {
            Json::CharReaderBuilder builder;
            Json::CharReader* reader = builder.newCharReader();
            string errors;

            bool parsingSuccessful = reader->parse(requestBody.c_str(),
                    requestBody.c_str() + requestBody.size(), 
                    &metadataRoot, &errors);
            delete reader;

            if (!parsingSuccessful)
            {
                string errorMessage = string("Json metadata failed during the parsing")
                        + ", errors: " + errors
                        + ", json data: " + requestBody
                        ;
                _logger->error(__FILEREF__ + errorMessage);

                sendError(request, 400, errorMessage);

                throw runtime_error(errorMessage);
            }
        }
        catch(exception e)
        {
            string errorMessage = string("Json metadata failed during the parsing"
                    ", json data: " + requestBody
                    );
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            throw runtime_error(errorMessage);
        }

        // name, email and password
        {
            vector<string> mandatoryFields = {
                "Name",
                "EMail",
                "Password"
            };
            for (string field: mandatoryFields)
            {
                if (!_mmsEngineDBFacade->isMetadataPresent(metadataRoot, field))
                {
                    string errorMessage = string("Json field is not present or it is null")
                            + ", Field: " + field;
                    _logger->error(__FILEREF__ + errorMessage);

                    sendError(request, 400, errorMessage);

                    throw runtime_error(errorMessage);
                }
            }

            name = metadataRoot.get("Name", "XXX").asString();
            email = metadataRoot.get("EMail", "XXX").asString();
            password = metadataRoot.get("Password", "XXX").asString();
        }

        // encodingPriority
        {
            string field = "EncodingPriority";
            if (!_mmsEngineDBFacade->isMetadataPresent(metadataRoot, field))
            {
                _logger->info(__FILEREF__ + "encodingPriority is not present, set the default value"
                    + ", _encodingPriorityCustomerDefaultValue: " + MMSEngineDBFacade::toString(_encodingPriorityCustomerDefaultValue)
                );

                encodingPriority = _encodingPriorityCustomerDefaultValue;
            }
            else
            {
                string sEncodingPriority = metadataRoot.get(field, "XXX").asString();
                try
                {                        
                    encodingPriority = MMSEngineDBFacade::toEncodingPriority(sEncodingPriority);
                }
                catch(exception e)
                {
                    string errorMessage = string("Json value is wrong. Correct values are: Low, Medium or High")
                            + ", Field: " + field
                            + ", Value: " + sEncodingPriority
                            ;
                    _logger->error(__FILEREF__ + errorMessage);

                    sendError(request, 400, errorMessage);

                    throw runtime_error(errorMessage);
                }
            }
        }

        // EncodingPeriod
        {
            string field = "EncodingPeriod";
            if (!_mmsEngineDBFacade->isMetadataPresent(metadataRoot, field))
            {
                _logger->info(__FILEREF__ + "encodingPeriod is not present, set the default value"
                    + ", _encodingPeriodCustomerDefaultValue: " + MMSEngineDBFacade::toString(_encodingPeriodCustomerDefaultValue)
                );

                encodingPeriod = _encodingPeriodCustomerDefaultValue;
            }
            else
            {
                string sEncodingPeriod = metadataRoot.get(field, "XXX").asString();
                try
                {                        
                    encodingPeriod = MMSEngineDBFacade::toEncodingPeriod(sEncodingPeriod);
                }
                catch(exception e)
                {
                    string errorMessage = string("Json value is wrong. Correct values are: Daily, Weekly, Monthly or Yearly")
                            + ", Field: " + field
                            + ", Value: " + sEncodingPeriod
                            ;
                    _logger->error(__FILEREF__ + errorMessage);

                    sendError(request, 400, errorMessage);

                    throw runtime_error(errorMessage);
                }
            }
        }

        // MaxIngestionsNumber
        {
            string field = "MaxIngestionsNumber";
            if (!_mmsEngineDBFacade->isMetadataPresent(metadataRoot, field))
            {
                _logger->info(__FILEREF__ + "MaxIngestionsNumber is not present, set the default value"
                    + ", _maxIngestionsNumberCustomerDefaultValue: " + to_string(_maxIngestionsNumberCustomerDefaultValue)
                );

                maxIngestionsNumber = _maxIngestionsNumberCustomerDefaultValue;
            }
            else
            {
                string sMaxIngestionsNumber = metadataRoot.get(field, "XXX").asString();
                try
                {                        
                    maxIngestionsNumber = stol(sMaxIngestionsNumber);
                }
                catch(exception e)
                {
                    string errorMessage = string("Json value is wrong, a number is expected")
                            + ", Field: " + field
                            + ", Value: " + sMaxIngestionsNumber
                            ;
                    _logger->error(__FILEREF__ + errorMessage);

                    sendError(request, 400, errorMessage);

                    throw runtime_error(errorMessage);
                }
            }
        }

        // MaxStorageInGB
        {
            string field = "MaxStorageInGB";
            if (!_mmsEngineDBFacade->isMetadataPresent(metadataRoot, field))
            {
                _logger->info(__FILEREF__ + "MaxStorageInGB is not present, set the default value"
                    + ", _maxStorageInGBCustomerDefaultValue: " + to_string(_maxStorageInGBCustomerDefaultValue)
                );

                maxStorageInGB = _maxStorageInGBCustomerDefaultValue;
            }
            else
            {
                string sMaxStorageInGB = metadataRoot.get(field, "XXX").asString();
                try
                {                        
                    maxStorageInGB = stol(sMaxStorageInGB);
                }
                catch(exception e)
                {
                    string errorMessage = string("Json value is wrong, a number is expected")
                            + ", Field: " + field
                            + ", Value: " + sMaxStorageInGB
                            ;
                    _logger->error(__FILEREF__ + errorMessage);

                    sendError(request, 400, errorMessage);

                    throw runtime_error(errorMessage);
                }
            }
        }

        try
        {
            string customerDirectoryName;

            customerDirectoryName.resize(name.size());

            transform(
                name.begin(), 
                name.end(), 
                customerDirectoryName.begin(), 
                [](unsigned char c){
                    if (isalpha(c)) 
                        return c; 
                    else 
                        return (unsigned char) '_'; } 
            );

            _logger->info(__FILEREF__ + "Registering Customer"
                + ", name: " + name
                + ", email: " + email
            );
            
            tuple<int64_t,int64_t,string> customerKeyUserKeyAndConfirmationCode = 
                _mmsEngineDBFacade->registerCustomer(
                    name, 
                    customerDirectoryName,
                    "",                             // string street,
                    "",                             // string city,
                    "",                             // string state,
                    "",                             // string zip,
                    "",                             // string phone,
                    "",                             // string countryCode,
                    MMSEngineDBFacade::CustomerType::IngestionAndDelivery,  // MMSEngineDBFacade::CustomerType customerType
                    "",                             // string deliveryURL,
                    encodingPriority,               //  MMSEngineDBFacade::EncodingPriority maxEncodingPriority,
                    encodingPeriod,                 //  MMSEngineDBFacade::EncodingPeriod encodingPeriod,
                    maxIngestionsNumber,            // long maxIngestionsNumber,
                    maxStorageInGB,                 // long maxStorageInGB,
                    "",                             // string languageCode,
                    name,                           // string userName,
                    password,                       // string userPassword,
                    email,                          // string userEmailAddress,
                    chrono::system_clock::now() + chrono::hours(24 * 365 * 10)     // chrono::system_clock::time_point userExpirationDate
                );

            string responseBody = string("{ ")
                + "\"customerKey\": " + to_string(get<0>(customerKeyUserKeyAndConfirmationCode)) + " "
                + "}";
            sendSuccess(request, 201, responseBody);
            
            string to = "giulianoc@catrasoftware.it";
            string subject = "Confirmation code";
            
            vector<string> emailBody;
            emailBody.push_back("<p>Hi John,</p>");
            emailBody.push_back(string("<p>This is the confirmation code ") + get<2>(customerKeyUserKeyAndConfirmationCode) + "</p>");
            emailBody.push_back(string("<p>for the customer key ") + to_string(get<0>(customerKeyUserKeyAndConfirmationCode)) + "</p>");
            emailBody.push_back("<p>Bye!</p>");

            sendEmail(to, subject, emailBody);
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + api + " failed"
                + ", e.what(): " + e.what()
            );

            string errorMessage = string("Internal server error");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 500, errorMessage);

            throw runtime_error(errorMessage);
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + api + " failed"
                + ", e.what(): " + e.what()
            );

            string errorMessage = string("Internal server error");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 500, errorMessage);

            throw runtime_error(errorMessage);
        }
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        );

        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        );

        string errorMessage = string("Internal server error");
        _logger->error(__FILEREF__ + errorMessage);

        sendError(request, 500, errorMessage);

        throw runtime_error(errorMessage);
    }
}

void API::confirmCustomer(
        FCGX_Request& request,
        unordered_map<string, string> queryParameters)
{
    string api = "confirmCustomer";

    _logger->info(__FILEREF__ + "Received " + api
    );

    try
    {
        auto confirmationCodeIt = queryParameters.find("confirmationeCode");
        if (confirmationCodeIt == queryParameters.end())
        {
            string errorMessage = string("The 'confirmationeCode' parameter is not found");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            throw runtime_error(errorMessage);
        }

        try
        {
            _mmsEngineDBFacade->confirmCustomer(confirmationCodeIt->second);

            string responseBody;
            sendSuccess(request, 200, responseBody);
            
            string to = "giulianoc@catrasoftware.it";
            string subject = "Welcome";
            
            vector<string> emailBody;
            emailBody.push_back("<p>Hi John,</p>");
            emailBody.push_back(string("<p>Your registration is now completed and you can start working with ...</p>"));
            emailBody.push_back("<p>Bye!</p>");

            sendEmail(to, subject, emailBody);
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + api + " failed"
                + ", e.what(): " + e.what()
            );

            string errorMessage = string("Internal server error");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 500, errorMessage);

            throw runtime_error(errorMessage);
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + api + " failed"
                + ", e.what(): " + e.what()
            );

            string errorMessage = string("Internal server error");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 500, errorMessage);

            throw runtime_error(errorMessage);
        }
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", e.what(): " + e.what()
        );

        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", e.what(): " + e.what()
        );

        string errorMessage = string("Internal server error");
        _logger->error(__FILEREF__ + errorMessage);

        sendError(request, 500, errorMessage);

        throw runtime_error(errorMessage);
    }
}

void API::createAPIKey(
        FCGX_Request& request,
        unordered_map<string, string> queryParameters)
{
    string api = "createAPIKey";

    _logger->info(__FILEREF__ + "Received " + api
    );

    try
    {
        auto customerKeyIt = queryParameters.find("customerKey");
        if (customerKeyIt == queryParameters.end())
        {
            string errorMessage = string("The 'customerKey' parameter is not found");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            throw runtime_error(errorMessage);
        }

        auto userKeyIt = queryParameters.find("userKey");
        if (userKeyIt == queryParameters.end())
        {
            string errorMessage = string("The 'userKey' parameter is not found");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            throw runtime_error(errorMessage);
        }

        try
        {
            bool adminAPI = false; 
            bool userAPI = true;
            chrono::system_clock::time_point apiKeyExpirationDate = 
                    chrono::system_clock::now() + chrono::hours(24 * 365 * 20);
            
            string apiKey = _mmsEngineDBFacade->createAPIKey(
                    stol(customerKeyIt->second),
                    stol(userKeyIt->second),
                    adminAPI, 
                    userAPI, 
                    apiKeyExpirationDate);

            string responseBody = string("{ ")
                + "\"apiKey\": \"" + apiKey + "\" "
                + "}";
            sendSuccess(request, 201, responseBody);
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + api + " failed"
                + ", e.what(): " + e.what()
            );

            string errorMessage = string("Internal server error");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 500, errorMessage);

            throw runtime_error(errorMessage);
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + api + " failed"
                + ", e.what(): " + e.what()
            );

            string errorMessage = string("Internal server error");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 500, errorMessage);

            throw runtime_error(errorMessage);
        }
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", e.what(): " + e.what()
        );

        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", e.what(): " + e.what()
        );

        string errorMessage = string("Internal server error");
        _logger->error(__FILEREF__ + errorMessage);

        sendError(request, 500, errorMessage);

        throw runtime_error(errorMessage);
    }
}

void API::ingestContent(
        FCGX_Request& request,
        shared_ptr<Customer> customer,
        unordered_map<string, string> queryParameters,
        string requestBody)
{
    string api = "ingestContent";

    _logger->info(__FILEREF__ + "Received " + api
        + ", requestBody: " + requestBody
    );

    try
    { 
        int64_t ingestionJobKey = _mmsEngineDBFacade->addIngestionJob(
                customer->_customerKey,
                requestBody,
                MMSEngineDBFacade::IngestionType::Unknown,
                MMSEngineDBFacade::IngestionStatus::Start_Ingestion);

        string responseBody = string("{ ")
                + "\"ingestionJobKey\": " + to_string(ingestionJobKey) + " "
                + "}";

        sendSuccess(request, 201, responseBody);
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        );

        string errorMessage = string("Internal server error");
        _logger->error(__FILEREF__ + errorMessage);

        sendError(request, 500, errorMessage);

        throw runtime_error(errorMessage);
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        );

        string errorMessage = string("Internal server error");
        _logger->error(__FILEREF__ + errorMessage);

        sendError(request, 500, errorMessage);

        throw runtime_error(errorMessage);
    }
}

void API::uploadBinary(
        FCGX_Request& request,
        string requestMethod,
        string xCatraMMSResumeHeader,
        unordered_map<string, string> queryParameters,
        tuple<shared_ptr<Customer>,bool,bool> customerAndFlags,
        unsigned long contentLength
)
{
    string api = "uploadBinary";

    char* buffer = nullptr;

    try
    {
        auto ingestionJobKeyIt = queryParameters.find("ingestionJobKey");
        if (ingestionJobKeyIt == queryParameters.end())
        {
            string errorMessage = string("'ingestionJobKey' URI parameter is missing");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            throw runtime_error(errorMessage);            
        }
        int64_t ingestionJobKey = stol(ingestionJobKeyIt->second);

        shared_ptr<Customer> customer = get<0>(customerAndFlags);
        string customerIngestionBinaryPathName = _mmsStorage->getCustomerIngestionRepository(customer);
        customerIngestionBinaryPathName
                .append("/")
                .append(to_string(ingestionJobKey))
                .append(".binary")
                ;
        
        _logger->info(__FILEREF__ + "Customer Ingestion Binary path name"
            + ", customerIngestionBinaryPathName: " + customerIngestionBinaryPathName
        );
        
        if (requestMethod == "HEAD")
        {
            unsigned long fileSize = 0;
            try
            {
                if (FileIO::fileExisting(customerIngestionBinaryPathName))
                {
                    bool inCaseOfLinkHasItToBeRead = false;
                    fileSize = FileIO::getFileSizeInBytes (
                        customerIngestionBinaryPathName, inCaseOfLinkHasItToBeRead);
                }
            }
            catch(exception e)
            {
                string errorMessage = string("Error to retrieve the file size")
                    + ", customerIngestionBinaryPathName: " + customerIngestionBinaryPathName
                ;
                _logger->error(__FILEREF__ + errorMessage);

                sendError(request, 500, errorMessage);

                throw runtime_error(errorMessage);            
            }

            sendHeadSuccess(request, 200, fileSize);
        }
        else
        {
            chrono::system_clock::time_point uploadStartTime = chrono::system_clock::now();

            bool resume = false;
            {
                if (xCatraMMSResumeHeader != "")
                {
                    unsigned long fileSize = 0;
                    try
                    {
                        if (FileIO::fileExisting(customerIngestionBinaryPathName))
                        {
                            bool inCaseOfLinkHasItToBeRead = false;
                            fileSize = FileIO::getFileSizeInBytes (
                                customerIngestionBinaryPathName, inCaseOfLinkHasItToBeRead);
                        }
                    }
                    catch(exception e)
                    {
                        string errorMessage = string("Error to retrieve the file size")
                            + ", customerIngestionBinaryPathName: " + customerIngestionBinaryPathName
                        ;
                        _logger->error(__FILEREF__ + errorMessage);
    //
    //                    sendError(500, errorMessage);
    //
    //                    throw runtime_error(errorMessage);            
                    }

                    if (stol(xCatraMMSResumeHeader) == fileSize)
                    {
                        _logger->info(__FILEREF__ + "Resume is enabled"
                            + ", xCatraMMSResumeHeader: " + xCatraMMSResumeHeader
                            + ", fileSize: " + to_string(fileSize)
                        );
                        resume = true;
                    }
                    else
                    {
                        _logger->info(__FILEREF__ + "Resume is NOT enabled (X-CatraMMS-Resume header found but different length)"
                            + ", xCatraMMSResumeHeader: " + xCatraMMSResumeHeader
                            + ", fileSize: " + to_string(fileSize)
                        );
                    }
                }
                else
                {
                    _logger->info(__FILEREF__ + "Resume flag is NOT present (No X-CatraMMS-Resume header found)"
                    );
                }
            }
            
            ofstream binaryFileStream(customerIngestionBinaryPathName, 
                    resume ? (ofstream::binary | ofstream::app) : (ofstream::binary | ofstream::trunc));
            buffer = new char [_binaryBufferLength];

            unsigned long currentRead;
            unsigned long totalRead = 0;
            {
                // we have the content-length and we will use it to read the binary

                chrono::system_clock::time_point lastTimeProgressUpdate = chrono::system_clock::now();
                double lastPercentageUpdated = -1;
                
                unsigned long bytesToBeRead;
                while (totalRead < contentLength)
                {
                    if (contentLength - totalRead >= _binaryBufferLength)
                        bytesToBeRead = _binaryBufferLength;
                    else
                        bytesToBeRead = contentLength - totalRead;

                    currentRead = FCGX_GetStr(buffer, bytesToBeRead, request.in);
                    // cin.read(buffer, bytesToBeRead);
                    // currentRead = cin.gcount();
                    if (currentRead != bytesToBeRead)
                    {
                        // this should never happen because it will be against the content-length
                        string errorMessage = string("Error reading the binary")
                            + ", contentLength: " + to_string(contentLength)
                            + ", totalRead: " + to_string(totalRead)
                            + ", bytesToBeRead: " + to_string(bytesToBeRead)
                            + ", currentRead: " + to_string(currentRead)
                        ;
                        _logger->error(__FILEREF__ + errorMessage);

                        sendError(request, 400, errorMessage);

                        throw runtime_error(errorMessage);            
                    }

                    totalRead   += currentRead;

                    binaryFileStream.write(buffer, currentRead); 
                    
                    {
                        chrono::system_clock::time_point now = chrono::system_clock::now();

                        if (now - lastTimeProgressUpdate >= chrono::seconds(_progressUpdatePeriodInSeconds))
                        {
                            double progress = ((double) totalRead / (double) contentLength) * 100;
                            // int uploadingPercentage = floorf(progress * 100) / 100;
                            // this is to have one decimal in the percentage
                            double uploadingPercentage = ((double) ((int) (progress * 10))) / 10;

                            _logger->info(__FILEREF__ + "Upload still running"
                                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                + ", progress: " + to_string(progress)
                                + ", uploadingPercentage: " + to_string(uploadingPercentage)
                                + ", totalRead: " + to_string(totalRead)
                                + ", contentLength: " + to_string(contentLength)
                            );

                            lastTimeProgressUpdate = now;

                            if (lastPercentageUpdated != uploadingPercentage)
                            {
                                _logger->info(__FILEREF__ + "Update IngestionJob"
                                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                    + ", uploadingPercentage: " + to_string(uploadingPercentage)
                                );                            
                                _mmsEngineDBFacade->updateIngestionJobSourceUploadingInProgress (
                                    ingestionJobKey, uploadingPercentage);

                                lastPercentageUpdated = uploadingPercentage;
                            }
                        }
                    }
                }
            }

            binaryFileStream.close();

            delete buffer;

            unsigned long elapsedUploadInSeconds = chrono::duration_cast<chrono::seconds>(chrono::system_clock::now() - uploadStartTime).count();
            _logger->info(__FILEREF__ + "Binary read"
                + ", contentLength: " + to_string(contentLength)
                + ", totalRead: " + to_string(totalRead)
                + ", elapsedUploadInSeconds: " + to_string(elapsedUploadInSeconds)
            );

            /*
            {
                // Chew up any remaining stdin - this shouldn't be necessary
                // but is because mod_fastcgi doesn't handle it correctly.

                // ignore() doesn't set the eof bit in some versions of glibc++
                // so use gcount() instead of eof()...
                do 
                    cin.ignore(bufferLength); 
                while (cin.gcount() == bufferLength);
            }    
            */

            bool sourceBinaryTransferred = true;
            _logger->info(__FILEREF__ + "Update IngestionJob"
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", sourceBinaryTransferred: " + to_string(sourceBinaryTransferred)
            );                            
            _mmsEngineDBFacade->updateIngestionJobSourceBinaryTransferred (
                ingestionJobKey, sourceBinaryTransferred);

            string responseBody = string("{ ")
                + "\"contentLength\": " + to_string(contentLength) + ", "
                + "\"writtenBytes\": " + to_string(totalRead) + ", "
                + "\"elapsedUploadInSeconds\": " + to_string(elapsedUploadInSeconds) + " "
                + "}";
            sendSuccess(request, 201, responseBody);
        }
    }
    catch (exception e)
    {
        if (buffer != nullptr)
            delete [] buffer;

        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", e.what(): " + e.what()
        );

        string errorMessage = string("Internal server error");
        _logger->error(__FILEREF__ + errorMessage);

        sendError(request, 500, errorMessage);

        throw runtime_error(errorMessage);
    }    
}