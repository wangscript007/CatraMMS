package com.catramms.webservice.rest;

import com.catramms.backing.newWorkflow.IngestionResult;
import com.catramms.utility.catramms.CatraMMS;
import org.apache.commons.io.IOUtils;
import org.apache.log4j.Logger;
import org.json.JSONArray;
import org.json.JSONObject;

import javax.servlet.http.HttpServletRequest;
import javax.ws.rs.*;
import javax.ws.rs.core.Context;
import javax.ws.rs.core.MediaType;
import javax.ws.rs.core.Response;
import java.io.*;
import java.text.DateFormat;
import java.text.SimpleDateFormat;
import java.util.*;

/**
 * Created by multi on 16.09.18.
 */
@Path("/api")
public class CatraMMSServices {

    private static final Logger mLogger = Logger.getLogger(CatraMMSServices.class);

    @GET
    @Produces(MediaType.APPLICATION_JSON)
    @Path("status")
    public Response getStatus()
    {
        mLogger.info("Received getStatus");

        return Response.ok("{ \"status\": \"REST CatraMMS webservice running\" }").build();
    }

    @POST
    @Consumes(MediaType.APPLICATION_JSON + ";charset=utf-8")
    @Produces(MediaType.APPLICATION_JSON)
    @Path("cutMedia")
    public Response cutMedia(InputStream json, @Context HttpServletRequest pRequest
    )
    {
        String response = null;
        Long cutIngestionJobKey = null;

        mLogger.info("Received cutMedia");

        try
        {
            mLogger.info("InputStream to String..." );
            StringWriter writer = new StringWriter();
            IOUtils.copy(json, writer, "UTF-8");
            String sCutMedia = writer.toString();
            mLogger.info("sCutMedia (string): " + sCutMedia);

            JSONObject joCutMedia = new JSONObject(sCutMedia);
            mLogger.info("joCutMedia (json): " + joCutMedia);

            DateFormat simpleDateFormat = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss.SSS");

            String cutMediaId = joCutMedia.getString("id");
            String cutMediaTitle = joCutMedia.getString("title");
            String cutMediaChannel = joCutMedia.getString("channel");
            String ingester = joCutMedia.getString("userName");
            Long cutMediaStartTimeInMilliseconds = joCutMedia.getLong("startTime");   // millisecs
            Long cutMediaEndTimeInMilliseconds = joCutMedia.getLong("endTime");
            String sCutMediaStartTime = simpleDateFormat.format(cutMediaStartTimeInMilliseconds);
            String sCutMediaEndTime = simpleDateFormat.format(cutMediaEndTimeInMilliseconds);

            {
                Long userKey = new Long(1);
                String apiKey = "SU1.8ZO1O2zTg_5SvI12rfN9oQdjRru90XbMRSvACIxfqBXMYGj8k1P9lV4ZcvMRJL";
                String la1MediaDirectoryPathName = "/mnt/stream_recording/makitoRecording/La1/";
                String la2MediaDirectoryPathName = "/mnt/stream_recording/makitoRecording/La2/";
                String radioMediaDirectoryPathName = "/mnt/stream_recording/makitoRecording/Radio/";
                int reteUnoTrackNumber = 0;
                int reteDueTrackNumber = 1;
                int reteTreTrackNumber = 2;
                String cutMediaRetention = "2d";
                boolean addContentPull = true;

                int secondsToWaitBeforeStartProcessingAFile = 5;

                try
                {
                    boolean firstOrLastChunkNotFound = false;

                    mLogger.info("cutMedia"
                                    + ", cutMediaTitle: " + cutMediaTitle
                    );

                    TreeMap<Date, File> fileTreeMap = new TreeMap<>();
                    String fileExtension = null;

                    // List<File> mediaFilesToBeManaged = new ArrayList<>();
                    {
                        Calendar calendarStart = Calendar.getInstance();
                        calendarStart.setTime(new Date(cutMediaStartTimeInMilliseconds));
                        // there is one case where we have to consider the previous dir:
                        //  i.e.: Media start: 08:00:15 and chunk start: 08:00:32
                        // in this case we need the last chunk of the previous dir
                        calendarStart.add(Calendar.HOUR_OF_DAY, -1);

                        Calendar calendarEnd = Calendar.getInstance();
                        calendarEnd.setTime(new Date(cutMediaEndTimeInMilliseconds));

                        DateFormat fileDateFormat = new SimpleDateFormat("yyyy/MM/dd/HH");

                        while (fileDateFormat.format(calendarStart.getTime()).compareTo(
                                fileDateFormat.format(calendarEnd.getTime())) <= 0)
                        {
                            String mediaDirectoryPathName;

                            if (cutMediaChannel.toLowerCase().contains("la1"))
                                mediaDirectoryPathName = la1MediaDirectoryPathName;
                            else if (cutMediaChannel.toLowerCase().contains("la2"))
                                mediaDirectoryPathName = la2MediaDirectoryPathName;
                            else
                                mediaDirectoryPathName = radioMediaDirectoryPathName;

                            mediaDirectoryPathName += fileDateFormat.format(calendarStart.getTime());
                            mLogger.info("Reading directory: " + mediaDirectoryPathName);
                            File mediaDirectoryFile = new File(mediaDirectoryPathName);
                            if (mediaDirectoryFile.exists())
                            {
                                File[] mediaFiles = mediaDirectoryFile.listFiles();

                                // mediaFilesToBeManaged.addAll(Arrays.asList(mediaFiles));
                                // fill fileTreeMap
                                for (File mediaFile: mediaFiles)
                                {
                                    try {
                                        // File mediaFile = mediaFilesToBeManaged.get(mediaFileIndex);

                                        mLogger.info("Processing mediaFile"
                                                        + ", cutMediaId: " + cutMediaId
                                                        + ", mediaFile.getName: " + mediaFile.getName()
                                        );

                                        if (mediaFile.isDirectory()) {
                                            // mLogger.info("Found a directory, ignored. Directory name: " + mediaFile.getName());

                                            continue;
                                        } else if (new Date().getTime() - mediaFile.lastModified()
                                                < secondsToWaitBeforeStartProcessingAFile * 1000) {
                                            mLogger.info("Waiting at least " + secondsToWaitBeforeStartProcessingAFile + " seconds before start processing the file. File name: " + mediaFile.getName());

                                            continue;
                                        } else if (mediaFile.length() == 0) {
                                            mLogger.info("Waiting mediaFile size is greater than 0"
                                                            + ", File name: " + mediaFile.getName()
                                                            + ", File lastModified: " + simpleDateFormat.format(mediaFile.lastModified())
                                            );

                                            continue;
                                        } else if (!mediaFile.getName().endsWith(".mp4")
                                                && !mediaFile.getName().endsWith(".ts")) {
                                            // mLogger.info("Found a NON mp4 file, ignored. File name: " + ftpFile.getName());

                                            continue;
                                        }

                                        if (fileExtension == null)
                                            fileExtension = mediaFile.getName().substring(mediaFile.getName().lastIndexOf('.') + 1);

                                        fileTreeMap.put(getMediaChunkStartTime(mediaFile.getName()), mediaFile);
                                    }
                                    catch (Exception ex)
                                    {
                                        String errorMessage = "exception processing the " + mediaFile.getName() + " file. Exception: " + ex
                                                + ", cutMediaId: " + cutMediaId
                                                ;
                                        mLogger.warn(errorMessage);

                                        continue;
                                    }
                                }
                            }

                            calendarStart.add(Calendar.HOUR_OF_DAY, 1);
                        }
                    }

                    mLogger.info("Found " + fileTreeMap.size() + " media files (" + "/" + cutMediaChannel + ")");

                    long cutStartTimeInMilliSeconds = -1;
                    boolean firstChunkFound = false;
                    boolean lastChunkFound = false;

                    ArrayList<Map.Entry<Date, File>> filesArray = new ArrayList(fileTreeMap.entrySet());

                    for (int mediaFileIndex = 0; mediaFileIndex < filesArray.size(); mediaFileIndex++)
                    {
                        try
                        {
                            Map.Entry<Date, File> dateFileEntry = filesArray.get(mediaFileIndex);

                            File mediaFile = dateFileEntry.getValue();
                            Date mediaChunkStartTime = dateFileEntry.getKey();

                            Date nextMediaChunkStart = null;
                            if (mediaFileIndex + 1 < filesArray.size())
                                nextMediaChunkStart = filesArray.get(mediaFileIndex + 1).getKey();

                            mLogger.info("Processing mediaFile"
                                            + ", cutMediaId: " + cutMediaId
                                            + ", mediaFile.getName: " + mediaFile.getName()
                            );

                            {
                                // Channel_1-2018-06-26-10h00m39s.mp4
                                // mLogger.info("###Processing of the " + ftpFile.getName() + " ftp file");

                                // Date mediaChunkStartTime = getMediaChunkStartTime(mediaFile.getName());

                                // SC: Start Chunk
                                // PS: Playout Start, PE: Playout End
                                // --------------SC--------------SC--------------SC--------------SC--------------
                                //                        PS-------------------------------PE


                                if (mediaChunkStartTime.getTime() <= cutMediaStartTimeInMilliseconds
                                        && (nextMediaChunkStart == null || cutMediaStartTimeInMilliseconds <= nextMediaChunkStart.getTime()))
                                {
                                    // first chunk

                                    firstChunkFound = true;

                                    // fileTreeMap.put(mediaChunkStartTime, mediaFile);

                                    /*
                                    double playoutMediaStartTimeInSeconds = ((double) cutMediaStartTimeInMilliseconds) / 1000;
                                    double mediaChunkStartTimeInSeconds = ((double) mediaChunkStartTime.getTime()) / 1000;

                                    cutStartTimeInSeconds = playoutMediaStartTimeInSeconds - mediaChunkStartTimeInSeconds;
                                    */
                                    cutStartTimeInMilliSeconds = cutMediaStartTimeInMilliseconds - mediaChunkStartTime.getTime();

                                    if (mediaChunkStartTime.getTime() <= cutMediaEndTimeInMilliseconds
                                            && (nextMediaChunkStart == null || cutMediaEndTimeInMilliseconds <= nextMediaChunkStart.getTime()))
                                    {
                                        // playout start-end is within just one chunk

                                        lastChunkFound = true;

                                        // double playoutMediaEndTimeInSeconds = ((double) cutMediaEndTimeInMilliseconds) / 1000;

                                        // cutEndTimeInSeconds += (playoutMediaEndTimeInSeconds - mediaChunkStartTimeInSeconds);
                                    }
                                    /*
                                    else
                                    {
                                        cutEndTimeInSeconds += mediaChunkPeriodInSeconds;
                                    }
                                    */

                                    mLogger.info("Found first media chunk"
                                                    + ", cutMediaId: " + cutMediaId
                                                    + ", ftpFile.getName: " + mediaFile.getName()
                                                    + ", mediaChunkStartTime: " + mediaChunkStartTime
                                                    + ", sCutMediaStartTime: " + sCutMediaStartTime
                                                    + ", cutStartTimeInMilliSeconds: " + cutStartTimeInMilliSeconds
                                                    // + ", cutEndTimeInSeconds: " + cutEndTimeInSeconds
                                    );
                                }
                                else if (cutMediaStartTimeInMilliseconds <= mediaChunkStartTime.getTime()
                                        && (nextMediaChunkStart == null || nextMediaChunkStart.getTime() <= cutMediaEndTimeInMilliseconds))
                                {
                                    // internal chunk

                                    // fileTreeMap.put(mediaChunkStartTime, mediaFile);

                                    // cutEndTimeInSeconds += mediaChunkPeriodInSeconds;

                                    mLogger.info("Found internal media chunk"
                                                    + ", cutMediaId: " + cutMediaId
                                                    + ", mediaFile.getName: " + mediaFile.getName()
                                                    + ", mediaChunkStartTime: " + mediaChunkStartTime
                                                    + ", sCutMediaStartTime: " + sCutMediaStartTime
                                                    // + ", cutEndTimeInSeconds: " + cutEndTimeInSeconds
                                    );
                                }
                                else if (mediaChunkStartTime.getTime() <= cutMediaEndTimeInMilliseconds
                                        && (nextMediaChunkStart == null || cutMediaEndTimeInMilliseconds <= nextMediaChunkStart.getTime()))
                                {
                                    // last chunk

                                    lastChunkFound = true;

                                    // fileTreeMap.put(mediaChunkStartTime, mediaFile);

                                    // double playoutMediaEndTimeInSeconds = ((double) cutMediaEndTimeInMilliseconds) / 1000;
                                    // double mediaChunkStartTimeInSeconds = ((double) mediaChunkStartTime.getTime()) / 1000;

                                    // cutEndTimeInSeconds += (playoutMediaEndTimeInSeconds - mediaChunkStartTimeInSeconds);

                                    mLogger.info("Found last media chunk"
                                                    + ", cutMediaId: " + cutMediaId
                                                    + ", mediaFile.getName: " + mediaFile.getName()
                                                    + ", mediaChunkStartTime: " + mediaChunkStartTime
                                                    + ", sCutMediaStartTime: " + sCutMediaStartTime
                                                    // + ", cutEndTimeInSeconds: " + cutEndTimeInSeconds
                                                    // + ", playoutMediaEndTimeInSeconds: " + playoutMediaEndTimeInSeconds
                                                    // + ", mediaChunkStartTimeInSeconds: " + mediaChunkStartTimeInSeconds
                                    );
                                }
                                else
                                {
                                    // external chunk

                                    fileTreeMap.remove(mediaChunkStartTime);
                                    /*
                                    mLogger.info("Found external media chunk"
                                                    + ", ftpFile.getName: " + ftpFile.getName()
                                                    + ", mediaChunkStartTime: " + mediaChunkStartTime
                                                    + ", sCutMediaStartTime: " + sCutMediaStartTime
                                    );
                                    */
                                }
                            }
                        }
                        catch (Exception ex)
                        {
                            String errorMessage = "exception processing the " + filesArray.get(mediaFileIndex).getValue().getName() + " file. Exception: " + ex
                                    + ", cutMediaId: " + cutMediaId
                                    ;
                            mLogger.warn(errorMessage);

                            continue;
                        }
                    }

                    if (!firstChunkFound || !lastChunkFound)
                    {
                        String errorMessage = "First and/or Last chunk were not generated yet. No media files found"
                                + ", cutMediaId: " + cutMediaId
                                + ", cutMediaTitle: " + cutMediaTitle
                                + ", cutMediaChannel: " + cutMediaChannel
                                + ", sCutMediaStartTime: " + sCutMediaStartTime
                                + ", sCutMediaEndTime: " + sCutMediaEndTime
                                + ", firstChunkFound: " + firstChunkFound
                                + ", lastChunkFound: " + lastChunkFound
                                ;
                        mLogger.warn(errorMessage);

                        firstOrLastChunkNotFound = true;

                        // return firstOrLastChunkNotFound;
                        throw new Exception(errorMessage);
                    }

                    if (fileTreeMap.size() == 0)
                    {
                        String errorMessage = "No media files found"
                                + ", cutMediaId: " + cutMediaId
                                + ", cutMediaTitle: " + cutMediaTitle
                                + ", cutMediaChannel: " + cutMediaChannel
                                + ", sCutMediaStartTime: " + sCutMediaStartTime
                                + ", sCutMediaEndTime: " + sCutMediaEndTime
                                ;
                        mLogger.error(errorMessage);

                        throw new Exception(errorMessage);
                    }

                    if (cutStartTimeInMilliSeconds == -1) // || cutEndTimeInSeconds == 0)
                    {
                        String errorMessage = "No media files found"
                                + ", cutMediaId: " + cutMediaId
                                + ", cutMediaTitle: " + cutMediaTitle
                                + ", cutMediaChannel: " + cutMediaChannel
                                + ", sCutMediaStartTime: " + sCutMediaStartTime
                                + ", sCutMediaEndTime: " + sCutMediaEndTime
                                ;
                        mLogger.error(errorMessage);

                        throw new Exception(errorMessage);
                    }

                    // build json
                    JSONObject joWorkflow = null;
                    String keyContentLabel;
                    if (cutMediaChannel.equalsIgnoreCase("la1")
                            || cutMediaChannel.equalsIgnoreCase("la2"))
                    {
                        keyContentLabel = "Cut: " + cutMediaTitle;

                        joWorkflow = buildTVJson(cutMediaTitle, keyContentLabel, ingester, fileExtension,
                            addContentPull, cutMediaRetention,
                            cutStartTimeInMilliSeconds, cutMediaEndTimeInMilliseconds - cutMediaStartTimeInMilliseconds,
                            cutMediaId, cutMediaChannel, sCutMediaStartTime, sCutMediaEndTime,
                            fileTreeMap);
                    }
                    else
                    {
                        // radio

                        int audioTrackNumber = 0;
                        if (cutMediaChannel.equalsIgnoreCase("RETE UNO"))
                            audioTrackNumber = reteUnoTrackNumber;
                        else if (cutMediaChannel.equalsIgnoreCase("RETE DUE"))
                            audioTrackNumber = reteDueTrackNumber;
                        else if (cutMediaChannel.equalsIgnoreCase("RETE TRE"))
                            audioTrackNumber = reteTreTrackNumber;

                        keyContentLabel = "Extract: " + cutMediaTitle;

                        joWorkflow = buildRadioJson(cutMediaTitle, keyContentLabel, ingester, fileExtension,
                                addContentPull, cutMediaRetention,
                                cutStartTimeInMilliSeconds, cutMediaEndTimeInMilliseconds - cutMediaStartTimeInMilliseconds,
                                cutMediaId, cutMediaChannel, sCutMediaStartTime, sCutMediaEndTime,
                                audioTrackNumber, fileTreeMap);
                    }

                    {
                        List<IngestionResult> ingestionJobList = new ArrayList<>();

                        CatraMMS catraMMS = new CatraMMS();

                        IngestionResult workflowRoot = catraMMS.ingestWorkflow(userKey.toString(), apiKey,
                                joWorkflow.toString(4), ingestionJobList);

                        if (!addContentPull)
                            ingestBinaries(userKey, apiKey, cutMediaChannel, fileTreeMap, ingestionJobList);

                        for (IngestionResult ingestionResult: ingestionJobList)
                        {
                            if (ingestionResult.getLabel().equals(keyContentLabel))
                            {
                                cutIngestionJobKey = ingestionResult.getKey();

                                break;
                            }
                        }
                    }
                }
                catch (Exception ex)
                {
                    String errorMessage = "Exception: " + ex;
                    mLogger.error(errorMessage);

                    throw ex;
                }
            }

            // here cutIngestionJobKey should be != null, anyway we will do the check
            if (cutIngestionJobKey == null)
            {
                String errorMessage = "cutIngestionJobKey is null!!!";
                mLogger.error(errorMessage);

                throw new Exception(errorMessage);
            }

            response = "{\"status\": \"Success\", "
                    + "\"cutIngestionJobKey\": " + cutIngestionJobKey + ", "
                    + "\"errMsg\": null }";
            mLogger.info("cutMedia response: " + response);

            return Response.ok(response).build();
        }
        catch (Exception e)
        {
            String errorMessage = "cutMedia failed. Exception: " + e;
            mLogger.error(errorMessage);

            response = "{\"status\": \"Failed\", "
                    + "\"errMsg\": \"" + errorMessage + "\" "
                    + "}";

            mLogger.info("cutMedia response: " + response);

            if (e.getMessage().toLowerCase().contains("no media files found"))
                return Response.status(510).entity(response).build();
            else
                return Response.status(Response.Status.INTERNAL_SERVER_ERROR).entity(response).build();
        }
    }

