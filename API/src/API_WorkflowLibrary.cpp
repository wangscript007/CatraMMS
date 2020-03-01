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

#include "JSONUtils.h"
#include <fstream>
#include <sstream>
#include <curlpp/cURLpp.hpp>
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/Exception.hpp>
#include <curlpp/Infos.hpp>
#include "Validator.h"
#include "catralibraries/Convert.h"
#include "API.h"


void API::workflowsAsLibraryList(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters)
{
    string api = "workflowsAsLibraryList";

    _logger->info(__FILEREF__ + "Received " + api
    );

    try
    {
		Json::Value workflowListRoot = _mmsEngineDBFacade->getWorkflowsAsLibraryList(
			workspace->_workspaceKey);

		Json::StreamWriterBuilder wbuilder;
		string responseBody = Json::writeString(wbuilder, workflowListRoot);

		sendSuccess(request, 200, responseBody);
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", e.what(): " + e.what()
        );

        string errorMessage = string("Internal server error: ") + e.what();
        _logger->error(__FILEREF__ + errorMessage);

        sendError(request, 500, errorMessage);

        throw runtime_error(errorMessage);
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


void API::workflowAsLibraryContent(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters)
{
	string api = "workflowAsLibraryContent";

	_logger->info(__FILEREF__ + "Received " + api
	);

    try
    {
		int64_t workflowLibraryKey = -1;
		auto workflowLibraryKeyIt = queryParameters.find("workflowLibraryKey");
		if (workflowLibraryKeyIt == queryParameters.end())
		{
			string errorMessage = string("'workflowLibraryKey' URI parameter is missing");
			_logger->error(__FILEREF__ + errorMessage);

			sendError(request, 400, errorMessage);

			throw runtime_error(errorMessage);
		}
		workflowLibraryKey = stoll(workflowLibraryKeyIt->second);

		string workflowLibraryContent = _mmsEngineDBFacade->getWorkflowAsLibraryContent(
			workspace->_workspaceKey, workflowLibraryKey);

		sendSuccess(request, 200, workflowLibraryContent);
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", e.what(): " + e.what()
        );

        string errorMessage = string("Internal server error: ") + e.what();
        _logger->error(__FILEREF__ + errorMessage);

        sendError(request, 500, errorMessage);

        throw runtime_error(errorMessage);
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


void API::saveWorkflowAsLibrary(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody)
{
    string api = "saveWorkflowAsLibrary";

    _logger->info(__FILEREF__ + "Received " + api
        + ", requestBody: " + requestBody
    );

    try
    {
        string responseBody;    

        try
        {
			// validation and retrieving of the Label
			string workflowLabel;
			{
				Json::Value requestBodyRoot = manageWorkflowVariables(requestBody, Json::nullValue);

				Validator validator(_logger, _mmsEngineDBFacade, _configuration);
				// it starts from the root and validate recursively the entire body
				validator.validateIngestedRootMetadata(workspace->_workspaceKey,
                    requestBodyRoot);

				string field = "Label";
				if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
				{
					string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
				workflowLabel = requestBodyRoot.get(field, "XXX").asString();
			}

			int64_t thumbnailMediaItemKey = -1;
			auto thumbnailMediaItemKeyIt = queryParameters.find("thumbnailMediaItemKey");
			if (thumbnailMediaItemKeyIt != queryParameters.end() && thumbnailMediaItemKeyIt->second != "")
				thumbnailMediaItemKey = stoll(thumbnailMediaItemKeyIt->second);

            int64_t workflowLibraryKey = _mmsEngineDBFacade->addUpdateWorkflowAsLibrary(
                workspace->_workspaceKey, workflowLabel,
                thumbnailMediaItemKey, requestBody);

            responseBody = (
                    string("{ ") 
                    + "\"workflowLibraryKey\": " + to_string(workflowLibraryKey)
                    + ", \"label\": \"" + workflowLabel + "\" "
                    + "}"
                    );            
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->addUpdateWorkflowAsLibrary failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->addUpdateWorkflowAsLibrary failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }

        sendSuccess(request, 201, responseBody);
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        );

        string errorMessage = string("Internal server error: ") + e.what();
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

void API::removeWorkflowAsLibrary(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters)
{
    string api = "removeWorkflowAsLibrary";

    _logger->info(__FILEREF__ + "Received " + api
    );

    try
    {
        auto workflowLibraryKeyIt = queryParameters.find("workflowLibraryKey");
        if (workflowLibraryKeyIt == queryParameters.end())
        {
            string errorMessage = string("'workflowLibraryKey' URI parameter is missing");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            throw runtime_error(errorMessage);            
        }
        int64_t workflowLibraryKey = stoll(workflowLibraryKeyIt->second);
        
        try
        {
            _mmsEngineDBFacade->removeWorkflowAsLibrary(
                workspace->_workspaceKey, workflowLibraryKey);
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->removeWorkflowAsLibrary failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->removeWorkflowAsLibrary failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }

        string responseBody;
        
        sendSuccess(request, 200, responseBody);
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", e.what(): " + e.what()
        );

        string errorMessage = string("Internal server error: ") + e.what();
        _logger->error(__FILEREF__ + errorMessage);

        sendError(request, 500, errorMessage);

        throw runtime_error(errorMessage);
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

