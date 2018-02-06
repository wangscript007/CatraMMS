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

#ifndef EncoderVideoAudioProxy_h
#define EncoderVideoAudioProxy_h

#include "CMSEngineDBFacade.h"
#include "CMSStorage.h"
#include "EncodingItem.h"
#include "spdlog/spdlog.h"

struct MaxConcurrentJobsReached: public exception {
    char const* what() const throw() 
    {
        return "Encoder reached the max number of concurrent jobs";
    }; 
};

struct EncoderError: public exception {
    char const* what() const throw() 
    {
        return "Encoder error";
    }; 
};

class EncoderVideoAudioProxy {
public:
    EncoderVideoAudioProxy(
        shared_ptr<CMSEngineDBFacade> cmsEngineDBFacade,
        shared_ptr<CMSStorage> cmsStorage,
        EncodingItem* pEncodingItem,
        shared_ptr<spdlog::logger> logger);

    virtual ~EncoderVideoAudioProxy();
    
    void operator ()();

private:
    shared_ptr<spdlog::logger>          _logger;
    shared_ptr<CMSEngineDBFacade>       _cmsEngineDBFacade;
    shared_ptr<CMSStorage>              _cmsStorage;
    EncodingItem*                       _pEncodingItem;
    
    string                              _ffmpegPathName;
    string                              _3GPPEncoder;
    string                              _mpeg2TSEncoder;


    string encodeContentVideoAudio();
    string encodeContent_VideoAudio_through_ffmpeg();

    void processEncodedContentVideoAudio(string stagingEncodedAssetPathName);
};

#endif

