package com.catramms.backing.workflowEditor.Properties;

import com.catramms.backing.workflowEditor.utility.IngestionData;
import org.apache.log4j.Logger;
import org.json.JSONArray;
import org.json.JSONObject;

import java.io.*;
import java.util.ArrayList;
import java.util.List;

public class GroupOfTasksProperties extends WorkflowProperties implements Serializable {

    private static final Logger mLogger = Logger.getLogger(GroupOfTasksProperties.class);

    private String groupOfTaskExecutionType;
    private List<WorkflowProperties> tasks = new ArrayList<>();

    public GroupOfTasksProperties(String positionX, String positionY,
                                  int elementId, String label)
    {
        super(positionX, positionY, elementId, label, "GroupOfTasks" + "-icon.png", "GroupOfTasks", "GroupOfTasks");

        groupOfTaskExecutionType = "parallel";
    }

    public GroupOfTasksProperties clone()
    {
        String localGroupOfTaskExecutionType = getGroupOfTaskExecutionType();

        GroupOfTasksProperties groupOfTasksProperties = new GroupOfTasksProperties(
                super.getPositionX(), super.getPositionY(),
                super.getElementId(), super.getLabel());

        groupOfTasksProperties.setGroupOfTaskExecutionType(localGroupOfTaskExecutionType);
        groupOfTasksProperties.setTasks(getTasks());

        return groupOfTasksProperties;
    }

    public void setData(GroupOfTasksProperties workflowProperties)
    {
        super.setData(workflowProperties);

        setGroupOfTaskExecutionType(workflowProperties.getGroupOfTaskExecutionType());
        setTasks(workflowProperties.getTasks());
    }

    public void setData(JSONObject jsonWorkflowElement)
    {
        try {
            super.setData(jsonWorkflowElement);

            JSONObject joParameters = jsonWorkflowElement.getJSONObject("Parameters");

            if (joParameters.has("GroupOfTaskExecutionType") && !joParameters.getString("GroupOfTaskExecutionType").equalsIgnoreCase(""))
                setGroupOfTaskExecutionType(joParameters.getString("GroupOfTaskExecutionType"));
        }
        catch (Exception e)
        {
            mLogger.error("WorkflowProperties:setData failed, exception: " + e);
        }
    }

    public JSONObject buildWorkflowElementJson(IngestionData ingestionData)
            throws Exception
    {
        JSONObject jsonWorkflowElement = new JSONObject();

        try
        {
            jsonWorkflowElement.put("Type", super.getType());

            JSONObject joParameters = new JSONObject();
            jsonWorkflowElement.put("Parameters", joParameters);

            mLogger.info("task.getType: " + super.getType());

            joParameters.put("ExecutionType", getGroupOfTaskExecutionType());

            JSONArray jaTasks = new JSONArray();
            joParameters.put("Tasks", jaTasks);

            // tasks
            {
                int tasksNumber = getTasks().size();
                mLogger.info("GroupOfTasksProperties::buildWorkflowElementJson"
                        + ", tasksNumber: " + tasksNumber
                );
                for (int taskIndex = 0; taskIndex < tasksNumber; taskIndex++)
                {
                    WorkflowProperties taskWorkflowProperties = getTasks().get(taskIndex);

                    jaTasks.put(taskWorkflowProperties.buildWorkflowElementJson(ingestionData));
                }
            }

            super.addEventsPropertiesToJson(jsonWorkflowElement, ingestionData);
        }
        catch (Exception e)
        {
            mLogger.error("buildWorkflowJson failed: " + e);

            throw e;
        }

        return jsonWorkflowElement;
    }

    public String getGroupOfTaskExecutionType() {
        return groupOfTaskExecutionType;
    }

    public void setGroupOfTaskExecutionType(String groupOfTaskExecutionType) {
        this.groupOfTaskExecutionType = groupOfTaskExecutionType;
    }

    public List<WorkflowProperties> getTasks() {
        return tasks;
    }

    public void setTasks(List<WorkflowProperties> tasks) {
        this.tasks = tasks;
    }
}