    private Date getMediaChunkStartTime(String mediaFileName)
            throws Exception
    {
        // Channel_1-2018-06-26-10h00m39s.mp4
        // mLogger.info("###Processing of the " + ftpFile.getName() + " ftp file");

        String fileExtension = mediaFileName.substring(mediaFileName.lastIndexOf('.') + 1);

        int mediaChunkStartIndex = mediaFileName.length() - ("2018-06-26-10h00m39s.".length() + fileExtension.length());
        if (!Character.isDigit(mediaFileName.charAt(mediaChunkStartIndex)))
            mediaChunkStartIndex++; // case when hour is just one digit
        int mediaChunkEndIndex = mediaFileName.length() - (fileExtension.length() + 1);
        String sMediaChunkStartTime = mediaFileName.substring(mediaChunkStartIndex, mediaChunkEndIndex);

        DateFormat fileNameSimpleDateFormat = new SimpleDateFormat("yyyy-MM-dd-H'h'mm'm'ss's'");

        return fileNameSimpleDateFormat.parse(sMediaChunkStartTime);
    }

    private JSONObject buildTVJson(String keyTitle, String keyLabel, String ingester, String fileExtension,
                                   boolean addContentPull, String cutMediaRetention,
                                   Long cutStartTimeInMilliSeconds, Long cutMediaDurationInMilliSeconds,
                                   String cutMediaId, String cutMediaChannel, String sCutMediaStartTime, String sCutMediaEndTime,
                                   TreeMap<Date, File> fileTreeMap)
            throws Exception
    {
        try {
            JSONObject joWorkflow = new JSONObject();
            joWorkflow.put("Type", "Workflow");
            joWorkflow.put("Label", keyTitle);

            JSONObject joOnSuccess = new JSONObject();


            if (fileTreeMap.size() > 1)
            {
                JSONObject joGroupOfTasks = new JSONObject();
                joWorkflow.put("Task", joGroupOfTasks);

                joGroupOfTasks.put("Type", "GroupOfTasks");

                JSONObject joParameters = new JSONObject();
                joGroupOfTasks.put("Parameters", joParameters);

                joParameters.put("ExecutionType", "parallel");

                JSONArray jaTasks = new JSONArray();
                joParameters.put("Tasks", jaTasks);

                for (Date fileDate : fileTreeMap.keySet())
                {
                    File mediaFile = fileTreeMap.get(fileDate);

                    JSONObject joAddContent = new JSONObject();
                    jaTasks.put(joAddContent);

                    joAddContent.put("Label", mediaFile.getName());
                    joAddContent.put("Type", "Add-Content");

                    JSONObject joAddContentParameters = new JSONObject();
                    joAddContent.put("Parameters", joAddContentParameters);

                    joAddContentParameters.put("Ingester", ingester);
                    joAddContentParameters.put("FileFormat", fileExtension);
                    joAddContentParameters.put("Retention", "0");
                    joAddContentParameters.put("Title", mediaFile.getName());
                    joAddContentParameters.put("FileSizeInBytes", mediaFile.length());
                    if (addContentPull)
                        joAddContentParameters.put("SourceURL", "copy://" + mediaFile.getAbsolutePath());
                }

                joGroupOfTasks.put("OnSuccess", joOnSuccess);
            }
            else
            {
                File mediaFile = fileTreeMap.firstEntry().getValue();

                JSONObject joAddContent = new JSONObject();
                joWorkflow.put("Task", joAddContent);

                joAddContent.put("Label", mediaFile.getName());
                joAddContent.put("Type", "Add-Content");

                JSONObject joAddContentParameters = new JSONObject();
                joAddContent.put("Parameters", joAddContentParameters);

                joAddContentParameters.put("Ingester", ingester);
                joAddContentParameters.put("FileFormat", "mp4");
                joAddContentParameters.put("Retention", "0");
                joAddContentParameters.put("Title", mediaFile.getName());
                joAddContentParameters.put("FileSizeInBytes", mediaFile.length());
                if (addContentPull)
                    joAddContentParameters.put("SourceURL", "copy://" + mediaFile.getAbsolutePath());

                joAddContent.put("OnSuccess", joOnSuccess);
            }

            JSONObject joConcatDemux = new JSONObject();
            {
                joOnSuccess.put("Task", joConcatDemux);

                joConcatDemux.put("Label", "Concat: " + keyTitle);
                joConcatDemux.put("Type", "Concat-Demuxer");

                JSONObject joConcatDemuxParameters = new JSONObject();
                joConcatDemux.put("Parameters", joConcatDemuxParameters);

                joConcatDemuxParameters.put("Ingester", ingester);
                joConcatDemuxParameters.put("Retention", "0");
                joConcatDemuxParameters.put("Title", "Concat: " + keyTitle);
            }

            JSONObject joCut = new JSONObject();
            {
                JSONObject joConcatDemuxOnSuccess = new JSONObject();
                joConcatDemux.put("OnSuccess", joConcatDemuxOnSuccess);

                joConcatDemuxOnSuccess.put("Task", joCut);

                joCut.put("Label", keyLabel);
                joCut.put("Type", "Cut");

                JSONObject joCutParameters = new JSONObject();
                joCut.put("Parameters", joCutParameters);

                joCutParameters.put("Ingester", ingester);
                joCutParameters.put("Retention", cutMediaRetention);
                joCutParameters.put("Title", keyTitle);
                {
                    double cutStartTimeInSeconds = ((double) cutStartTimeInMilliSeconds) / 1000;
                    joCutParameters.put("StartTimeInSeconds", cutStartTimeInSeconds);

                    Calendar calendar = Calendar.getInstance();
                    calendar.setTime(new Date(cutStartTimeInMilliSeconds));
                    calendar.add(Calendar.MILLISECOND, (int) (cutMediaDurationInMilliSeconds.longValue()));

                    double cutEndTimeInSeconds = ((double) calendar.getTime().getTime()) / 1000;
                    joCutParameters.put("EndTimeInSeconds", cutEndTimeInSeconds);
                }

                {
                    JSONObject joCutUserData = new JSONObject();
                    joCutParameters.put("UserData", joCutUserData);

                    joCutUserData.put("Channel", cutMediaChannel);
                    joCutUserData.put("StartTime", sCutMediaStartTime);
                    joCutUserData.put("EndTime", sCutMediaEndTime);
                }
            }

            {
                JSONObject joCutOnSuccess = new JSONObject();
                joCut.put("OnSuccess", joCutOnSuccess);

                JSONObject joGroupOfTasks = new JSONObject();
                joCutOnSuccess.put("Task", joGroupOfTasks);

                joGroupOfTasks.put("Type", "GroupOfTasks");

                JSONObject joParameters = new JSONObject();
                joGroupOfTasks.put("Parameters", joParameters);

                joParameters.put("ExecutionType", "parallel");

                JSONArray jaTasks = new JSONArray();
                joParameters.put("Tasks", jaTasks);

                {
                    JSONObject joCallback = new JSONObject();
                    jaTasks.put(joCallback);

                    joCallback.put("Label", "Callback: " + keyTitle);
                    joCallback.put("Type", "HTTP-Callback");

                    JSONObject joCallbackParameters = new JSONObject();
                    joCallback.put("Parameters", joCallbackParameters);

                    joCallbackParameters.put("Protocol", "http");
                    joCallbackParameters.put("HostName", "mp-backend.rsi.ch");
                    joCallbackParameters.put("Port", 80);
                    joCallbackParameters.put("URI",
                            "/metadataProcessorService/rest/veda/playoutMedia/" + cutMediaId + "/mmsFinished");
                    joCallbackParameters.put("Parameters", "");
                    joCallbackParameters.put("Method", "GET");
                    joCallbackParameters.put("Timeout", 60);

                                /*
                                JSONArray jaHeaders = new JSONArray();
                                joCallbackParameters.put("Headers", jaHeaders);

                                jaHeaders.put("");
                                */
                }

                {
                    JSONObject joEncode = new JSONObject();
                    jaTasks.put(joEncode);

                    joEncode.put("Label", "Encode: " + keyTitle);
                    joEncode.put("Type", "Encode");

                    JSONObject joEncodeParameters = new JSONObject();
                    joEncode.put("Parameters", joEncodeParameters);

                    joEncodeParameters.put("EncodingPriority", "Low");
                    joEncodeParameters.put("EncodingProfileLabel", "MMS_H264_veryslow_360p25_aac_92");
                }
            }
            mLogger.info("Ready for the ingest"
                    + ", sCutMediaStartTime: " + sCutMediaStartTime
                    + ", sCutMediaEndTime: " + sCutMediaEndTime
                    // + ", cutStartTimeInSeconds: " + cutStartTimeInSeconds
                    // + ", cutEndTimeInSeconds: " + cutEndTimeInSeconds
                    + ", fileTreeMap.size: " + fileTreeMap.size()
                    + ", json Workflow: " + joWorkflow.toString(4));

            return joWorkflow;
        }
        catch (Exception e)
        {
            String errorMessage = "buildTVJson failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw e;
        }
    }

