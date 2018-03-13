/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   EnodingsManager.h
 * Author: giuliano
 *
 * Created on February 4, 2018, 7:18 PM
 */

#ifndef ActiveEncodingsManager_h
#define ActiveEncodingsManager_h

#include <chrono>
#include <vector>
#include <condition_variable>
#include "EncoderVideoAudioProxy.h"
#include "MMSEngineDBFacade.h"
#include "MMSStorage.h"
#include "spdlog/spdlog.h"
#include "Magick++.h"

#define MAXHIGHENCODINGSTOBEMANAGED     30
#define MAXMEDIUMENCODINGSTOBEMANAGED  20
#define MAXLOWENCODINGSTOBEMANAGED      20

struct MaxEncodingsManagerCapacityReached: public exception {    
    char const* what() const throw() 
    {
        return "Max Encoding Manager capacity reached";
    }; 
};

class ActiveEncodingsManager {
public:
    ActiveEncodingsManager(       
            Json::Value configuration,
            shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade,
            shared_ptr<MMSStorage> mmsStorage,
            shared_ptr<spdlog::logger> logger);

    virtual ~ActiveEncodingsManager();
    
    void operator ()();

    unsigned long addEncodingItems (
	vector<shared_ptr<MMSEngineDBFacade::EncodingItem>>& vEncodingItems);
    
    static void encodingImageFormatValidation(string newFormat);
    static Magick::InterlaceType encodingImageInterlaceTypeValidation(string sNewInterlaceType);

private:
    struct EncodingJob
    {
        EncodingJobStatus			_status;
        chrono::system_clock::time_point	_encodingJobStart;

        shared_ptr<MMSEngineDBFacade::EncodingItem>	_encodingItem;
        EncoderVideoAudioProxy                  _encoderVideoAudioProxy;
        
        EncodingJob()
        {
            _status         = EncodingJobStatus::Free;
        }
    };

    shared_ptr<spdlog::logger>                  _logger;
    Json::Value                                 _configuration;
    shared_ptr<MMSEngineDBFacade>               _mmsEngineDBFacade;
    shared_ptr<MMSStorage>                      _mmsStorage;
    
    condition_variable                          _cvAddedEncodingJob;
    mutex                                       _mtEncodingJobs;
    
    EncodingJob     _highPriorityEncodingJobs[MAXHIGHENCODINGSTOBEMANAGED];
    EncodingJob     _mediumPriorityEncodingJobs[MAXMEDIUMENCODINGSTOBEMANAGED];
    EncodingJob     _lowPriorityEncodingJobs[MAXLOWENCODINGSTOBEMANAGED];

    #ifdef __LOCALENCODER__
        int                 _runningEncodingsNumber;
    #endif

    void processEncodingJob(mutex* mtEncodingJobs, EncodingJob* encodingJob);
    void addEncodingItem(shared_ptr<MMSEngineDBFacade::EncodingItem> encodingItem);
    string encodeContentImage(
        shared_ptr<MMSEngineDBFacade::EncodingItem> encodingItem);
    void processEncodedImage(
        shared_ptr<MMSEngineDBFacade::EncodingItem> encodingItem, 
        string stagingEncodedAssetPathName);

    void readingImageProfile(
        string profileDetails,
        string& newFormat,
        int& newWidth,
        int& newHeight,
        bool& newAspect,
        string& sNewInterlaceType,
        Magick::InterlaceType& newInterlaceType
    );
};

#endif
