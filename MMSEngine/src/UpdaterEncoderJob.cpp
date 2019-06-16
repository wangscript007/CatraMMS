
#include "PersistenceLock.h"
#include "UpdaterEncoderJob.h"

UpdaterEncoderJob::UpdaterEncoderJob(
	shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade,
	shared_ptr<spdlog::logger> logger)
{
	_mmsEngineDBFacade = mmsEngineDBFacade;
	_logger = logger;
}

UpdaterEncoderJob::~UpdaterEncoderJob()
{
}

int UpdaterEncoderJob::updateEncodingJob (
	int64_t encodingJobKey,
	MMSEngineDBFacade::EncodingError encodingError,
	int64_t mediaItemKey,
	int64_t encodedPhysicalPathKey,
	int64_t ingestionJobKey,
	string processorMMS)
{

	try
	{
		int waitingTimeoutInSecondsIfLocked = 30;

		PersistenceLock persistenceLock(_mmsEngineDBFacade,
			MMSEngineDBFacade::LockType::EncodingJobs,
			waitingTimeoutInSecondsIfLocked,
			processorMMS);

		return _mmsEngineDBFacade->updateEncodingJob (
				encodingJobKey, encodingError, mediaItemKey,
				encodedPhysicalPathKey, ingestionJobKey);
	}
	catch(AlreadyLocked e)
	{
		_logger->error(__FILEREF__ + "updateEncodingJob failed"
			+ ", encodingJobKey: " + to_string(encodingJobKey)
			+ ", mediaItemKey: " + to_string(mediaItemKey)
			+ ", encodedPhysicalPathKey: " + to_string(encodedPhysicalPathKey)
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", exception: " + e.what()
		);

		throw e;
	}
	catch(runtime_error e)
	{
		_logger->error(__FILEREF__ + "updateEncodingJob failed"
			+ ", encodingJobKey: " + to_string(encodingJobKey)
			+ ", mediaItemKey: " + to_string(mediaItemKey)
			+ ", encodedPhysicalPathKey: " + to_string(encodedPhysicalPathKey)
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", exception: " + e.what()
		);

		throw e;
	}
	catch(exception e)
	{
		_logger->error(__FILEREF__ + "updateEncodingJob failed"
			+ ", encodingJobKey: " + to_string(encodingJobKey)
			+ ", mediaItemKey: " + to_string(mediaItemKey)
			+ ", encodedPhysicalPathKey: " + to_string(encodedPhysicalPathKey)
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", exception: " + e.what()
		);

		throw e;
	}
}