    private JSONObject buildRadioJson(String keyTitle, String keyLabel, String ingester, String fileExtension,
                                   boolean addContentPull, String cutMediaRetention,
                                   Long cutStartTimeInMilliSeconds, Long cutMediaDurationInMilliSeconds,
                                   String cutMediaId, String cutMediaChannel, String sCutMediaStartTime, String sCutMediaEndTime,
                                   int audioTrackNumber, TreeMap<Date, File> fileTreeMap)
            throws Exception
    {
        try {
            JSONObject joWorkflow = new JSONObject();
            joWorkflow.put("Type", "Workflow");
            joWorkflow.put("Label", keyTitle);

            JSONObject joOnSuccess = new JSONObject();


            if (fileTreeMap.size() > 1)
            {
                JSONObject joGroupOfTasks = new JSONObject();
                joWorkflow.put("Task", joGroupOfTasks);

                joGroupOfTasks.put("Type", "GroupOfTasks");

                JSONObject joParameters = new JSONObject();
                joGroupOfTasks.put("Parameters", joParameters);

                joParameters.put("ExecutionType", "parallel");

                JSONArray jaTasks = new JSONArray();
                joParameters.put("Tasks", jaTasks);

                for (Date fileDate : fileTreeMap.keySet())
                {
                    File mediaFile = fileTreeMap.get(fileDate);

                    JSONObject joAddContent = new JSONObject();
                    jaTasks.put(joAddContent);

                    joAddContent.put("Label", mediaFile.getName());
                    joAddContent.put("Type", "Add-Content");

                    JSONObject joAddContentParameters = new JSONObject();
                    joAddContent.put("Parameters", joAddContentParameters);

                    joAddContentParameters.put("Ingester", ingester);
                    joAddContentParameters.put("FileFormat", fileExtension);
                    joAddContentParameters.put("Retention", "0");
                    joAddContentParameters.put("Title", mediaFile.getName());
                    joAddContentParameters.put("FileSizeInBytes", mediaFile.length());
                    if (addContentPull)
                        joAddContentParameters.put("SourceURL", "copy://" + mediaFile.getAbsolutePath());
                }

                joGroupOfTasks.put("OnSuccess", joOnSuccess);
            }
            else
            {
                File mediaFile = fileTreeMap.firstEntry().getValue();

                JSONObject joAddContent = new JSONObject();
                joWorkflow.put("Task", joAddContent);

                joAddContent.put("Label", mediaFile.getName());
                joAddContent.put("Type", "Add-Content");

                JSONObject joAddContentParameters = new JSONObject();
                joAddContent.put("Parameters", joAddContentParameters);

                joAddContentParameters.put("Ingester", ingester);
                joAddContentParameters.put("FileFormat", "mp4");
                joAddContentParameters.put("Retention", "0");
                joAddContentParameters.put("Title", mediaFile.getName());
                joAddContentParameters.put("FileSizeInBytes", mediaFile.length());
                if (addContentPull)
                    joAddContentParameters.put("SourceURL", "copy://" + mediaFile.getAbsolutePath());

                joAddContent.put("OnSuccess", joOnSuccess);
            }

            JSONObject joConcatDemux = new JSONObject();
            {
                joOnSuccess.put("Task", joConcatDemux);

                joConcatDemux.put("Label", "Concat: " + keyTitle);
                joConcatDemux.put("Type", "Concat-Demuxer");

                JSONObject joConcatDemuxParameters = new JSONObject();
                joConcatDemux.put("Parameters", joConcatDemuxParameters);

                joConcatDemuxParameters.put("Ingester", ingester);
                joConcatDemuxParameters.put("Retention", "0");
                joConcatDemuxParameters.put("Title", "Concat: " + keyTitle);
            }

            JSONObject joCut = new JSONObject();
            {
                JSONObject joConcatDemuxOnSuccess = new JSONObject();
                joConcatDemux.put("OnSuccess", joConcatDemuxOnSuccess);

                joConcatDemuxOnSuccess.put("Task", joCut);

                joCut.put("Label", "Cut: " + keyTitle);
                joCut.put("Type", "Cut");

                JSONObject joCutParameters = new JSONObject();
                joCut.put("Parameters", joCutParameters);

                joCutParameters.put("Ingester", ingester);
                joCutParameters.put("Retention", "0");
                joCutParameters.put("Title", "Cut: " + keyTitle);
                {
                    double cutStartTimeInSeconds = ((double) cutStartTimeInMilliSeconds) / 1000;
                    joCutParameters.put("StartTimeInSeconds", cutStartTimeInSeconds);

                    Calendar calendar = Calendar.getInstance();
                    calendar.setTime(new Date(cutStartTimeInMilliSeconds));
                    calendar.add(Calendar.MILLISECOND, (int) (cutMediaDurationInMilliSeconds.longValue()));

                    double cutEndTimeInSeconds = ((double) calendar.getTime().getTime()) / 1000;
                    joCutParameters.put("EndTimeInSeconds", cutEndTimeInSeconds);
                }
            }

            JSONObject joExtract = new JSONObject();
            {
                JSONObject joCutOnSuccess = new JSONObject();
                joCut.put("OnSuccess", joCutOnSuccess);

                joCutOnSuccess.put("Task", joExtract);

                joExtract.put("Label", keyLabel);
                joExtract.put("Type", "Extract-Tracks");

                JSONObject joExtractParameters = new JSONObject();
                joExtract.put("Parameters", joExtractParameters);

                joExtractParameters.put("Ingester", ingester);
                joExtractParameters.put("Retention", cutMediaRetention);
                joExtractParameters.put("Title", keyTitle);
                {
                    joExtractParameters.put("OutputFileFormat", "mp4");

                    JSONArray jaTracks = new JSONArray();
                    joExtractParameters.put("Tracks", jaTracks);

                    JSONObject joTrack = new JSONObject();
                    jaTracks.put(joTrack);

                    joTrack.put("TrackType", "audio");
                    joTrack.put("TrackNumber", audioTrackNumber);
                }

                {
                    JSONObject joCutAndExtractUserData = new JSONObject();
                    joExtractParameters.put("UserData", joCutAndExtractUserData);

                    joCutAndExtractUserData.put("Channel", cutMediaChannel);
                    joCutAndExtractUserData.put("StartTime", sCutMediaStartTime);
                    joCutAndExtractUserData.put("EndTime", sCutMediaEndTime);
                }
            }

            {
                JSONObject joExtractOnSuccess = new JSONObject();
                joExtract.put("OnSuccess", joExtractOnSuccess);

                JSONObject joGroupOfTasks = new JSONObject();
                joExtractOnSuccess.put("Task", joGroupOfTasks);

                joGroupOfTasks.put("Type", "GroupOfTasks");

                JSONObject joParameters = new JSONObject();
                joGroupOfTasks.put("Parameters", joParameters);

                joParameters.put("ExecutionType", "parallel");

                JSONArray jaTasks = new JSONArray();
                joParameters.put("Tasks", jaTasks);

                {
                    JSONObject joCallback = new JSONObject();
                    jaTasks.put(joCallback);

                    joCallback.put("Label", "Callback: " + keyTitle);
                    joCallback.put("Type", "HTTP-Callback");

                    JSONObject joCallbackParameters = new JSONObject();
                    joCallback.put("Parameters", joCallbackParameters);

                    joCallbackParameters.put("Protocol", "http");
                    joCallbackParameters.put("HostName", "mp-backend.rsi.ch");
                    joCallbackParameters.put("Port", 80);
                    joCallbackParameters.put("URI",
                            "/metadataProcessorService/rest/veda/playoutMedia/" + cutMediaId + "/mmsFinished");
                    joCallbackParameters.put("Parameters", "");
                    joCallbackParameters.put("Method", "GET");
                    joCallbackParameters.put("Timeout", 60);

                                /*
                                JSONArray jaHeaders = new JSONArray();
                                joCallbackParameters.put("Headers", jaHeaders);

                                jaHeaders.put("");
                                */
                }

                {
                    JSONObject joEncode = new JSONObject();
                    jaTasks.put(joEncode);

                    joEncode.put("Label", "Encode: " + keyTitle);
                    joEncode.put("Type", "Encode");

                    JSONObject joEncodeParameters = new JSONObject();
                    joEncode.put("Parameters", joEncodeParameters);

                    joEncodeParameters.put("EncodingPriority", "Low");
                    joEncodeParameters.put("EncodingProfileLabel", "MMS_aac_92");
                }
            }
            mLogger.info("Ready for the ingest"
                    + ", sCutMediaStartTime: " + sCutMediaStartTime
                    + ", sCutMediaEndTime: " + sCutMediaEndTime
                    // + ", cutStartTimeInSeconds: " + cutStartTimeInSeconds
                    // + ", cutEndTimeInSeconds: " + cutEndTimeInSeconds
                    + ", fileTreeMap.size: " + fileTreeMap.size()
                    + ", json Workflow: " + joWorkflow.toString(4));

            return joWorkflow;
        }
        catch (Exception e)
        {
            String errorMessage = "buildTVJson failed. Exception: " + e;
            mLogger.error(errorMessage);

            throw e;
        }
    }

