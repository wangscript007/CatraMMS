
#include <thread>

#include "catralibraries/Scheduler2.h"

#include "CMSEngineProcessor.h"
#include "CheckIngestionTimes.h"
#include "CheckEncodingTimes.h"
#include "CMSEngineDBFacade.h"
#include "ActiveEncodingsManager.h"
#include "CMSStorage.h"
#include "CMSEngine.h"


int main (int iArgc, char *pArgv [])
{

    auto logger = spdlog::stdout_logger_mt("cmsEngineService");
    spdlog::set_level(spdlog::level::info); // trace, debug, info, warn, err, critical, off

    // globally register the loggers so so the can be accessed using spdlog::get(logger_name)
    // spdlog::register_logger(logger);

    size_t dbPoolSize = 5;
    string dbServer ("tcp://127.0.0.1:3306");
    #ifdef __APPLE__
        string dbUsername("root"); string dbPassword("giuliano"); string dbName("workKing");
    #else
        string dbUsername("root"); string dbPassword("root"); string dbName("catracms");
    #endif
    logger->info(__FILEREF__ + "Creating CMSEngineDBFacade"
        + ", dbPoolSize: " + to_string(dbPoolSize)
        + ", dbServer: " + dbServer
        + ", dbUsername: " + dbUsername
        + ", dbPassword: " + dbPassword
        + ", dbName: " + dbName
            );
    shared_ptr<CMSEngineDBFacade>       cmsEngineDBFacade = make_shared<CMSEngineDBFacade>(
            dbPoolSize, dbServer, dbUsername, dbPassword, dbName, logger);
    
    #ifdef __APPLE__
        string storage ("/Users/multi/GestioneProgetti/Development/catrasoftware/storage/");
    #else
        string storage ("/home/giuliano/storage/");
    #endif
    unsigned long freeSpaceToLeaveInEachPartitionInMB = 5;
    logger->info(__FILEREF__ + "Creating CMSStorage"
        + ", storage: " + storage
        + ", freeSpaceToLeaveInEachPartitionInMB: " + to_string(freeSpaceToLeaveInEachPartitionInMB)
            );
    shared_ptr<CMSStorage>       cmsStorage = make_shared<CMSStorage>(
            storage, 
            freeSpaceToLeaveInEachPartitionInMB,
            logger);

    logger->info(__FILEREF__ + "Creating CMSEngine"
            );
    shared_ptr<CMSEngine>       cmsEngine = make_shared<CMSEngine>(cmsEngineDBFacade, logger);
        
    logger->info(__FILEREF__ + "Creating MultiEventsSet"
        + ", addDestination: " + CMSENGINEPROCESSORNAME
            );
    shared_ptr<MultiEventsSet>          multiEventsSet = make_shared<MultiEventsSet>();
    multiEventsSet->addDestination(CMSENGINEPROCESSORNAME);

    logger->info(__FILEREF__ + "Creating ActiveEncodingsManager"
            );
    ActiveEncodingsManager      activeEncodingsManager(cmsEngineDBFacade, cmsStorage, logger);

    logger->info(__FILEREF__ + "Creating CMSEngineProcessor"
            );
    CMSEngineProcessor      cmsEngineProcessor(logger, multiEventsSet, cmsEngineDBFacade, cmsStorage, &activeEncodingsManager);
    
    unsigned long           ulThreadSleepInMilliSecs = 100;
    logger->info(__FILEREF__ + "Creating Scheduler2"
        + ", ulThreadSleepInMilliSecs: " + to_string(ulThreadSleepInMilliSecs)
            );
    Scheduler2              scheduler(ulThreadSleepInMilliSecs);


    logger->info(__FILEREF__ + "Starting ActiveEncodingsManager"
            );
    thread activeEncodingsManagerThread (ref(activeEncodingsManager));

    logger->info(__FILEREF__ + "Starting CMSEngineProcessor"
            );
    thread cmsEngineProcessorThread (cmsEngineProcessor);

    logger->info(__FILEREF__ + "Starting Scheduler2"
            );
    thread schedulerThread (ref(scheduler));

    unsigned long           checkIngestionTimesPeriodInMilliSecs = 4 * 1000;
    logger->info(__FILEREF__ + "Creating and Starting CheckIngestionTimes"
        + ", checkIngestionTimesPeriodInMilliSecs: " + to_string(checkIngestionTimesPeriodInMilliSecs)
            );
    shared_ptr<CheckIngestionTimes>     checkIngestionTimes =
            make_shared<CheckIngestionTimes>(checkIngestionTimesPeriodInMilliSecs, multiEventsSet, logger);
    checkIngestionTimes->start();
    scheduler.activeTimes(checkIngestionTimes);

    unsigned long           checkEncodingTimesPeriodInMilliSecs = 15 * 1000;
    logger->info(__FILEREF__ + "Creating and Starting CheckEncodingTimes"
        + ", checkEncodingTimesPeriodInMilliSecs: " + to_string(checkEncodingTimesPeriodInMilliSecs)
            );
    shared_ptr<CheckEncodingTimes>     checkEncodingTimes =
            make_shared<CheckEncodingTimes>(checkEncodingTimesPeriodInMilliSecs, multiEventsSet, logger);
    checkEncodingTimes->start();
    scheduler.activeTimes(checkEncodingTimes);

    
    logger->info(__FILEREF__ + "Waiting ActiveEncodingsManager"
            );
    activeEncodingsManagerThread.join();
    
    logger->info(__FILEREF__ + "Waiting CMSEngineProcessor"
            );
    cmsEngineProcessorThread.join();
    
    logger->info(__FILEREF__ + "Waiting Scheduler2"
            );
    schedulerThread.join();

    logger->info(__FILEREF__ + "Shutdown done"
            );
    
    return 0;
}
