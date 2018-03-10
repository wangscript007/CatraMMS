/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   UploadBinary.cpp
 * Author: giuliano
 * 
 * Created on February 18, 2018, 1:27 AM
 */

#include <fstream>
#include "UploadBinary.h"

int main(int argc, char** argv) 
{
    const char* configurationPathName = getenv("MMS_CONFIGPATHNAME");
    if (configurationPathName == nullptr)
    {
        cerr << "MMS API: the MMS_CONFIGPATHNAME environment variable is not defined" << endl;
        
        return 1;
    }

    UploadBinary binary(configurationPathName);
    
    return binary.manageBinaryRequest();
}

UploadBinary::UploadBinary(const char* configurationPathName): APICommon(configurationPathName) 
{
    _binaryBufferLength             = _configuration["uploadBinary"].get("binaryBufferLength", "XXX").asInt();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", uploadBinary->binaryBufferLength: " + to_string(_binaryBufferLength)
    );
    _progressUpdatePeriodInSeconds  = _configuration["uploadBinary"].get("progressUpdatePeriodInSeconds", "XXX").asInt();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", uploadBinary->progressUpdatePeriodInSeconds: " + to_string(_progressUpdatePeriodInSeconds)
    );
}

UploadBinary::~UploadBinary() {
}

void UploadBinary::manageRequestAndResponse(
        string requestURI,
        string requestMethod,
        unordered_map<string, string> queryParameters,
        tuple<shared_ptr<Customer>,bool,bool>& customerAndFlags,
        unsigned long contentLength,
        string requestBody
)
{
    _logger->error(__FILEREF__ + "UploadBinary application is able to manage ONLY UploadBinary requests");
    
    string errorMessage = string("Internal server error");
    _logger->error(__FILEREF__ + errorMessage);

    sendError(500, errorMessage);

    throw runtime_error(errorMessage);
}

void UploadBinary::getBinaryAndResponse(
        string requestURI,
        string requestMethod,
        string xCatraMMSResumeHeader,
        unordered_map<string, string> queryParameters,
        tuple<shared_ptr<Customer>,bool,bool>& customerAndFlags,
        unsigned long contentLength
)
{
    char* buffer = nullptr;

    try
    {
        if (contentLength == -1)
        {
            string errorMessage = string("'Content-Length' header is missing");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(400, errorMessage);

            throw runtime_error(errorMessage);            
        }
        
        auto ingestionJobKeyIt = queryParameters.find("ingestionJobKey");
        if (ingestionJobKeyIt == queryParameters.end())
        {
            string errorMessage = string("'ingestionJobKey' URI parameter is missing");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(400, errorMessage);

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

                sendError(500, errorMessage);

                throw runtime_error(errorMessage);            
            }

            sendHeadSuccess(200, fileSize);
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
            /*
            if (contentLength == -1)
            {
                // we do not have the content-length header, we will read all what we receive
                do
                {
                    cin.read(buffer, _binaryBufferLength);

                    currentRead = cin.gcount();

                    totalRead   += currentRead;

                    binaryFileStream.write(buffer, currentRead); 

                    if (totalRead >= _maxBinaryContentLength)
                    {
                        string errorMessage = string("Binary too long")
                            + ", totalRead: " + to_string(totalRead)
                            + ", _maxBinaryContentLength: " + to_string(_maxBinaryContentLength)
                        ;
                        _logger->error(__FILEREF__ + errorMessage);

                        sendError(400, errorMessage);

                        throw runtime_error(errorMessage);            
                    }
                }
                while (currentRead == _binaryBufferLength);
            }
            else
            */
            {
                // we have the content-length and we will use it to read the binary

                chrono::system_clock::time_point lastTimeProgressUpdate = chrono::system_clock::now();
                int lastPercentageUpdated = -1;
                
                unsigned long bytesToBeRead;
                while (totalRead < contentLength)
                {
                    if (contentLength - totalRead >= _binaryBufferLength)
                        bytesToBeRead = _binaryBufferLength;
                    else
                        bytesToBeRead = contentLength - totalRead;

                    cin.read(buffer, bytesToBeRead);
                    currentRead = cin.gcount();
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

                        sendError(400, errorMessage);

                        throw runtime_error(errorMessage);            
                    }

                    totalRead   += currentRead;

                    binaryFileStream.write(buffer, currentRead); 
                    
                    {
                        chrono::system_clock::time_point now = chrono::system_clock::now();

                        if (now - lastTimeProgressUpdate >= chrono::seconds(_progressUpdatePeriodInSeconds))
                        {
                            double progress = (totalRead / contentLength) * 100;
                            int uploadingPercentage = floorf(progress * 1000) / 1000;

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
            sendSuccess(201, responseBody);
        }
    }
    catch (exception e)
    {
        _logger->error(__FILEREF__ + "getBinaryAndResponse failed");
        
        if (buffer != nullptr)
            delete [] buffer;
            
        throw e;
    }    
}