    private void ingestBinaries(Long userKey, String apiKey,
                                String channel,
                                TreeMap<Date, File> fileTreeMap,
                                List<IngestionResult> ingestionJobList)
            throws Exception
    {
        try
        {
            for (Date fileDate : fileTreeMap.keySet())
            {
                File mediaFile = fileTreeMap.get(fileDate);

                IngestionResult fileIngestionTask = null;
                try
                {
                    for (IngestionResult ingestionTaskResult: ingestionJobList)
                    {
                        if (ingestionTaskResult.getLabel().equalsIgnoreCase(mediaFile.getName()))
                        {
                            fileIngestionTask = ingestionTaskResult;

                            break;
                        }
                    }

                    if (fileIngestionTask == null)
                    {
                        String errorMessage = "Content to be pushed was not found among the IngestionResults"
                                + ", mediaFile.getName: " + mediaFile.getName()
                                ;
                        mLogger.error(errorMessage);

                        continue;
                    }

                    mLogger.info("ftpClient.retrieveFileStream. Channel: " + channel
                            + ", Name: " + mediaFile.getName() + ", size (bytes): " + mediaFile.length());
                    InputStream inputStream = new DataInputStream(new FileInputStream(mediaFile));

                    CatraMMS catraMMS = new CatraMMS();
                    catraMMS.ingestBinaryContent(userKey.toString(), apiKey,
                            inputStream, mediaFile.length(),
                            fileIngestionTask.getKey());
                }
                catch (Exception e)
                {
                    String errorMessage = "Upload Push Content failed"
                            + ", fileIngestionTask.getLabel: " + fileIngestionTask.getLabel()
                            + ", Exception: " + e
                            ;
                    mLogger.error(errorMessage);
                }
            }
        }
        catch (Exception e)
        {
            String errorMessage = "Exception: " + e;
            mLogger.error(errorMessage);

            throw e;
        }
    }
}